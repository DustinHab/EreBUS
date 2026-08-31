#ifndef EB_SNAPSHOT_H
#define EB_SNAPSHOT_H

#include <eb/types.h>
#include <eb/object.h>

/* Persistence by snapshot.
 *
 * There is no save command in this system and no file to save into.
 * What gets written to disk is the object graph itself: start from the
 * roots, follow every reference, and put the whole reachable set down
 * as it stands. Coming back up, the graph is rebuilt and the system
 * carries on from where it was.
 *
 * That removes a distinction other systems have to maintain. A document
 * in memory and a document on disk are usually two things in two
 * formats, and everything between them -- serialising, parsing,
 * autosave, "unsaved changes", recovering a crashed session -- exists
 * to keep them in step. Here there is one representation and the disk
 * holds a copy of it.
 *
 * Two slots, written alternately, each carrying a generation number and
 * a checksum. A snapshot interrupted halfway leaves the other slot
 * untouched, so the worst a power cut can cost is the changes since the
 * last complete write -- never the graph itself. Writing in place would
 * make every save a moment where the system has no valid state at all.
 */

bool snap_save(object **roots, u32 root_count);

/* Rebuilds the graph from the newer valid slot. Fills in up to
 * max_roots root objects, each with one reference held by the caller,
 * and returns how many there were. Zero means there was nothing to
 * restore. */
u32 snap_load(object **roots, u32 max_roots);

bool snap_present(void);      /* is there a valid snapshot on the disk */
u64  snap_generation(void);   /* which one was last read or written */
u64  snap_bytes(void);        /* size of the last snapshot */
u32  snap_object_count(void);

#endif /* EB_SNAPSHOT_H */
