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
#include <eb/msg.h>
#include <eb/object.h>
#include <eb/blk.h>
#include <eb/cap.h>
#include <eb/cpu.h>
#include <eb/fb.h>
#include <eb/fmt.h>
#include <eb/gdt.h>
#include <eb/kheap.h>
#include <eb/panic.h>
#include <eb/pci.h>
#include <eb/proc.h>
#include <eb/journal.h>
#include <eb/settings.h>
#include <eb/activity.h>
#include <eb/standard.h>
#include <eb/net.h>
#include <eb/string.h>
#include <eb/syscall.h>
#include <eb/pic.h>
#include <eb/pmm.h>
#include <eb/ps2.h>
#include <eb/serial.h>
#include <eb/shell.h>
#include <eb/snapshot.h>
#include <eb/thread.h>
#include <eb/time.h>
#include <eb/trap.h>
#include <eb/vmm.h>
#include <common/bootinfo.h>

#define EREBUS_VERSION "0.1"

/* The graph a fresh system starts with.
 *
 * Small on purpose, and shaped to show three things a tree of files
 * cannot do. The same object appears twice under two different names,
 * because a name belongs to the reference and not to the thing it
 * points at. It appears with different rights each time, because
 * following a reference narrows what one holds. And the graph contains
 * a loop, which is an ordinary shape here and a broken filesystem
 * anywhere else. */
static object *seed_graph(void)
{
    object *root  = obj_create(TYPE_LIST, 0, 4);
    object *notes = obj_create(TYPE_TEXT, 512, 0);
    object *idea  = obj_create(TYPE_TEXT, 512, 0);
    object *raw   = obj_create(TYPE_BYTES, 64, 0);
    object *aside = obj_create(TYPE_LIST, 0, 3);
    if (!root || !notes || !idea || !raw || !aside) return NULL;

    obj_set_name(root, "home");

    static const char notes_text[] =
        "type here. this is an object, not a file.\n"
        "nothing is saved, because nothing was ever\n"
        "loaded. it simply stays.\n";
    static const char idea_text[] =
        "a name belongs to the reference, not to\n"
        "the thing it points at. you wrote it down\n"
        "about something you already held, so an\n"
        "object handed to you cannot announce\n"
        "itself as something it is not.\n"
        "\n"
        "this text is reachable twice, under two\n"
        "names, with different rights each time.\n";

    u8 *d = (u8 *)obj_data(notes);
    for (u32 i = 0; i < sizeof(notes_text); i++) d[i] = (u8)notes_text[i];
    d = (u8 *)obj_data(idea);
    for (u32 i = 0; i < sizeof(idea_text); i++) d[i] = (u8)idea_text[i];
    d = (u8 *)obj_data(raw);
    for (u32 i = 0; i < 64; i++) d[i] = (u8)(i * 37 + 5);

    obj_set_slot(root, 0, notes, CAP_READ | CAP_WRITE);
    obj_set_slot_name(root, 0, "notes");

    obj_set_slot(root, 1, idea, CAP_READ | CAP_WRITE);
    obj_set_slot_name(root, 1, "the idea");

    obj_set_slot(root, 2, raw, CAP_READ);
    obj_set_slot_name(root, 2, "some bytes");

    obj_set_slot(root, 3, aside, CAP_READ | CAP_WRITE);
    obj_set_slot_name(root, 3, "aside");

    /* The same text again: a different name, and readable only. */
    obj_set_slot(aside, 0, idea, CAP_READ);
    obj_set_slot_name(aside, 0, "the idea, read only");

    /* And back where we began, which closes a loop. */
    obj_set_slot(aside, 1, root, CAP_READ);
    obj_set_slot_name(aside, 1, "back home");

    obj_release(notes);
    obj_release(idea);
    obj_release(raw);
    obj_release(aside);
    return root;
}

/* The programs that run outside the kernel, from user/. */
extern char user_hello[];
extern char user_trespass[];
extern char user_agent[];
extern char user_courier[];
extern char user_clock[];
extern char user_cipher[];
extern char user_tally[];
extern char user_sums[];
extern char user_watch[];
extern char user_wipe[];
extern char user_fetch[];

/* What the system ships with. hello and trespass make their point at
 * start-up and end; these stay, each doing one thing to whatever it is
 * pointed at: the agent reports, the courier passes on, the clock
 * keeps the time somewhere, the cipher turns writing over, the tally
 * counts. None of them can reach anything it was not handed. */
static const struct {
    const char *name;
    char       *entry;
    const char *petname;
} standard[] = {
    { "agent",   user_agent,   "agent" },
    { "courier", user_courier, "courier" },
    { "clock",   user_clock,   "clock" },
    { "cipher",  user_cipher,  "cipher" },
    { "tally",   user_tally,   "tally" },
    { "sums",    user_sums,    "sums" },
    { "watch",   user_watch,   "watch" },
    { "wipe",    user_wipe,    "wipe" },
    { "fetch",   user_fetch,   "fetch" },
};
#define STANDARD_COUNT ((u32)ARRAY_LEN(standard))

