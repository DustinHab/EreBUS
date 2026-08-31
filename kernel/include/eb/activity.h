#ifndef EB_ACTIVITY_H
#define EB_ACTIVITY_H

#include <eb/types.h>
#include <eb/object.h>

/* What the machine is doing, as an object.
 *
 * A text the kernel rewrites once a second: uptime, memory, the object
 * count, and one row per running program with the share of the
 * processor it actually held -- measured at every handover, not
 * sampled. The reference the person holds is read-only; the machine
 * reports, nobody edits the report.
 */

bool    activity_create(void);
void    activity_adopt(object *o);
object *activity_object(void);

/* Rewrites the table from the current state of everything. */
void activity_update(void);

#endif /* EB_ACTIVITY_H */
