/*
 * types.h -- Grundtypen des Kernels.
 *
 * Erebus benutzt keine C-Bibliothek. Alles, was sonst aus <stdint.h>
 * käme, steht hier -- und zwar mit kurzen Namen, weil diese Typen in
 * jeder zweiten Zeile vorkommen.
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
typedef u64                phys_addr;   /* physische Adresse */
typedef u64                virt_addr;   /* virtuelle Adresse */

typedef _Bool              bool;
#define true  1
#define false 0

#ifndef NULL
#define NULL ((void *)0)
#endif

#define PAGE_SIZE  4096ULL
#define PAGE_SHIFT 12

/* Auf- und Abrunden auf Seitengrenzen. */
#define PAGE_DOWN(x) ((u64)(x) & ~(PAGE_SIZE - 1))
#define PAGE_UP(x)   PAGE_DOWN((u64)(x) + PAGE_SIZE - 1)

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Vom Bindeskript gesetzt, umschließen das Kernelabbild. */
extern char __kernel_start[];
extern char __kernel_end[];
extern char __bss_start[];
extern char __bss_end[];

#endif /* EB_TYPES_H */
