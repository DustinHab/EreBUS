/*
 * snapshot.c -- writing the object graph down, and reading it back from
 * any point in its history.
 */
#include <eb/snapshot.h>
#include <eb/blob.h>
#include <eb/blk.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/crypto.h>

#define SNAP_MAGIC   0x50414E5342455245ULL   /* "EREBSNAP" */
#define SNAP_VERSION 5u
#define SNAP_OLDEST  4u                      /* still read: everything inline */

/* A ring of slots rather than two.
 *
 * Two slots make a snapshot survivable: one is always intact while the
 * other is being written. A ring makes it navigable as well. Every
 * write lands in the next slot round, so the last sixteen states of the
 * system are all still on the disk, and going back is reading one of
 * them rather than undoing anything.
 *
 * That is the difference between this and an undo history inside a
 * program. Undo belongs to the program and dies with it, covers only
 * what that program did, and has to be written by hand for every
 * document type. This belongs to the system, covers everything at once,
 * and nobody had to implement it. */
#define SNAP_BASE_LBA 1024u
#define SLOT_SECTORS  2048u                 /* 1 MiB each */
#define SLOT_COUNT    16u
#define SLOT_MAX_DATA ((u64)(SLOT_SECTORS - 1) * BLK_SECTOR_SIZE)

#define SNAP_MAX_OBJECTS 4096u

/* Big payloads leave the generation and go to the log.
 *
 * A slot holds a megabyte, and a system that keeps its own sources
 * carries more than that. So a text or a run of bytes from this size up
 * is written to the log of big objects instead, once, under the hash of
 * its contents, and the generation keeps the hash and where the log had
 * it. Two generations that share an unchanged text share one entry; an
 * edit adds one. Pictures, and everything that changes by design, stay
 * in the generation: they are bounded, and hashing them at every save
 * would buy nothing. */
#define BIG_FROM 4096u
#define REC_BIG  0x80000000u   /* on a record's type: its payload is a big_ref */

typedef struct {
    u8  hash[32];
    u64 lba;                      /* where the log had it; a hint */
} big_ref;

typedef struct {
    u64 magic;
    u32 version;
    u32 _pad;
    u64 generation;
    u64 object_count;
    u64 root_count;
    u64 data_bytes;
    u64 checksum;
} snap_header;

typedef struct {
    u32  type;
    u32  slot_count;
    u64  size;
    char name[OBJ_NAME_MAX];
} snap_record;

/* A reference on disk: where the target sits in this snapshot, what
 * following it permits, and what the holder calls it. */
typedef struct {
    i64  index;                   /* -1 for an empty slot */
    u32  rights;
    u32  _pad;
    char name[OBJ_NAME_MAX];
} snap_ref;

static bool have_snapshot;
static u64  current_generation;
static u64  last_bytes;
static u32  last_objects;

static u8  *buffer;
static u64  buffer_bytes;

static object *collected[SNAP_MAX_OBJECTS];
static u32     collected_count;

/* The big ones of the save at hand: hash and place, by collected index. */
static u8  big_hash[SNAP_MAX_OBJECTS][32];
static u64 big_lba[SNAP_MAX_OBJECTS];
static u8  is_big[SNAP_MAX_OBJECTS];

/* Every hash some generation on the disk still refers to, gathered
 * before the log is compacted. */
#define LIVE_MAX 8192u
static u8  *live;
static u32  live_count;

/* ------------------------------------------------------------------ */

static u32 slot_lba(u32 slot) { return SNAP_BASE_LBA + slot * SLOT_SECTORS; }

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

    u64 pages = PAGE_UP(SLOT_MAX_DATA) / PAGE_SIZE;
    phys_addr p = pmm_alloc_contig(pages);
    if (p == PMM_NO_FRAME) return false;

    buffer = (u8 *)phys_to_virt(p);
    buffer_bytes = pages * PAGE_SIZE;
    return true;
}

static u64 align8(u64 v) { return (v + 7) & ~7ULL; }

static bool same32(const u8 *a, const u8 *b)
{
    for (u32 i = 0; i < 32; i++) if (a[i] != b[i]) return false;
    return true;
}