static object *standard_obj[STANDARD_COUNT];

/* The one capability the kernel keeps on the console port: the right to
 * take messages out of it. Programs get send-only copies. */
static domain     *kernel_domain;
static cap_handle  console_receive;

/* Kept for starting programs after boot: every instance begins holding
 * a way to speak into this and its own letter box, nothing else. */
static object     *console_port;

u32 standard_count(void) { return STANDARD_COUNT; }

const char *standard_name(u32 i)
{
    return i < STANDARD_COUNT ? standard[i].name : "?";
}

/* What some programs are born holding beyond the voice and the letter
 * box. fetch gets the wire, send-only -- recorded on its object like
 * every giving, so the graph itself says who can reach outside. */
static void standard_wire(const char *name, object *prog)
{
    if (!prog || strcmp(name, "fetch") != 0 || !net_port()) return;
    obj_set_slot(prog, 0, net_port(), CAP_CALL);
    obj_set_slot_name(prog, 0, "the wire");
    proc_grant(prog, net_port(), CAP_CALL);
}

object *standard_launch(u32 i)
{
    if (i >= STANDARD_COUNT || !console_port) return NULL;

    process *p = proc_create(standard[i].name, standard[i].entry,
                             console_port);
    if (!p) return NULL;
    if (!proc_start(p)) return NULL;
    standard_wire(standard[i].name, proc_object(p));

    kprintf("proc: %llu (%s) started from the shell\n",
            proc_id(p), proc_name(p));
    return proc_object(p);
}

/* The interpreter, from user/runner.c -- the one C program in the
 * user section. */
extern void user_runner(u64 console, u64 inbox);

object *runner_launch(object *script)
{
    if (!script || obj_type(script) != TYPE_TEXT || !console_port)
        return NULL;

    process *p = proc_create("script", (const void *)user_runner,
                             console_port);
    if (!p) return NULL;
    if (!proc_start(p)) return NULL;

    /* Its words: the first giving, recorded on the program object like
     * every giving, and read-only like every set of orders should be
     * from the inside. The text stays editable from the outside, which
     * is exactly the difference between author and program. */
    object *prog = proc_object(p);
    obj_set_slot(prog, 0, script, CAP_READ);
    obj_set_slot_name(prog, 0, "its words");
    proc_grant(prog, script, CAP_READ);

    kprintf("proc: %llu (script) running a text\n", proc_id(p));
    return prog;
}

/* Prints the packed characters a message carries. Anything outside
 * printable ASCII is dropped rather than sent to the console, so a
 * program cannot drive the terminal with escape sequences. */
static void print_message_text(const message *m)
{
    for (u32 i = 0; i < m->nwords; i++) {
        u64 w = m->words[i];
        for (u32 b = 0; b < 8; b++) {
            char c = (char)((w >> (b * 8)) & 0xFF);
            if (c >= 0x20 && c < 0x7F) kputc(c);
        }
    }
}

/* A kernel thread that serves the console port. Programs cannot print;
 * they can only ask this to, through a capability, and only if they
 * were given one. What they say also goes into the journal, attributed
 * to what the kernel knows the sender to be -- a program does not get
 * to sign its lines with somebody else's name. */
static void console_server(void *arg)
{
    (void)arg;
    for (;;) {
        message m;
        const char *from = NULL;
        if (!port_receive_labelled(kernel_domain, console_receive,
                                   &m, &from)) {
            sched_yield();
            continue;
        }
        kprintf("user: ");
        print_message_text(&m);
        kprintf("\n");

        char said[3 * 8 + 1];
        u32 n = 0;
        for (u32 i = 0; i < m.nwords && i < 3; i++)
            for (u32 b = 0; b < 8; b++)
                said[n++] = (char)((m.words[i] >> (b * 8)) & 0xFF);
        said[n] = 0;
        journal_says(from ? from : "someone", said);
    }
}

/* What persistence watches over. One object here; a real system would
 * hand it the roots of everything the user owns. */
static object *persistent_root;

/* How texts become programs -- the one page of the language, kept as
 * an object like everything else: readable, searchable, one run away
 * from being tried. The kernel is its author, which is why this page
 * is refreshed on every boot: it states what the system of today
 * understands, and a page describing yesterday's language would be
 * documentation lying about its subject. */
static const char lang_text[] =
    "any text can be a program. stand on it, press run.\n"
    "\n"
    "the first gift is the text itself. \"it\" is the\n"
    "latest gift after that: point the running script at\n"
    "something and the script can reach it. variables a\n"
    "to z hold numbers. r holds how the last get, put or\n"
    "tell went: 0 for done, -1 for refused.\n"
    "\n"
    "say <words>      up to 24 letters, to the console\n"
    "tell <words>     the same, to it -- when it listens\n"
    "show x           say a variable and its value\n"
    "wait             sleep until the next gift\n"
    "set x <n or v>   also: add sub mul div\n"
    "get x <offset>   x = eight bytes of it\n"
    "put x <offset>   eight bytes of x into it\n"
    "time x           x = the second of the day\n"
    "rest <n or v>    sleep that many seconds\n"
    "if x < <n or v>  also = and >. false skips a line\n"
    "skip <n>         n lines forward\n"
    "back <n>         n lines back\n"
    "note ...         a remark\n"
    "stop             the end\n"
    "\n"
    "edit a running script and the next pass through a\n"
    "line runs the new words.\n";

