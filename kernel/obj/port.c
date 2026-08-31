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

/* ------------------------------------------------------------------ */

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

    u64 flags = irq_save();
    port_state *s = state_of(p);

    if (s->count >= s->capacity) {
        irq_restore(flags);
        return false;
    }

    queued *q = &ring_of(s)[s->tail];
    q->tag    = m->tag;
    q->nwords = m->nwords;
    q->ncaps  = m->ncaps;
    for (u32 i = 0; i < m->nwords; i++) q->words[i] = m->words[i];
    for (u32 i = 0; i < m->ncaps; i++) {
        q->caps[i] = attach[i];
        q->rights[i] = rights[i];
        obj_retain(attach[i]);   /* the queue holds it while in flight */
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

/* Takes the next message out. Interrupts must be off. */
static bool dequeue(domain *to, port_state *s, message *out)
{
    if (s->count == 0) return false;

    queued *q = &ring_of(s)[s->head];
    s->head = (s->head + 1) % s->capacity;
    s->count--;

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
    bool got = dequeue(to, state_of(p), out);
    irq_restore(flags);
    return got;
}

bool port_receive(domain *to, cap_handle h, message *out)
{
    object *p = cap_lookup(to, h, CAP_READ);
    if (!p || obj_type(p) != TYPE_PORT || !out) return false;

    for (;;) {
        u64 flags = irq_save();
        port_state *s = state_of(p);

        if (dequeue(to, s, out)) {
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
        irq_restore(flags);
    }
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
