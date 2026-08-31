/*
 * main.c -- Einstieg in den Erebus-Kernel.
 *
 * Meilenstein 1: Übergabe vom Lader prüfen, Bildschirm und serielle
 * Schnittstelle in Betrieb nehmen, den vorgefundenen Rechner beschreiben.
 * Noch keine Unterbrechungen, keine Speicherverwaltung, keine Prozesse --
 * das kommt der Reihe nach.
 */
#include <eb/types.h>
#include <eb/io.h>
#include <eb/fb.h>
#include <eb/fmt.h>
#include <eb/panic.h>
#include <eb/serial.h>
#include <common/bootinfo.h>

#define EREBUS_VERSION "0.1"

/* Farbwelt: dunkel, ruhig, wenige Akzente. */
#define C_BG      RGB( 14,  16,  22)
#define C_HEAD_A  RGB( 26,  31,  44)
#define C_HEAD_B  RGB( 16,  19,  27)
#define C_ACCENT  RGB(122, 172, 255)
#define C_TEXT    RGB(208, 214, 226)
#define C_DIM     RGB(118, 128, 148)

/* Alle Abstände hängen an dieser Zahl, damit das Bild auf 1024x768
 * genauso aufgeht wie auf einem 4K-Panel. */
static i32 ui;

static const char *mem_type_name(u32 t)
{
    switch (t) {
    case EB_MEM_FREE:     return "frei";
    case EB_MEM_RESERVED: return "reserviert";
    case EB_MEM_LOADER:   return "Lader";
    case EB_MEM_KERNEL:   return "Kernel";
    case EB_MEM_ACPI:     return "ACPI";
    case EB_MEM_MMIO:     return "Geräte";
    default:              return "unbekannt";
    }
}

/* Größe in einer Einheit ausgeben, die man lesen kann, ohne Nullen zu
 * zählen. Eine Nachkommastelle, ganzzahlig gerechnet. */
static void print_size(u64 bytes)
{
    static const char *unit[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    u32 u = 0;
    u64 whole = bytes, frac = 0;

    while (whole >= 1024 && u < ARRAY_LEN(unit) - 1) {
        frac  = ((whole % 1024) * 10) / 1024;
        whole /= 1024;
        u++;
    }
    if (u == 0) kprintf("%llu %s", whole, unit[u]);
    else        kprintf("%llu,%llu %s", whole, frac, unit[u]);
}

static i32 header_height(void) { return 2 * 24 * ui + 54 * ui; }
static i32 footer_height(void) { return 28 * ui; }

static void draw_chrome(void)
{
    i32 w = (i32)fb_width(), h = (i32)fb_height();
    i32 pad = 24 * ui;
    i32 head = header_height();

    fb_clear(C_BG);
    fb_gradient(0, 0, w, head, C_HEAD_A, C_HEAD_B);
    fb_rect(0, head, w, ui, C_ACCENT);

    fb_text_scaled(pad, pad, "EREBUS", C_TEXT, 2 * ui);

    i32 tw = fb_text_width("EREBUS", 2 * ui);
    fb_text_scaled(pad + tw + 8 * ui, pad + 16 * ui,
                   "Version " EREBUS_VERSION, C_DIM, ui);

    fb_text_scaled(pad, pad + 38 * ui,
                   "objektbasiert · rechtegebunden · kein Unix", C_DIM, ui);

    fb_rect(0, h - footer_height(), w, footer_height(), C_HEAD_A);
    fb_text_scaled(pad, h - footer_height() + 6 * ui,
                   "Meilenstein 1 — Übergabe, Bild, Speicherkarte",
                   C_DIM, ui);
}

/* Vollständige Speicherkarte, nur aufs Kabel -- 122 Zeilen will man
 * nicht auf dem Bildschirm haben, aber beim Prüfen braucht man sie. */
static void dump_ranges(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)(virt_addr)bi->mem_ranges;

    kout_mute_screen(true);
    kprintf("\n--- vollständige Speicherkarte (%llu Bereiche) ---\n",
            bi->mem_count);
    for (u64 i = 0; i < bi->mem_count; i++) {
        kprintf("  %3llu  %016llx - %016llx  %-11s  ",
                i, r[i].base, r[i].base + r[i].pages * PAGE_SIZE,
                mem_type_name(r[i].type));
        print_size(r[i].pages * PAGE_SIZE);
        kprintf("\n");
    }
    kprintf("--- Ende der Speicherkarte ---\n\n");
    kout_mute_screen(false);
}

