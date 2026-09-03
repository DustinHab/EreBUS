/*
 * fat.c -- the exchange disk and the boot disk, spoken in the world's
 * own format.
 *
 * FAT32 because that is what sticks and cards actually wear, and what
 * the firmware reads the kernel from. The reader takes a directory's
 * files in as objects; the writer lays objects down as files with
 * plain 8.3 names; the installer turns the names in \erebus so that
 * the next start runs a kernel built here. Nothing here mounts,
 * caches or defers beyond one sector of the fat: every operation
 * walks the disk when it runs, and the honest limits -- 4 MiB per
 * file coming in and 16 MiB going out, long names read but not
 * written -- are stated rather than papered over.
 */
#include <eb/fat.h>
#include <eb/blk.h>
#include <eb/cap.h>
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/lang.h>

#define FILE_MAX  (4u * 1024 * 1024)      /* what comes in: the reader's buffer */
#define WRITE_MAX (16u * 1024 * 1024)     /* what goes out, straight from the object */
#define END_CHAIN 0x0FFFFFFF

typedef struct {
    bool ready;
    bool boot;              /* the boot disk, or else the exchange disk */
    u64  base;              /* lba where the volume begins */
    u32  spc;               /* sectors per cluster */
    u32  fat_start;         /* lba of the first fat */
    u32  fat_sectors;
    u32  nfats;
    u32  data_start;        /* lba of cluster 2 */
    u32  root_cluster;
    u32  clusters;

    /* One sector of the fat, held between uses: a chain's entries lie
     * side by side, and reading the same sector back for each of a
     * thousand clusters -- and writing it out twice per cluster -- was
     * most of what made a large file take minutes. Written back, to
     * every mirror, when another sector is needed or the work is done. */
    u8   fatsec[512];
    u64  fatsec_lba;
    bool fatsec_valid, fatsec_dirty;
} fat_vol;

static fat_vol xchg;        /* the exchange disk */
static fat_vol bootv;       /* the boot disk */

static u8 sec[512];

/* --- the disks ------------------------------------------------------- */

static bool vread(fat_vol *v, u64 lba, u32 n, void *b)
{
    return v->boot ? blk_boot_read(lba, n, b) : blk_aux_read(lba, n, b);
}

static bool vwrite(fat_vol *v, u64 lba, u32 n, const void *b)
{
    return v->boot ? blk_boot_write(lba, n, b) : blk_aux_write(lba, n, b);
}

/* --- the volume ------------------------------------------------------ */

static bool looks_fat32(const u8 *b)
{
    if (b[510] != 0x55 || b[511] != 0xAA) return false;
    u16 bps = (u16)b[11] | ((u16)b[12] << 8);
    u16 root_entries = (u16)b[17] | ((u16)b[18] << 8);
    u16 fatsz16 = (u16)b[22] | ((u16)b[23] << 8);
    return bps == 512 && root_entries == 0 && fatsz16 == 0 && b[13] != 0;
}

static bool vol_open(fat_vol *v, u64 base)
{
    if (!vread(v, base, 1, sec)) return false;
    if (!looks_fat32(sec)) return false;

    u16 reserved = (u16)sec[14] | ((u16)sec[15] << 8);
    u32 fatsz = (u32)sec[36] | ((u32)sec[37] << 8) |
                ((u32)sec[38] << 16) | ((u32)sec[39] << 24);
    u32 total = (u32)sec[32] | ((u32)sec[33] << 8) |
                ((u32)sec[34] << 16) | ((u32)sec[35] << 24);

    v->base = base;
    v->spc = sec[13];
    v->nfats = sec[16];
    v->fat_start = (u32)base + reserved;
    v->fat_sectors = fatsz;
    v->data_start = v->fat_start + v->nfats * fatsz;
    v->root_cluster = (u32)sec[44] | ((u32)sec[45] << 8) |
                      ((u32)sec[46] << 16) | ((u32)sec[47] << 24);
    v->clusters = (total - (v->data_start - (u32)base)) / v->spc;
    v->fatsec_valid = false;
    v->fatsec_dirty = false;
    v->ready = true;
    return true;
}

