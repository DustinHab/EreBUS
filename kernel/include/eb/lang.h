#ifndef EB_LANG_H
#define EB_LANG_H

#include <eb/types.h>

/* The compiler and the assembler keep a few large tables -- the tree,
 * the code and data being laid down, the labels. Those do not live in
 * the kernel image, whose room below the firmware's own memory is
 * small; they are asked for once, on first use, and kept. Zeroed
 * memory, or NULL when there is none. The host build answers this
 * from its own heap. */
void *lang_big_alloc(u64 size);

#endif /* EB_LANG_H */
