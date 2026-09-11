/*
 * lib.c -- what the compiler calls on its own: the mem* family and the stack guard. This program is linked
 * against nothing, so these are its own; the guard is set from the cycle counter in start.S before main runs.
 */
#include <eb/types.h>

void *memset(void *dst, int c, usize n);
void *memcpy(void *dst, const void *src, usize n);
void *memmove(void *dst, const void *src, usize n);
int   memcmp(const void *a, const void *b, usize n);
void  __stack_chk_fail(void);

u64 __stack_chk_guard = 0x9E3779B97F4A7C15ULL;

void *memset(void *dst, int c, usize n)
{
    u8 *d = (u8 *)dst;
    while (n--) *d++ = (u8)c;
    return dst;
}

void *memcpy(void *dst, const void *src, usize n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, usize n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, usize n)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

/* A smashed frame: leave with a code the kernel does not read; the
 * browser sees a program that ended without an answer. */
void __stack_chk_fail(void)
{
    u64 nr = 0, code = 2;
    __asm__ volatile ("syscall" : "+a"(nr), "+D"(code) : : "rcx", "r11", "memory");
    for (;;) {}
}
