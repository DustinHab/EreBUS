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

/* Starts a fresh instance. The returned program object is held by the
 * process itself; put it somewhere or it is merely running unheld. */
object *standard_launch(u32 i);

/* Starts the interpreter on a text: the text becomes a running
 * program, granted its own words read-only as the first gift. */
object *runner_launch(object *script);

/* Starts the interpreter on a visiting text, for the pipe: the words
 * read-only, a reply port send-only, and a time budget the
 * interpreter itself enforces. Holds nothing else. A divided job's
 * range rides along: lo on the way-home gift, hi as a bare number. */
object *work_launch(object *script, object *reply, u64 budget_seconds,
                    i64 lo, i64 hi);

/* Saves the graph and asks the machine to sleep. Comes back only on
 * hardware that ignored the asking. */
void system_off(void);

#endif /* EB_STANDARD_H */
