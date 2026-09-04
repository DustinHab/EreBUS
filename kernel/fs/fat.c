/*
 * fat.c -- FAT32: the exchange disk and the boot volume.
 * - read: directory files become objects (take in); write: objects become 8.3 files (write out)
 * - install: kernel.new -> kernel.elf with kernel.old kept; loader replaced via BOOTX64.NEW
 * - boot volume found on the store's disk (GPT EFI partition, MBR, or LBA 0), else the first port
 * - one FAT sector cached; limits: 4 MiB in, 16 MiB out, long names read only
 */
#include <eb/fat.h>
#include <eb/blk.h>
#include <eb/gpt.h>
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
    u32  disk;              /* which disk on the bus */
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
    return blk_disk_read(v->disk, lba, n, b);
}

static bool vwrite(fat_vol *v, u64 lba, u32 n, const void *b)
{
    return blk_disk_write(v->disk, lba, n, b);
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
static bool mount_on(fat_vol *v, i32 disk, const char *what)
{
    v->ready = false;
    v->fatsec_valid = false;
    v->fatsec_dirty = false;
    if (disk < 0) return false;
    v->disk = (u32)disk;

    if (vol_open(v, 0)) goto up;

    /* A disk with a partition table: the firmware's own partition,
     * wherever the table puts it. A disk this system settled on has
     * one, and it is not at the front. */
    {
        static gpt_table t;
        if (gpt_read((u32)disk, &t)) {
            for (u32 i = 0; i < GPT_ENTRIES; i++) {
                u8 type[16];
                u64 first, last;
                if (!gpt_entry(&t, i, type, &first, &last)) continue;
                if (memcmp(type, GPT_TYPE_EFI, 16) != 0) continue;
                if (first < 0xFFFFFFFFULL && vol_open(v, (u32)first)) goto up;
            }
        }
    }

    /* The old-style table: its first partition. */
    if (vread(v, 0, 1, sec) && sec[510] == 0x55 && sec[511] == 0xAA) {
        u32 pstart = (u32)sec[454] | ((u32)sec[455] << 8) |
                     ((u32)sec[456] << 16) | ((u32)sec[457] << 24);
        if (pstart && vol_open(v, pstart)) goto up;
    }
    kprintf("fat:  the %s is not fat32\n", what);
    return false;

up:
    kprintf("fat:  %s: fat32 volume, %u clusters of %u sectors\n",
            what, v->clusters, v->spc);
    return true;
}

bool fat_mount(void)        { return mount_on(&xchg, blk_aux_disk(), "exchange disk"); }
bool fat_present(void)      { return xchg.ready; }

/* The boot volume that counts is the one on the disk carrying the
 * store this machine runs with: that is the disk it will start from
 * next time, and the one 'install' should write. A machine running
 * from a stick beside a settled disk has its store on that disk and
 * nothing at the first port; a machine settled at the first port has
 * both there. Only with no store at all is the first port the guess. */
bool fat_boot_present(void)
{
    if (!bootv.ready) {
        /* The store's disk first; where that carries no boot volume --
         * a store that is a bare disk beside a boot disk, as the test
         * rig has it -- the first port after all. */
        i32 d = blk_store_disk();
        if (d >= 0) mount_on(&bootv, d, "boot disk");
        if (!bootv.ready && blk_boot_disk() >= 0 && blk_boot_disk() != d)
            mount_on(&bootv, blk_boot_disk(), "boot disk");
    }
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

static void fill_entry(u8 *e, const u8 name83[11], u32 first, u32 size, u8 attr)
{
    memset(e, 0, 32);
    memcpy(e, name83, 11);
    e[11] = attr;
    e[26] = (u8)first; e[27] = (u8)(first >> 8);
    e[20] = (u8)(first >> 16); e[21] = (u8)(first >> 24);
    e[28] = (u8)size; e[29] = (u8)(size >> 8);
    e[30] = (u8)(size >> 16); e[31] = (u8)(size >> 24);
}

/* One directory entry, placed in the first free slot; the directory
 * is grown by a cluster when it has none. */
static bool dir_add_entry(fat_vol *v, u32 dir, const u8 name83[11],
                          u32 first, u32 size, u8 attr)
{
    static u8 cbuf[512];
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
                fill_entry(sec + i, name83, first, size, attr);
                return vwrite(v, lba, 1, sec) && fat_flush(v);
            }
        }
        cluster = fat_get(v, cluster);
    }

    u32 c = fat_free_cluster(v, 3);
    if (c == 0) return false;
    if (!fat_set(v, c, END_CHAIN) || !fat_set(v, last, c)) return false;
    memset(cbuf, 0, 512);
    for (u32 s = 1; s < v->spc; s++)
        if (!vwrite(v, cluster_lba(v, c) + s, 1, cbuf)) return false;
    fill_entry(cbuf, name83, first, size, attr);
    return vwrite(v, cluster_lba(v, c), 1, cbuf) && fat_flush(v);
}

