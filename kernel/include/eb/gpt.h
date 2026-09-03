#ifndef EB_GPT_H
#define EB_GPT_H

#include <eb/types.h>

/* The partition table of a disk, read and written whole.
 *
 * A GPT is two headers -- one behind the protective mbr, one at the
 * very end -- each pointing at a copy of the same array of entries,
 * every piece checked by a crc. The table here is the array with the
 * few header facts that matter; writing it lays down both copies. */

#define GPT_ENTRIES 128
#define GPT_ENTRY_SIZE 128

typedef struct {
    bool present;                     /* a table was read from the disk */
    u64  disk_sectors;
    u64  first_usable, last_usable;
    u8   disk_guid[16];
    u8   entries[GPT_ENTRIES * GPT_ENTRY_SIZE];
} gpt_table;

/* The kinds this system knows by name. */
extern const u8 GPT_TYPE_EFI[16];
extern const u8 GPT_TYPE_STORE[16];

bool gpt_read(u32 disk, gpt_table *t);
void gpt_make(gpt_table *t, u64 disk_sectors);       /* a fresh, empty table */
bool gpt_write(u32 disk, const gpt_table *t);

/* One entry: its type, first and last sector; empty when the type is
 * all zeros. gpt_add lays a new one in the first empty slot. */
bool gpt_entry(const gpt_table *t, u32 i, u8 type[16], u64 *first, u64 *last);
void gpt_entry_name(const gpt_table *t, u32 i, char *out, u32 max);
void gpt_entry_set_type(gpt_table *t, u32 i, const u8 type[16]);
i32  gpt_add(gpt_table *t, const u8 type[16], u64 first, u64 last, const char *name);

/* What a type is called, for people. */
const char *gpt_type_name(const u8 type[16]);

u32 crc32(const void *data, u64 len);

#endif /* EB_GPT_H */
