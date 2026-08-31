#ifndef EB_STRING_H
#define EB_STRING_H

#include <eb/types.h>

/* The routines in lib/string.c. The compiler calls the mem* family on
 * its own wherever it pleases; declaring them here is for the code that
 * calls them on purpose. */

void *memset(void *dst, int c, usize n);
void *memcpy(void *dst, const void *src, usize n);
void *memmove(void *dst, const void *src, usize n);
int   memcmp(const void *a, const void *b, usize n);
usize strlen(const char *s);
int   strcmp(const char *a, const char *b);

#endif /* EB_STRING_H */
