/*
 * main.c -- kernel entry point.
 *
 * Milestone 2: validate the handover, take over the display and the
 * serial port, start the clock, install descriptor and interrupt
 * tables, and report the machine we found.
 *
 * The output is a terse start-up log, not a demonstration: one line per
 * finding, prefixed by the part that reported it. Anything long-winded
 * goes to the serial port only.
 */
#include <eb/types.h>
#include <eb/io.h>
#include <eb/mm.h>
#include <eb/cpu.h>
#include <eb/fb.h>
#include <eb/fmt.h>
#include <eb/gdt.h>
#include <eb/panic.h>
#include <eb/pic.h>
#include <eb/serial.h>
#include <eb/time.h>
#include <eb/trap.h>
#include <common/bootinfo.h>

#define EREBUS_VERSION "0.1"

/* Top of the boot stack, laid out in start.S. */
extern char stack_top[];

#define C_BG   RGB(  0,   0,   0)
#define C_TEXT RGB(198, 198, 198)

/* Vector base for the two interrupt controllers. Vectors below 32
 * belong to the processor's own exceptions and must not be reused. */
#define IRQ_BASE_MASTER 32
#define IRQ_BASE_SLAVE  40

static const char *mem_type_name(u32 t)
{
    switch (t) {
    case EB_MEM_FREE:     return "free";
    case EB_MEM_RESERVED: return "reserved";
    case EB_MEM_LOADER:   return "loader";
    case EB_MEM_KERNEL:   return "kernel";
    case EB_MEM_ACPI:     return "acpi";
    case EB_MEM_MMIO:     return "device";
    default:              return "unknown";
    }
}

/* Prints a size in a unit one can read without counting zeros. One
 * decimal place, computed in integers. */
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
    else        kprintf("%llu.%llu %s", whole, frac, unit[u]);
}

static void log_cpu(void)
{
    cpu_info cpu;
    cpu_detect(&cpu);

    if (cpu.brand[0]) kprintf("cpu0: %s\n", cpu.brand);
    else              kprintf("cpu0: %s\n", cpu.vendor);

    kprintf("cpu0: family %u model %u stepping %u, "
            "%u physical and %u virtual address bits\n",
            cpu.family, cpu.model, cpu.stepping,
            cpu.phys_bits, cpu.virt_bits);

    /* Report only what is there -- a list of "no" is not worth
     * reading. What is missing shows up in the line below. */
    kprintf("cpu0: features");
    if (cpu.nx)     kprintf(" nx");
    if (cpu.smep)   kprintf(" smep");
    if (cpu.smap)   kprintf(" smap");
    if (cpu.umip)   kprintf(" umip");
    if (cpu.pge)    kprintf(" pge");
    if (cpu.pse1g)  kprintf(" 1g-pages");
    if (cpu.rdrand) kprintf(" rdrand");
    if (cpu.rdseed) kprintf(" rdseed");
    if (cpu.x2apic) kprintf(" x2apic");
    else if (cpu.apic) kprintf(" apic");
    if (cpu.invariant_tsc) kprintf(" invariant-tsc");
    kprintf("\n");

    /* Missing protections belong in the log -- not as an error, but
     * visibly. On real hardware this is the line that matters. */
    if (!cpu.nx || !cpu.smep || !cpu.smap) {
        kprintf("cpu0: without");
        if (!cpu.nx)   kprintf(" nx");
        if (!cpu.smep) kprintf(" smep");
        if (!cpu.smap) kprintf(" smap");
        kprintf(" -- hardware protections only partly available\n");
    }

    if (cpu.hypervisor)
        kprintf("cpu0: running under a hypervisor\n");
}

