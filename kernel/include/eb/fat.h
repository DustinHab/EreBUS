#ifndef EB_FAT_H
#define EB_FAT_H

#include <eb/types.h>
#include <eb/object.h>

/* The exchange disk: a FAT32 disk beside the store, read and written
 * the way the rest of the world formats sticks and cards.
 *
 * On boot, when such a disk is attached, its root directory's files
 * become objects in a list called "the disk" on the system shelf --
 * texts when they read like text, bytes otherwise. Writing back is a
 * deliberate act: "write out" on that list writes every entry that
 * does not have a file yet, under a plain 8.3 name, into the root
 * directory. Honest limits: FAT32 only, the root directory only,
 * files up to 64 KiB either way, and long names are read but not
 * written.
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

#endif /* EB_FAT_H */
