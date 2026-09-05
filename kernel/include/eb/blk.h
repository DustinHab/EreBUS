#ifndef EB_BLK_H
#define EB_BLK_H

#include <eb/types.h>

/* Block storage.
 *
 * One interface, one implementation for now: AHCI, the SATA controller
 * every x86 machine has had since about 2008. NVMe would be the choice
 * for anything newer, and slots in behind the same calls.
 *
 * Sectors are 512 bytes. Buffers may be anywhere in kernel memory --
 * the driver copies through a physically contiguous staging area of its
 * own, because memory from the heap is contiguous in the address space
 * and nowhere else, and the controller only understands the other one. */

#define BLK_SECTOR_SIZE 512

bool blk_init(void);

/* Every disk the controller found, by number, for the words that
 * choose one. Reading and writing here reach the whole disk. */
u32  blk_disk_count(void);
u64  blk_disk_sectors(u32 which);
const char *blk_disk_model(u32 which);
u32  blk_disk_port(u32 which);
bool blk_disk_read(u32 which, u64 lba, u32 count, void *dst);
bool blk_disk_write(u32 which, u64 lba, u32 count, const void *src);

/* The roles: the disk the machine booted from, the one the store lies
 * on and where on it, the exchange disk. -1 for a role nobody has. */
i32  blk_boot_disk(void);
i32  blk_store_disk(void);
u64  blk_store_base(void);
i32  blk_aux_disk(void);

/* The store: the graph's home. Its sectors count from where it begins
 * and end where it ends. */
bool blk_present(void);
u64  blk_sectors(void);
const char *blk_model(void);
u32  blk_port(void);
bool blk_read(u64 lba, u32 count, void *dst);
bool blk_write(u64 lba, u32 count, const void *src);

/* Makes this stretch of that disk the store, now: the same claim the
 * boot makes, and from then on everything the boot would have done. */
bool blk_adopt(u32 which, u64 first, u64 count);

/* The exchange disk: the data disk after the store, when one is
 * attached. Files cross between worlds on it. */
bool blk_aux_present(void);
u64  blk_aux_sectors(void);
bool blk_aux_read(u64 lba, u32 count, void *dst);
bool blk_aux_write(u64 lba, u32 count, const void *src);

/* The boot disk: port zero, when it is a disk of its own. The kernel
 * the machine starts from lies on it, and a kernel built here is
 * installed there. */
bool blk_boot_present(void);
u64  blk_boot_sectors(void);
bool blk_boot_read(u64 lba, u32 count, void *dst);
bool blk_boot_write(u64 lba, u32 count, const void *src);

/* Writes a pattern, reads it back, and puts the sector back as it was.
 * Runs on a sector below anything the store keeps. */
bool blk_selftest(void);

/* A disk on another bus -- a usb stick -- registered with its own way
 * of moving sectors (count of them, at lba, into or out of buf; the
 * driver copies through its own memory). It takes a number like the
 * others; the roles are looked at anew: with no store yet, a store
 * partition on it becomes the store and it counts as the boot disk;
 * else, with no exchange disk yet, it is that. Returns the number, or
 * -1 when the row is full. */
typedef bool (*blk_io)(void *ctx, u64 lba, u32 count, void *buf, bool write);
i32  blk_add(const char *model, u64 sectors, blk_io io, void *ctx);
/* The disk is gone: reads and writes to it fail from here on. */
void blk_remove(i32 which);
/* Where a disk hangs, in a few words: "ahci port 0", "usb". */
const char *blk_disk_where(u32 which);
/* Whether the store lies on a usb disk; whether a disk is one. */
bool blk_store_on_usb(void);
bool blk_disk_on_usb(u32 which);

#endif /* EB_BLK_H */
