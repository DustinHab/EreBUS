/*
 * bootinfo.h -- loader-to-kernel handover structure.
 * - included by loader (MS ABI) and kernel (SysV ABI): fixed-width types only, no bitfields
 */
#ifndef EREBUS_BOOTINFO_H
#define EREBUS_BOOTINFO_H

typedef unsigned char      eb_u8;
typedef unsigned short     eb_u16;
typedef unsigned int       eb_u32;
typedef unsigned long long eb_u64;

/* "EREB" -- the kernel checks this first and stops if it is missing.
 * The version is bumped whenever this struct changes shape, so a stale
 * loader paired with a fresh kernel says so instead of reading rubbish. */
#define EREBUS_BOOT_MAGIC   0x45524542u
#define EREBUS_BOOT_VERSION 3u   /* 3: the loader hands its files over; a kernel still reads 2 */

/* How the kernel sees a region of the address space. */
#define EB_MEM_FREE     0u  /* usable */
#define EB_MEM_RESERVED 1u  /* firmware or hardware, never touch */
#define EB_MEM_LOADER   2u  /* loader data, reclaimable after takeover */
#define EB_MEM_KERNEL   3u  /* kernel image, handover data, page tables */
#define EB_MEM_ACPI     4u  /* ACPI tables, reclaimable once parsed */
#define EB_MEM_MMIO     5u  /* memory-mapped devices */

typedef struct {
    eb_u64 base;   /* physical start address, 4 KiB aligned */
    eb_u64 pages;  /* length in 4 KiB pages */
    eb_u32 type;   /* EB_MEM_* */
    eb_u32 _pad;
} eb_mem_range;

/* The installed kernel did not come up twice running; the loader put
 * the previous one back and booted that. */
#define EB_BOOT_FELL_BACK 1u

/* Framebuffer pixel format. Only 32 bits per pixel is supported. */
#define EB_FB_BGRX8888 0u  /* byte order B,G,R,X -- the common case */
#define EB_FB_RGBX8888 1u

/* Where the loader puts things in the virtual address space.
 *
 * The direct map is a plain window onto all of physical memory: the
 * byte at physical address P is readable at EB_PHYSMAP_BASE + P. That
 * is how the kernel touches page tables and other physical memory once
 * the identity mapping is gone. */
#define EB_PHYSMAP_BASE 0xFFFF800000000000ULL
#define EB_KERNEL_BASE  0xFFFFFFFF80000000ULL

typedef struct {
    eb_u32 magic;
    eb_u32 version;

    /* --- display ---------------------------------------------------- */
    eb_u64 fb_base;    /* physical address of the framebuffer */
    eb_u64 fb_size;    /* size in bytes */
    eb_u32 fb_width;   /* visible width in pixels */
    eb_u32 fb_height;  /* visible height in pixels */
    eb_u32 fb_stride;  /* pixels per scanline -- may exceed fb_width */
    eb_u32 fb_format;  /* EB_FB_* */
    eb_u32 flags;      /* EB_BOOT_* : what the loader had to do on the way */

    /* --- memory map -------------------------------------------------- */
    eb_u64 mem_ranges; /* PHYSICAL pointer to eb_mem_range[mem_count] */
    eb_u64 mem_count;
    eb_u64 mem_free;   /* total free bytes, for reporting only */

    /* --- kernel image ------------------------------------------------ */
    eb_u64 kernel_phys; /* physical load address */
    eb_u64 kernel_virt; /* virtual address it was linked for */
    eb_u64 kernel_size; /* bytes occupied, including bss */

    /* --- address space the loader established ------------------------ */
    eb_u64 pml4;         /* physical address of the top level table */
    eb_u64 physmap_base; /* EB_PHYSMAP_BASE, repeated for clarity */
    eb_u64 physmap_size; /* how much physical memory is mapped there */
    eb_u64 pagetab_base; /* physical start of the loader's table pool */
    eb_u64 pagetab_size; /* its size in bytes */

    /* --- firmware ---------------------------------------------------- */
    eb_u64 acpi_rsdp;        /* zero if not found */
    eb_u64 efi_system_table; /* kept for later runtime services */

    /* --- the files the loader came with (from version 3) ------------ */
    eb_u64 loader_file;      /* physical address of BOOTX64.EFI as read, or zero */
    eb_u64 loader_file_size;
    eb_u64 kernel_file;      /* physical address of kernel.elf as read, or zero */
    eb_u64 kernel_file_size;
} eb_boot_info;

#endif /* EREBUS_BOOTINFO_H */