static void log_memory(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)phys_to_virt(bi->mem_ranges);
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
    kprintf(" free across %llu of %llu ranges\n",
            count[EB_MEM_FREE], bi->mem_count);

    kprintf("mem:  largest contiguous block ");
    print_size(largest);
    kprintf(" at %p\n", (void *)(virt_addr)largest_at);

    kprintf("mem:  kernel %p-%p (", (void *)__kernel_start,
            (void *)__kernel_end);
    print_size(bi->kernel_size);
    kprintf("), loader ");
    print_size(by_type[EB_MEM_LOADER]);
    kprintf(", acpi ");
    print_size(by_type[EB_MEM_ACPI]);
    kprintf("\n");

    /* The large reserved regions are address space for devices, not
     * occupied RAM. Without saying so the figure reads like a fault. */
    kprintf("mem:  ");
    print_size(by_type[EB_MEM_RESERVED] + by_type[EB_MEM_MMIO]);
    kprintf(" of address space reserved for firmware and devices\n");
}

/* The full map, serial only -- over a hundred lines is not something to
 * put on screen, but it is exactly what one wants when checking. */
static void dump_ranges(const eb_boot_info *bi)
{
    const eb_mem_range *r = (const eb_mem_range *)phys_to_virt(bi->mem_ranges);

    kout_mute_screen(true);
    kprintf("\nfull memory map, %llu ranges:\n", bi->mem_count);
    for (u64 i = 0; i < bi->mem_count; i++) {
        kprintf("  %3llu  %016llx-%016llx  %-8s  ",
                i, r[i].base, r[i].base + r[i].pages * PAGE_SIZE,
                mem_type_name(r[i].type));
        print_size(r[i].pages * PAGE_SIZE);
        kprintf("\n");
    }
    kprintf("\n");
    kout_mute_screen(false);
}

/* Confirms that interrupts really are being delivered. Spinning rather
 * than halting on purpose: if nothing arrives, hlt would wait forever
 * and the boot would stop here with no explanation. */
static bool interrupts_arriving(void)
{
    u64 before = pit_ticks();
    for (u64 spin = 0; spin < 400000000ULL; spin++) {
        if (pit_ticks() - before >= 3) return true;
        __asm__ volatile ("pause");
    }
    return false;
}

/* Holds the two clocks against each other. They are independent: the
 * counter was calibrated once during start-up, while the tick comes
 * from the crystal continuously. Agreement means the calibration was
 * sound; a large drift means the timestamps are wrong, and it is far
 * better to know that than to trust them.
 *
 * Under emulation without hardware assistance a sizeable deviation is
 * normal -- the counter then follows the host processor while the
 * timer runs on the emulator's own notion of time. */
static void check_clocks(void)
{
    const u64 want_ticks = 25;

    u64 t0 = time_ns();
    u64 c0 = pit_ticks();
    while (pit_ticks() - c0 < want_ticks)
        __asm__ volatile ("pause");
    u64 measured = time_ns() - t0;

    u64 expected = want_ticks * 1000000000ULL / pit_hz();
    if (expected == 0 || measured == 0) return;

    i64 deviation = (i64)((measured * 100ULL) / expected) - 100;

    kprintf("time: %llu ticks took %llu.%llu ms, timer says %llu.%llu ms",
            want_ticks,
            measured / 1000000ULL, (measured / 100000ULL) % 10,
            expected / 1000000ULL, (expected / 100000ULL) % 10);

    if (deviation > 5 || deviation < -5)
        kprintf(" -- counter off by %lld%%\n", deviation);
    else
        kprintf(" -- clocks agree\n");
}

