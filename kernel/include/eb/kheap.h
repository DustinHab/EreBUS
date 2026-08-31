#ifndef EB_KHEAP_H
#define EB_KHEAP_H

#include <eb/types.h>

/* The kernel heap: small, odd-sized allocations that would waste most
 * of a 4 KiB frame.
 *
 * It lives in its own window of virtual address space, grown a page at
 * a time from the frame allocator. Nothing here is fast; it is meant to
 * be obvious. Speed becomes interesting when there is something to
 * measure. */

void  kheap_init(void);

void *kmalloc(u64 size);
void *kzalloc(u64 size);       /* kmalloc plus zeroing */
void  kfree(void *p);

u64 kheap_bytes_used(void);
u64 kheap_bytes_free(void);
u64 kheap_mapped(void);

bool kheap_selftest(void);

#endif /* EB_KHEAP_H */
