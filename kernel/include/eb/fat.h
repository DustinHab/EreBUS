#ifndef EB_FAT_H
#define EB_FAT_H

#include <eb/types.h>
#include <eb/object.h>

/* Two FAT32 disks, spoken in the world's own format.
 *
 * The exchange disk sits beside the store. On boot, when one is
 * attached, its root directory's files become objects in a list
 * called "the disk" on the system shelf -- texts when they read like
 * text, bytes otherwise. Writing back is a deliberate act: "write
 * out" on that list writes every entry that does not have a file yet,
 * under a plain 8.3 name, into the root directory. Honest limits:
 * FAT32 only, the root directory only, files up to 4 MiB coming in
 * and 16 MiB going out, long names read but not written.
 *
 * The boot disk is where the kernel lives, in \erebus. A kernel built
 * here is installed there: laid down as kernel.new, then the names
 * turned -- the running kernel becomes kernel.old, the new one
 * kernel.elf -- and the count of starts set to zero. The loader counts
 * the starts; a kernel that does not come up twice is set aside for
 * kernel.old again, by the loader, and the kernel says so.
 */

/* Mounts the exchange disk, if one is there and speaks FAT32. */
bool fat_mount(void);
bool fat_present(void);

/* Reads the root directory into the given list: every file that fits
 * becomes a fresh object; files already in the list (by name) are
 * left alone. Returns how many came in. */
u32 fat_take_in(object *into);

/* Writes entries of the list that have no file of their name yet.
 * Returns how many went out. */
u32 fat_write_out(object *from);

/* The boot disk's volume, mounted on first need. */
bool fat_boot_present(void);

/* Lays a kernel down as the one the next start runs, keeping the
 * running one as kernel.old. False with a reason in why. */
bool fat_install_kernel(const u8 *elf, u64 len, char *why, u32 max);

/* The loader, replaced under the name the firmware reads. No stepping
 * aside here: the firmware reads exactly one name. */
bool fat_install_loader(const u8 *pe, u64 len, char *why, u32 max);

/* The kernel has come up: the loader's count of starts goes back to
 * zero. Answers what the count was, or 0 when there was none. */
u32 fat_boot_settle(void);

/* A boot volume made from nothing on a stretch of a disk: fat32, the
 * folders, the loader and the kernel laid in. For settling on a
 * fresh disk. */
bool fat_lay_boot_volume(u32 disk, u64 first, u64 sectors,
                         const u8 *loader, u64 lsize, const u8 *kernel, u64 ksize,
                         char *why, u32 max);

#endif /* EB_FAT_H */
