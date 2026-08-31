/*
 * main.c -- Einstieg in den Erebus-Kernel.
 *
 * Meilenstein 1: Übergabe vom Lader prüfen, serielle Schnittstelle und
 * Bildschirm in Betrieb nehmen, den vorgefundenen Rechner protokollieren.
 * Noch keine Unterbrechungen, keine Speicherverwaltung, keine Prozesse.
 *
 * Die Ausgabe ist bewusst ein knappes Startprotokoll und keine
 * Vorführung: eine Zeile je Erkenntnis, vorne die Baugruppe, die sie
 * gemeldet hat. Ausführliches geht nur auf die serielle Schnittstelle.
 */
#include <eb/types.h>
#include <eb/io.h>
#include <eb/cpu.h>
#include <eb/fb.h>
#include <eb/fmt.h>
#include <eb/panic.h>
#include <eb/serial.h>
#include <common/bootinfo.h>

#define EREBUS_VERSION "0.1"

#define C_BG   RGB(  0,   0,   0)
#define C_TEXT RGB(198, 198, 198)

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

static void log_cpu(void)
{
    cpu_info cpu;
    cpu_detect(&cpu);

    if (cpu.brand[0]) kprintf("cpu0: %s\n", cpu.brand);
    else              kprintf("cpu0: %s\n", cpu.vendor);

    kprintf("cpu0: Familie %u, Modell %u, Stufe %u; "
            "%u Bit physisch, %u Bit virtuell\n",
            cpu.family, cpu.model, cpu.stepping,
            cpu.phys_bits, cpu.virt_bits);

    /* Nur melden, was da ist -- eine Liste mit lauter "nein" liest
     * niemand. Was fehlt, faellt beim Einrichten der Sperren auf. */
    kprintf("cpu0: Merkmale");
    if (cpu.nx)     kprintf(" NX");
    if (cpu.smep)   kprintf(" SMEP");
    if (cpu.smap)   kprintf(" SMAP");
    if (cpu.umip)   kprintf(" UMIP");
    if (cpu.pge)    kprintf(" PGE");
    if (cpu.pse1g)  kprintf(" 1G-Seiten");
    if (cpu.rdrand) kprintf(" RDRAND");
    if (cpu.rdseed) kprintf(" RDSEED");
    if (cpu.x2apic) kprintf(" x2APIC");
    else if (cpu.apic) kprintf(" APIC");
    if (cpu.invariant_tsc) kprintf(" gleichlaufender TSC");
    kprintf("\n");

    /* Was für die Härtung fehlt, gehört ins Protokoll -- nicht als
     * Fehler, aber sichtbar. */
    if (!cpu.nx || !cpu.smep || !cpu.smap) {
        kprintf("cpu0: ohne");
        if (!cpu.nx)   kprintf(" NX");
        if (!cpu.smep) kprintf(" SMEP");
        if (!cpu.smap) kprintf(" SMAP");
        kprintf(" -- Schutzsperren nur teilweise verfügbar\n");
    }

    if (cpu.hypervisor)
        kprintf("cpu0: virtualisierte Umgebung erkannt\n");
}

/* Vollständige Speicherkarte, nur aufs Kabel -- über hundert Zeilen
 * will man nicht auf dem Bildschirm haben, beim Prüfen aber schon. */
static void dump_ranges(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)(virt_addr)bi->mem_ranges;

    kout_mute_screen(true);
    kprintf("\nvollständige Speicherkarte, %llu Bereiche:\n", bi->mem_count);
    for (u64 i = 0; i < bi->mem_count; i++) {
        kprintf("  %3llu  %016llx-%016llx  %-11s  ",
                i, r[i].base, r[i].base + r[i].pages * PAGE_SIZE,
                mem_type_name(r[i].type));
        print_size(r[i].pages * PAGE_SIZE);
        kprintf("\n");
    }
    kprintf("\n");
    kout_mute_screen(false);
}