void kmain(eb_boot_info *bi)
{
    /* Zuerst die serielle Schnittstelle: sie funktioniert auch dann,
     * wenn mit dem Bildpuffer etwas nicht stimmt. */
    serial_init();
    if (serial_present()) kout_add_sink(serial_putc);

    kprintf("\n\nErebus %s — Kernel gestartet\n", EREBUS_VERSION);

    /* Die Übergabe prüfen, bevor wir irgendetwas daraus lesen. Ein
     * falsches Magic heißt: Lader und Kernel passen nicht zusammen. */
    if (!bi) {
        kprintf("FEHLER: keine Übergabedaten (Zeiger ist null)\n");
        cpu_stop();
    }
    if (bi->magic != EREBUS_BOOT_MAGIC) {
        kprintf("FEHLER: Übergabe unerwartet — Magic 0x%08x statt 0x%08x\n",
                bi->magic, EREBUS_BOOT_MAGIC);
        cpu_stop();
    }
    if (bi->version != EREBUS_BOOT_VERSION) {
        kprintf("FEHLER: Lader spricht Version %u, Kernel erwartet %u\n",
                bi->version, EREBUS_BOOT_VERSION);
        cpu_stop();
    }

    /* Bildschirm in Betrieb nehmen und die Konsole daraufsetzen. */
    fb_init(bi);
    ui = fb_width() >= 2400 ? 3 : (fb_width() >= 1500 ? 2 : 1);
    draw_chrome();

    i32 pad = 24 * ui;
    i32 top = header_height() + 16 * ui;
    fbcon_set_origin(pad, top,
                     (i32)fb_width() - 2 * pad,
                     (i32)fb_height() - top - footer_height() - 8 * ui);
    fbcon_init(C_TEXT, C_BG, ui);
    kout_add_sink(fbcon_putc);

    /* Ab hier geht jede Ausgabe an beide Senken. */
    kprintf("Übergabe vom Lader gelesen und geprüft.\n\n");

    kprintf("Bildschirm\n");
    kprintf("  %-14s%u x %u, %u Pixel pro Zeile\n", "Auflösung",
            bi->fb_width, bi->fb_height, bi->fb_stride);
    kprintf("  %-14s%p, ", "Puffer", (void *)(virt_addr)bi->fb_base);
    print_size(bi->fb_size);
    kprintf("\n");
    kprintf("  %-14s%s\n\n", "Format",
            bi->fb_format == EB_FB_RGBX8888 ? "RGBX8888" : "BGRX8888");

    kprintf("Kernel\n");
    kprintf("  %-14s%p bis %p\n", "Abbild",
            (void *)__kernel_start, (void *)__kernel_end);
    kprintf("  %-14s", "belegt");
    print_size(bi->kernel_size);
    kprintf("\n");
    kprintf("  %-14s%p\n\n", "ACPI-Wurzel", (void *)(virt_addr)bi->acpi_rsdp);

    /* Speicherkarte zusammenfassen statt alle Einträge auszuwerfen --
     * eine typische Karte hat weit über hundert Bereiche. */
    const eb_mem_range *ranges = (const eb_mem_range *)(virt_addr)bi->mem_ranges;
    u64 by_type[8] = { 0 };
    u64 count[8] = { 0 };
    u64 largest = 0, largest_at = 0;

    for (u64 i = 0; i < bi->mem_count; i++) {
        u32 t = ranges[i].type < 8 ? ranges[i].type : 7;
        u64 bytes = ranges[i].pages * PAGE_SIZE;
        by_type[t] += bytes;
        count[t]++;
        if (ranges[i].type == EB_MEM_FREE && bytes > largest) {
            largest = bytes;
            largest_at = ranges[i].base;
        }
    }

    kprintf("Speicher  (%llu Bereiche)\n", bi->mem_count);
    for (u32 t = 0; t < 7; t++) {
        if (count[t] == 0) continue;
        kprintf("  %-14s", mem_type_name(t));
        print_size(by_type[t]);
        kprintf("   (%llu %s)\n", count[t],
                count[t] == 1 ? "Bereich" : "Bereiche");
    }
    kprintf("  %-14s", "größter Block");
    print_size(largest);
    kprintf(" bei %p\n\n", (void *)(virt_addr)largest_at);

    dump_ranges(bi);

    kprintf("Bereit. Nächster Schritt: Unterbrechungen und Speicherverwaltung.\n");

    /* Nichts weiter zu tun -- schlafen legen, bis es einen Zeitgeber
     * gibt. hlt statt einer Leerschleife, damit die CPU nicht heizt. */
    for (;;) cpu_halt();
}
