/*
 * blob.c -- the log of big objects on the store.
 *
 * The generation ring keeps the graph, a megabyte per generation. What
 * does not fit that scale -- a source text, an image, a kernel -- lies
 * here instead, once, under the hash of its contents, and the
 * generations refer to it. The log is a plain sequence of entries, each
 * a header sector followed by the bytes, growing at the end. When it is
 * full it is compacted: whatever no generation refers to any more is
 * dropped, and the rest moves down.
 *
 * The hash is the name. Nothing here knows which object an entry
 * belongs to or how many generations point at it; the snapshot code
 * knows that, and hands the set of names still wanted to blob_compact.
 *
 * Crash safety follows the ring's rule: bytes first, header last. An
 * entry whose header never landed is not there, and the next write
 * takes its place. A compaction cut short can lose entries that only
 * older generations referred to; the generation being written next
 * stores anew whatever it needs and is whole.
 */
#include <eb/blob.h>
#include <eb/blk.h>
#include <eb/crypto.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/fmt.h>

#define BLOB_MAGIC    0x424F4C4242455245ULL     /* "EREBBLOB" */
#define BLOB_BASE_LBA 34816u                     /* 17 MiB: past the ring of generations */
#define BLOB_MAX      4096u
#define CHUNK_SECTORS 512u                       /* 256 KiB moved at a time */
#define IO_SECTORS    1024u                      /* one transfer at most */

typedef struct {
    u64 magic;
    u64 size;
    u8  hash[32];
    u64 seq;
} blob_header;

typedef struct {
    u8  hash[32];
    u64 lba;
    u64 size;
} entry;

static bool   scanned, usable;
static u64    base, end, next;        /* sectors: first, one past the last, first free */
static entry *table;
static u32    count;
static u64    seq;
static u8    *chunk;
static u8     sector[BLK_SECTOR_SIZE];