/* Finds the language page -- a reference named "the language" on the
 * root or one list below it -- or makes one, preferring to live in
 * the aside next to the other explanations. Then brings its words up
 * to date. Graphs from earlier days gain the page this way too. */
static void ensure_language(object *root)
{
    object *found = NULL;
    object *aside = NULL;

    for (u64 i = 0; i < obj_slots(root) && !found; i++) {
        object *s = obj_get_slot(root, i);
        if (!s) continue;
        const char *nm = obj_slot_name(root, i);

        if (nm && strcmp(nm, "the language") == 0 &&
            obj_type(s) == TYPE_TEXT) { found = s; break; }

        if (obj_type(s) != TYPE_LIST) continue;
        if (nm && strcmp(nm, "aside") == 0) aside = s;

        for (u64 j = 0; j < obj_slots(s) && !found; j++) {
            object *t = obj_get_slot(s, j);
            const char *tn = obj_slot_name(s, j);
            if (t && tn && strcmp(tn, "the language") == 0 &&
                obj_type(t) == TYPE_TEXT)
                found = t;
        }
    }

    if (!found) {
        object *made = obj_create(TYPE_TEXT, 1024, 0);
        if (!made) return;
        obj_set_name(made, "the language");

        object *place = aside ? aside : root;
        u64 n = obj_slots(place), at = n;
        for (u64 i = 0; i < n; i++)
            if (!obj_get_slot(place, i)) { at = i; break; }
        if (at == n && !obj_grow_slots(place, n + 1)) {
            obj_release(made);
            return;
        }
        obj_set_slot(place, at, made, CAP_READ);
        obj_set_slot_name(place, at, "the language");
        obj_release(made);
        found = made;                     /* the slot holds it now */
    }

    u8 *d = (u8 *)obj_data(found);
    if (!d || obj_size(found) < sizeof(lang_text)) return;

    bool same = true;
    for (u32 i = 0; i < sizeof(lang_text) && same; i++)
        if (d[i] != (u8)lang_text[i]) same = false;
    if (same) return;

    for (u64 i = 0; i < obj_size(found); i++)
        d[i] = (i < sizeof(lang_text)) ? (u8)lang_text[i] : 0;
    journal_says("system", "the language page speaks today's words");
}

/* Rewrites the activity table once a second. Between rewrites it only
 * yields; the table is not worth waking anyone for. */
static void activity_thread(void *arg)
{
    (void)arg;
    for (;;) {
        activity_update();
        u64 since = time_ns();
        while (time_ns() - since < 1000000000ULL) sched_yield();
    }
}

/* Writes the graph out once changes have stopped arriving.
 *
 * There is no save command, so something has to decide when. Waiting
 * for quiet rather than saving on every keystroke means a burst of
 * typing costs one write instead of thirty, and half a second of
 * stillness is far below the point where anyone would notice. */
static void persist_thread(void *arg)
{
    (void)arg;

    u64 seen = shell_changes() + obj_touches();
    u64 written = seen;
    u64 quiet_since = time_ns();

    for (;;) {
        /* Two hands change the graph: the person through the shell,
         * and programs -- the network included -- writing through
         * their capabilities. A page fetched and never typed near
         * must survive the next boot all the same. */
        u64 now = shell_changes() + obj_touches();
        if (now != seen) {
            seen = now;
            quiet_since = time_ns();
        } else if (seen != written &&
                   time_ns() - quiet_since > settings_save_quiet_ns()) {
            object *roots[2] = { persistent_root, shell_session() };
            if (snap_save(roots, roots[1] ? 2 : 1)) {
                written = seen;
                kprintf("snap: generation %llu written, %u objects, %llu bytes\n",
                        snap_generation(), snap_object_count(), snap_bytes());
            } else {
                kprintf("snap: could not write the graph\n");
                written = seen;      /* do not spin on a failing disk */
            }

            /* The same quiet moment is the right one to sweep in.
             * Cutting a reference out of the graph is the only way a
             * cycle ever comes loose, the write above has just recorded
             * the cut, and a walk here is one nobody is waiting on. */
            u64 swept = obj_collect();
            if (swept) {
                kprintf("obj:  %llu unreachable object(s) collected\n",
                        swept);
                journal_says("system", swept == 1
                             ? "one unreachable object was collected"
                             : "unreachable objects were collected");
            }
        }
        sched_yield();
    }
}

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

