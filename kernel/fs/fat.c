/*
 * fat.c -- the exchange disk, spoken in the world's own format.
 *
 * FAT32 because that is what sticks and cards actually wear. The
 * reader takes the root directory's files in as objects; the writer
 * lays objects down as files with plain 8.3 names. Nothing here
 * mounts, caches or defers: every operation walks the disk when it
 * runs, and the honest limits -- root directory only, 64 KiB per
 * file, long names read but not written -- are stated rather than
 * papered over.
 */
#include <eb/fat.h>
#include <eb/blk.h>
#include <eb/cap.h>
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/fmt.h>
#include <eb/io.h>

#define FILE_MAX 65536

static struct {
    bool ready;
    u64  base;              /* lba where the volume begins */
    u32  spc;               /* sectors per cluster */
    u32  fat_start;         /* lba of the first fat */
    u32  fat_sectors;
    u32  nfats;
    u32  data_start;        /* lba of cluster 2 */
    u32  root_cluster;
    u32  clusters;
} vol;

static u8 sec[512];

/* --- the volume ------------------------------------------------------ */

static bool looks_fat32(const u8 *b)
{
    if (b[510] != 0x55 || b[511] != 0xAA) return false;
    u16 bps = (u16)b[11] | ((u16)b[12] << 8);
    u16 root_entries = (u16)b[17] | ((u16)b[18] << 8);
    u16 fatsz16 = (u16)b[22] | ((u16)b[23] << 8);
    return bps == 512 && root_entries == 0 && fatsz16 == 0 && b[13] != 0;
}

static bool vol_open(u64 base)
{
    if (!blk_aux_read(base, 1, sec)) return false;
    if (!looks_fat32(sec)) return false;

    u16 reserved = (u16)sec[14] | ((u16)sec[15] << 8);
    u32 fatsz = (u32)sec[36] | ((u32)sec[37] << 8) |
                ((u32)sec[38] << 16) | ((u32)sec[39] << 24);
    u32 total = (u32)sec[32] | ((u32)sec[33] << 8) |
                ((u32)sec[34] << 16) | ((u32)sec[35] << 24);

    vol.base = base;
    vol.spc = sec[13];
    vol.nfats = sec[16];
    vol.fat_start = (u32)base + reserved;
    vol.fat_sectors = fatsz;
    vol.data_start = vol.fat_start + vol.nfats * fatsz;
    vol.root_cluster = (u32)sec[44] | ((u32)sec[45] << 8) |
                       ((u32)sec[46] << 16) | ((u32)sec[47] << 24);
    vol.clusters = (total - (vol.data_start - (u32)base)) / vol.spc;
    vol.ready = true;
    return true;
}

bool fat_mount(void)
{
    vol.ready = false;
    if (!blk_aux_present()) return false;

    /* The volume begins at sector zero, or behind an mbr's first
     * partition -- both shapes exist in the world. */
    if (vol_open(0)) goto up;
    if (blk_aux_read(0, 1, sec) && sec[510] == 0x55 && sec[511] == 0xAA) {
        u32 pstart = (u32)sec[454] | ((u32)sec[455] << 8) |
                     ((u32)sec[456] << 16) | ((u32)sec[457] << 24);
        if (pstart && vol_open(pstart)) goto up;
    }
    kprintf("fat:  the exchange disk does not speak fat32\n");
    return false;

up:
    kprintf("fat:  fat32 volume, %u clusters of %u sectors\n",
            vol.clusters, vol.spc);
    return true;
}

bool fat_present(void) { return vol.ready; }

/* --- the fat itself -------------------------------------------------- */

static u32 fat_get(u32 cluster)
{
    u32 off = cluster * 4;
    if (!blk_aux_read(vol.fat_start + off / 512, 1, sec)) return 0x0FFFFFFF;
    u32 v = (u32)sec[off % 512] | ((u32)sec[off % 512 + 1] << 8) |
            ((u32)sec[off % 512 + 2] << 16) |
            ((u32)sec[off % 512 + 3] << 24);
    return v & 0x0FFFFFFF;
}

