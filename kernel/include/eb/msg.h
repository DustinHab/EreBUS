#ifndef EB_MSG_H
#define EB_MSG_H

#include <eb/types.h>
#include <eb/cap.h>

/* Typed messages and the ports they travel through.
 *
 * There is no stdin, no stdout, no pipe carrying bytes. A message is a
 * tag saying what it is, a few values, and up to two capabilities. The
 * receiver knows what it is holding without parsing anything, and there
 * is no format for a sender to lie about.
 *
 * Capabilities travelling in a message are the interesting part. What
 * crosses is not a handle -- a handle means nothing outside the table
 * it came from -- but the object itself together with the rights the
 * sender chose to pass. The receiver gets a fresh handle in its own
 * table, and the rights are the intersection of what the sender held
 * with what the sender offered. The same rule as any other delegation,
 * enforced in the same place.
 *
 * The two rights on a port are deliberately separate. CAP_CALL lets you
 * send to it; CAP_READ lets you take things out of it. Handing someone
 * a send-only capability to your port lets them talk to you without
 * letting them read your incoming messages -- which is exactly the
 * shape of a service that anyone may call and only the owner may serve.
 */

#define MSG_MAX_WORDS 4
#define MSG_MAX_CAPS  2

typedef struct {
    u64 tag;                        /* what kind of message this is */
    u32 nwords;
    u32 ncaps;
    u64 words[MSG_MAX_WORDS];

    /* On send: handles in the sender's domain, plus the rights to pass
     * on. On receive: fresh handles in the receiver's domain. */
    cap_handle caps[MSG_MAX_CAPS];
    u32        cap_mask[MSG_MAX_CAPS];
} message;

void port_init(void);
type_id port_type(void);

/* A port with room for `capacity` undelivered messages. Comes back with
 * one reference held by the caller. */
object *port_create(u64 capacity);

/* Requires CAP_CALL on the port. Returns false if the port is full;
 * senders are never blocked, so a slow receiver cannot wedge whoever is
 * talking to it. */
bool port_send(domain *from, cap_handle port, const message *m);

/* Requires CAP_READ on the port. Blocks until something arrives. */
bool port_receive(domain *to, cap_handle port, message *out);

/* Same, but returns false instead of waiting. */
bool port_try_receive(domain *to, cap_handle port, message *out);

u64 port_pending(object *port);

/* The primitive underneath port_send: puts a message into a port with
 * the objects it carries given directly rather than as handles.
 *
 * Only the kernel uses this, and only where there is no holder whose
 * authority could be checked -- handing a program something it was
 * never holding in the first place. port_send is this plus the checks,
 * which is where the checks belong: on the path a program can take. */
bool port_post(object *port, const message *m,
               object **carried, const u32 *rights, u32 ncaps);

/* Empties a port's queue, releasing whatever the messages carried. Part
 * of tearing a port down; a queue that outlives its port would hold its
 * cargo forever. */
void port_drop_queued(object *port);

/* Calls visit for every object sitting in this port's queue. Messages
 * in flight are held by the queue and by nothing else, so a graph walk
 * that skipped them would be walking an incomplete graph. */
void port_visit_queued(object *port, void (*visit)(object *o));

bool msg_selftest(void);

#endif /* EB_MSG_H */