static void log_memory(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)(virt_addr)bi->mem_ranges;
    u64 by_type[8] = { 0 }, count[8] = { 0 };
    u64 largest = 0, largest_at = 0;

    for (u64 i = 0; i < bi->mem_count; i++) {
        u32 t = r[i].type < 8 ? r[i].type : 7;
        u64 bytes = r[i].pages * PAGE_SIZE;
        by_type[t] += bytes;
        count[t]++;
        if (r[i].type == EB_MEM_FREE && bytes > largest) {
            largest = bytes;
            largest_at = r[i].base;
        }
    }

    kprintf("mem:  ");
    print_size(by_type[EB_MEM_FREE]);
    kprintf(" frei in %llu von %llu Bereichen\n",
            count[EB_MEM_FREE], bi->mem_count);

    kprintf("mem:  größter zusammenhängender Block ");
    print_size(largest);
    kprintf(" bei %p\n", (void *)(virt_addr)largest_at);

    kprintf("mem:  Kernel %p-%p (", (void *)__kernel_start,
            (void *)__kernel_end);
    print_size(bi->kernel_size);
    kprintf("), Lader ");
    print_size(by_type[EB_MEM_LOADER]);
    kprintf(", ACPI ");
    print_size(by_type[EB_MEM_ACPI]);
    kprintf("\n");

    /* Die grossen reservierten Bereiche sind Adressraum fuer Geraete,
     * nicht belegter Arbeitsspeicher. Ohne diesen Hinweis liest sich
     * die Zahl wie ein Fehler. */
    kprintf("mem:  ");
    print_size(by_type[EB_MEM_RESERVED] + by_type[EB_MEM_MMIO]);
    kprintf(" reservierter Adressraum für Firmware und Geräte\n");
}

void kmain(eb_boot_info *bi)
{
    /* Zuerst die serielle Schnittstelle: sie funktioniert auch dann,
     * wenn mit dem Bildpuffer etwas nicht stimmt. */
    bool com = serial_init();
    if (com) kout_add_sink(serial_putc);

    /* Die Übergabe prüfen, bevor wir irgendetwas daraus lesen. Ein
     * falsches Magic heißt: Lader und Kernel passen nicht zusammen. */
    if (!bi) {
        kprintf("\nboot: keine Übergabedaten, Zeiger ist null\n");
        cpu_stop();
    }
    if (bi->magic != EREBUS_BOOT_MAGIC) {
        kprintf("\nboot: fremde Übergabe, Kennung %08x statt %08x\n",
                bi->magic, EREBUS_BOOT_MAGIC);
        cpu_stop();
    }
    if (bi->version != EREBUS_BOOT_VERSION) {
        kprintf("\nboot: Lader spricht Fassung %u, Kernel erwartet %u\n",
                bi->version, EREBUS_BOOT_VERSION);
        cpu_stop();
    }

    /* Bildschirm übernehmen und die Konsole daraufsetzen. */
    fb_init(bi);
    fb_clear(C_BG);

    i32 m = (i32)fb_width() >= 1500 ? 16 : 8;
    fbcon_set_origin(m, m,
                     (i32)fb_width() - 2 * m, (i32)fb_height() - 2 * m);
    fbcon_init(C_TEXT, C_BG, 0);
    kout_add_sink(fbcon_putc);

    kprintf("\n\nErebus %s (x86_64)\n\n", EREBUS_VERSION);

    kprintf("boot: Übergabe vom Lader geprüft, Fassung %u\n", bi->version);

    log_cpu();
    log_memory(bi);

    kprintf("fb0:  %ux%u, 32 Bit %s, ", bi->fb_width, bi->fb_height,
            bi->fb_format == EB_FB_RGBX8888 ? "RGBX" : "BGRX");
    print_size(bi->fb_size);
    kprintf(" bei %p\n", (void *)(virt_addr)bi->fb_base);
    kprintf("fb0:  Textkonsole %dx%d Zeichen\n", fbcon_cols(), fbcon_rows());

    if (bi->acpi_rsdp)
        kprintf("acpi: RSDP bei %p\n", (void *)(virt_addr)bi->acpi_rsdp);
    else
        kprintf("acpi: kein RSDP in der Firmware-Tabelle\n");

    if (com) kprintf("com0: 115200 8N1\n");

    dump_ranges(bi);

    kprintf("kern: Leerlauf, noch keine Unterbrechungssteuerung\n");

    /* hlt statt einer Leerschleife, damit die CPU nicht heizt. */
    for (;;) cpu_halt();
}
