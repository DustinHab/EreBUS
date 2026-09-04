#ifndef EB_VERSION_H
#define EB_VERSION_H

/* What this build calls itself, written by the Makefile from the
 * nearest tag and the commit: "v0.3.1", "v0.3.1-4-g07589ac", or
 * "v0.3.1-4-g07589ac-dirty" for a tree with changes not committed.
 *
 * It is the one line that tells two kernels apart on a screen. A
 * machine that keeps its graph on a disk looks the same whichever
 * kernel it starts with, because the graph is the same; without this
 * there is no telling a new kernel from the disk's old one. */
extern const char erebus_version[];

#endif /* EB_VERSION_H */
