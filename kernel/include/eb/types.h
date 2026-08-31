/*
 * types.h -- the kernel's base types.
 *
 * Erebus has no C library. Everything that would otherwise come from
 * <stdint.h> lives here, with short names, because these types appear
 * in every other line.
 */
#ifndef EB_TYPES_H
#define EB_TYPES_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;

typedef u64                usize;
typedef i64                isize;
typedef u64                phys_addr;
typedef u64                virt_addr;

typedef _Bool              bool;
#define true  1
#define false 0

#ifndef NULL
#define NULL ((void *)0)
#endif

#define PAGE_SIZE  4096ULL
#define PAGE_SHIFT 12

#define PAGE_DOWN(x) ((u64)(x) & ~(PAGE_SIZE - 1))
#define PAGE_UP(x)   PAGE_DOWN((u64)(x) + PAGE_SIZE - 1)

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Provided by the linker script. The section markers are what lets the
 * kernel map itself with different permissions per section instead of
 * one permissive block: code executable but not writable, constants
 * neither, data writable but never executable. */
extern char __kernel_start[];
extern char __kernel_end[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __bss_start[];
extern char __bss_end[];

#endif /* EB_TYPES_H */
