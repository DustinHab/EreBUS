/*
 * big.c -- contiguous frames for the language tools' tables; allocated once, never freed.
 */
#include <eb/lang.h>
#include <eb/pmm.h>
#include <eb/mm.h>
#include <eb/string.h>

void *lang_big_alloc(u64 size)
{
    u64 pages = (size + 4095) / 4096;
    phys_addr p = pmm_alloc_contig(pages);
    if (p == PMM_NO_FRAME) return NULL;
    void *v = phys_to_virt(p);
    memset(v, 0, pages * 4096);
    return v;
}