/* How long a record's payload is in the buffer: a big one holds only
 * the reference, whatever its size says. */
static u64 payload_bytes(const snap_record *r)
{
    return (r->type & REC_BIG) ? sizeof(big_ref) : align8(r->size);
}

/* ------------------------------------------------------------------ */
/* Walking the graph                                                    */
/* ------------------------------------------------------------------ */

/* Depth first, using the mark on each object, so a shared target is
 * collected once and a loop terminates instead of running forever. This
 * is the walk a garbage collector would do, which is why the mark lives
 * on the object rather than in a table here. */
static bool collect(object *o)
{
    if (!o || obj_marked(o)) return true;
    if (obj_is_transient(o)) return true;     /* the tools' products: not history */
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

static i64 index_of(const object *o)
{
    if (!o) return -1;
    for (u32 i = 0; i < collected_count; i++)
        if (collected[i] == o) return (i64)i;
    return -1;
}

static void copy_name(char *dst, const char *src)
{
    for (u32 i = 0; i < OBJ_NAME_MAX; i++) dst[i] = 0;
    if (!src) return;
    for (u32 i = 0; i < OBJ_NAME_MAX - 1 && src[i]; i++) dst[i] = src[i];
}

static bool big_worthy(object *o)
{
    if (obj_size(o) < BIG_FROM || obj_is_fleeting(o)) return false;
    type_id t = obj_type(o);
    return t == TYPE_TEXT || t == TYPE_BYTES;
}

/* ------------------------------------------------------------------ */
/* Reading headers                                                     */
/* ------------------------------------------------------------------ */

static bool read_header(u32 slot, snap_header *out)
{
    static u8 sector[BLK_SECTOR_SIZE];
    if (!blk_read(slot_lba(slot), 1, sector)) return false;

    const snap_header *h = (const snap_header *)sector;
    if (h->magic != SNAP_MAGIC) return false;
    if (h->version < SNAP_OLDEST || h->version > SNAP_VERSION) return false;
    if (h->data_bytes > SLOT_MAX_DATA) return false;
    if (h->object_count > SNAP_MAX_OBJECTS) return false;

    *out = *h;
    return true;
}

/* ------------------------------------------------------------------ */
/* Room in the log                                                     */
/* ------------------------------------------------------------------ */

static bool ensure_live(void)
{
    if (live) return true;
    phys_addr p = pmm_alloc_contig(PAGE_UP((u64)LIVE_MAX * 32) / PAGE_SIZE);
    if (p == PMM_NO_FRAME) return false;
    live = (u8 *)phys_to_virt(p);
    return true;
}

static bool live_add(const u8 *h)
{
    for (u32 i = 0; i < live_count; i++)
        if (same32(live + (u64)i * 32, h)) return true;
    if (live_count >= LIVE_MAX) return false;
    for (u32 i = 0; i < 32; i++) live[(u64)live_count * 32 + i] = h[i];
    live_count++;
    return true;
}

/* Adds the big objects one generation on the disk refers to. The slot
 * is read into the buffer, which is free at this point: the save that
 * needs the room has not begun building yet. A generation that does
 * not read holds nothing. */
static bool live_from_slot(u32 slot)
{
    snap_header h;
    if (!read_header(slot, &h)) return true;
    if (h.version < 5) return true;

    u32 sectors = (u32)((h.data_bytes + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE);
    if (sectors && !blk_read(slot_lba(slot) + 1, sectors, buffer)) return true;
    if (checksum(buffer, h.data_bytes) != h.checksum) return true;

    u64 at = h.root_count * sizeof(u64);
    for (u64 i = 0; i < h.object_count; i++) {
        if (at + sizeof(snap_record) > h.data_bytes) return true;
        const snap_record *r = (const snap_record *)(buffer + at);
        at += sizeof(snap_record);
        u64 payload = payload_bytes(r);
        if (at + payload + (u64)r->slot_count * sizeof(snap_ref) > h.data_bytes) return true;
        if (r->type & REC_BIG) {
            const big_ref *b = (const big_ref *)(buffer + at);
            if (!live_add(b->hash)) return false;
        }
        at += payload + (u64)r->slot_count * sizeof(snap_ref);
    }
    return true;
}

/* Blanks the oldest generation, so the log can let go of what only it
 * referred to. The newest is never let go. */
static bool drop_oldest(void)
{
    u64 oldest = ~0ULL;
    i32 slot = -1;
    u32 n = 0;
    for (u32 s = 0; s < SLOT_COUNT; s++) {
        snap_header h;
        if (!read_header(s, &h)) continue;
        n++;
        if (h.generation < oldest) { oldest = h.generation; slot = (i32)s; }
    }
    if (n < 2 || slot < 0) return false;

    static u8 zero[BLK_SECTOR_SIZE];
    if (!blk_write(slot_lba((u32)slot), 1, zero)) return false;
    kprintf("snap: generation %llu let go, to make room in the log\n", oldest);
    return true;
}

/* Makes room in the log for the big objects of the save at hand: first
 * by compacting it down to what some generation still refers to, then,
 * if that is not enough, by letting the oldest generations go one by
 * one. */
static bool make_room(u64 need_sectors)
{
    for (;;) {
        if (blob_free_sectors() >= need_sectors) return true;
        if (!ensure_live()) return false;

        live_count = 0;
        for (u32 i = 0; i < collected_count; i++)
            if (is_big[i] && !live_add(big_hash[i])) return false;
        for (u32 s = 0; s < SLOT_COUNT; s++)
            if (!live_from_slot(s)) return false;

        if (!blob_compact(live, live_count)) return false;
        if (blob_free_sectors() >= need_sectors) return true;
        if (!drop_oldest()) return false;
    }
}

/* ------------------------------------------------------------------ */
/* Writing                                                             */
/* ------------------------------------------------------------------ */

bool snap_save(object **roots, u32 root_count)
{
    if (!blk_present() || !ensure_buffer()) return false;
    if (root_count == 0) return false;

    collected_count = 0;
    for (u32 i = 0; i < root_count; i++)
        if (!collect(roots[i])) { clear_marks(); return false; }

    /* The big ones first: hashed, looked up in the log, and written
     * there when new -- before the generation is built, so that it can
     * say where each of them lies. */
    u64 need = 0;
    for (u32 i = 0; i < collected_count; i++) {
        object *o = collected[i];
        is_big[i] = big_worthy(o) ? 1 : 0;
        if (!is_big[i]) continue;
        sha256(obj_data(o), obj_size(o), big_hash[i]);
        u64 lba, size;
        if (blob_find(big_hash[i], &lba, &size) && size == obj_size(o)) {
            big_lba[i] = lba;
        } else {
            big_lba[i] = 0;
            need += 1 + (obj_size(o) + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE;
        }
    }
    if (need && !make_room(need)) {
        clear_marks();
        kprintf("snap: no room in the log for %llu KiB of big objects\n",
                need * BLK_SECTOR_SIZE / 1024);
        return false;
    }
    for (u32 i = 0; i < collected_count; i++) {
        if (!is_big[i] || big_lba[i]) continue;
        object *o = collected[i];
        if (!blob_store(big_hash[i], obj_data(o), obj_size(o), &big_lba[i])) {
            clear_marks();
            kprintf("snap: the log would not take %llu bytes\n", obj_size(o));
            return false;
        }
    }

    /* The roots, by position, before anything else.
     *
     * They cannot be recovered from the order of the records. The walk
     * is depth first, so everything reachable from the first root is
     * collected before the second root is even reached -- taking the
     * first N records as the N roots silently hands back the wrong
     * objects, and the wrong ones are plausible enough to be used. */
    u64 at = (u64)root_count * sizeof(u64);
    if (at > SLOT_MAX_DATA) { clear_marks(); return false; }
    for (u32 i = 0; i < root_count; i++)
        ((u64 *)buffer)[i] = (u64)index_of(roots[i]);

    for (u32 i = 0; i < collected_count; i++) {
        object *o = collected[i];
        u64 payload = obj_size(o);
        u64 slots = obj_slots(o);
        u64 body = is_big[i] ? sizeof(big_ref) : align8(payload);
        u64 need_here = sizeof(snap_record) + body + slots * sizeof(snap_ref);

        if (at + need_here > SLOT_MAX_DATA) { clear_marks(); return false; }

        snap_record *r = (snap_record *)(buffer + at);
        r->type = obj_type(o) | (is_big[i] ? REC_BIG : 0);
        r->slot_count = (u32)slots;
        r->size = payload;
        copy_name(r->name, obj_name(o));
        at += sizeof(snap_record);

        if (is_big[i]) {
            big_ref *b = (big_ref *)(buffer + at);
            for (u32 k = 0; k < 32; k++) b->hash[k] = big_hash[i][k];
            b->lba = big_lba[i];
        } else {
            const u8 *src = (const u8 *)obj_data(o);
            for (u64 b = 0; b < payload; b++) buffer[at + b] = src[b];
            for (u64 b = payload; b < align8(payload); b++) buffer[at + b] = 0;
        }
        at += body;

        /* References become positions in this snapshot. A pointer means
         * nothing once the memory is gone; a position is still true
         * when the graph is rebuilt somewhere else entirely. */
        snap_ref *refs = (snap_ref *)(buffer + at);
        for (u64 s = 0; s < slots; s++) {
            refs[s].index = index_of(obj_get_slot(o, s));
            refs[s].rights = obj_slot_rights(o, s);
            refs[s]._pad = 0;
            copy_name(refs[s].name, obj_slot_name(o, s));
        }
        at += slots * sizeof(snap_ref);
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

    u32 slot = (u32)(h.generation % SLOT_COUNT);
    u32 base = slot_lba(slot);
    u32 sectors = (u32)((at + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE);
    if (sectors + 1 > SLOT_SECTORS) return false;

    /* Data first, header last. Until the header lands the slot is not
     * claimed, so an interruption anywhere before that leaves the slot
     * holding whatever it held before -- an older generation, still
     * whole. */
    if (sectors && !blk_write(base + 1, sectors, buffer)) return false;

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

u32 snap_history(u64 *generations, u32 max)
{
    if (!blk_present()) return 0;

    u32 n = 0;
    for (u32 s = 0; s < SLOT_COUNT && n < max; s++) {
        snap_header h;
        if (read_header(s, &h)) generations[n++] = h.generation;
    }

    /* Newest first: a history is read from the present backwards. */
    for (u32 i = 0; i + 1 < n; i++)
        for (u32 j = i + 1; j < n; j++)
            if (generations[j] > generations[i]) {
                u64 t = generations[i];
                generations[i] = generations[j];
                generations[j] = t;
            }
    return n;
}

/* Rebuilds a graph from the buffer. Two passes: the first creates every
 * object, because a reference may point forward as easily as back, and
 * the second fills the references in once there is something to point
 * at. A big object's bytes come from the log; a generation whose log
 * entry is gone is not rebuilt half-way but refused whole, and the one
 * before it is tried. */
static u32 rebuild(const snap_header *h, object **roots, u32 max_roots)
{
    collected_count = 0;
    u64 root_table = (u64)h->root_count * sizeof(u64);
    u64 at = root_table;

    for (u64 i = 0; i < h->object_count; i++) {
        /* The checksum vouches for the bytes, not for their sense: a
         * record that reaches past the data is refused rather than
         * read past the buffer. */
        if (at + sizeof(snap_record) > h->data_bytes) goto refuse;
        const snap_record *r = (const snap_record *)(buffer + at);
        at += sizeof(snap_record);
        u64 payload = payload_bytes(r);
        if (at + payload + (u64)r->slot_count * sizeof(snap_ref) > h->data_bytes) goto refuse;

        object *o = obj_create(r->type & ~REC_BIG, r->size, r->slot_count);
        if (!o) goto refuse;
        collected[collected_count++] = o;

        u8 *dst = (u8 *)obj_data(o);
        if (r->type & REC_BIG) {
            const big_ref *b = (const big_ref *)(buffer + at);
            if (!blob_read(b->hash, b->lba, r->size, dst)) {
                kprintf("snap: generation %llu names a big object of %llu bytes the log no longer has\n",
                        h->generation, r->size);
                goto refuse;
            }
        } else {
            for (u64 b = 0; b < r->size; b++) dst[b] = buffer[at + b];
        }
        obj_set_name(o, r->name[0] ? r->name : NULL);

        at += payload + (u64)r->slot_count * sizeof(snap_ref);
    }

    at = root_table;
    for (u64 i = 0; i < h->object_count; i++) {
        const snap_record *r = (const snap_record *)(buffer + at);
        at += sizeof(snap_record) + payload_bytes(r);

        const snap_ref *refs = (const snap_ref *)(buffer + at);
        for (u32 s = 0; s < r->slot_count; s++) {
            i64 target = refs[s].index;
            if (target >= 0 && target < (i64)collected_count) {
                obj_set_slot(collected[i], s, collected[target],
                             refs[s].rights);
                obj_set_slot_name(collected[i], s,
                                  refs[s].name[0] ? refs[s].name : NULL);
            }
        }
        at += (u64)r->slot_count * sizeof(snap_ref);
    }

    u32 n = (u32)h->root_count;
    if (n > max_roots) n = max_roots;
    for (u32 i = 0; i < n; i++) {
        u64 index = ((const u64 *)buffer)[i];
        if (index >= collected_count) { roots[i] = NULL; continue; }
        roots[i] = collected[index];
        obj_retain(roots[i]);
    }

    /* Everything created above came back holding one reference. The
     * graph now holds the rest and the roots have been retained for the
     * caller, so let ours go. */
    for (u32 i = 0; i < collected_count; i++) obj_release(collected[i]);
    return n;

refuse:
    /* Nothing points at what was made so far; letting go frees it. */
    for (u32 i = 0; i < collected_count; i++) obj_release(collected[i]);
    collected_count = 0;
    return 0;
}

static u32 load_slot(u32 slot, object **roots, u32 max_roots,
                     bool make_current)
{
    snap_header h;
    if (!read_header(slot, &h)) return 0;

    u32 sectors = (u32)((h.data_bytes + BLK_SECTOR_SIZE - 1)
                        / BLK_SECTOR_SIZE);
    if (sectors && !blk_read(slot_lba(slot) + 1, sectors, buffer)) return 0;

    if (checksum(buffer, h.data_bytes) != h.checksum) {
        kprintf("snap: generation %llu fails its checksum, skipping\n",
                h.generation);
        return 0;
    }

    u32 n = rebuild(&h, roots, max_roots);
    if (n && make_current) {
        current_generation = h.generation;
        have_snapshot = true;
        last_bytes = h.data_bytes;
        last_objects = (u32)h.object_count;
    }
    return n;
}

u32 snap_load(object **roots, u32 max_roots)
{
    if (!blk_present() || !ensure_buffer()) return 0;

    /* Newest first, and if that one is damaged the one before it is
     * still there -- which is the whole reason for a ring. */
    for (;;) {
        u64 best = 0;
        i32 best_slot = -1;

        for (u32 s = 0; s < SLOT_COUNT; s++) {
            snap_header h;
            if (!read_header(s, &h)) continue;
            if (h.generation > best) { best = h.generation; best_slot = (i32)s; }
        }
        if (best_slot < 0) return 0;

        u32 n = load_slot((u32)best_slot, roots, max_roots, true);
        if (n) return n;

        /* That one is unreadable. Blank its header so the next round
         * looks past it rather than at it again. */
        static u8 zero[BLK_SECTOR_SIZE];
        blk_write(slot_lba((u32)best_slot), 1, zero);
    }
}

u32 snap_load_generation(u64 generation, object **roots, u32 max_roots)
{
    if (!blk_present() || !ensure_buffer()) return 0;

    for (u32 s = 0; s < SLOT_COUNT; s++) {
        snap_header h;
        if (!read_header(s, &h)) continue;
        if (h.generation != generation) continue;

        /* Reading an old generation does not make it the present one.
         * Looking at the past must not be able to overwrite the future:
         * the next save still follows on from the newest state. */
        return load_slot(s, roots, max_roots, false);
    }
    return 0;
}

bool snap_present(void)      { return have_snapshot; }
u64  snap_generation(void)   { return current_generation; }
u64  snap_bytes(void)        { return last_bytes; }
u32  snap_object_count(void) { return last_objects; }
u32  snap_slot_count(void)   { return SLOT_COUNT; }
