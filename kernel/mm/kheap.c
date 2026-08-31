/*
 * kheap.c -- the kernel heap.
 *
 * A doubly linked list of blocks in address order, first fit, with
 * neighbours merged on release. The simplest arrangement that does not
 * fragment itself to death.
 *
 * The header sits immediately in front of every allocation, which is
 * the usual arrangement and also the usual weakness: writing past the
 * end of one allocation lands on the next block's bookkeeping, and an
 * attacker who controls that controls where a later kfree writes. Two
 * things take the edge off it here.
 *
 * Every header carries a magic word, checked on release and on every
 * walk. Overrunning a block by even a few bytes destroys it, so the
 * damage is reported at the next operation instead of turning into a
 * corrupted list. And the magic is mixed with the block's own address,
 * so learning one header's value tells an attacker nothing about the
 * next -- a single constant would be written back trivially.
 *
 * That is detection, not prevention. Moving the metadata out of band
 * entirely is the real answer, and the object store in the next
 * milestone will not use this allocator for anything an untrusted
 * program can influence.
 */
#include <eb/kheap.h>
#include <eb/vmm.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/fmt.h>
#include <eb/panic.h>

/* Well clear of the direct map, which stops at PHYSMAP_BASE + 64 GiB. */
#define KHEAP_BASE      0xFFFFFF8000000000ULL
#define KHEAP_MAX       (1ULL << 30)      /* 1 GiB of window */
#define KHEAP_GROW_BY   (64 * PAGE_SIZE)  /* 256 KiB at a time */

#define ALIGN_TO        16ULL
#define MIN_PAYLOAD     32ULL

#define BLOCK_MAGIC     0x4B48454150ULL   /* "KHEAP" */

typedef struct block {
    u64           magic;
    u64           size;    /* payload bytes, not counting this header */
    struct block *next;    /* next block in address order */
    struct block *prev;
    u64           free;
    u64           _pad;    /* see the assertion below */
} block;

/* The header size decides the alignment of every payload: the caller
 * gets base + sizeof(block), so a header that is not itself a multiple
 * of the alignment hands out addresses that are not either. Five words
 * came to 40 bytes and quietly produced 8-byte-aligned allocations. */
_Static_assert(sizeof(block) % ALIGN_TO == 0,
               "block header must be a multiple of the alignment");

static block *first;
static virt_addr heap_end;      /* first address past the mapped window */
static u64 used_bytes, free_bytes;

/* The magic is per block, derived from where the block sits. A fixed
 * constant would be copied along with anything that overwrites it. */
static inline u64 magic_for(const block *b)
{
    return BLOCK_MAGIC ^ ((virt_addr)b * 0x9E3779B97F4A7C15ULL);
}

static inline void stamp(block *b) { b->magic = magic_for(b); }

static inline bool intact(const block *b)
{
    return b->magic == magic_for(b);
}

static void check(const block *b, const char *where)
{
    if (!intact(b))
        panic("heap header at %p damaged, found during %s", (void *)b, where);
}

/* ------------------------------------------------------------------ */

/* Extends the window by mapping fresh frames at its end and appending
 * one free block covering them. */
static bool grow(u64 need)
{
    u64 want = need + sizeof(block);
    if (want < KHEAP_GROW_BY) want = KHEAP_GROW_BY;
    want = PAGE_UP(want);

    if (heap_end + want > KHEAP_BASE + KHEAP_MAX) return false;

    virt_addr at = heap_end;
    for (u64 off = 0; off < want; off += PAGE_SIZE) {
        phys_addr frame = pmm_alloc();
        if (frame == PMM_NO_FRAME) {
            /* Hand back whatever was mapped in this attempt, so a
             * failed growth does not leak frames. */
            for (u64 back = 0; back < off; back += PAGE_SIZE) {
                phys_addr p;
                if (vmm_resolve(at + back, &p, NULL)) pmm_free(p);
            }
            return false;
        }
        if (!vmm_map(vmm_kernel_pml4(), at + off, frame, PAGE_SIZE,
                     PAGE_KERNEL_DATA)) {
            pmm_free(frame);
            return false;
        }
    }

    block *b = (block *)at;
    b->size = want - sizeof(block);
    b->free = 1;
    b->next = NULL;
    b->prev = NULL;
    stamp(b);

    if (!first) {
        first = b;
    } else {
        block *last = first;
        while (last->next) { check(last, "growth"); last = last->next; }
        last->next = b;
        b->prev = last;
    }

    heap_end += want;
    free_bytes += b->size;
    return true;
}

void kheap_init(void)
{
    first = NULL;
    heap_end = KHEAP_BASE;
    used_bytes = free_bytes = 0;

    if (!grow(KHEAP_GROW_BY))
        panic("could not establish the kernel heap");
}

