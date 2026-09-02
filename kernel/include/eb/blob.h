#ifndef EB_BLOB_H
#define EB_BLOB_H

#include <eb/types.h>

/* The log of big objects on the store.
 *
 * A generation slot holds a megabyte, and a system that keeps its own
 * sources carries more than that. Payloads from a few kilobytes up lie
 * in this log instead, once each, under the SHA-256 of their contents;
 * the generations refer to them by that hash. Two generations sharing
 * an unchanged text share one entry, an edit adds one, and an entry
 * whose bytes do not hash to its name is never handed back.
 *
 * Hashes are 32 bytes. The set handed to blob_compact is n of them,
 * back to back. */

bool blob_find(const u8 *hash, u64 *lba, u64 *size);

/* Writes the bytes to the log unless it holds them already; either way
 * answers where they lie. False when the log has no room: compact it
 * and try again. */
bool blob_store(const u8 *hash, const void *data, u64 size, u64 *lba);

/* Reads an entry back and checks it against its hash. The place is a
 * hint from the generation that wrote it; when the log has moved since,
 * the hash finds it. */
bool blob_read(const u8 *hash, u64 lba_hint, u64 size, void *dst);

u64  blob_free_sectors(void);
u32  blob_count(void);

/* Drops every entry not in the live set and moves the rest down. */
bool blob_compact(const u8 *live, u32 count);

#endif /* EB_BLOB_H */