#if defined(EREBUS_TEST_FAULT) && EREBUS_TEST_FAULT == 3
/* Eats stack as fast as it can.
 *
 * optnone is not decoration. Written the obvious way with volatile
 * locals and a running sum, clang recognised the accumulator pattern
 * and turned the recursion into a loop reusing a single frame -- the
 * function ran forever without consuming a byte of stack, and the test
 * quietly proved nothing. Switching optimisation off for this one
 * function is the only way to be sure the frames are really stacked. */
__attribute__((noinline, optnone))
static u64 devour(u64 depth, volatile u8 *previous)
{
    volatile u8 pad[512];
    pad[0] = (u8)depth;
    pad[511] = previous ? previous[0] : 0;
    return devour(depth + 1, pad);
}

static void devour_stack_thread(void *arg)
{
    (void)arg;
    kprintf("kern: descending...\n");
    kprintf("kern: returned from depth %llu, which should not happen\n",
            devour(0, NULL));
}
#endif

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
    time_read_rtc();

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

    /* --- memory ---------------------------------------------------- */

    pmm_init(bi);
    kprintf("pmm:  ");
    print_size(pmm_total_frames() * PAGE_SIZE);
    kprintf(" managed in %llu frames, ", pmm_total_frames());
    print_size(pmm_free_frames() * PAGE_SIZE);
    kprintf(" free\n");

    if (pmm_selftest())
        kprintf("pmm:  self test passed\n");
    else
        panic("the frame allocator failed its own test");

    vmm_init(bi);

    vmm_protections p = vmm_active_protections();
    kprintf("vmm:  protections");
    if (p.nx)   kprintf(" nx");
    if (p.wp)   kprintf(" write-protect");
    if (p.smep) kprintf(" smep");
    if (p.smap) kprintf(" smap");
    if (p.umip) kprintf(" umip");
    if (!p.nx && !p.smep && !p.smap) kprintf(" none");
    kprintf("\n");

    if (vmm_selftest())
        kprintf("vmm:  self test passed, W^X holds across the kernel image\n");
    else
        panic("the page tables are not what they were meant to be");

    kheap_init();
    if (kheap_selftest()) {
        kprintf("heap: ");
        print_size(kheap_mapped());
        kprintf(" window mapped, self test passed\n");
    } else {
        panic("the kernel heap failed its own test");
    }

    u64 before_reclaim = pmm_free_frames();
    vmm_reclaim_loader_tables(bi);
    kprintf("pmm:  reclaimed ");
    print_size((pmm_free_frames() - before_reclaim) * PAGE_SIZE);
    kprintf(" of loader page tables\n");

    /* --- objects and capabilities ---------------------------------- */

    obj_store_init();
    kprintf("obj:  store ready, %u built-in types\n", type_count());

    if (obj_selftest())
        kprintf("obj:  self test passed\n");
    else
        panic("the object store failed its own test");

    if (cap_selftest())
        kprintf("cap:  self test passed -- isolation, attenuation, "
                "revocation, generations\n");
    else
        panic("the capability system failed its own test");

    kprintf("obj:  %llu objects created during start-up, %llu still live\n",
            obj_total_created(), obj_live_count());

    /* --- threads and messages -------------------------------------- */

    kernel_domain = domain_create("kernel", 256);
    if (!kernel_domain) panic("no memory for the kernel domain");

    sched_init(kernel_domain);
    port_init();
    kprintf("sched: round robin, %u ms slice, boot thread adopted\n", 50u);

    if (sched_selftest())
        kprintf("sched: self test passed -- threads interleave and a "
                "spinning thread is preempted\n");
    else
        panic("the scheduler failed its own test");

    if (msg_selftest())
        kprintf("msg:  self test passed -- capabilities survive transit "
                "with the rights they were sent with\n");
    else
        panic("message passing failed its own test");

    /* After the port test, so the collector's walk has real ports to
     * step around: a bystander swept here would fail the exact count. */
    if (obj_collect_selftest())
        kprintf("obj:  collector reclaims loose cycles; held cycles and "
                "everything reachable stay\n");
    else
        panic("the cycle collector failed its own test");

