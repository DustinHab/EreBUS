#ifndef EB_PIPE_H
#define EB_PIPE_H

#include <eb/types.h>
#include <eb/object.h>

/* The object pipe: objects, lines, tasks and kernels between EreBUS machines over UDP.
 * - a kernel function, not an object: "send" on a readable data object, the kernel transfers it, the receiver places it in arrivals
 * - only data crosses: type, name, payload; no references, no rights
 * - the handshake is signed with the door key; the nodes table holds the rights granted to each node */

#define PIPE_PORT 7800

/* The kernel domain, for the reply ports far work needs. */
struct domain;
void pipe_prepare(struct domain *k);

/* Queues one object for the peer the settings name (by address or by
 * node name). Retains the object and returns; the network thread does
 * the carrying. False when the pipe is already carrying something, the
 * object is not a plain data kind, or no peer is named. */
bool pipe_post(object *o);

/* Hands a task to the desk. A task whose first line says
 * "split P from LO to HI" is divided into parts and dealt to the
 * machines that answered the scan willing, their numeric answers
 * summed; any other text is one recipe, asked of the peer. The result
 * is written back into the task when `writable` says so, otherwise laid
 * into arrivals. Far machines refuse unless their settings say
 * "work | welcomed" or their nodes table lets this machine work. */
bool pipe_ask(object *o, bool writable);

/* The same, with an object every worker gets ahead of its part: the
 * script's third gift, after its words and the way home, read-only.
 * A text, bytes or a picture, up to 8 MiB. */
bool pipe_ask_with(object *o, bool writable, object *input);

/* The list arrivals are laid in, adopted or created by main. */
void pipe_arrivals_set(object *list);
object *pipe_arrivals(void);

/* One arriving datagram, from the pump. */
void pipe_input(const u8 src[4], u16 sport, const u8 *data, u32 len);

/* Carrying and housekeeping, called from the net thread's loop. */
void pipe_service(void);

/* The line: one running conversation, kept as a text the kernel appends to. */
void    pipe_line_set(object *text);
object *pipe_line(void);
bool    pipe_say(const char *text);

/* Looking for company: a scan calls out on the wire; machines running
 * this system answer with their names. The found are the ones heard
 * since the last scan began. */
void pipe_scan(void);
bool pipe_scanning(void);
u32  pipe_found_count(void);
bool pipe_found_at(u32 i, u8 ip[4], char name[24],
                   bool *works, u32 *free_mib);

/* Seconds since a machine at that address was last heard; ~0 when never. */
u64  pipe_seen_ago_s(const u8 ip[4]);

/* The "network" page: what the kernel sees on the wire, rewritten
 * every two seconds. */
void pipe_page_set(object *text);

/* A kernel to another node: the one this machine runs (image NULL) or a
 * built kernel.elf. The node installs it and restarts when its nodes
 * table lets this machine update it. False with a reason in why. */
bool pipe_update(const char *node, object *image, char *why, u32 max);

/* The same to every node with an address, one after another. How many
 * were queued; zero with a reason in why. */
u32  pipe_update_all(object *image, char *why, u32 max);

#endif /* EB_PIPE_H */
