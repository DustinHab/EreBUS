#ifndef EB_PIPE_H
#define EB_PIPE_H

#include <eb/types.h>
#include <eb/object.h>

/* The object pipe: how an object travels to another Erebus.
 *
 * A system function, not a thing in the graph: the shell offers
 * "send" on any readable data object, the kernel carries it to the
 * peer the settings name, and whatever arrives from over there is
 * laid into the arrivals list. There is nothing to wire up and no
 * program in the middle -- moving an object between machines is as
 * much a core duty here as writing one to disk.
 *
 * What travels is the object's substance -- its type, its own name,
 * its payload -- and nothing else: no references, no rights, no
 * reach back. Authority does not cross machines; only data does.
 * And what arrives is deliberately dull: a fresh object of a plain
 * kind -- text, bytes, a picture, never anything that runs -- wearing
 * its sender's name as a claim to weigh, not a fact to trust.
 */

#define PIPE_PORT 7800

/* The kernel domain, for the reply ports far work needs. */
struct domain;
void pipe_prepare(struct domain *k);

/* Queues one object for the peer. Copies the substance and returns;
 * the network thread does the carrying. False when the pipe is
 * already carrying something, the object is not a plain data kind,
 * or no peer is named. */
bool pipe_post(object *o);

/* Asks the peer to run a text: it travels as a recipe, runs over
 * there under the interpreter's own time budget, and its answer
 * comes home as a journal line and a small text in arrivals. One
 * ask at a time. The far machine refuses unless its settings say
 * "work | welcomed". */
bool pipe_ask(object *o);

/* The list arrivals are laid in, adopted or created by main. */
void pipe_arrivals_set(object *list);

/* One arriving datagram, from the pump. The source port matters:
 * the answer that says "taken" goes back exactly there, because the
 * road here may have run through a translator that renamed it. */
void pipe_input(const u8 src[4], u16 sport, const u8 *data, u32 len);

/* Carrying and housekeeping, called from the net thread's loop. */
void pipe_service(void);

/* The arrivals list, for the shell to recognise. */
object *pipe_arrivals(void);

/* Looking for company: a scan calls out on the wire, and machines
 * running this same system answer with their names. The found stand
 * in a small table until the next scan; pointing the pipe at one is
 * the shell's single click. */
void pipe_scan(void);
bool pipe_scanning(void);
u32  pipe_found_count(void);

/* One found machine: address, claimed name, whether it says it takes
 * far work, and how much memory it says it has free. The last two
 * may be NULL when the caller does not care. */
bool pipe_found_at(u32 i, u8 ip[4], char name[24],
                   bool *works, u32 *free_mib);

#endif /* EB_PIPE_H */
