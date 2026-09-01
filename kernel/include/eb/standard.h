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

#endif /* EB_STANDARD_H */