/* The volume begins at sector zero, or behind an mbr's first
 * partition -- both shapes exist in the world. */
static bool mount_on(fat_vol *v, bool boot, const char *what)
{
    v->ready = false;
    v->boot = boot;
    v->fatsec_valid = false;
    v->fatsec_dirty = false;
    if (boot ? !blk_boot_present() : !blk_aux_present()) return false;

    if (vol_open(v, 0)) goto up;
    if (vread(v, 0, 1, sec) && sec[510] == 0x55 && sec[511] == 0xAA) {
        u32 pstart = (u32)sec[454] | ((u32)sec[455] << 8) |
                     ((u32)sec[456] << 16) | ((u32)sec[457] << 24);
        if (pstart && vol_open(v, pstart)) goto up;
    }
    kprintf("fat:  the %s does not speak fat32\n", what);
    return false;

up:
    kprintf("fat:  %s: fat32 volume, %u clusters of %u sectors\n",
            what, v->clusters, v->spc);
    return true;
}

bool fat_mount(void)        { return mount_on(&xchg, false, "exchange disk"); }
bool fat_present(void)      { return xchg.ready; }

bool fat_boot_present(void)
{
    if (!bootv.ready && blk_boot_present()) mount_on(&bootv, true, "boot disk");
    return bootv.ready;
}

/* --- the fat itself -------------------------------------------------- */

static bool fat_flush(fat_vol *v)
{
    if (!v->fatsec_valid || !v->fatsec_dirty) return true;
    for (u32 f = 0; f < v->nfats; f++)
        if (!vwrite(v, v->fatsec_lba + (u64)f * v->fat_sectors, 1, v->fatsec))
            return false;
    v->fatsec_dirty = false;
    return true;
}

static bool fat_load(fat_vol *v, u64 lba)
{
    if (v->fatsec_valid && v->fatsec_lba == lba) return true;
    if (!fat_flush(v)) return false;
    v->fatsec_valid = false;
    if (!vread(v, lba, 1, v->fatsec)) return false;
    v->fatsec_lba = lba;
    v->fatsec_valid = true;
    return true;
}

static u32 fat_get(fat_vol *v, u32 cluster)
{
    u32 off = cluster * 4;
    if (!fat_load(v, v->fat_start + off / 512)) return END_CHAIN;
    u32 r = (u32)v->fatsec[off % 512] | ((u32)v->fatsec[off % 512 + 1] << 8) |
            ((u32)v->fatsec[off % 512 + 2] << 16) |
            ((u32)v->fatsec[off % 512 + 3] << 24);
    return r & 0x0FFFFFFF;
}

static bool fat_set(fat_vol *v, u32 cluster, u32 value)
{
    u32 off = cluster * 4;
    if (!fat_load(v, v->fat_start + off / 512)) return false;
    v->fatsec[off % 512]     = (u8)value;
    v->fatsec[off % 512 + 1] = (u8)(value >> 8);
    v->fatsec[off % 512 + 2] = (u8)(value >> 16);
    v->fatsec[off % 512 + 3] = (u8)((value >> 24) & 0x0F) |
                               (v->fatsec[off % 512 + 3] & 0xF0);
    v->fatsec_dirty = true;
    return true;
}

static u64 cluster_lba(const fat_vol *v, u32 cluster)
{
    return v->data_start + (u64)(cluster - 2) * v->spc;
}

static bool chain_end(u32 c) { return c < 2 || c >= 0x0FFFFFF8; }

static u32 fat_free_cluster(fat_vol *v, u32 after)
{
    for (u32 c = after < 3 ? 3 : after; c < v->clusters + 2; c++)
        if (fat_get(v, c) == 0) return c;
    return 0;
}

static u32 fat_free_count(fat_vol *v)
{
    u32 n = 0;
    for (u32 c = 3; c < v->clusters + 2; c++) if (fat_get(v, c) == 0) n++;
    return n;
}

