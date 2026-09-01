/*
 * port.c -- message ports.
 */
#include <eb/msg.h>
#include <eb/object.h>
#include <eb/thread.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/panic.h>

#define MAX_WAITERS 8

/* What a message looks like once it is in the queue: the objects
 * themselves, not handles. A handle belongs to a table; an object in
 * flight belongs to nobody. */
typedef struct {
    u64     tag;
    u32     nwords;
    u32     ncaps;
    u64     words[MSG_MAX_WORDS];
    object *caps[MSG_MAX_CAPS];
    u32     rights[MSG_MAX_CAPS];

    /* Who sent it, stamped by the kernel on the way in. Not part of the
     * message a program receives -- a sender cannot claim to be
     * somebody, and a receiver in ring 3 is not handed kernel pointers.
     * The console server reads it to attribute what programs say. */
    const char *from;
} queued;

typedef struct {
    u64      capacity;
    u64      head, tail, count;
    u32      nwaiters;
    thread  *waiters[MAX_WAITERS];
    /* the ring follows */
} port_state;

static type_id TYPE_PORT;

void port_init(void)
{
    TYPE_PORT = type_register("port");
}

type_id port_type(void) { return TYPE_PORT; }

static port_state *state_of(object *p)
{
    return (port_state *)obj_data(p);
}

static queued *ring_of(port_state *s)
{
    return (queued *)((u8 *)s + sizeof(port_state));
}

object *port_create(u64 capacity)
{
    if (capacity == 0) capacity = 8;

    u64 bytes = sizeof(port_state) + capacity * sizeof(queued);
    object *p = obj_create(TYPE_PORT, bytes, 0);
    if (!p) return NULL;

    port_state *s = state_of(p);
    s->capacity = capacity;
    return p;
}

u64 port_pending(object *p)
{
    if (!p || obj_type(p) != TYPE_PORT) return 0;
    return state_of(p)->count;
}

void port_drop_queued(object *p)
{
    /* Empties the queue and lets go of everything it was carrying.
     *
     * A message in flight holds its capabilities the way a slot holds
     * its target, and a port that dies with messages inside would
     * otherwise take those holds to the grave: never released, never
     * again releasable. Called when a port is torn down, however the
     * teardown came about. */
    if (!p || obj_type(p) != TYPE_PORT) return;

    u64 flags = irq_save();
    port_state *s = state_of(p);
    queued *ring = ring_of(s);
    while (s->count > 0) {
        queued *q = &ring[s->head];
        for (u32 i = 0; i < q->ncaps; i++) {
            if (q->caps[i]) obj_release(q->caps[i]);
            q->caps[i] = NULL;
        }
        q->ncaps = 0;
        s->head = (s->head + 1) % s->capacity;
        s->count--;
    }
    irq_restore(flags);
}

void port_visit_queued(object *p, void (*visit)(object *o))
{
    /* A capability in flight is held by nobody: it has left the
     * sender's table and has not yet reached the receiver's. Without
     * this the collector would see a message's contents as unreachable
     * and free them out from under a message that is about to be
     * delivered. */
    if (!p || obj_type(p) != TYPE_PORT || !visit) return;

    port_state *s = state_of(p);
    queued *ring = ring_of(s);
    for (u64 n = 0; n < s->count; n++) {
        queued *q = &ring[(s->head + n) % s->capacity];
        for (u32 i = 0; i < q->ncaps; i++)
            if (q->caps[i]) visit(q->caps[i]);
    }
}

/* ------------------------------------------------------------------ */

bool port_post(object *p, const message *m,
               object **carried, const u32 *rights, u32 ncaps,
               const char *from)
{
    if (!p || obj_type(p) != TYPE_PORT || !m) return false;
    if (ncaps > MSG_MAX_CAPS) return false;

    u64 flags = irq_save();
    port_state *s = state_of(p);

    if (s->count >= s->capacity) {
        irq_restore(flags);
        return false;
    }

    queued *q = &ring_of(s)[s->tail];
    q->tag    = m->tag;
    q->nwords = m->nwords;
    q->ncaps  = ncaps;
    q->from   = from;
    for (u32 i = 0; i < m->nwords; i++) q->words[i] = m->words[i];
    for (u32 i = 0; i < ncaps; i++) {
        q->caps[i] = carried[i];
        q->rights[i] = rights[i];
        obj_retain(carried[i]);   /* the queue holds it while in flight */
    }

    s->tail = (s->tail + 1) % s->capacity;
    s->count++;

    /* Wake one waiter, if any is parked here. */
    thread *woken = NULL;
    if (s->nwaiters > 0) {
        woken = s->waiters[0];
        for (u32 i = 1; i < s->nwaiters; i++) s->waiters[i - 1] = s->waiters[i];
        s->nwaiters--;
    }
    irq_restore(flags);

    if (woken) sched_wake(woken);
    return true;
}

