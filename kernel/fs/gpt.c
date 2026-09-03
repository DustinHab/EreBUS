/*
 * gpt.c -- the partition table of a disk, read and written whole.
 */
#include <eb/gpt.h>
#include <eb/blk.h>
#include <eb/crypto.h>
#include <eb/string.h>

const u8 GPT_TYPE_EFI[16]   = { 0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                                0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B };
const u8 GPT_TYPE_STORE[16] = { 0x00, 0x05, 0xEB, 0xE2, 0x54, 0x53, 0x52, 0x4F,
                                0x45, 0x52, 0x45, 0x42, 0x55, 0x53, 0x00, 0x01 };

static const struct { u8 guid[16]; const char *name; } kinds[] = {
    { { 0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B }, "efi system" },
    { { 0x00, 0x05, 0xEB, 0xE2, 0x54, 0x53, 0x52, 0x4F, 0x45, 0x52, 0x45, 0x42, 0x55, 0x53, 0x00, 0x01 }, "our store" },
    { { 0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 }, "windows or data" },
    { { 0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE }, "windows reserved" },
    { { 0xA4, 0xBB, 0x94, 0xDE, 0xD1, 0x06, 0x40, 0x4D, 0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC }, "windows recovery" },
    { { 0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 }, "linux" },
    { { 0x6D, 0xFD, 0x57, 0x06, 0xAB, 0xA4, 0xC4, 0x43, 0x84, 0xE5, 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F }, "linux swap" },
    { { 0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6F, 0x6E, 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 }, "bios boot" },
};

/* ------------------------------------------------------------------ */
/* crc                                                                 */
/* ------------------------------------------------------------------ */

static u32 crc_table[256];
static bool crc_ready;

u32 crc32(const void *data, u64 len)
{
    if (!crc_ready) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (u32 k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            crc_table[i] = c;
        }
        crc_ready = true;
    }
    const u8 *p = (const u8 *)data;
    u32 c = 0xFFFFFFFFu;
    for (u64 i = 0; i < len; i++) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Little-endian fields                                                */
/* ------------------------------------------------------------------ */

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u64 rd64(const u8 *p) { return (u64)rd32(p) | ((u64)rd32(p + 4) << 32); }
static void wr32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void wr64(u8 *p, u64 v) { wr32(p, (u32)v); wr32(p + 4, (u32)(v >> 32)); }

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

bool gpt_read(u32 disk, gpt_table *t)
{
    static u8 hdr[512];
    memset(t, 0, sizeof(*t));
    t->disk_sectors = blk_disk_sectors(disk);
    if (!blk_disk_read(disk, 1, 1, hdr)) return false;
    if (memcmp(hdr, "EFI PART", 8) != 0) return false;

    u64 elba = rd64(hdr + 72);
    u32 n = rd32(hdr + 80), sz = rd32(hdr + 84);
    if (sz != GPT_ENTRY_SIZE || n == 0 || elba < 2) return false;
    if (n > GPT_ENTRIES) n = GPT_ENTRIES;
    u32 sectors = (n * sz + 511) / 512;
    if (!blk_disk_read(disk, elba, sectors, t->entries)) return false;

    t->first_usable = rd64(hdr + 40);
    t->last_usable = rd64(hdr + 48);
    memcpy(t->disk_guid, hdr + 56, 16);
    t->present = true;
    return true;
}

bool gpt_entry(const gpt_table *t, u32 i, u8 type[16], u64 *first, u64 *last)
{
    if (i >= GPT_ENTRIES) return false;
    const u8 *e = t->entries + i * GPT_ENTRY_SIZE;
    bool empty = true;
    for (u32 k = 0; k < 16; k++) if (e[k]) empty = false;
    if (empty) return false;
    if (type) memcpy(type, e, 16);
    if (first) *first = rd64(e + 32);
    if (last) *last = rd64(e + 40);
    return true;
}