static u64 sectors_of(u64 size)
{
    return (size + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE;
}

static bool same(const u8 *a, const u8 *b)
{
    for (u32 i = 0; i < 32; i++) if (a[i] != b[i]) return false;
    return true;
}

static void copy32(u8 *dst, const u8 *src)
{
    for (u32 i = 0; i < 32; i++) dst[i] = src[i];
}

/* Reads one header and checks that it is one. */
static bool header_at(u64 lba, blob_header *h)
{
    if (lba < base || lba >= end) return false;
    if (!blk_read(lba, 1, sector)) return false;
    const blob_header *p = (const blob_header *)sector;
    if (p->magic != BLOB_MAGIC || p->size == 0) return false;
    if (lba + 1 + sectors_of(p->size) > end) return false;
    h->magic = p->magic;
    h->size = p->size;
    copy32(h->hash, p->hash);
    h->seq = p->seq;
    return true;
}

/* The log is read once, header by header, the first time anything asks
 * for it. Each header says how long its entry is, so the walk touches
 * one sector per entry and stops at the first sector that is no
 * header -- which is where the log ends. */
static bool ready(void)
{
    if (scanned) return usable;
    scanned = true;
    if (!blk_present()) return false;

    base = BLOB_BASE_LBA;
    end = blk_sectors();
    if (end < base + 64) {
        kprintf("blob: the store is too small for a log of big objects\n");
        return false;
    }

    phys_addr t = pmm_alloc_contig(PAGE_UP((u64)BLOB_MAX * sizeof(entry)) / PAGE_SIZE);
    phys_addr c = pmm_alloc_contig(CHUNK_SECTORS * BLK_SECTOR_SIZE / PAGE_SIZE);
    if (t == PMM_NO_FRAME || c == PMM_NO_FRAME) return false;
    table = (entry *)phys_to_virt(t);
    chunk = (u8 *)phys_to_virt(c);

    u64 lba = base, bytes = 0;
    blob_header h;
    count = 0;
    while (count < BLOB_MAX && header_at(lba, &h)) {
        copy32(table[count].hash, h.hash);
        table[count].lba = lba;
        table[count].size = h.size;
        count++;
        if (h.seq >= seq) seq = h.seq + 1;
        bytes += h.size;
        lba += 1 + sectors_of(h.size);
    }
    next = lba;
    usable = true;
    if (count)
        kprintf("blob: %u big objects in the log, %llu KiB, %llu KiB free\n",
                count, bytes / 1024, (end - next) * BLK_SECTOR_SIZE / 1024);
    return true;
}

bool blob_find(const u8 *hash, u64 *lba, u64 *size)
{
    if (!ready()) return false;
    for (u32 i = 0; i < count; i++) {
        if (!same(table[i].hash, hash)) continue;
        *lba = table[i].lba;
        *size = table[i].size;
        return true;
    }
    return false;
}

u64 blob_free_sectors(void)
{
    if (!ready()) return 0;
    return end - next;
}

u32 blob_count(void)
{
    if (!ready()) return 0;
    return count;
}

/* Whole sectors straight from the caller's memory, the tail through a
 * padded sector of our own: the payload ends where it ends, and the
 * bytes past it are nobody's to read. */
static bool write_bytes(u64 lba, const u8 *data, u64 size)
{
    u64 whole = size / BLK_SECTOR_SIZE, tail = size % BLK_SECTOR_SIZE;
    while (whole) {
        u64 take = whole < IO_SECTORS ? whole : IO_SECTORS;
        if (!blk_write(lba, (u32)take, data)) return false;
        lba += take;
        data += take * BLK_SECTOR_SIZE;
        whole -= take;
    }
    if (tail) {
        for (u32 i = 0; i < BLK_SECTOR_SIZE; i++) sector[i] = 0;
        for (u32 i = 0; i < tail; i++) sector[i] = data[i];
        if (!blk_write(lba, 1, sector)) return false;
    }
    return true;
}

static bool read_bytes(u64 lba, u8 *dst, u64 size)
{
    u64 whole = size / BLK_SECTOR_SIZE, tail = size % BLK_SECTOR_SIZE;
    while (whole) {
        u64 take = whole < IO_SECTORS ? whole : IO_SECTORS;
        if (!blk_read(lba, (u32)take, dst)) return false;
        lba += take;
        dst += take * BLK_SECTOR_SIZE;
        whole -= take;
    }
    if (tail) {
        if (!blk_read(lba, 1, sector)) return false;
        for (u32 i = 0; i < tail; i++) dst[i] = sector[i];
    }
    return true;
}

bool blob_store(const u8 *hash, const void *data, u64 size, u64 *lba_out)
{
    if (!ready() || size == 0) return false;

    u64 have, have_size;
    if (blob_find(hash, &have, &have_size) && have_size == size) {
        *lba_out = have;
        return true;
    }
    if (count >= BLOB_MAX) return false;

    u64 sectors = sectors_of(size);
    if (next + 1 + sectors > end) return false;

    if (!write_bytes(next + 1, (const u8 *)data, size)) return false;

    blob_header h;
    h.magic = BLOB_MAGIC;
    h.size = size;
    copy32(h.hash, hash);
    h.seq = seq++;
    for (u32 i = 0; i < BLK_SECTOR_SIZE; i++) sector[i] = 0;
    for (u32 i = 0; i < sizeof(h); i++) sector[i] = ((const u8 *)&h)[i];
    if (!blk_write(next, 1, sector)) return false;

    copy32(table[count].hash, hash);
    table[count].lba = next;
    table[count].size = size;
    count++;

    *lba_out = next;
    next += 1 + sectors;
    return true;
}

bool blob_read(const u8 *hash, u64 lba_hint, u64 size, void *dst)
{
    if (!ready() || size == 0) return false;

    u64 lba = lba_hint, have_size;
    blob_header h;
    bool where = header_at(lba, &h) && same(h.hash, hash) && h.size == size;
    if (!where) {
        if (!blob_find(hash, &lba, &have_size) || have_size != size) return false;
    }

    if (!read_bytes(lba + 1, (u8 *)dst, size)) return false;

    u8 got[32];
    sha256(dst, size, got);
    if (!same(got, hash)) {
        kprintf("blob: an entry of %llu bytes does not hash to its name\n", size);
        return false;
    }
    return true;
}

/* Sectors moved down the disk, front to back, so a destination that
 * overlaps its source is written only where it has already been read. */
static bool move_sectors(u64 from, u64 to, u64 n)
{
    while (n) {
        u64 take = n < CHUNK_SECTORS ? n : CHUNK_SECTORS;
        if (!blk_read(from, (u32)take, chunk)) return false;
        if (!blk_write(to, (u32)take, chunk)) return false;
        from += take;
        to += take;
        n -= take;
    }
    return true;
}

bool blob_compact(const u8 *live, u32 nlive)
{
    if (!ready()) return false;

    u64 dst = base;
    u32 kept = 0;
    for (u32 i = 0; i < count; i++) {
        entry *e = &table[i];
        bool wanted = false;
        for (u32 j = 0; j < nlive && !wanted; j++)
            if (same(live + (u64)j * 32, e->hash)) wanted = true;
        if (!wanted) continue;

        u64 sectors = sectors_of(e->size);
        if (e->lba != dst) {
            /* The header is taken into memory before the bytes move:
             * moving them can run over the sector it stood in. It is
             * written last, so a move cut short leaves no header
             * claiming bytes that are not all there. */
            static u8 head[BLK_SECTOR_SIZE];
            if (!blk_read(e->lba, 1, head)) return false;
            if (!move_sectors(e->lba + 1, dst + 1, sectors)) return false;
            if (!blk_write(dst, 1, head)) return false;
            e->lba = dst;
        }
        if (kept != i) {
            copy32(table[kept].hash, e->hash);
            table[kept].lba = e->lba;
            table[kept].size = e->size;
        }
        kept++;
        dst += 1 + sectors;
    }

    /* The end of the log: a sector that is no header. */
    if (dst < end) {
        for (u32 i = 0; i < BLK_SECTOR_SIZE; i++) sector[i] = 0;
        blk_write(dst, 1, sector);
    }

    u32 dropped = count - kept;
    count = kept;
    next = dst;
    kprintf("blob: compacted the log: %u kept, %u dropped, %llu KiB free\n",
            kept, dropped, (end - next) * BLK_SECTOR_SIZE / 1024);
    return true;
}
