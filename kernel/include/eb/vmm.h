#ifndef EB_VMM_H
#define EB_VMM_H

#include <eb/types.h>
#include <common/bootinfo.h>

/* Page table entry bits. Bit 63 is the one that matters most here: set,
 * the processor refuses to fetch instructions from the page. */
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITE    (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_PWT      (1ULL << 3)   /* write-through */
#define PTE_PCD      (1ULL << 4)   /* cache disable */
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY    (1ULL << 6)
#define PTE_HUGE     (1ULL << 7)   /* 2 MiB leaf at the directory level */
#define PTE_GLOBAL   (1ULL << 8)
#define PTE_NX       (1ULL << 63)

/* Ready-made combinations, so intent is visible at the call site. */
#define PAGE_KERNEL_CODE (PTE_PRESENT)                          /* r-x */
#define PAGE_KERNEL_RO   (PTE_PRESENT | PTE_NX)                 /* r-- */
#define PAGE_KERNEL_DATA (PTE_PRESENT | PTE_WRITE | PTE_NX)     /* rw- */
#define PAGE_KERNEL_MMIO (PTE_PRESENT | PTE_WRITE | PTE_NX | PTE_PCD)

/* Builds the kernel's own page tables, switches to them, and turns on
 * the hardware protections the processor offers. Replaces the coarse
 * tables the loader left behind, and drops the identity mapping with
 * them -- from here the lower half of the address space is empty and
 * belongs to user processes. */
void vmm_init(const eb_boot_info *bi);

/* Maps size bytes at va onto pa in the given table. Uses 2 MiB pages
 * where alignment and length allow, 4 KiB otherwise. */
bool vmm_map(phys_addr pml4, virt_addr va, phys_addr pa, u64 size, u64 flags);

/* Resolves a virtual address through the active tables. Returns false
 * if nothing is mapped there. */
bool vmm_resolve(virt_addr va, phys_addr *out_pa, u64 *out_flags);

/* Removes one 4 KiB leaf and returns the frame it mapped. Freeing that
 * frame is the caller's decision -- only the caller knows whether the
 * page was owned or shared. */
bool vmm_unmap_page(phys_addr pml4, virt_addr va, phys_addr *out_frame);

phys_addr vmm_kernel_pml4(void);

/* Gives the loader's now-unused page tables back to the frame
 * allocator. Only safe once vmm_init has switched away from them. */
void vmm_reclaim_loader_tables(const eb_boot_info *bi);

/* Which of the hardware protections are actually switched on. */
typedef struct {
    bool nx;
    bool wp;      /* CR0.WP -- read-only pages bind ring 0 as well */
    bool smep;
    bool smap;
    bool umip;
} vmm_protections;

vmm_protections vmm_active_protections(void);

/* Confirms that the mapping really is what it claims: code not
 * writable, data not executable, the identity mapping gone. */
bool vmm_selftest(void);

#endif /* EB_VMM_H */
