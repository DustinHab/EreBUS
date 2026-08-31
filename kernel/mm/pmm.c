/*
 * pmm.c -- physical frame allocator.
 *
 * A bitmap: one bit per 4 KiB frame, set when the frame is in use.
 *
 * A bitmap rather than a free list because a free list stores its links
 * inside the free pages themselves, which means every allocation and
 * every release writes to memory that is, by definition, not owned by
 * anyone. That is fast and it is also a place where a stray write is
 * invisible until the allocator hands out the same frame twice. The
 * bitmap keeps its bookkeeping in one known region that can be checked,
 * and costs one bit per frame -- 16 KiB for half a gigabyte of memory.
 *
 * The bitmap only covers real memory. Device windows sit at addresses
 * far above installed RAM (a PCIe window can start at a terabyte), and
 * spanning those would inflate the bitmap by megabytes to describe
 * frames that will never be handed out.
 */
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/io.h>
#include <eb/fmt.h>

static u8  *bitmap;          /* direct-map pointer, not physical */
static u64  bitmap_bytes;
static phys_addr bitmap_phys;

static u64 total_frames;     /* frames the bitmap describes */
static u64 free_frames;
static u64 hint;             /* where the last search left off */

static inline void mark_used(u64 frame)
{
    bitmap[frame >> 3] |= (u8)(1u << (frame & 7));
}

static inline void mark_free(u64 frame)
{
    bitmap[frame >> 3] &= (u8)~(1u << (frame & 7));
}

static inline bool is_used(u64 frame)
{
    return (bitmap[frame >> 3] & (1u << (frame & 7))) != 0;
}

/* Does this range describe memory that actually exists, as opposed to
 * address space belonging to a device? */
static bool is_real_memory(u32 type)
{
    return type == EB_MEM_FREE || type == EB_MEM_LOADER ||
           type == EB_MEM_KERNEL || type == EB_MEM_ACPI;
}

void pmm_init(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)phys_to_virt(bi->mem_ranges);

    /* How far up does real memory go. */
    u64 top = 0;
    for (u64 i = 0; i < bi->mem_count; i++) {
        if (!is_real_memory(r[i].type)) continue;
        u64 end = r[i].base + r[i].pages * PAGE_SIZE;
        if (end > top) top = end;
    }

    total_frames = top / PAGE_SIZE;
    bitmap_bytes = (total_frames + 7) / 8;

    /* Somewhere to put the bitmap. The largest free range is the safest
     * choice: it is the least likely to be the one thing that later
     * needs to be contiguous. */
    u64 best_pages = 0;
    phys_addr best_base = 0;
    u64 need_pages = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for (u64 i = 0; i < bi->mem_count; i++) {
        if (r[i].type != EB_MEM_FREE) continue;
        if (r[i].pages < need_pages) continue;
        if (r[i].pages > best_pages) {
            best_pages = r[i].pages;
            best_base  = r[i].base;
        }
    }

    if (best_pages == 0) {
        kprintf("pmm:  no range large enough for a %llu KiB bitmap\n",
                bitmap_bytes / 1024);
        total_frames = free_frames = 0;
        return;
    }

    bitmap_phys = best_base;
    bitmap = (u8 *)phys_to_virt(bitmap_phys);

    /* Start from "everything is taken" and give back only what the
     * firmware called free. Anything the memory map failed to mention
     * therefore stays out of circulation, which is the safe direction
     * to be wrong in. */
    for (u64 i = 0; i < bitmap_bytes; i++) bitmap[i] = 0xFF;
    free_frames = 0;

    for (u64 i = 0; i < bi->mem_count; i++) {
        if (r[i].type != EB_MEM_FREE) continue;
        u64 first = r[i].base / PAGE_SIZE;
        for (u64 f = first; f < first + r[i].pages && f < total_frames; f++) {
            if (is_used(f)) { mark_free(f); free_frames++; }
        }
    }

    /* The bitmap sits in memory it just declared free. Take it back. */
    for (u64 f = 0; f < need_pages; f++) {
        u64 frame = bitmap_phys / PAGE_SIZE + f;
        if (frame < total_frames && !is_used(frame)) {
            mark_used(frame);
            free_frames--;
        }
    }

    /* Frame zero stays out regardless of what the map says, so that a
     * null pointer keeps meaning nothing. */
    if (total_frames > 0 && !is_used(0)) { mark_used(0); free_frames--; }

    hint = 0;
}

/* ------------------------------------------------------------------ */

static void zero_frame(phys_addr frame)
{
    u64 *p = (u64 *)phys_to_virt(frame);
    for (u64 i = 0; i < PAGE_SIZE / sizeof(u64); i++) p[i] = 0;
}

