#ifndef EB_SETTLE_H
#define EB_SETTLE_H

#include <eb/types.h>

/* Making a home for the graph on a disk, from a machine that runs
 * from a stick or has none: the words "disks", "settle" and "yes".
 *
 * "disks" shows every disk on the bus with its partitions and the
 * room left on it. "settle on disk N" takes a disk whole -- a boot
 * volume with the loader and this kernel, and a store on the rest;
 * "settle in partition P of disk N" makes one partition the store;
 * "settle in the free space of disk N" makes a store in a gap and
 * leaves every partition as it is. Each says what is lost and waits
 * for "yes". Afterwards the running machine keeps its graph there. */

typedef void (*settle_say_fn)(void *ctx, const char *line);

void settle_disks(settle_say_fn say, void *ctx);
void settle_plan(const char *what, settle_say_fn say, void *ctx);
void settle_yes(settle_say_fn say, void *ctx);

#endif /* EB_SETTLE_H */