/* Splits a block when the leftover is worth having as its own. */
static void split(block *b, u64 size)
{
    if (b->size < size + sizeof(block) + MIN_PAYLOAD) return;

    block *rest = (block *)((u8 *)b + sizeof(block) + size);
    rest->size = b->size - size - sizeof(block);
    rest->free = 1;
    rest->next = b->next;
    rest->prev = b;
    stamp(rest);

    if (b->next) b->next->prev = rest;
    b->next = rest;
    b->size = size;

    free_bytes -= sizeof(block);
}

void *kmalloc(u64 size)
{
    if (size == 0) return NULL;
    size = (size + ALIGN_TO - 1) & ~(ALIGN_TO - 1);
    if (size < MIN_PAYLOAD) size = MIN_PAYLOAD;

    for (u32 attempt = 0; attempt < 2; attempt++) {
        for (block *b = first; b; b = b->next) {
            check(b, "allocation");
            if (!b->free || b->size < size) continue;

            split(b, size);
            b->free = 0;
            used_bytes += b->size;
            free_bytes -= b->size;
            return (u8 *)b + sizeof(block);
        }
        if (attempt == 0 && !grow(size)) break;
    }
    return NULL;
}

void *kzalloc(u64 size)
{
    u8 *p = (u8 *)kmalloc(size);
    if (!p) return NULL;

    block *b = (block *)(p - sizeof(block));
    for (u64 i = 0; i < b->size; i++) p[i] = 0;
    return p;
}

/* Merges a block with the one after it, when both are free and they
 * really are adjacent -- blocks from different growth steps are not. */
static void merge_forward(block *b)
{
    block *n = b->next;
    if (!n || !n->free) return;
    if ((u8 *)b + sizeof(block) + b->size != (u8 *)n) return;

    check(n, "merge");
    b->size += sizeof(block) + n->size;
    b->next = n->next;
    if (n->next) n->next->prev = b;
    free_bytes += sizeof(block);
}

void kfree(void *p)
{
    if (!p) return;

    block *b = (block *)((u8 *)p - sizeof(block));
    check(b, "release");

    if (b->free)
        panic("heap block at %p released twice", p);

    b->free = 1;
    used_bytes -= b->size;
    free_bytes += b->size;

    /* Clearing on release rather than on allocation: a block sitting in
     * the free list is a block whose old contents nobody should be able
     * to read back, deliberately or by accident. */
    u8 *payload = (u8 *)p;
    for (u64 i = 0; i < b->size; i++) payload[i] = 0;

    merge_forward(b);
    if (b->prev && b->prev->free) merge_forward(b->prev);
}

u64 kheap_bytes_used(void) { return used_bytes; }
u64 kheap_bytes_free(void) { return free_bytes; }
u64 kheap_mapped(void)     { return heap_end - KHEAP_BASE; }

/* ------------------------------------------------------------------ */

bool kheap_selftest(void)
{
    u64 before = used_bytes;

    /* Distinct, aligned, non-overlapping. */
    void *a = kmalloc(24);
    void *b = kmalloc(1000);
    void *c = kmalloc(64);
    if (!a || !b || !c) return false;

    if (((virt_addr)a | (virt_addr)b | (virt_addr)c) & (ALIGN_TO - 1)) {
        kprintf("heap: an allocation came back misaligned\n");
        return false;
    }
    if (a == b || b == c || a == c) return false;

    /* Writing the full requested length must not disturb the others. */
    for (u64 i = 0; i < 24; i++)   ((u8 *)a)[i] = 0x11;
    for (u64 i = 0; i < 1000; i++) ((u8 *)b)[i] = 0x22;
    for (u64 i = 0; i < 64; i++)   ((u8 *)c)[i] = 0x33;
    for (u64 i = 0; i < 24; i++)   if (((u8 *)a)[i] != 0x11) return false;
    for (u64 i = 0; i < 1000; i++) if (((u8 *)b)[i] != 0x22) return false;

    /* Zeroed memory really is zeroed. */
    u8 *z = (u8 *)kzalloc(512);
    if (!z) return false;
    for (u64 i = 0; i < 512; i++) if (z[i] != 0) return false;
    kfree(z);

    kfree(b);
    kfree(a);
    kfree(c);

    /* A large request that forces the window to grow. */
    void *big = kmalloc(300 * 1024);
    if (!big) return false;
    ((u8 *)big)[300 * 1024 - 1] = 0x44;   /* the last byte must be there */
    kfree(big);

    if (used_bytes != before) {
        kprintf("heap: %llu bytes still booked after releasing everything\n",
                used_bytes - before);
        return false;
    }
    return true;
}