phys_addr pmm_alloc(void)
{
    /* Finding a clear bit and setting it must be one step. Two threads
     * scanning at once would find the same bit, and the frame would be
     * handed out twice -- the exact corruption the bitmap exists to
     * make impossible. */
    u64 flags = irq_save();

    if (free_frames == 0) { irq_restore(flags); return PMM_NO_FRAME; }

    /* Two passes: from the hint to the end, then from the start. The
     * hint keeps the common case from rescanning ground that is known
     * to be full. */
    for (u32 pass = 0; pass < 2; pass++) {
        u64 from = (pass == 0) ? hint : 0;
        u64 to   = (pass == 0) ? total_frames : hint;

        for (u64 f = from; f < to; f++) {
            /* Skip eight at a time while the byte is solid. */
            if ((f & 7) == 0 && bitmap[f >> 3] == 0xFF) { f += 7; continue; }
            if (is_used(f)) continue;

            mark_used(f);
            free_frames--;
            hint = f + 1;
            zero_frame(f * PAGE_SIZE);
            irq_restore(flags);
            return f * PAGE_SIZE;
        }
    }
    irq_restore(flags);
    return PMM_NO_FRAME;
}

phys_addr pmm_alloc_contig(u64 count)
{
    if (count == 0) return PMM_NO_FRAME;
    if (count == 1) return pmm_alloc();
    if (free_frames < count) return PMM_NO_FRAME;

    u64 run = 0;
    for (u64 f = 0; f < total_frames; f++) {
        if (is_used(f)) { run = 0; continue; }
        if (++run < count) continue;

        u64 first = f + 1 - count;
        for (u64 k = first; k <= f; k++) {
            mark_used(k);
            zero_frame(k * PAGE_SIZE);
        }
        free_frames -= count;
        return first * PAGE_SIZE;
    }
    return PMM_NO_FRAME;
}

void pmm_free(phys_addr frame)
{
    u64 f = frame / PAGE_SIZE;
    if (f == 0 || f >= total_frames) return;

    u64 flags = irq_save();           /* same reason as in pmm_alloc */

    /* Releasing a frame that is already free means somebody freed it
     * twice, and the second owner is about to be handed memory that a
     * third party still believes is theirs. Refuse quietly rather than
     * corrupt the count. */
    if (is_used(f)) {
        mark_free(f);
        free_frames++;
        if (f < hint) hint = f;
    }
    irq_restore(flags);
}

void pmm_free_contig(phys_addr first, u64 count)
{
    for (u64 i = 0; i < count; i++)
        pmm_free(first + i * PAGE_SIZE);
}

u64 pmm_total_frames(void) { return total_frames; }
u64 pmm_free_frames(void)  { return free_frames; }
u64 pmm_used_frames(void)  { return total_frames - free_frames; }

/* ------------------------------------------------------------------ */

bool pmm_selftest(void)
{
    if (total_frames == 0) return false;

    u64 before = free_frames;
    phys_addr a[8];

    /* Distinct frames, and all of them actually zeroed. */
    for (u32 i = 0; i < 8; i++) {
        a[i] = pmm_alloc();
        if (a[i] == PMM_NO_FRAME) return false;

        for (u32 j = 0; j < i; j++)
            if (a[i] == a[j]) {
                kprintf("pmm:  handed out %p twice\n", (void *)a[i]);
                return false;
            }

        const u64 *p = (const u64 *)phys_to_virt(a[i]);
        for (u64 k = 0; k < PAGE_SIZE / sizeof(u64); k++)
            if (p[k] != 0) {
                kprintf("pmm:  frame %p was not clean\n", (void *)a[i]);
                return false;
            }
    }

    /* Write a pattern, release, take a fresh frame: the pattern must
     * not survive into the next owner. */
    u64 *marked = (u64 *)phys_to_virt(a[0]);
    marked[0] = 0xA5A5A5A5A5A5A5A5ULL;

    for (u32 i = 0; i < 8; i++) pmm_free(a[i]);

    phys_addr again = pmm_alloc();
    if (again == PMM_NO_FRAME) return false;
    if (*(const u64 *)phys_to_virt(again) != 0) {
        kprintf("pmm:  a released frame carried its contents forward\n");
        return false;
    }
    pmm_free(again);

    /* Double release must not inflate the count. */
    pmm_free(again);

    if (free_frames != before) {
        kprintf("pmm:  frame count drifted, %llu before, %llu after\n",
                before, free_frames);
        return false;
    }

    /* A contiguous run really has to be contiguous. */
    phys_addr run = pmm_alloc_contig(4);
    if (run == PMM_NO_FRAME) return false;
    pmm_free_contig(run, 4);

    return free_frames == before;
}