static bool fat_set(u32 cluster, u32 value)
{
    u32 off = cluster * 4;
    u64 lba = vol.fat_start + off / 512;
    if (!blk_aux_read(lba, 1, sec)) return false;
    sec[off % 512]     = (u8)value;
    sec[off % 512 + 1] = (u8)(value >> 8);
    sec[off % 512 + 2] = (u8)(value >> 16);
    sec[off % 512 + 3] = (u8)((value >> 24) & 0x0F) |
                         (sec[off % 512 + 3] & 0xF0);
    if (!blk_aux_write(lba, 1, sec)) return false;

    /* The mirrored fats stay in step. */
    for (u32 f = 1; f < vol.nfats; f++)
        if (!blk_aux_write(lba + (u64)f * vol.fat_sectors, 1, sec))
            return false;
    return true;
}

static u64 cluster_lba(u32 cluster)
{
    return vol.data_start + (u64)(cluster - 2) * vol.spc;
}

static u32 fat_free_cluster(u32 after)
{
    for (u32 c = after < 3 ? 3 : after; c < vol.clusters + 2; c++)
        if (fat_get(c) == 0) return c;
    return 0;
}

/* --- names ----------------------------------------------------------- */

/* Assembles the long name walking backwards over lfn entries, or
 * falls back to the plain 8.3. */