/* Lays one payload down as a new file in the directory: clusters
 * chained, the entry placed in the first free slot, the directory
 * grown by a cluster when it has none. */
static bool write_file_in(fat_vol *v, u32 dir, const u8 name83[11],
                          const u8 *data, u32 size)
{
    static u8 run[64 * 512];
    u32 need = (size + v->spc * 512 - 1) / (v->spc * 512);
    u32 first = 0, prev = 0;

    /* The chain first: every cluster claimed and linked, and on the
     * disk before anything points at it. */
    for (u32 n = 0; n < need; n++) {
        u32 c = fat_free_cluster(v, prev ? prev + 1 : 3);
        if (c == 0) return false;
        if (!fat_set(v, c, END_CHAIN)) return false;
        if (prev && !fat_set(v, prev, c)) return false;
        if (!first) first = c;
        prev = c;
    }
    if (!fat_flush(v)) return false;

    /* Then the bytes, in runs of neighbouring clusters up to sixty-four
     * sectors long: one command and one flush per run instead of one
     * per sector. On a fresh volume a file is one long run; a kernel
     * of two megabytes took a minute sector by sector. */
    u32 c = first, done = 0;
    while (done < size && c >= 2 && !chain_end(c)) {
        u32 run_first = c, clusters = 1;
        while ((clusters + 1) * v->spc <= 64) {
            u32 nx = fat_get(v, c);
            if (nx != c + 1) break;
            c = nx;
            clusters++;
        }
        u32 sectors = clusters * v->spc;
        u32 bytes = sectors * 512;
        u32 take = size - done < bytes ? size - done : bytes;
        memset(run, 0, bytes);
        memcpy(run, data + done, take);
        if (!vwrite(v, cluster_lba(v, run_first), sectors, run)) return false;
        done += take;
        c = fat_get(v, c);
    }

    return dir_add_entry(v, dir, name83, first, size, 0x20);
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
        kprintf("fat:  %u files exceeded the size limit\n", ctx.skipped);
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
        say_why(why, max, "not an elf image");
        return false;
    }
    if (len > WRITE_MAX) { say_why(why, max, "the kernel exceeds the size limit"); return false; }
    u32 dir;
    if (!boot_folder(&dir)) {
        say_why(why, max, "boot disk or its erebus folder not found");
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
    if (!dir_delete(&bootv, dir, "kernel.new")) { say_why(why, max, "kernel.new could not be deleted"); return false; }
    make_short("kernel.new", short83);
    if (!write_file_in(&bootv, dir, short83, elf, (u32)len)) {
        say_why(why, max, "the kernel could not be written; the boot disk is unchanged");
        return false;
    }

    /* Then the names turn: the running kernel steps aside as
     * kernel.old, the new one takes its place. */
    if (!dir_delete(&bootv, dir, "kernel.old")) { say_why(why, max, "kernel.old could not be deleted"); return false; }
    dir_entry e;
    if (dir_find(&bootv, dir, "kernel.elf", &e) &&
        !dir_rename(&bootv, dir, "kernel.elf", "kernel.old")) {
        say_why(why, max, "kernel.elf could not be renamed to kernel.old");
        return false;
    }
    if (!dir_rename(&bootv, dir, "kernel.new", "kernel.elf")) {
        say_why(why, max, "kernel.new could not be renamed to kernel.elf");
        return false;
    }
    put_count(dir, 0);
    fat_flush(&bootv);
    return true;
}

/* The loader, replaced under the name the firmware reads. There is no
 * stepping aside for this one: the firmware reads exactly one name,
 * and a loader is small and changes rarely. It goes in whole before
 * the old entry is removed, so a failure part way leaves the new file
 * as a stranger beside a working old one rather than nothing. */