bool port_send(domain *from, cap_handle h, const message *m)
{
    if (!m || m->nwords > MSG_MAX_WORDS || m->ncaps > MSG_MAX_CAPS)
        return false;

    object *p = cap_lookup(from, h, CAP_CALL);
    if (!p || obj_type(p) != TYPE_PORT) return false;

    /* Resolve the attachments before touching the queue: a message that
     * carries a handle the sender does not actually hold must fail
     * whole, not half. */
    object *attach[MSG_MAX_CAPS] = { NULL, NULL };
    u32     rights[MSG_MAX_CAPS] = { 0, 0 };

    for (u32 i = 0; i < m->ncaps; i++) {
        u32 have = cap_rights(from, m->caps[i]);
        if (have == 0) return false;
        if (!(have & CAP_GRANT)) return false;   /* not the sender's to give */

        u32 pass = have & m->cap_mask[i];
        if (pass == 0) return false;

        attach[i] = cap_lookup(from, m->caps[i], pass);
        if (!attach[i]) return false;
        rights[i] = pass;
    }

    /* The checks are done; the rest is the same work for everyone. The
     * label is the sender's domain label -- what the kernel knows the
     * sender to be, not what the sender says. */
    return port_post(p, m, attach, rights, m->ncaps, domain_label(from));
}

/* Takes the next message out. Interrupts must be off. */
static bool dequeue(domain *to, port_state *s, message *out,
                    const char **from)
{
    if (s->count == 0) return false;

    queued *q = &ring_of(s)[s->head];
    s->head = (s->head + 1) % s->capacity;
    s->count--;

    if (from) *from = q->from;
    out->tag    = q->tag;
    out->nwords = q->nwords;
    out->ncaps  = 0;
    for (u32 i = 0; i < q->nwords; i++) out->words[i] = q->words[i];

    for (u32 i = 0; i < q->ncaps; i++) {
        /* Now it becomes a handle again, in the receiver's table, with
         * the rights the sender let go of. */
        cap_handle nh = cap_insert(to, q->caps[i], q->rights[i]);
        obj_release(q->caps[i]);           /* the queue lets go */
        q->caps[i] = NULL;

        if (nh != CAP_INVALID) {
            out->caps[out->ncaps] = nh;
            out->cap_mask[out->ncaps] = q->rights[i];
            out->ncaps++;
        }
        /* If the receiver's table is full the capability is dropped
         * rather than forced on it. The message still arrives; the
         * count of attachments says what came with it. */
    }
    return true;
}

bool port_try_receive(domain *to, cap_handle h, message *out)
{
    object *p = cap_lookup(to, h, CAP_READ);
    if (!p || obj_type(p) != TYPE_PORT || !out) return false;

    u64 flags = irq_save();
    bool got = dequeue(to, state_of(p), out, NULL);
    irq_restore(flags);
    return got;
}

bool port_receive_labelled(domain *to, cap_handle h, message *out,
                           const char **from)
{
    object *p = cap_lookup(to, h, CAP_READ);
    if (!p || obj_type(p) != TYPE_PORT || !out) return false;

    for (;;) {
        u64 flags = irq_save();
        port_state *s = state_of(p);

        if (dequeue(to, s, out, from)) {
            irq_restore(flags);
            return true;
        }

        if (s->nwaiters >= MAX_WAITERS) {
            irq_restore(flags);
            return false;
        }

        /* Join the wait list and go to sleep without opening a window
         * in between: a sender that arrives now sees us on the list. */
        s->waiters[s->nwaiters++] = sched_current();
        sched_block();

        /* Woken to end, not to receive: step off the wait list --
         * a sender's wake removes its waiter, a condemning one does
         * not -- and answer false, so the way out leads through the
         * syscall door where the ending is waiting. */
        if (thread_condemned(sched_current())) {
            for (u32 i = 0; i < s->nwaiters; i++) {
                if (s->waiters[i] != sched_current()) continue;
                for (u32 k = i + 1; k < s->nwaiters; k++)
                    s->waiters[k - 1] = s->waiters[k];
                s->nwaiters--;
                break;
            }
            irq_restore(flags);
            return false;
        }
        irq_restore(flags);
    }
}

bool port_receive(domain *to, cap_handle h, message *out)
{
    return port_receive_labelled(to, h, out, NULL);
}