#ifdef EREBUS_STRESS_COLLECT
    /* What the collector costs, measured rather than assumed.
     *
     * It runs with interrupts off from the first count to the last
     * free, so its duration is not an average to be smoothed over
     * somewhere -- it is exactly how long the machine stands still.
     * That number should be known, not guessed at, and this makes it a
     * number the build can print. */
    {
        u64 t0 = time_ns();
        u64 got = obj_collect();
        u64 t1 = time_ns();
        kprintf("stress: sweep of the boot graph alone: %llu us, "
                "%llu swept\n", (t1 - t0) / 1000, got);

        /* Rings of three, let go of the moment they close. Counting
         * alone can never free a single one of them. */
        const u64 rings = 10000;
        u64 heap0 = kheap_bytes_used();
        u64 built = 0;
        t0 = time_ns();
        for (u64 r = 0; r < rings; r++) {
            object *a = obj_create(TYPE_LIST, 0, 1);
            object *b = obj_create(TYPE_LIST, 0, 1);
            object *c = obj_create(TYPE_LIST, 0, 1);
            if (!a || !b || !c) {
                kprintf("stress: the heap ran out at ring %llu\n", r);
                break;
            }
            obj_set_slot(a, 0, b, CAP_READ);
            obj_set_slot(b, 0, c, CAP_READ);
            obj_set_slot(c, 0, a, CAP_READ);
            obj_release(a);
            obj_release(b);
            obj_release(c);
            built++;
        }
        t1 = time_ns();
        kprintf("stress: built %llu rings of three in %llu ms, "
                "%llu KiB of heap now unreachable\n",
                built, (t1 - t0) / 1000000,
                (kheap_bytes_used() - heap0) / 1024);

        t0 = time_ns();
        got = obj_collect();
        t1 = time_ns();
        kprintf("stress: swept %llu objects in %llu ms -- the machine "
                "stood still for exactly that long\n",
                got, (t1 - t0) / 1000000);

        t0 = time_ns();
        got = obj_collect();
        t1 = time_ns();
        kprintf("stress: swept again: %llu objects, %llu us; the heap "
                "kept %llu bytes of the exercise\n",
                got, (t1 - t0) / 1000,
                kheap_bytes_used() - heap0);
    }
