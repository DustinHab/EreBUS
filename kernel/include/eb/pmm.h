#ifndef EB_PMM_H
#define EB_PMM_H

#include <eb/types.h>
#include <common/bootinfo.h>

/* Physical frame allocator.
 *
 * Hands out 4 KiB frames of physical memory. Everything above it --
 * page tables, the kernel heap, later the object store -- gets its
 * memory from here.
 *
 * Frames are handed back as physical addresses, deliberately. A
 * physical address is a number for hardware and page tables; to touch
 * the memory, run it through phys_to_virt first. Keeping the two types
 * distinct in the reader's head is worth the small inconvenience. */

#define PMM_NO_FRAME ((phys_addr)0)

void pmm_init(const eb_boot_info *bi);

/* One frame, or PMM_NO_FRAME when memory is exhausted. The frame is
 * always zeroed before it is handed out: a frame that once belonged to
 * something else must not carry its contents into its next owner. That
 * costs a page of writes per allocation and closes an entire class of
 * information leak. */
phys_addr pmm_alloc(void);

/* A run of consecutive frames, for the few things that genuinely need
 * contiguous physical memory (device buffers, large tables). */
phys_addr pmm_alloc_contig(u64 count);

void pmm_free(phys_addr frame);
void pmm_free_contig(phys_addr first, u64 count);

u64 pmm_total_frames(void);
u64 pmm_free_frames(void);
u64 pmm_used_frames(void);

/* Runs a short exercise of the allocator and reports the outcome.
 * Called once during start-up: a memory allocator that hands out the
 * same frame twice is not something to discover later. */
bool pmm_selftest(void);

#endif /* EB_PMM_H */