/* Gives a chain back. */
static bool chain_free(fat_vol *v, u32 first)
{
    u32 c = first;
    u32 guard = 0;
    while (!chain_end(c) && guard++ < v->clusters + 2) {
        u32 next = fat_get(v, c);
        if (!fat_set(v, c, 0)) return false;
        c = next;
    }
    return true;
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

static bool same_name(const char *a, const char *b)
{
    u32 i = 0;
    for (; a[i] && b[i]; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return !a[i] && !b[i];
}

/* --- directories ------------------------------------------------------ */

/* Where an entry lies: the sector and the offset in it, and where the
 * run of long-name entries before it began. */
typedef struct {
    u64 lba, lfn_lba;
    u32 off, lfn_off;
    u32 lfn_count;
} dir_at;

typedef struct {
    char name[64];
    u32  cluster;
    u32  size;
    u8   attr;
    dir_at at;
} dir_entry;

/* Walks a directory, calling visit for each entry that is a file or
 * a folder; visit answers false to stop. */
typedef bool (*dir_visit)(const dir_entry *e, void *ctx);

static void walk_dir(fat_vol *v, u32 cluster, dir_visit visit, void *ctx)
{
    char lfn[64];
    bool have_lfn = false;
    lfn[0] = 0;
    static u8 dbuf[512];
    dir_at run = { 0, 0, 0, 0, 0 };

    u32 guard = 0;
    while (!chain_end(cluster) && guard++ < v->clusters + 2) {
        for (u32 s = 0; s < v->spc; s++) {
            u64 lba = cluster_lba(v, cluster) + s;
            if (!vread(v, lba, 1, dbuf)) return;
            for (u32 i = 0; i < 512; i += 32) {
                const u8 *e = dbuf + i;
                if (e[0] == 0x00) return;            /* the end */
                if (e[0] == 0xE5) { have_lfn = false; run.lfn_count = 0; continue; }

                if ((e[11] & 0x0F) == 0x0F) {
                    /* A piece of the long name: thirteen characters,
                     * ordered by the sequence byte. */
                    u32 ord = (e[0] & 0x1F);
                    if (!run.lfn_count) { run.lfn_lba = lba; run.lfn_off = i; }
                    run.lfn_count++;
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

                dir_entry d;
                if (e[11] & 0x08) { have_lfn = false; run.lfn_count = 0; continue; }   /* the label */
                take_name(e, lfn, have_lfn, d.name, sizeof(d.name));
                have_lfn = false;
                lfn[0] = 0;
                d.cluster = ((u32)e[26] | ((u32)e[27] << 8)) |
                            (((u32)e[20] | ((u32)e[21] << 8)) << 16);
                d.size = (u32)e[28] | ((u32)e[29] << 8) |
                         ((u32)e[30] << 16) | ((u32)e[31] << 24);
                d.attr = e[11];
                d.at.lba = lba;
                d.at.off = i;
                d.at.lfn_lba = run.lfn_lba;
                d.at.lfn_off = run.lfn_off;
                d.at.lfn_count = run.lfn_count;
                run.lfn_count = 0;
                if (d.name[0] == '.' && (d.name[1] == 0 || (d.name[1] == '.' && d.name[2] == 0))) continue;
                if (!visit(&d, ctx)) return;
            }
        }
        cluster = fat_get(v, cluster);
    }
}

struct find_ctx { const char *want; dir_entry *out; bool found; };

static bool find_visit(const dir_entry *e, void *ctxp)
{
    struct find_ctx *c = (struct find_ctx *)ctxp;
    if (!same_name(e->name, c->want)) return true;
    *c->out = *e;
    c->found = true;
    return false;
}

static bool dir_find(fat_vol *v, u32 dir, const char *name, dir_entry *out)
{
    struct find_ctx c = { name, out, false };
    walk_dir(v, dir, find_visit, &c);
    return c.found;
}

/* Marks an entry and the long-name entries before it as gone. */
static bool dir_erase(fat_vol *v, const dir_at *at)
{
    u64 lba = at->lfn_count ? at->lfn_lba : at->lba;
    u32 off = at->lfn_count ? at->lfn_off : at->off;
    u32 left = at->lfn_count + 1;
    while (left) {
        if (!vread(v, lba, 1, sec)) return false;
        while (left && off < 512) { sec[off] = 0xE5; off += 32; left--; }
        if (!vwrite(v, lba, 1, sec)) return false;
        off = 0;
        lba++;                      /* the run continues in the next sector of the cluster */
    }
    return true;
}

static bool dir_delete(fat_vol *v, u32 dir, const char *name)
{
    dir_entry e;
    if (!dir_find(v, dir, name, &e)) return true;      /* nothing to delete */
    if (e.attr & 0x10) return false;                    /* a folder stays */
    if (e.cluster >= 2 && !chain_free(v, e.cluster)) return false;
    return dir_erase(v, &e.at);
}

/* Gives an entry a new 8.3 name; its long name, if it had one, is
 * let go with the old name. */
static bool dir_rename(fat_vol *v, u32 dir, const char *from, const char *to)
{
    dir_entry e;
    if (!dir_find(v, dir, from, &e)) return false;
    u8 short83[11];
    make_short(to, short83);
    if (!vread(v, e.at.lba, 1, sec)) return false;
    memcpy(sec + e.at.off, short83, 11);
    sec[e.at.off + 12] = 0;                            /* no lowercase marks */
    if (!vwrite(v, e.at.lba, 1, sec)) return false;
    if (e.at.lfn_count) {
        dir_at lfn = e.at;
        lfn.lfn_count = e.at.lfn_count - 1;             /* the last is the entry itself, kept */
        /* erase only the long-name run: lfn_count entries from lfn_lba/lfn_off */
        u64 lba = e.at.lfn_lba;
        u32 off = e.at.lfn_off;
        u32 left = e.at.lfn_count;
        while (left) {
            if (!vread(v, lba, 1, sec)) return false;
            while (left && off < 512) { sec[off] = 0xE5; off += 32; left--; }
            if (!vwrite(v, lba, 1, sec)) return false;
            off = 0;
            lba++;
        }
        (void)lfn;
    }
    return true;
}

/* Lays one payload down as a new file in the directory: clusters
 * chained, the entry placed in the first free slot, the directory
 * grown by a cluster when it has none. */
static bool write_file_in(fat_vol *v, u32 dir, const u8 name83[11],
                          const u8 *data, u32 size)
{
    u32 first = 0, prev = 0;
    u32 left = size, cur = 0;
    static u8 cbuf[512];

    u32 need = (size + v->spc * 512 - 1) / (v->spc * 512);

    for (u32 n = 0; n < need; n++) {
        u32 c = fat_free_cluster(v, prev ? prev + 1 : 3);
        if (c == 0) return false;
        if (!fat_set(v, c, END_CHAIN)) return false;
        if (prev && !fat_set(v, prev, c)) return false;
        if (!first) first = c;

        for (u32 s = 0; s < v->spc; s++) {
            memset(cbuf, 0, 512);
            u32 take = left < 512 ? left : 512;
            if (take) memcpy(cbuf, data + cur, take);
            cur += take;
            left -= take;
            if (!vwrite(v, cluster_lba(v, c) + s, 1, cbuf))
                return false;
        }
        prev = c;
    }
    /* The chain is on the disk before anything points at it. */
    if (!fat_flush(v)) return false;

    /* Then the entry. */
    u32 cluster = dir;
    u32 last = dir;
    u32 guard = 0;
    while (!chain_end(cluster) && guard++ < v->clusters + 2) {
        last = cluster;
        for (u32 s = 0; s < v->spc; s++) {
            u64 lba = cluster_lba(v, cluster) + s;
            if (!vread(v, lba, 1, sec)) return false;
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
                return vwrite(v, lba, 1, sec) && fat_flush(v);
            }
        }
        cluster = fat_get(v, cluster);
    }

    /* No room: one more cluster for the directory, zeroed, and the
     * entry at its head. */
    u32 c = fat_free_cluster(v, 3);
    if (c == 0) return false;
    if (!fat_set(v, c, END_CHAIN) || !fat_set(v, last, c)) return false;
    memset(cbuf, 0, 512);
    for (u32 s = 1; s < v->spc; s++)
        if (!vwrite(v, cluster_lba(v, c) + s, 1, cbuf)) return false;
    memcpy(cbuf, name83, 11);
    cbuf[11] = 0x20;
    cbuf[26] = (u8)first; cbuf[27] = (u8)(first >> 8);
    cbuf[20] = (u8)(first >> 16); cbuf[21] = (u8)(first >> 24);
    cbuf[28] = (u8)size; cbuf[29] = (u8)(size >> 8);
    cbuf[30] = (u8)(size >> 16); cbuf[31] = (u8)(size >> 24);
    return vwrite(v, cluster_lba(v, c), 1, cbuf) && fat_flush(v);
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

/* The reader's buffer, asked for on first use: large enough for a
 * kernel's own sources, too large for the kernel image to carry. */
static u8 *filebuf;

/* Reads one chain into filebuf. Returns bytes read, capped. */
static u32 read_chain(fat_vol *v, u32 cluster, u32 size)
{
    u32 got = 0;
    u32 want = size > FILE_MAX ? FILE_MAX : size;
    static u8 cbuf[512];
    if (!filebuf) filebuf = (u8 *)lang_big_alloc(FILE_MAX);
    if (!filebuf) return 0;

    while (!chain_end(cluster) && got < want) {
        for (u32 s = 0; s < v->spc && got < want; s++) {
            if (!vread(v, cluster_lba(v, cluster) + s, 1, cbuf))
                return got;
            u32 take = want - got < 512 ? want - got : 512;
            memcpy(filebuf + got, cbuf, take);
            got += take;
        }
        cluster = fat_get(v, cluster);
    }
    return got;
}

struct take_ctx { object *into; u32 taken, skipped; };

static bool take_visit(const dir_entry *e, void *ctxp)
{
    struct take_ctx *ctx = (struct take_ctx *)ctxp;
    if (e->attr & 0x10) return true;                   /* folders stay on the disk */
    if (e->size > FILE_MAX) { ctx->skipped++; return true; }
    if (list_has_name(ctx->into, e->name)) return true;

    u32 got = e->size ? read_chain(&xchg, e->cluster, e->size) : 0;
    if (e->size && !filebuf) return false;

    bool text = text_like(filebuf, got);
    object *o = obj_create(text ? TYPE_TEXT : TYPE_BYTES,
                           got + (text ? 512 : (got ? 0 : 8)), 0);
    if (!o) return false;
    if (got) memcpy(obj_data(o), filebuf, got);
    obj_set_name(o, e->name);
    /* The machine's own copy from here on: it stays across boots, and
     * the disk's version is taken again only once this one is let go. */

    if (list_add(ctx->into, o, e->name)) ctx->taken++;
    obj_release(o);
    return true;
}

u32 fat_take_in(object *into)
{
    if (!xchg.ready || !into) return 0;
    struct take_ctx ctx = { into, 0, 0 };
    walk_dir(&xchg, xchg.root_cluster, take_visit, &ctx);
    if (ctx.skipped)
        kprintf("fat:  %u files were too big to carry\n", ctx.skipped);
    kprintf("fat:  took %u files in\n", ctx.taken);
    if (ctx.taken) obj_touch(into);        /* the graph changed; the next quiet moment writes it */
    return ctx.taken;
}

/* --- writing out ----------------------------------------------------- */

u32 fat_write_out(object *from)
{
    if (!xchg.ready || !from) return 0;
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
        dir_entry e;
        make_short(nm, short83);
        take_name(short83, NULL, false, asname, sizeof(asname));
        if (dir_find(&xchg, xchg.root_cluster, nm, &e) ||
            dir_find(&xchg, xchg.root_cluster, asname, &e)) continue;

        const u8 *d = (const u8 *)obj_data(t);
        if (!d) continue;
        u64 size = obj_size(t);
        u32 len = (u32)size;
        if (obj_type(t) == TYPE_TEXT) {
            u32 n = 0;
            while (n < size && d[n]) n++;
            len = n;
        }
        if (len > WRITE_MAX) {
            kprintf("fat:  %s is too big to write out\n", nm);
            continue;
        }

        if (write_file_in(&xchg, xchg.root_cluster, short83, d, len)) {
            wrote++;
            kprintf("fat:  wrote %s out\n", nm);
        } else {
            kprintf("fat:  could not write %s\n", nm);
        }
    }

    fat_flush(&xchg);
    kprintf("fat:  wrote %u files out\n", wrote);
    return wrote;
}

/* --- the boot disk: installing a kernel ------------------------------- */

static void say_why(char *why, u32 max, const char *s)
{
    if (!why || !max) return;
    u32 i = 0;
    while (s[i] && i + 1 < max) { why[i] = s[i]; i++; }
    why[i] = 0;
}

/* \erebus on the boot disk: its first cluster. */
static bool boot_folder(u32 *cluster)
{
    if (!fat_boot_present()) return false;
    dir_entry e;
    if (!dir_find(&bootv, bootv.root_cluster, "erebus", &e)) return false;
    if (!(e.attr & 0x10) || e.cluster < 2) return false;
    *cluster = e.cluster;
    return true;
}

/* A one-byte file, made anew. */
static bool put_count(u32 dir, u8 value)
{
    if (!dir_delete(&bootv, dir, "tries")) return false;
    u8 short83[11];
    make_short("tries", short83);
    u8 b = value;
    return write_file_in(&bootv, dir, short83, &b, 1);
}

bool fat_install_kernel(const u8 *elf, u64 len, char *why, u32 max)
{
    if (!elf || len < 64 || elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') {
        say_why(why, max, "that is not a kernel: it does not begin the way an elf does");
        return false;
    }
    if (len > WRITE_MAX) { say_why(why, max, "the kernel is too large to lay down"); return false; }
    u32 dir;
    if (!boot_folder(&dir)) {
        say_why(why, max, "the boot disk, or its erebus folder, is not to be found");
        return false;
    }
    u32 need = (u32)((len + bootv.spc * 512 - 1) / (bootv.spc * 512));
    if (fat_free_count(&bootv) < need + 4) {
        say_why(why, max, "the boot disk has no room for another kernel");
        return false;
    }

    /* The new one first, under a name of its own; nothing the next
     * start reads has changed yet. */
    u8 short83[11];
    if (!dir_delete(&bootv, dir, "kernel.new")) { say_why(why, max, "an old kernel.new would not go"); return false; }
    make_short("kernel.new", short83);
    if (!write_file_in(&bootv, dir, short83, elf, (u32)len)) {
        say_why(why, max, "the kernel could not be written; the boot disk is unchanged");
        return false;
    }

    /* Then the names turn: the running kernel steps aside as
     * kernel.old, the new one takes its place. */
    if (!dir_delete(&bootv, dir, "kernel.old")) { say_why(why, max, "the older kernel.old would not go"); return false; }
    dir_entry e;
    if (dir_find(&bootv, dir, "kernel.elf", &e) &&
        !dir_rename(&bootv, dir, "kernel.elf", "kernel.old")) {
        say_why(why, max, "the running kernel could not step aside");
        return false;
    }
    if (!dir_rename(&bootv, dir, "kernel.new", "kernel.elf")) {
        say_why(why, max, "the new kernel could not take the name; kernel.new lies there still");
        return false;
    }
    put_count(dir, 0);
    fat_flush(&bootv);
    return true;
}

u32 fat_boot_settle(void)
{
    u32 dir;
    if (!boot_folder(&dir)) return 0;
    dir_entry e;
    if (!dir_find(&bootv, dir, "tries", &e)) return 0;
    u32 was = 0;
    if (e.size >= 1 && e.cluster >= 2 && vread(&bootv, cluster_lba(&bootv, e.cluster), 1, sec)) was = sec[0];
    if (was) { put_count(dir, 0); fat_flush(&bootv); }
    return was;
}