bool fat_install_loader(const u8 *pe, u64 len, char *why, u32 max)
{
    if (!pe || len < 64 || pe[0] != 'M' || pe[1] != 'Z') {
        say_why(why, max, "not a pe image");
        return false;
    }
    if (len > WRITE_MAX) { say_why(why, max, "the loader exceeds the size limit"); return false; }
    if (!fat_boot_present()) { say_why(why, max, "boot disk not found"); return false; }

    dir_entry e;
    if (!dir_find(&bootv, bootv.root_cluster, "EFI", &e) || !(e.attr & 0x10) || e.cluster < 2) {
        say_why(why, max, "the boot disk has no EFI folder");
        return false;
    }
    u32 efi = e.cluster;
    if (!dir_find(&bootv, efi, "BOOT", &e) || !(e.attr & 0x10) || e.cluster < 2) {
        say_why(why, max, "the boot disk has no EFI\\BOOT folder");
        return false;
    }
    u32 boot = e.cluster;

    u8 short83[11];
    if (!dir_delete(&bootv, boot, "BOOTX64.NEW")) { say_why(why, max, "BOOTX64.NEW could not be deleted"); return false; }
    make_short("BOOTX64.NEW", short83);
    if (!write_file_in(&bootv, boot, short83, pe, (u32)len)) {
        say_why(why, max, "the loader could not be written; the boot disk is unchanged");
        return false;
    }
    if (!dir_delete(&bootv, boot, "BOOTX64.EFI")) { say_why(why, max, "BOOTX64.EFI could not be deleted; BOOTX64.NEW remains"); return false; }
    if (!dir_rename(&bootv, boot, "BOOTX64.NEW", "BOOTX64.EFI")) {
        say_why(why, max, "BOOTX64.NEW could not be renamed to BOOTX64.EFI");
        return false;
    }
    fat_flush(&bootv);
    return true;
}

/* --- a fresh boot volume --------------------------------------------- */

/* A directory, made in another: its own cluster with "." and "..",
 * and an entry in the parent. */
static bool make_dir_in(fat_vol *v, u32 parent, const char *name, u32 *made)
{
    static u8 cbuf[512];
    u32 c = fat_free_cluster(v, 3);
    if (c == 0) return false;
    if (!fat_set(v, c, END_CHAIN)) return false;

    memset(cbuf, 0, 512);
    u8 dot[11], dotdot[11];
    memset(dot, ' ', 11); dot[0] = '.';
    memset(dotdot, ' ', 11); dotdot[0] = '.'; dotdot[1] = '.';
    fill_entry(cbuf, dot, c, 0, 0x10);
    fill_entry(cbuf + 32, dotdot, parent == v->root_cluster ? 0 : parent, 0, 0x10);
    if (!vwrite(v, cluster_lba(v, c), 1, cbuf)) return false;
    memset(cbuf, 0, 512);
    for (u32 s = 1; s < v->spc; s++)
        if (!vwrite(v, cluster_lba(v, c) + s, 1, cbuf)) return false;
    if (!fat_flush(v)) return false;

    u8 name83[11];
    make_short(name, name83);
    if (!dir_add_entry(v, parent, name83, c, 0, 0x10)) return false;
    *made = c;
    return true;
}

/* A fat32 volume written from nothing onto a stretch of a disk: the
 * boot sector and its copy, the fs information sector, two fats with
 * the root directory's chain begun, the root directory empty. One
 * sector per cluster keeps the arithmetic plain; below some seventy
 * thousand sectors there would be too few clusters for fat32 at all. */
