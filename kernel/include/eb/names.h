#ifndef EB_NAMES_H
#define EB_NAMES_H

#include <eb/types.h>

/* The names of the kernel's own code, for reading an address back.
 *
 * A table lies in the image between __names_start and __names_end:
 * a count, the offset of the letters, then entries of address and
 * name offset in address order, then the letters. The outside build
 * makes it from the first link's symbols and links again with the
 * table last among the data, so nothing before it moves; the
 * machine's own linker writes it directly. A kernel without one
 * answers no name, and the crash report prints the address alone. */

/* The name of the code at addr and how far in, or NULL. */
const char *names_of(u64 addr, u64 *off);

/* How many names the table holds. */
u32 names_count(void);

#endif /* EB_NAMES_H */
