#ifndef EB_BLK_H
#define EB_BLK_H

#include <eb/types.h>

/* Block storage.
 *
 * One interface, one implementation for now: AHCI, the SATA controller
 * every x86 machine has had since about 2008. NVMe would be the choice
 * for anything newer, and slots in behind the same four calls.
 *
 * Sectors are 512 bytes. Buffers may be anywhere in kernel memory --
 * the driver copies through a physically contiguous staging area of its
 * own, because memory from the heap is contiguous in the address space
 * and nowhere else, and the controller only understands the other one. */

#define BLK_SECTOR_SIZE 512

bool blk_init(void);
bool blk_present(void);
u64  blk_sectors(void);
const char *blk_model(void);
u32  blk_port(void);        /* which AHCI port it settled on */
u32  blk_disk_count(void);  /* how many disks were found */

bool blk_read(u64 lba, u32 count, void *dst);
bool blk_write(u64 lba, u32 count, const void *src);

/* The exchange disk: the data disk after the store, when one is
 * attached. Files cross between worlds on it. */
bool blk_aux_present(void);
u64  blk_aux_sectors(void);
bool blk_aux_read(u64 lba, u32 count, void *dst);
bool blk_aux_write(u64 lba, u32 count, const void *src);

/* The boot disk: port zero, when it is a disk of its own and not also
 * the store. The kernel the machine starts from lies on it, and a
 * kernel built here is installed there. */
bool blk_boot_present(void);
u64  blk_boot_sectors(void);
bool blk_boot_read(u64 lba, u32 count, void *dst);
bool blk_boot_write(u64 lba, u32 count, const void *src);

/* Writes a pattern, reads it back, and puts the sector back as it was.
 * Runs on a sector well past anything we use. */
bool blk_selftest(void);

#endif /* EB_BLK_H */