#endif

    kprintf("sched: %llu threads, %llu context switches so far\n",
            sched_threads(), sched_switches());

    /* --- user mode --------------------------------------------------- */

    percpu_init();
    syscall_init();
    kprintf("cpu0: syscall entry armed, %u calls in the interface\n", SYS_MAX);

    object *console = port_create(16);
    if (!console) panic("no memory for the console port");
    console_receive = cap_insert(kernel_domain, console,
                                 CAP_READ | CAP_CALL | CAP_GRANT);
    thread_create("console", console_server, NULL, kernel_domain);

    /* The network's door has to exist before fetch starts holding a
     * way to it; the card behind the door is sought later, once the
     * bus has been scanned. Requests simply queue until then. */
    net_prepare(kernel_domain);

    static const struct { const char *name; char *entry; } programs[] = {
        { "hello",    user_hello },
        { "trespass", user_trespass },
    };

    for (u32 i = 0; i < ARRAY_LEN(programs); i++) {
        /* The entire authority these programs will ever have: permission
         * to send to the console port, and a letter box of their own.
         * Not to read the console, not to pass it on, and nothing else
         * at all. */
        process *proc = proc_create(programs[i].name, programs[i].entry,
                                    console);
        if (!proc) { kprintf("proc: could not create %s\n", programs[i].name); continue; }

        if (proc_start(proc))
            kprintf("proc: %llu (%s) starting in ring 3 with two capabilities\n",
                    proc_id(proc), proc_name(proc));
    }

    /* The ones that stay: the programs this system ships with. Each
     * begins holding exactly what hello and trespass held -- a console
     * and a letter box -- and can do nothing but wait; there is no call
     * any of them could make that would produce the name of anything
     * else. What each ends up able to touch is decided entirely from
     * the outside, by pointing its program object at things. */
    for (u32 i = 0; i < STANDARD_COUNT; i++) {
        process *proc = proc_create(standard[i].name, standard[i].entry,
                                    console);
        if (!proc) {
            kprintf("proc: could not create %s\n", standard[i].name);
            continue;
        }
        if (proc_start(proc)) {
            standard_obj[i] = proc_object(proc);
            standard_wire(standard[i].name, standard_obj[i]);
            kprintf("proc: %llu (%s) waiting on its letter box\n",
                    proc_id(proc), proc_name(proc));
        }
    }

    /* The reference stays: launching a program later needs the same
     * console every boot-time program was given. */
    console_port = console;

    /* Let them run. */
    for (u32 i = 0; i < 400; i++) sched_yield();

    /* The copy path is where the kernel touches an address a program
     * chose. Handing it a kernel address is the oldest trick there is:
     * get the privileged side to do the reaching for you. */
    u8 probe = 0;
    bool refused = !copy_to_user((virt_addr)__kernel_start, &probe, 1) &&
                   !copy_from_user(&probe, (virt_addr)__kernel_start, 1);
    kprintf("proc: kernel addresses %s by the user copy path\n",
            refused ? "refused" : "ACCEPTED, which is a hole");

    kprintf("proc: %llu processes started, %llu ended by a fault\n",
            proc_count(), proc_faults());

    /* --- storage ------------------------------------------------------ */

    pci_scan();
    kprintf("pci:  %u devices\n", pci_device_count());
    for (u32 i = 0; i < pci_device_count(); i++) {
        const pci_device *d = pci_get(i);
        if (d->class_code == 0x06 && d->subclass == 0x00) continue;
        kprintf("pci:  %02x:%02x.%u  %04x:%04x  %s\n",
                d->bus, d->device, d->function, d->vendor, d->device_id,
                pci_class_name(d->class_code, d->subclass));
    }

    if (blk_init()) {
        kprintf("blk:  %s on ahci port %u, %llu sectors (",
                blk_model(), blk_port(), blk_sectors());
        print_size(blk_sectors() * BLK_SECTOR_SIZE);
        kprintf("), %u disks found\n", blk_disk_count());

        if (blk_selftest())
            kprintf("blk:  self test passed, a written sector reads back\n");
        else
            kprintf("blk:  self test FAILED\n");
    } else {
        kprintf("blk:  no ahci disk found\n");
    }

    if (!net_start())
        kprintf("net:  no network card found; the wire goes nowhere\n");

    /* --- the desktop ------------------------------------------------- */

    ps2_init();
    kprintf("ps2:  keyboard %s, mouse %s\n",
            ps2_keyboard_present() ? "ready" : "absent",
            ps2_mouse_present() ? "ready" : "absent");

    u64 back_bytes = fb_backbuffer_bytes();
    phys_addr back = pmm_alloc_contig(PAGE_UP(back_bytes) / PAGE_SIZE);
    if (back != PMM_NO_FRAME) {
        fb_enable_backbuffer(phys_to_virt(back));
        kprintf("fb0:  double buffered, ");
        print_size(back_bytes);
        kprintf(" back buffer\n");
    } else {
        kprintf("fb0:  not enough contiguous memory for a back buffer\n");
    }

    /* One object, and three windows onto it.
     *
     * There is no file here and no name. The windows hold capabilities,
     * two of them read-only, and every one of them resolves its own
     * every time it draws. Typing into the writable one changes the
     * object, and the other two show the change because they were never
     * looking at anything else. */
    /* Try the disk before making anything. If a graph is there, the
     * system picks up where it left off; there is no separate "open" to
     * perform and nothing for the user to remember the name of. */
    object *root = NULL;
    object *session = NULL;
    object *loaded[2] = { NULL, NULL };

    if (snap_load(loaded, 2) >= 1) {
        root = loaded[0];
        session = loaded[1];
        kprintf("snap: graph restored from generation %llu, %u objects\n",
                snap_generation(), snap_object_count());
    } else {
        kprintf("snap: no snapshot on the disk, starting fresh\n");
        root = seed_graph();
    }

    if (root) {
        /* Reconnecting the records.
         *
         * A restored graph names programs that were running when it was
         * written. Those processes are gone; their successors started a
         * moment ago, holding a console and a letter box and nothing
         * else. If the world is to come back as it was left -- and that
         * is the whole promise -- then what the graph says a program
         * held must be handed to its successor again, not merely shown.
         * The record is matched to a successor by the program's own
         * name, its references are replayed onto it grant by grant, and
         * the record's place and petname in the graph are taken over.
         * A record with no successor is cleared: what happened is kept
         * by the journal, not by dead ends in the graph. */
        object *records[STANDARD_COUNT] = { NULL };
        bool placed[STANDARD_COUNT] = { false };

        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            if (!s || obj_type(s) != TYPE_PROGRAM || proc_is_running(s))
                continue;
            for (u32 k = 0; k < STANDARD_COUNT; k++)
                if (standard_obj[k] && obj_name(s) &&
                    strcmp(obj_name(s), standard[k].name) == 0)
                    records[k] = s;
        }

        /* Replay. A reference from one record to another is re-pointed
         * at the other's successor: the courier knew the agent, so it
         * knows the new agent. Every replayed reference is also granted
         * again -- the slot is the record of the giving, and the giving
         * is done anew. */
        for (u32 k = 0; k < STANDARD_COUNT; k++) {
            if (!records[k] || !standard_obj[k]) continue;
            for (u64 j = 0; j < obj_slots(records[k]); j++) {
                object *t = obj_get_slot(records[k], j);
                if (!t) continue;

                /* The wire is wired afresh at every start; a record's
                 * copy of it is last boot's door, not this one's. */
                const char *sn = obj_slot_name(records[k], j);
                if (sn && strcmp(sn, "the wire") == 0) continue;
                for (u32 k2 = 0; k2 < STANDARD_COUNT; k2++)
                    if (t == records[k2] && standard_obj[k2])
                        t = standard_obj[k2];

                u32 rr = obj_slot_rights(records[k], j);
                if (j >= obj_slots(standard_obj[k]) &&
                    !obj_grow_slots(standard_obj[k], j + 1)) break;
                obj_set_slot(standard_obj[k], j, t, rr);
                obj_set_slot_name(standard_obj[k], j,
                                  obj_slot_name(records[k], j));
                proc_grant(standard_obj[k], t, rr);
            }
        }

        /* The successors take the records' places, petnames included;
         * unmatched records go; anything still unplaced gets a fresh
         * slot with a plain name. Read and grant, never write: nobody
         * edits a running program by typing into it. */
        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            if (!s || obj_type(s) != TYPE_PROGRAM || proc_is_running(s))
                continue;
            bool matched = false;
            for (u32 k = 0; k < STANDARD_COUNT; k++) {
                if (s != records[k] || !standard_obj[k]) continue;
                obj_set_slot(root, i, standard_obj[k],
                             obj_slot_rights(root, i));
                placed[k] = true;
                matched = true;
            }

            /* A script's record still holds the script's words, and
             * words are all a script ever was: run them again. What
             * else the record held is granted anew, like any replay;
             * only a reference to another program stays a reference
             * to the record, since a dead recipient translates to
             * nothing. If the world is to come back as it was left,
             * the programs the person wrote are part of the world. */
            if (!matched && obj_name(s) && strcmp(obj_name(s), "script") == 0) {
                object *script_words = NULL;
                for (u64 j = 0; j < obj_slots(s); j++) {
                    object *t = obj_get_slot(s, j);
                    const char *jn = obj_slot_name(s, j);
                    if (t && jn && strcmp(jn, "its words") == 0 &&
                        obj_type(t) == TYPE_TEXT) { script_words = t; break; }
                }

                object *fresh = script_words ? runner_launch(script_words)
                                             : NULL;
                if (fresh) {
                    for (u64 j = 0; j < obj_slots(s); j++) {
                        object *t = obj_get_slot(s, j);
                        const char *jn = obj_slot_name(s, j);
                        if (!t || (jn && strcmp(jn, "its words") == 0))
                            continue;

                        u64 fn = obj_slots(fresh), fat = fn;
                        for (u64 q = 0; q < fn; q++)
                            if (!obj_get_slot(fresh, q)) { fat = q; break; }
                        if (fat == fn && !obj_grow_slots(fresh, fn + 1))
                            continue;

                        u32 rr = obj_slot_rights(s, j);
                        obj_set_slot(fresh, fat, t, rr);
                        obj_set_slot_name(fresh, fat, jn);
                        proc_grant(fresh, t, rr);
                    }
                    obj_set_slot(root, i, fresh, obj_slot_rights(root, i));
                    matched = true;
                }
            }

            if (!matched) {
                obj_set_slot(root, i, NULL, 0);
                obj_set_slot_name(root, i, NULL);
            }
        }

        for (u32 k = 0; k < STANDARD_COUNT; k++) {
            if (!standard_obj[k] || placed[k]) continue;

            u64 n = obj_slots(root), spot = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(root, i)) { spot = i; break; }
            if (spot == n && !obj_grow_slots(root, n + 1)) continue;

            obj_set_slot(root, spot, standard_obj[k], CAP_READ | CAP_GRANT);
            obj_set_slot_name(root, spot, standard[k].petname);
        }

        /* Out of the box, the clock has somewhere to write: a fresh
         * system boots with "the time" in the graph, ticking, held by
         * the person read-only. Not a special object -- a plain text
         * that one program keeps current, and the wiring is ordinary
         * too: a slot on the clock's program object, granted like any
         * other giving. Cutting that reference stops the clock; on a
         * restored graph the replay above brings the wiring back on
         * its own, which is why this runs only for a fresh start. */
        if (!session) {
            object *clock_obj = NULL;
            for (u32 k = 0; k < STANDARD_COUNT; k++)
                if (strcmp(standard[k].name, "clock") == 0)
                    clock_obj = standard_obj[k];

            if (clock_obj) {
                object *face = obj_create(TYPE_TEXT, 16, 0);
                if (face) {
                    obj_set_name(face, "the time");
                    obj_set_fleeting(face, true);

                    u64 n = obj_slots(root), spot = n;
                    for (u64 i = 0; i < n; i++)
                        if (!obj_get_slot(root, i)) { spot = i; break; }
                    if (spot < n || obj_grow_slots(root, n + 1)) {
                        obj_set_slot(root, spot, face, CAP_READ);
                        obj_set_slot_name(root, spot, "the time");
                    }

                    obj_set_slot(clock_obj, 0, face,
                                 CAP_READ | CAP_WRITE);
                    obj_set_slot_name(clock_obj, 0, "writes here");
                    proc_grant(clock_obj, face, CAP_READ | CAP_WRITE);
                    obj_release(face);
                }
            }
        }

        /* The journal. A restored graph brings its own back, found by
         * the name this code gave the reference; appending continues
         * into it rather than starting a second history. The reference
         * is read-only on purpose: history can be read by anyone who
         * holds it and rewritten by nobody. */
        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            const char *nm = obj_slot_name(root, i);
            if (s && nm && strcmp(nm, "log") == 0 &&
                obj_type(s) == TYPE_TEXT) {
                journal_adopt(s);
                break;
            }
        }
        if (!journal_object() && journal_create()) {
            u64 n = obj_slots(root), at = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(root, i)) { at = i; break; }
            if (at < n || obj_grow_slots(root, n + 1)) {
                obj_set_slot(root, at, journal_object(), CAP_READ);
                obj_set_slot_name(root, at, "log");
            }
        }
        /* The settings, found the same way as the journal or made
         * fresh. The reference the person holds is read AND write: how
         * the system is set is theirs to say, by writing sentences. */
        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            const char *nm = obj_slot_name(root, i);
            if (s && nm && strcmp(nm, "settings") == 0 &&
                obj_type(s) == TYPE_TEXT) {
                settings_adopt(s);
                break;
            }
        }
        if (!settings_object() && settings_create()) {
            u64 n = obj_slots(root), at = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(root, i)) { at = i; break; }
            if (at < n || obj_grow_slots(root, n + 1)) {
                obj_set_slot(root, at, settings_object(),
                             CAP_READ | CAP_WRITE);
                obj_set_slot_name(root, at, "settings");
            }
        }
        settings_apply();

        /* The activity table: what the machine is doing, rewritten
         * once a second by its own thread. Read-only for the person --
         * the machine reports, nobody edits the report. */
        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            const char *nm = obj_slot_name(root, i);
            if (s && nm && strcmp(nm, "activity") == 0 &&
                obj_type(s) == TYPE_TEXT) {
                activity_adopt(s);
                break;
            }
        }
        if (!activity_object() && activity_create()) {
            u64 n = obj_slots(root), at = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(root, i)) { at = i; break; }
            if (at < n || obj_grow_slots(root, n + 1)) {
                obj_set_slot(root, at, activity_object(), CAP_READ);
                obj_set_slot_name(root, at, "activity");
            }
        }

        /* A restored clock face is the same heartbeat it always was;
         * the mark does not ride the snapshot, so it is set anew. */
        for (u64 i = 0; i < obj_slots(root); i++) {
            object *s = obj_get_slot(root, i);
            const char *nm = obj_slot_name(root, i);
            if (s && nm && strcmp(nm, "the time") == 0 &&
                obj_type(s) == TYPE_TEXT)
                obj_set_fleeting(s, true);
        }

        ensure_language(root);

        journal_says("system", session ? "started; everything is as it was left"
                                       : "started fresh");

        /* One capability, held by the shell, carrying everything we can
         * do. Every step from here narrows it. */
        cap_insert(kernel_domain, root, CAP_READ | CAP_WRITE | CAP_GRANT);

        /* The persistence root keeps the reference we were holding
         * rather than borrowing one from the capability table. A pointer
         * that stays valid only because somebody else has not let go is
         * a pointer that breaks the day they do. */
        persistent_root = root;

        /* Whether the trail comes back is the person's setting; the
         * session object itself stays either way, so changing one's
         * mind later needs no ceremony. */
        object *resume = settings_start_home() ? NULL : session;

        shell_init(kernel_domain, root, CAP_READ | CAP_WRITE | CAP_GRANT,
                   resume);
        kprintf("shell: %s, %u generations kept on the disk\n",
                resume ? "resumed where it was left"
                       : "starting at the root",
                snap_slot_count());

        /* The screen belongs to the shell from here on. The log keeps
         * going to the serial port, where it paints over nothing. */
        kout_detach_screen();
        thread_create("shell", shell_run, NULL, kernel_domain);
        thread_create("activity", activity_thread, NULL, kernel_domain);
        if (blk_present()) thread_create("persist", persist_thread, NULL,
                                         kernel_domain);
    }

    dump_ranges(bi);