static void take_name(const u8 *e, char *lfn, bool have_lfn,
                      char *out, u32 max)
{
    if (have_lfn && lfn[0]) {
        u32 i = 0;
        while (lfn[i] && i < max - 1) { out[i] = lfn[i]; i++; }
        out[i] = 0;
        return;
    }
    u32 at = 0;
    for (u32 i = 0; i < 8 && e[i] != ' '; i++) {
        char c = (char)e[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out[at++] = c;
    }
    if (e[8] != ' ') {
        out[at++] = '.';
        for (u32 i = 8; i < 11 && e[i] != ' '; i++) {
            char c = (char)e[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            out[at++] = c;
        }
    }
    out[at] = 0;
}

/* A petname into a plain 8.3, uppercase, the dot splitting the tail. */
static void make_short(const char *nm, u8 out[11])
{
    for (u32 i = 0; i < 11; i++) out[i] = ' ';

    u32 dot = 0;
    for (u32 i = 0; nm[i]; i++) if (nm[i] == '.') dot = i;

    u32 at = 0;
    for (u32 i = 0; nm[i] && at < 8; i++) {
        if (dot && i >= dot) break;
        char c = nm[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[at++] = (u8)c;
        else if (c != '.' && c != ' ') out[at++] = '_';
    }
    if (at == 0) out[at++] = 'X';

    if (dot) {
        u32 et = 8;
        for (u32 i = dot + 1; nm[i] && et < 11; i++) {
            char c = nm[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                out[et++] = (u8)c;
        }
    }
}

/* --- reading in ------------------------------------------------------ */

static bool text_like(const u8 *d, u32 len)
{
    if (len == 0) return false;
    for (u32 i = 0; i < len; i++) {
        u8 c = d[i];
        if (c == 0x09 || c == 0x0A || c == 0x0D) continue;
        if (c >= 0x20 && c < 0x7F) continue;
        if (c >= 0xA0) continue;
        return false;
    }
    return true;
}

static bool list_has_name(object *l, const char *nm)
{
    for (u64 i = 0; i < obj_slots(l); i++) {
        object *t = obj_get_slot(l, i);
        const char *n = obj_slot_name(l, i);
        if (t && n && strcmp(n, nm) == 0) return true;
    }
    return false;
}

static bool list_add(object *l, object *o, const char *nm)
{
    u64 n = obj_slots(l), at = n;
    for (u64 i = 0; i < n; i++)
        if (!obj_get_slot(l, i)) { at = i; break; }
    if (at == n && !obj_grow_slots(l, n + 1)) return false;
    obj_set_slot(l, at, o, CAP_READ | CAP_WRITE);
    obj_set_slot_name(l, at, nm);
    return true;
}

static u8 filebuf[FILE_MAX];

/* Reads one chain into filebuf. Returns bytes read, capped. */
static u32 read_chain(u32 cluster, u32 size)
{
    u32 got = 0;
    u32 want = size > FILE_MAX ? FILE_MAX : size;
    static u8 cbuf[512];

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && got < want) {
        for (u32 s = 0; s < vol.spc && got < want; s++) {
            if (!blk_aux_read(cluster_lba(cluster) + s, 1, cbuf))
                return got;
            u32 take = want - got < 512 ? want - got : 512;
            memcpy(filebuf + got, cbuf, take);
            got += take;
        }
        cluster = fat_get(cluster);
    }
    return got;
}

/* Walks the root directory, calling visit for each plain file. */
typedef bool (*dir_visit)(const char *name, u32 cluster, u32 size,
                          void *ctx);

static void walk_root(dir_visit visit, void *ctx)
{
    char lfn[64];
    bool have_lfn = false;
    lfn[0] = 0;
    static u8 dbuf[512];

    u32 cluster = vol.root_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        for (u32 s = 0; s < vol.spc; s++) {
            if (!blk_aux_read(cluster_lba(cluster) + s, 1, dbuf)) return;
            for (u32 i = 0; i < 512; i += 32) {
                const u8 *e = dbuf + i;
                if (e[0] == 0x00) return;            /* the end */
                if (e[0] == 0xE5) { have_lfn = false; continue; }

                if ((e[11] & 0x0F) == 0x0F) {
                    /* A piece of the long name: thirteen characters,
                     * ordered by the sequence byte. */
                    u32 ord = (e[0] & 0x1F);
                    if (ord >= 1 && ord <= 4) {
                        u32 at = (ord - 1) * 13;
                        static const u8 spots[13] = {
                            1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24,
                            28, 30
                        };
                        for (u32 k = 0; k < 13 && at + k < 63; k++) {
                            u16 ch = (u16)e[spots[k]] |
                                     ((u16)e[spots[k] + 1] << 8);
                            lfn[at + k] =
                                (ch >= 0x20 && ch < 0x7F) ? (char)ch
                                : ch == 0 ? 0 : '_';
                        }
                        lfn[63] = 0;
                        have_lfn = true;
                    }
                    continue;
                }

                if (e[11] & 0x18) { have_lfn = false; continue; }
                                       /* directories and labels stay */

                char name[64];
                take_name(e, lfn, have_lfn, name, sizeof(name));
                have_lfn = false;
                lfn[0] = 0;

                u32 cl = ((u32)e[26] | ((u32)e[27] << 8)) |
                         (((u32)e[20] | ((u32)e[21] << 8)) << 16);
                u32 size = (u32)e[28] | ((u32)e[29] << 8) |
                           ((u32)e[30] << 16) | ((u32)e[31] << 24);
                if (!visit(name, cl, size, ctx)) return;
            }
        }
        cluster = fat_get(cluster);
    }
}

struct take_ctx { object *into; u32 taken, skipped; };

static bool take_visit(const char *name, u32 cluster, u32 size,
                       void *ctxp)
{
    struct take_ctx *ctx = (struct take_ctx *)ctxp;
    if (size > FILE_MAX) { ctx->skipped++; return true; }
    if (list_has_name(ctx->into, name)) return true;

    u32 got = size ? read_chain(cluster, size) : 0;

    bool text = text_like(filebuf, got);
    object *o = obj_create(text ? TYPE_TEXT : TYPE_BYTES,
                           got + (text ? 512 : (got ? 0 : 8)), 0);
    if (!o) return false;
    if (got) memcpy(obj_data(o), filebuf, got);
    obj_set_name(o, name);

    if (list_add(ctx->into, o, name)) ctx->taken++;
    obj_release(o);
    return true;
}

u32 fat_take_in(object *into)
{
    if (!vol.ready || !into) return 0;
    struct take_ctx ctx = { into, 0, 0 };
    walk_root(take_visit, &ctx);
    if (ctx.skipped)
        kprintf("fat:  %u files were too big to carry\n", ctx.skipped);
    kprintf("fat:  took %u files in\n", ctx.taken);
    return ctx.taken;
}

/* --- writing out ----------------------------------------------------- */

struct name_ctx { const char *want; bool found; };

static bool name_visit(const char *name, u32 cluster, u32 size,
                       void *ctxp)
{
    (void)cluster; (void)size;
    struct name_ctx *ctx = (struct name_ctx *)ctxp;
    if (strcmp(name, ctx->want) == 0) { ctx->found = false; return false; }
    return true;
}

static bool file_exists(const char *nm)
{
    struct name_ctx ctx = { nm, true };
    walk_root(name_visit, &ctx);
    return !ctx.found;
}

/* Lays one payload down as a new file: clusters chained, the entry
 * placed in the first free root slot. */
static bool write_file(const u8 name83[11], const u8 *data, u32 size)
{
    /* The chain first. */
    u32 first = 0, prev = 0;
    u32 left = size, cur = 0;
    static u8 cbuf[512];

    u32 need = (size + vol.spc * 512 - 1) / (vol.spc * 512);

    for (u32 n = 0; n < need; n++) {
        u32 c = fat_free_cluster(prev ? prev + 1 : 3);
        if (c == 0) return false;
        if (!fat_set(c, 0x0FFFFFFF)) return false;
        if (prev && !fat_set(prev, c)) return false;
        if (!first) first = c;

        for (u32 s = 0; s < vol.spc; s++) {
            memset(cbuf, 0, 512);
            u32 take = left < 512 ? left : 512;
            if (take) memcpy(cbuf, data + cur, take);
            cur += take;
            left -= take;
            if (!blk_aux_write(cluster_lba(c) + s, 1, cbuf))
                return false;
        }
        prev = c;
    }

    /* Then the entry. */
    u32 cluster = vol.root_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        for (u32 s = 0; s < vol.spc; s++) {
            u64 lba = cluster_lba(cluster) + s;
            if (!blk_aux_read(lba, 1, sec)) return false;
            for (u32 i = 0; i < 512; i += 32) {
                if (sec[i] != 0x00 && sec[i] != 0xE5) continue;
                memset(sec + i, 0, 32);
                memcpy(sec + i, name83, 11);
                sec[i + 11] = 0x20;                  /* archive */
                sec[i + 26] = (u8)first;
                sec[i + 27] = (u8)(first >> 8);
                sec[i + 20] = (u8)(first >> 16);
                sec[i + 21] = (u8)(first >> 24);
                sec[i + 28] = (u8)size;
                sec[i + 29] = (u8)(size >> 8);
                sec[i + 30] = (u8)(size >> 16);
                sec[i + 31] = (u8)(size >> 24);
                return blk_aux_write(lba, 1, sec);
            }
        }
        u32 next = fat_get(cluster);
        if (next < 2 || next >= 0x0FFFFFF8) break;
        cluster = next;
    }
    return false;                    /* no room in the root directory */
}

u32 fat_write_out(object *from)
{
    if (!vol.ready || !from) return 0;
    u32 wrote = 0;

    for (u64 i = 0; i < obj_slots(from); i++) {
        object *t = obj_get_slot(from, i);
        if (!t) continue;
        if (!(obj_slot_rights(from, i) & CAP_READ)) continue;
        if (obj_type(t) != TYPE_TEXT && obj_type(t) != TYPE_BYTES)
            continue;

        const char *nm = obj_slot_name(from, i);
        if (!nm) nm = obj_name(t);
        if (!nm || !nm[0]) continue;

        /* Files that came in stay as they are; only what has no file
         * of its name yet goes out. The 8.3 collapse can collide two
         * long names -- the first one wins, said honestly. */
        char asname[64];
        u8 short83[11];
        make_short(nm, short83);
        take_name(short83, NULL, false, asname, sizeof(asname));
        if (file_exists(nm) || file_exists(asname)) continue;

        const u8 *d = (const u8 *)obj_data(t);
        if (!d) continue;
        u64 size = obj_size(t);
        u32 len = (u32)size;
        if (obj_type(t) == TYPE_TEXT) {
            u32 n = 0;
            while (n < size && d[n]) n++;
            len = n;
        }
        if (len > FILE_MAX) {
            kprintf("fat:  %s is too big to write out\n", nm);
            continue;
        }

        if (write_file(short83, d, len)) {
            wrote++;
            kprintf("fat:  wrote %s out\n", nm);
        } else {
            kprintf("fat:  could not write %s\n", nm);
        }
    }

    kprintf("fat:  wrote %u files out\n", wrote);
    return wrote;
}
