#ifndef EB_STANDARD_H
#define EB_STANDARD_H

#include <eb/types.h>
#include <eb/object.h>

/* The programs the system ships with, offered for starting.
 *
 * A fresh instance begins the way every program begins: holding a way
 * to speak and a letter box, and nothing else. What it ends up able to
 * touch is decided by whoever points its program object at things --
 * starting a program and trusting it remain two separate acts.
 */

u32         standard_count(void);
const char *standard_name(u32 i);

/* Every launcher below hands the program object back with one hold
 * that is the caller's. The process holds another, let go by the
 * reaper -- and a short program can run to its end and be reaped
 * before the caller has laid the object anywhere, so the caller's
 * hold is what keeps it real across that gap. Release it once the
 * object lies where it belongs, or when it will not be laid at all. */

/* Starts a fresh instance. */
object *standard_launch(u32 i);

/* Starts the interpreter on a text: the text becomes a running
 * program, granted its own words read-only as the first gift. */
object *runner_launch(object *script);

/* Starts an image the assembler made: the bytes become a running
 * program, granted their own image read-only as the first gift, the
 * way a script is granted its words. NULL for bytes that are not an
 * image. */
object *code_launch(object *image);

/* Starts the interpreter on a visiting text, for the pipe: the words
 * read-only, a reply port send-only, and a time budget the
 * interpreter itself enforces. Holds nothing else. A divided job's
 * range rides along: lo on the way-home gift, hi as a bare number. */
object *work_launch(object *script, object *reply, u64 budget_seconds,
                    i64 lo, i64 hi);

/* Saves the graph and asks the machine to sleep. Comes back only on
 * hardware that ignored the asking. */
void system_off(void);

/* Saves the graph and starts the machine again. */
void system_restart(void);

/* The list named "the served", or NULL: what the web server offers.
 * The reference is the whole switch. */
object *system_served(void);

/* The exchange disk's list, or NULL when no FAT disk is attached. */
object *system_disk(void);

#endif /* EB_STANDARD_H */