void gpt_entry_name(const gpt_table *t, u32 i, char *out, u32 max)
{
    u32 n = 0;
    if (i < GPT_ENTRIES) {
        const u8 *e = t->entries + i * GPT_ENTRY_SIZE + 56;
        for (u32 k = 0; k < 36 && n + 1 < max; k++) {
            u16 c = (u16)e[k * 2] | ((u16)e[k * 2 + 1] << 8);
            if (!c) break;
            out[n++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
    }
    out[n] = 0;
}

void gpt_entry_set_type(gpt_table *t, u32 i, const u8 type[16])
{
    if (i < GPT_ENTRIES) memcpy(t->entries + i * GPT_ENTRY_SIZE, type, 16);
}

const char *gpt_type_name(const u8 type[16])
{
    for (u32 i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (memcmp(kinds[i].guid, type, 16) == 0) return kinds[i].name;
    return "another kind";
}

/* ------------------------------------------------------------------ */
/* Making and writing                                                  */
/* ------------------------------------------------------------------ */

void gpt_make(gpt_table *t, u64 disk_sectors)
{
    memset(t, 0, sizeof(*t));
    t->disk_sectors = disk_sectors;
    t->first_usable = 34;                          /* mbr, header, 32 sectors of entries */
    t->last_usable = disk_sectors - 34;
    rand_bytes(t->disk_guid, 16);
    t->disk_guid[7] = (u8)((t->disk_guid[7] & 0x0F) | 0x40);   /* version 4, random */
    t->disk_guid[8] = (u8)((t->disk_guid[8] & 0x3F) | 0x80);
    t->present = true;
}

i32 gpt_add(gpt_table *t, const u8 type[16], u64 first, u64 last, const char *name)
{
    for (u32 i = 0; i < GPT_ENTRIES; i++) {
        if (gpt_entry(t, i, NULL, NULL, NULL)) continue;
        u8 *e = t->entries + i * GPT_ENTRY_SIZE;
        memset(e, 0, GPT_ENTRY_SIZE);
        memcpy(e, type, 16);
        rand_bytes(e + 16, 16);
        e[16 + 7] = (u8)((e[16 + 7] & 0x0F) | 0x40);
        e[16 + 8] = (u8)((e[16 + 8] & 0x3F) | 0x80);
        wr64(e + 32, first);
        wr64(e + 40, last);
        for (u32 k = 0; k < 36 && name[k]; k++) { e[56 + k * 2] = (u8)name[k]; e[57 + k * 2] = 0; }
        return (i32)i;
    }
    return -1;
}

static void header_at(u8 *h, const gpt_table *t, u64 here, u64 other, u64 entries_lba)
{
    memset(h, 0, 512);
    memcpy(h, "EFI PART", 8);
    wr32(h + 8, 0x00010000);
    wr32(h + 12, 92);
    wr64(h + 24, here);
    wr64(h + 32, other);
    wr64(h + 40, t->first_usable);
    wr64(h + 48, t->last_usable);
    memcpy(h + 56, t->disk_guid, 16);
    wr64(h + 72, entries_lba);
    wr32(h + 80, GPT_ENTRIES);
    wr32(h + 84, GPT_ENTRY_SIZE);
    wr32(h + 88, crc32(t->entries, sizeof(t->entries)));
    wr32(h + 16, crc32(h, 92));
}

/* The protective mbr, the two headers, the two copies of the entries.
 * Entries first, headers last: a table cut short between them is not
 * a table, and the firmware falls back on the old one nowhere -- it
 * sees a blank disk, which is the safe way to be wrong. */
bool gpt_write(u32 disk, const gpt_table *t)
{
    static u8 sec[512];
    u64 n = t->disk_sectors;
    if (n < 68) return false;
    u32 esec = sizeof(t->entries) / 512;                 /* 32 */
    u64 backup_entries = n - 1 - esec;

    if (!blk_disk_write(disk, 2, esec, t->entries)) return false;
    if (!blk_disk_write(disk, backup_entries, esec, t->entries)) return false;

    memset(sec, 0, 512);
    sec[446 + 0] = 0x00;                                 /* one partition covering the disk */
    sec[446 + 1] = 0x00; sec[446 + 2] = 0x02; sec[446 + 3] = 0x00;
    sec[446 + 4] = 0xEE;                                 /* protective */
    sec[446 + 5] = 0xFF; sec[446 + 6] = 0xFF; sec[446 + 7] = 0xFF;
    wr32(sec + 446 + 8, 1);
    wr32(sec + 446 + 12, n - 1 > 0xFFFFFFFFULL ? 0xFFFFFFFFu : (u32)(n - 1));
    sec[510] = 0x55; sec[511] = 0xAA;
    if (!blk_disk_write(disk, 0, 1, sec)) return false;

    header_at(sec, t, n - 1, 1, backup_entries);
    if (!blk_disk_write(disk, n - 1, 1, sec)) return false;
    header_at(sec, t, 1, n - 1, 2);
    return blk_disk_write(disk, 1, 1, sec);
}
