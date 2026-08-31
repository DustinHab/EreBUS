#ifndef EB_MM_H
#define EB_MM_H

#include <eb/types.h>
#include <common/bootinfo.h>

/* ------------------------------------------------------------------ */
/* Moving between the two views of memory                              */
/* ------------------------------------------------------------------ */

/* The direct map means every physical byte has a fixed virtual address:
 * physical P is readable at PHYSMAP_BASE + P. So converting between the
 * two is addition, with no lookup and no failure case.
 *
 * The rule for the rest of the kernel: physical addresses are numbers
 * to be handed to hardware and page tables. They are never dereferenced.
 * Anything the kernel wants to read or write, it reaches through here. */
static inline void *phys_to_virt(phys_addr p)
{
    return (void *)(EB_PHYSMAP_BASE + p);
}

static inline phys_addr virt_to_phys(const void *v)
{
    return (phys_addr)v - EB_PHYSMAP_BASE;
}

/* The kernel image is mapped through its own window, not the direct
 * map, so it needs its own conversion. */
static inline phys_addr kernel_virt_to_phys(const void *v)
{
    return (phys_addr)v - EB_KERNEL_BASE;
}

#endif /* EB_MM_H */
