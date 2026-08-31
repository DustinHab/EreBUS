/*
 * snapshot.c -- writing the object graph down and reading it back.
 */
#include <eb/snapshot.h>
#include <eb/blk.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/fmt.h>

#define SNAP_MAGIC   0x50414E5342455245ULL   /* "EREBSNAP" */
#define SNAP_VERSION 1u

/* Two slots, far apart on the disk so a write to one cannot possibly
 * disturb the other. Each holds a header sector followed by its data. */
#define SLOT_A_LBA   1u
#define SLOT_B_LBA   32768u                  /* 16 MiB in */
#define SLOT_MAX_SECTORS 4096u               /* 2 MiB per snapshot */
#define SNAP_MAX_OBJECTS 4096u

typedef struct {
    u64 magic;
    u32 version;
    u32 _pad;
    u64 generation;
    u64 object_count;
    u64 root_count;
    u64 data_bytes;
    u64 checksum;       /* over the data, not over this header */
} snap_header;

/* One object as it appears on disk. The payload follows, padded out to
 * eight bytes, and then the outgoing references as indices into this
 * snapshot rather than as pointers. */
typedef struct {
    u32 type;
    u32 slot_count;
    u64 size;
} snap_record;

static bool  have_snapshot;
static u64   current_generation;
static u64   last_bytes;
static u32   last_objects;

static u8   *buffer;          /* staging area, direct-map pointer */
static u64   buffer_bytes;

/* Objects collected during a walk, and where each landed. */
static object *collected[SNAP_MAX_OBJECTS];
static u32     collected_count;

/* ------------------------------------------------------------------ */

/* FNV-1a. Not a cryptographic hash and not meant to be one: it is here
 * to catch a torn write, not a forged snapshot. Signing is a separate
 * question and needs a key that does not live on the same disk. */