void kmain(eb_boot_info *bi)
{
    /* The serial port comes first: it works even when something is
     * wrong with the framebuffer. */
    bool com = serial_init();
    if (com) kout_add_sink(serial_putc);

    /* The loader hands over a physical pointer, because it built the
     * structure before there was an address space to speak of. Move it
     * onto the direct map straight away, so nothing below this line
     * depends on the identity mapping still being in place. */
    if (bi) bi = (eb_boot_info *)phys_to_virt((phys_addr)bi);

    /* Check the handover before reading anything out of it. A wrong
     * magic means loader and kernel do not belong together. */
    if (!bi) {
        kprintf("\nboot: no handover data, pointer is null\n");
        cpu_stop();
    }
    if (bi->magic != EREBUS_BOOT_MAGIC) {
        kprintf("\nboot: foreign handover, magic %08x instead of %08x\n",
                bi->magic, EREBUS_BOOT_MAGIC);
        cpu_stop();
    }
    if (bi->version != EREBUS_BOOT_VERSION) {
        kprintf("\nboot: loader speaks version %u, kernel expects %u\n",
                bi->version, EREBUS_BOOT_VERSION);
        cpu_stop();
    }

    /* Take the display and put the console on it. */
    fb_init(bi);
    fb_clear(C_BG);

    i32 m = (i32)fb_width() >= 1500 ? 16 : 8;
    fbcon_set_origin(m, m,
                     (i32)fb_width() - 2 * m, (i32)fb_height() - 2 * m);
    fbcon_init(C_TEXT, C_BG, 0);
    kout_add_sink(fbcon_putc);

    /* Start the clock before the first line, so every line carries a
     * timestamp instead of the log starting halfway through. */
    bool clock_ok = time_init();
    if (clock_ok) kout_set_clock(time_ns);

    kprintf("\n\nErebus %s (x86_64)\n", EREBUS_VERSION);
    kprintf("boot: handover verified, version %u\n", bi->version);

    log_cpu();

    if (clock_ok) {
        u64 hz = time_tsc_hz();
        kprintf("time: tsc %llu.%03llu MHz, measured against the pit\n",
                hz / 1000000ULL, (hz / 1000ULL) % 1000ULL);
    } else {
        kprintf("time: could not measure the tsc, running without "
                "timestamps\n");
    }

    log_memory(bi);

    kprintf("fb0:  %ux%u, 32 bit %s, ", bi->fb_width, bi->fb_height,
            bi->fb_format == EB_FB_RGBX8888 ? "rgbx" : "bgrx");
    print_size(bi->fb_size);
    kprintf(" at %p\n", (void *)(virt_addr)bi->fb_base);
    kprintf("fb0:  text console %dx%d characters\n",
            fbcon_cols(), fbcon_rows());

    if (bi->acpi_rsdp)
        kprintf("acpi: rsdp at %p\n", (void *)(virt_addr)bi->acpi_rsdp);
    else
        kprintf("acpi: no rsdp in the firmware configuration table\n");

    if (com) kprintf("com0: 115200 8n1\n");

    /* --- descriptor tables and interrupts -------------------------- */

    gdt_init();
    tss_set_kernel_stack((u64)stack_top);
    trap_init();

    /* Move the controllers off the exception vectors and start with
     * everything masked: a line is only opened once something is
     * actually listening on it. */
    pic_init(IRQ_BASE_MASTER, IRQ_BASE_SLAVE);
    pic_mask_all();
    kprintf("pic0: 8259 pair remapped to vectors %u-%u, all lines masked\n",
            IRQ_BASE_MASTER, IRQ_BASE_SLAVE + 7);

    pit_init(100);
    kprintf("pit0: channel 0 at %u Hz on line 0\n", pit_hz());

    cpu_sti();
    kprintf("cpu0: interrupts enabled\n");

    if (interrupts_arriving()) {
        kprintf("pit0: ticking, %llu interrupts served so far\n",
                trap_irq_count());
        if (clock_ok) check_clocks();
    } else {
        kprintf("pit0: no interrupts arriving -- timer or controller "
                "is not responding\n");
    }

    dump_ranges(bi);

#ifdef EREBUS_TEST_FAULT
    /* Built only by "make fault": writes to an address that is not
     * mapped, to show that the fault path reports rather than reboots. */
    kprintf("kern: fault test, writing to an unmapped address\n");
    volatile u32 *bad = (volatile u32 *)0x0000DEADBEEF000ULL;
    *bad = 0x1234;
    kprintf("kern: fault test did not fault -- that is itself a bug\n");
#endif

    kprintf("kern: idle\n");

    /* hlt rather than a busy loop, so the processor does not heat up
     * for nothing. Interrupts wake it, the loop puts it back. */
    for (;;) cpu_halt();
}
