/*
 * bootinfo.h -- handover structure from the UEFI loader to the kernel.
 *
 * Included by BOTH sides: the loader (PE/COFF, MS ABI) and the kernel
 * (ELF, SysV ABI). Therefore fixed-width types only and no bitfields --
 * the memory layout must be identical on both sides.
 */
#ifndef EREBUS_BOOTINFO_H
#define EREBUS_BOOTINFO_H

typedef unsigned char      eb_u8;
typedef unsigned short     eb_u16;
typedef unsigned int       eb_u32;
typedef unsigned long long eb_u64;

/* "EREB" -- the kernel checks this first and stops if it is missing. */
#define EREBUS_BOOT_MAGIC   0x45524542u
#define EREBUS_BOOT_VERSION 1u

/* How the kernel sees a region of the address space. */
#define EB_MEM_FREE     0u  /* usable */
#define EB_MEM_RESERVED 1u  /* firmware or hardware, never touch */
#define EB_MEM_LOADER   2u  /* loader data, reclaimable after takeover */
#define EB_MEM_KERNEL   3u  /* kernel image and the handover data itself */
#define EB_MEM_ACPI     4u  /* ACPI tables, reclaimable once parsed */
#define EB_MEM_MMIO     5u  /* memory-mapped devices */

typedef struct {
    eb_u64 base;   /* physical start address, 4 KiB aligned */
    eb_u64 pages;  /* length in 4 KiB pages */
    eb_u32 type;   /* EB_MEM_* */
    eb_u32 _pad;
} eb_mem_range;

/* Framebuffer pixel format. Only 32 bits per pixel is supported. */
#define EB_FB_BGRX8888 0u  /* byte order B,G,R,X -- the common case */
#define EB_FB_RGBX8888 1u

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

    /* --- memory map -------------------------------------------------- */
    eb_u64 mem_ranges; /* pointer to eb_mem_range[mem_count] */
    eb_u64 mem_count;
    eb_u64 mem_free;   /* total free bytes, for reporting only */

    /* --- kernel image ------------------------------------------------ */
    eb_u64 kernel_phys; /* physical load address */
    eb_u64 kernel_size; /* bytes occupied, including bss */

    /* --- firmware ---------------------------------------------------- */
    eb_u64 acpi_rsdp;        /* zero if not found */
    eb_u64 efi_system_table; /* kept for later runtime services */
} eb_boot_info;

#endif /* EREBUS_BOOTINFO_H */