static u64 checksum(const u8 *data, u64 len)
{
    u64 h = 0xCBF29CE484222325ULL;
    for (u64 i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static bool ensure_buffer(void)
{
    if (buffer) return true;

    u64 pages = SLOT_MAX_SECTORS * BLK_SECTOR_SIZE / PAGE_SIZE;
    phys_addr p = pmm_alloc_contig(pages);
    if (p == PMM_NO_FRAME) return false;

    buffer = (u8 *)phys_to_virt(p);
    buffer_bytes = pages * PAGE_SIZE;
    return true;
}

/* ------------------------------------------------------------------ */
/* Walking the graph                                                    */
/* ------------------------------------------------------------------ */

/* Depth first, using the mark on each object so a shared target is
 * collected once and a cycle terminates instead of running forever.
 * This is the same walk a garbage collector would do, which is why the
 * mark lives on the object rather than in a table here. */
static bool collect(object *o)
{
    if (!o || obj_marked(o)) return true;
    if (collected_count >= SNAP_MAX_OBJECTS) return false;

    obj_set_mark(o, true);
    collected[collected_count++] = o;

    for (u64 i = 0; i < obj_slots(o); i++)
        if (!collect(obj_get_slot(o, i))) return false;

    return true;
}

static void clear_marks(void)
{
    for (u32 i = 0; i < collected_count; i++) obj_set_mark(collected[i], false);
}

/* Where an object ended up in the collected list, or -1. */
static i64 index_of(const object *o)
{
    if (!o) return -1;
    for (u32 i = 0; i < collected_count; i++)
        if (collected[i] == o) return (i64)i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

static u64 align8(u64 v) { return (v + 7) & ~7ULL; }

bool snap_save(object **roots, u32 root_count)
{
    if (!blk_present() || !ensure_buffer()) return false;
    if (root_count == 0) return false;

    collected_count = 0;

    /* Roots first and in order, so restoring hands them back the way
     * they were given. */
    for (u32 i = 0; i < root_count; i++)
        if (!collect(roots[i])) { clear_marks(); return false; }

    /* Lay the records out one after another. */
    u64 at = 0;
    for (u32 i = 0; i < collected_count; i++) {
        object *o = collected[i];
        u64 payload = obj_size(o);
        u64 slots = obj_slots(o);
        u64 need = sizeof(snap_record) + align8(payload) + slots * sizeof(i64);

        if (at + need > buffer_bytes) { clear_marks(); return false; }

        snap_record *r = (snap_record *)(buffer + at);
        r->type = obj_type(o);
        r->slot_count = (u32)slots;
        r->size = payload;
        at += sizeof(snap_record);

        const u8 *src = (const u8 *)obj_data(o);
        for (u64 b = 0; b < payload; b++) buffer[at + b] = src[b];
        for (u64 b = payload; b < align8(payload); b++) buffer[at + b] = 0;
        at += align8(payload);

        /* References become positions in this snapshot. A pointer means
         * nothing once the memory is gone; a position is still true
         * when the graph is built somewhere else entirely. */
        i64 *refs = (i64 *)(buffer + at);
        for (u64 s = 0; s < slots; s++)
            refs[s] = index_of(obj_get_slot(o, s));
        at += slots * sizeof(i64);
    }

    clear_marks();

    snap_header h = {
        .magic = SNAP_MAGIC,
        .version = SNAP_VERSION,
        .generation = current_generation + 1,
        .object_count = collected_count,
        .root_count = root_count,
        .data_bytes = at,
        .checksum = checksum(buffer, at),
    };

    /* Alternate slots, so the previous snapshot survives this one
     * failing halfway. */
    u32 base = (h.generation & 1) ? SLOT_A_LBA : SLOT_B_LBA;
    u32 data_sectors = (u32)((at + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE);
    if (data_sectors + 1 > SLOT_MAX_SECTORS) return false;

    /* Data first, header last. Until the header lands the slot is not
     * claimed, so an interruption anywhere before that leaves it as it
     * was rather than half new. */
    if (data_sectors && !blk_write(base + 1, data_sectors, buffer))
        return false;

    static u8 header_sector[BLK_SECTOR_SIZE];
    for (u32 i = 0; i < BLK_SECTOR_SIZE; i++) header_sector[i] = 0;
    for (u32 i = 0; i < sizeof(h); i++) header_sector[i] = ((const u8 *)&h)[i];

    if (!blk_write(base, 1, header_sector)) return false;

    current_generation = h.generation;
    have_snapshot = true;
    last_bytes = at;
    last_objects = collected_count;
    return true;
}

/* ------------------------------------------------------------------ */
/* Reading                                                             */
/* ------------------------------------------------------------------ */

static bool read_slot(u32 base, snap_header *out)
{
    static u8 sector[BLK_SECTOR_SIZE];
    if (!blk_read(base, 1, sector)) return false;

    const snap_header *h = (const snap_header *)sector;
    if (h->magic != SNAP_MAGIC) return false;
    if (h->version != SNAP_VERSION) return false;
    if (h->data_bytes > buffer_bytes) return false;
    if (h->object_count > SNAP_MAX_OBJECTS) return false;

    *out = *h;
    return true;
}

u32 snap_load(object **roots, u32 max_roots)
{
    if (!blk_present() || !ensure_buffer()) return 0;

    snap_header a, b;
    bool have_a = read_slot(SLOT_A_LBA, &a);
    bool have_b = read_slot(SLOT_B_LBA, &b);

    /* The newer generation wins, but only if its data survives the
     * checksum. If it does not, the older one is still there, which is
     * the entire reason for having two. */
    for (u32 attempt = 0; attempt < 2; attempt++) {
        bool use_a;
        if (attempt == 0)
            use_a = have_a && (!have_b || a.generation > b.generation);
        else
            use_a = have_a && have_b && !(a.generation > b.generation);

        if (attempt == 1 && !(have_a && have_b)) break;

        const snap_header *h = use_a ? &a : &b;
        if (!(use_a ? have_a : have_b)) continue;

        u32 base = use_a ? SLOT_A_LBA : SLOT_B_LBA;
        u32 sectors = (u32)((h->data_bytes + BLK_SECTOR_SIZE - 1)
                            / BLK_SECTOR_SIZE);

        if (sectors && !blk_read(base + 1, sectors, buffer)) continue;
        if (checksum(buffer, h->data_bytes) != h->checksum) {
            kprintf("snap: slot %c fails its checksum, falling back\n",
                    use_a ? 'a' : 'b');
            continue;
        }

        /* Two passes. The first creates every object, because a
         * reference may point forward as easily as back; the second
         * fills the references in, once there is something for them to
         * point at. */
        collected_count = 0;
        u64 at = 0;
        for (u64 i = 0; i < h->object_count; i++) {
            const snap_record *r = (const snap_record *)(buffer + at);
            at += sizeof(snap_record);

            object *o = obj_create(r->type, r->size, r->slot_count);
            if (!o) return 0;

            u8 *dst = (u8 *)obj_data(o);
            for (u64 bcount = 0; bcount < r->size; bcount++)
                dst[bcount] = buffer[at + bcount];

            at += align8(r->size) + (u64)r->slot_count * sizeof(i64);
            collected[collected_count++] = o;
        }

        at = 0;
        for (u64 i = 0; i < h->object_count; i++) {
            const snap_record *r = (const snap_record *)(buffer + at);
            at += sizeof(snap_record) + align8(r->size);

            const i64 *refs = (const i64 *)(buffer + at);
            for (u32 s = 0; s < r->slot_count; s++) {
                i64 target = refs[s];
                if (target >= 0 && target < (i64)collected_count)
                    obj_set_slot(collected[i], s, collected[target]);
            }
            at += (u64)r->slot_count * sizeof(i64);
        }

        u32 n = (u32)h->root_count;
        if (n > max_roots) n = max_roots;
        for (u32 i = 0; i < n; i++) {
            roots[i] = collected[i];
            obj_retain(roots[i]);
        }

        /* Everything created above came back with one reference of its
         * own. The graph now holds the rest, and the roots have been
         * retained for the caller, so let ours go. */
        for (u32 i = 0; i < collected_count; i++) obj_release(collected[i]);

        current_generation = h->generation;
        have_snapshot = true;
        last_bytes = h->data_bytes;
        last_objects = (u32)h->object_count;
        return n;
    }

    return 0;
}

bool snap_present(void)     { return have_snapshot; }
u64  snap_generation(void)  { return current_generation; }
u64  snap_bytes(void)       { return last_bytes; }
u32  snap_object_count(void){ return last_objects; }