/* ------------------------------------------------------------------ */
/* Self test                                                           */
/* ------------------------------------------------------------------ */

static domain *server_domain;
static domain *client_domain;
static cap_handle server_port_read;   /* server's own, read + call */
static cap_handle client_port_send;   /* client's, send only */
static volatile bool server_ready;
static volatile bool server_saw_message;
static volatile bool server_could_read_payload;
static volatile u64  server_received_value;

static void server_thread(void *arg)
{
    (void)arg;
    message m;

    server_ready = true;
    if (!port_receive(server_domain, server_port_read, &m)) return;

    server_saw_message = true;
    server_received_value = m.words[0];

    /* The message brought a capability with it. Whether it is usable,
     * and how far, is the point of the test. */
    if (m.ncaps == 1) {
        object *o = cap_lookup(server_domain, m.caps[0], CAP_READ);
        if (o) {
            const u8 *d = (const u8 *)obj_data(o);
            server_could_read_payload = (d && d[0] == 0x5A);
        }
        /* Write was not passed on, so this must come back empty. */
        if (cap_lookup(server_domain, m.caps[0], CAP_WRITE))
            server_could_read_payload = false;
    }
}

bool msg_selftest(void)
{
    u64 live_before = obj_live_count();

    server_domain = domain_create("server", 16);
    client_domain = domain_create("client", 16);
    if (!server_domain || !client_domain) return false;

    object *port = port_create(4);
    if (!port) return false;

    /* The server keeps a capability that can both receive and send. */
    server_port_read = cap_insert(server_domain, port,
                                  CAP_READ | CAP_CALL | CAP_GRANT);
    /* The client gets one that can only send. */
    client_port_send = cap_insert(client_domain, port, CAP_CALL);
    obj_release(port);

    if (server_port_read == CAP_INVALID || client_port_send == CAP_INVALID)
        return false;

    /* A send-only capability must not be able to drain the port. */
    message probe;
    if (port_try_receive(client_domain, client_port_send, &probe)) {
        kprintf("msg:  a send-only capability could receive\n");
        return false;
    }

    /* Something worth passing along, readable but not writable. */
    object *payload = obj_create(TYPE_BYTES, 8, 0);
    if (!payload) return false;
    ((u8 *)obj_data(payload))[0] = 0x5A;

    cap_handle c_payload = cap_insert(client_domain, payload,
                                      CAP_READ | CAP_WRITE | CAP_GRANT);
    obj_release(payload);

    server_ready = false;
    server_saw_message = false;
    server_could_read_payload = false;
    server_received_value = 0;

    thread *srv = thread_create("server", server_thread, NULL, server_domain);
    if (!srv) return false;

    /* Let the server get as far as blocking on the port. */
    for (u32 i = 0; i < 1000 && !server_ready; i++) sched_yield();
    if (!server_ready) {
        kprintf("msg:  the server thread never started\n");
        return false;
    }
    for (u32 i = 0; i < 100; i++) sched_yield();

    message m = { 0 };
    m.tag = 0x48454C4C4FULL;     /* "HELLO" */
    m.nwords = 1;
    m.words[0] = 0xC0FFEE;
    m.ncaps = 1;
    m.caps[0] = c_payload;
    m.cap_mask[0] = CAP_READ;    /* read only, deliberately */

    if (!port_send(client_domain, client_port_send, &m)) {
        kprintf("msg:  send failed\n");
        return false;
    }

    for (u32 i = 0; i < 1000 && !server_saw_message; i++) sched_yield();

    if (!server_saw_message) {
        kprintf("msg:  the message never arrived\n");
        return false;
    }
    if (server_received_value != 0xC0FFEE) {
        kprintf("msg:  the payload word arrived as 0x%llx\n",
                server_received_value);
        return false;
    }
    if (!server_could_read_payload) {
        kprintf("msg:  the attached capability did not arrive with the "
                "rights it was sent with\n");
        return false;
    }

    /* The port is bounded, and a full port refuses rather than
     * overwriting or blocking the sender. */
    message filler = { 0 };
    filler.tag = 1;
    u32 accepted = 0;
    for (u32 i = 0; i < 10; i++)
        if (port_send(client_domain, client_port_send, &filler)) accepted++;
    if (accepted != 4) {
        kprintf("msg:  a port with room for 4 took %u messages\n", accepted);
        return false;
    }

    domain_destroy(client_domain);
    domain_destroy(server_domain);

    if (obj_live_count() != live_before) {
        kprintf("msg:  %llu objects outlived the test\n",
                obj_live_count() - live_before);
        return false;
    }
    return true;
}