static bool fat_format32(u32 disk, u64 first, u64 sectors)
{
    static u8 chunk[64 * 512];
    if (sectors < 70000 || sectors > 0xFFFFFFFFULL) return false;
    u32 total = (u32)sectors;
    u32 reserved = 32, nfats = 2;
    u32 fatsz = (total + 127) / 128;            /* 128 entries a sector: more than enough */

    memset(chunk, 0, 512);
    u8 *b = chunk;
    b[0] = 0xEB; b[1] = 0x58; b[2] = 0x90;
    memcpy(b + 3, "EREBUS  ", 8);
    b[11] = 0; b[12] = 2;                        /* 512 bytes a sector */
    b[13] = 1;                                   /* one sector a cluster */
    b[14] = (u8)reserved; b[15] = (u8)(reserved >> 8);
    b[16] = (u8)nfats;
    b[21] = 0xF8;                                /* a fixed disk */
    b[24] = 63; b[26] = 255;                     /* sectors a track, heads: nobody looks */
    b[28] = (u8)first; b[29] = (u8)(first >> 8); b[30] = (u8)(first >> 16); b[31] = (u8)(first >> 24);
    b[32] = (u8)total; b[33] = (u8)(total >> 8); b[34] = (u8)(total >> 16); b[35] = (u8)(total >> 24);
    b[36] = (u8)fatsz; b[37] = (u8)(fatsz >> 8); b[38] = (u8)(fatsz >> 16); b[39] = (u8)(fatsz >> 24);
    b[44] = 2;                                   /* the root directory's cluster */
    b[48] = 1;                                   /* fs information sector */
    b[50] = 6;                                   /* the boot sector's copy */
    b[64] = 0x80;
    b[66] = 0x29;
    b[67] = 0x45; b[68] = 0x52; b[69] = 0x45; b[70] = 0x42;   /* a volume id: "EREB" */
    memcpy(b + 71, "EREBUS     ", 11);
    memcpy(b + 82, "FAT32   ", 8);
    b[510] = 0x55; b[511] = 0xAA;
    if (!blk_disk_write(disk, first, 1, chunk)) return false;
    if (!blk_disk_write(disk, first + 6, 1, chunk)) return false;

    memset(chunk, 0, 512);
    b[0] = 0x52; b[1] = 0x52; b[2] = 0x61; b[3] = 0x41;         /* "RRaA" */
    b[484] = 0x72; b[485] = 0x72; b[486] = 0x41; b[487] = 0x61; /* "rrAa" */
    memset(b + 488, 0xFF, 8);                                   /* free count, next free: unknown */
    b[510] = 0x55; b[511] = 0xAA;
    if (!blk_disk_write(disk, first + 1, 1, chunk)) return false;
    if (!blk_disk_write(disk, first + 7, 1, chunk)) return false;

    /* the fats, zero but for their first three entries; and the root
     * directory, zero */
    memset(chunk, 0, sizeof(chunk));
    for (u32 f = 0; f < nfats; f++) {
        u64 at = first + reserved + (u64)f * fatsz;
        for (u32 s = 0; s < fatsz; s += 64) {
            u32 n = fatsz - s < 64 ? fatsz - s : 64;
            if (!blk_disk_write(disk, at + s, n, chunk)) return false;
        }
    }
    memset(chunk, 0, 512);
    b[0] = 0xF8; b[1] = 0xFF; b[2] = 0xFF; b[3] = 0x0F;
    b[4] = 0xFF; b[5] = 0xFF; b[6] = 0xFF; b[7] = 0x0F;
    b[8] = 0xFF; b[9] = 0xFF; b[10] = 0xFF; b[11] = 0x0F;
    for (u32 f = 0; f < nfats; f++)
        if (!blk_disk_write(disk, first + reserved + (u64)f * fatsz, 1, chunk)) return false;
    memset(chunk, 0, 512);
    return blk_disk_write(disk, first + reserved + (u64)nfats * fatsz, 1, chunk);
}

bool fat_lay_boot_volume(u32 disk, u64 first, u64 sectors,
                         const u8 *loader, u64 lsize, const u8 *kernel, u64 ksize,
                         char *why, u32 max)
{
    static fat_vol tv;
    if (!fat_format32(disk, first, sectors)) { say_why(why, max, "the boot volume could not be formatted"); return false; }
    memset(&tv, 0, sizeof(tv));
    tv.disk = disk;
    if (!vol_open(&tv, first)) { say_why(why, max, "the fresh volume does not read back as fat32"); return false; }

    u32 efi = 0, boot = 0, erebus = 0;
    if (!make_dir_in(&tv, tv.root_cluster, "EFI", &efi) ||
        !make_dir_in(&tv, efi, "BOOT", &boot) ||
        !make_dir_in(&tv, tv.root_cluster, "erebus", &erebus)) {
        say_why(why, max, "the folders could not be made");
        return false;
    }
    u8 short83[11];
    make_short("BOOTX64.EFI", short83);
    if (!write_file_in(&tv, boot, short83, loader, (u32)lsize)) { say_why(why, max, "the loader could not be written"); return false; }
    make_short("kernel.elf", short83);
    if (!write_file_in(&tv, erebus, short83, kernel, (u32)ksize)) { say_why(why, max, "the kernel could not be written"); return false; }
    return fat_flush(&tv);
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
