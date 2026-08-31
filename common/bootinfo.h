/*
 * bootinfo.h -- Übergabestruktur vom UEFI-Lader an den Kernel.
 *
 * Diese Datei wird von BEIDEN Seiten eingebunden: vom Lader (PE/COFF,
 * MS-ABI) und vom Kernel (ELF, SysV-ABI). Deshalb ausschließlich
 * festbreitige Typen und keine Bitfelder -- das Speicherlayout muss auf
 * beiden Seiten identisch sein.
 */
#ifndef EREBUS_BOOTINFO_H
#define EREBUS_BOOTINFO_H

typedef unsigned char      eb_u8;
typedef unsigned short     eb_u16;
typedef unsigned int       eb_u32;
typedef unsigned long long eb_u64;

/* "EREB" -- der Kernel prüft das als erstes und hält an, wenn es fehlt. */
#define EREBUS_BOOT_MAGIC   0x45524542u
#define EREBUS_BOOT_VERSION 1u

/* Art eines Speicherbereichs, wie ihn der Kernel sieht. */
#define EB_MEM_FREE     0u  /* frei nutzbar */
#define EB_MEM_RESERVED 1u  /* Firmware/Hardware, nie anfassen */
#define EB_MEM_LOADER   2u  /* Lader-Daten, nach Übernahme freigebbar */
#define EB_MEM_KERNEL   3u  /* Kernel-Abbild und Bootinfo selbst */
#define EB_MEM_ACPI     4u  /* ACPI-Tabellen, nach Auswertung freigebbar */
#define EB_MEM_MMIO     5u  /* speicherabgebildete Geräte */

typedef struct {
    eb_u64 base;   /* physische Startadresse, 4-KiB-ausgerichtet */
    eb_u64 pages;  /* Länge in 4-KiB-Seiten */
    eb_u32 type;   /* EB_MEM_* */
    eb_u32 _pad;
} eb_mem_range;

/* Pixelformat des Bildpuffers. Wir unterstützen nur 32 Bit pro Pixel. */
#define EB_FB_BGRX8888 0u  /* Byte-Reihenfolge B,G,R,X -- der Normalfall */
#define EB_FB_RGBX8888 1u

typedef struct {
    eb_u32 magic;
    eb_u32 version;

    /* --- Bildschirm ------------------------------------------------ */
    eb_u64 fb_base;    /* physische Adresse des Bildpuffers */
    eb_u64 fb_size;    /* Größe in Bytes */
    eb_u32 fb_width;   /* sichtbare Breite in Pixeln */
    eb_u32 fb_height;  /* sichtbare Höhe in Pixeln */
    eb_u32 fb_stride;  /* Pixel pro Zeile (kann > fb_width sein!) */
    eb_u32 fb_format;  /* EB_FB_* */

    /* --- Speicherkarte --------------------------------------------- */
    eb_u64 mem_ranges; /* Zeiger auf eb_mem_range[mem_count] */
    eb_u64 mem_count;
    eb_u64 mem_free;   /* Summe der freien Bytes, nur zur Anzeige */

    /* --- Kernel-Abbild --------------------------------------------- */
    eb_u64 kernel_phys; /* physische Ladeadresse */
    eb_u64 kernel_size; /* belegte Bytes inkl. bss */

    /* --- Firmware --------------------------------------------------- */
    eb_u64 acpi_rsdp;        /* 0, falls nicht gefunden */
    eb_u64 efi_system_table; /* für spätere Runtime-Services */
} eb_boot_info;

#endif /* EREBUS_BOOTINFO_H */