#if defined(EREBUS_TEST_FAULT) && EREBUS_TEST_FAULT == 3
    /* Built by "make stack". Runs a thread off the bottom of its own
     * stack, which is where the guard page is waiting.
     *
     * Two milestones meet here. The guard page turns a silent overwrite
     * of neighbouring memory into a fault at the instruction that did
     * it. And the fault arrives with no stack left to handle it on,
     * which is exactly what the separate interrupt stacks in the TSS
     * are for -- without them this would be a triple fault and a
     * wordless reboot. */
    kprintf("kern: stack test, running a thread off the end of its stack\n");
    thread_create("devour", devour_stack_thread, NULL, kernel_domain);
    for (;;) sched_yield();

#elif defined(EREBUS_TEST_FAULT) && EREBUS_TEST_FAULT == 2
    /* Built by "make wx". Writes into the kernel's own code.
     *
     * This is the test that says whether W^X is real. The page is
     * mapped and present, so nothing is missing; the write must fail
     * purely because the page is read-only and CR0.WP makes that bind
     * ring 0 as well. Without that bit the store would quietly succeed
     * and the kernel would have just rewritten its own instructions. */
    kprintf("kern: protection test, writing into the kernel's own code "
            "at %p\n", (void *)__kernel_start);
    volatile u8 *code = (volatile u8 *)__kernel_start;
    *code = 0x90;
    kprintf("kern: the write went through -- W^X is NOT holding\n");

#elif defined(EREBUS_TEST_FAULT)
    /* Built by "make fault": writes to an address that is not mapped,
     * to show that the fault path reports rather than reboots. */
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
