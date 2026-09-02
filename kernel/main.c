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
#include <eb/fat.h>
#include <eb/term.h>
#include <eb/ssh.h>
#include <eb/asm.h>
#include <eb/activity.h>
#include <eb/standard.h>
#include <eb/net.h>
#include <eb/pipe.h>
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
extern void user_foreman(u64 console, u64 inbox);
extern void user_reckon(u64 console, u64 inbox);
extern void user_pulse(u64 console, u64 inbox);

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
    { "foreman", (char *)user_foreman, "foreman" },
    { "reckon",  (char *)user_reckon,  "reckon" },
    { "pulse",   (char *)user_pulse,   "pulse" },
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
 * box. fetch and the foreman get the wire, send-only -- recorded on
 * their objects like every giving, so the graph itself says who can
 * reach outside. */
static void standard_wire(const char *name, object *prog)
{
    if (!prog || !net_port()) return;
    if (strcmp(name, "fetch") != 0 && strcmp(name, "foreman") != 0)
        return;
    obj_set_slot(prog, 0, net_port(), CAP_CALL);
    obj_set_slot_name(prog, 0, "the wire");
    proc_grant(prog, net_port(), CAP_CALL);
}

/* The caller's hold on a program object, taken before the program
 * starts. A program can run to its end and be reaped between its
 * start and the caller's next line; the process's own hold goes with
 * the reaping, and without this one the object would be freed memory
 * by the time it is laid anywhere. */
static object *held(process *p)
{
    object *o = proc_object(p);
    obj_retain(o);
    return o;
}

object *standard_launch(u32 i)
{
    if (i >= STANDARD_COUNT || !console_port) return NULL;

    process *p = proc_create(standard[i].name, standard[i].entry,
                             console_port);
    if (!p) return NULL;
    object *prog = held(p);
    if (!proc_start(p)) { obj_release(prog); return NULL; }
    standard_wire(standard[i].name, prog);

    kprintf("proc: %llu (%s) started from the shell\n",
            proc_id(p), proc_name(p));
    return prog;
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
    object *prog = held(p);
    if (!proc_start(p)) { obj_release(prog); return NULL; }

    /* Its words: the first giving, recorded on the program object like
     * every giving, and read-only like every set of orders should be
     * from the inside. The text stays editable from the outside, which
     * is exactly the difference between author and program. */
    obj_set_slot(prog, 0, script, CAP_READ);
    obj_set_slot_name(prog, 0, "its words");
    proc_grant(prog, script, CAP_READ);

    kprintf("proc: %llu (script) running a text\n", proc_id(p));
    return prog;
}

object *code_launch(object *image)
{
    if (!image || obj_type(image) != TYPE_BYTES || !console_port)
        return NULL;
    const u8 *d = (const u8 *)obj_data(image);
    if (!code_image_ok(d, obj_size(image), NULL, NULL, NULL)) return NULL;

    process *p = proc_create_code("code", d, obj_size(image), console_port);
    if (!p) return NULL;
    object *prog = held(p);
    if (!proc_start(p)) { obj_release(prog); return NULL; }

    /* Its image: the first giving, recorded on the program object like
     * every giving, and read-only like a set of orders should be from
     * the inside. The bytes stay the person's to change from outside;
     * what runs is what was loaded. */
    obj_set_slot(prog, 0, image, CAP_READ);
    obj_set_slot_name(prog, 0, "its code");
    proc_grant(prog, image, CAP_READ);

    kprintf("proc: %llu (code) running an image\n", proc_id(p));
    return prog;
}

/* A visiting script: the same interpreter, holding exactly two
 * things -- its words, read-only, and the way home, send-only. The
 * budget rides on the first gift and the interpreter enforces it,
 * which is what makes running a stranger's text tolerable: the
 * language is the jail, and this hands the visitor a cell with a
 * clock in it and nothing else. A divided job also brings its range:
 * the low end rides on the way home, the high end follows as a bare
 * number, so the script's first two waits read them as m. */
object *work_launch(object *script, object *reply, u64 budget_seconds,
                    i64 lo, i64 hi)
{
    if (!script || obj_type(script) != TYPE_TEXT || !reply ||
        !console_port)
        return NULL;

    process *p = proc_create("work", (const void *)user_runner,
                             console_port);
    if (!p) return NULL;
    object *prog = held(p);
    if (!proc_start(p)) { obj_release(prog); return NULL; }

    obj_set_slot(prog, 0, script, CAP_READ);
    obj_set_slot_name(prog, 0, "its words");
    proc_grant_word(prog, script, CAP_READ, budget_seconds);

    obj_set_slot(prog, 1, reply, CAP_CALL);
    obj_set_slot_name(prog, 1, "the way home");
    proc_grant_word(prog, reply, CAP_CALL, (u64)lo);
    proc_post_number(prog, 0x424D554EULL /* "NUMB" */, (u64)hi);

    kprintf("proc: %llu (work) running a visiting text\n", proc_id(p));
    return prog;
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
        /* The line goes out whole, in one call, so that two threads'
         * words do not interleave letter by letter; anything outside
         * printable ASCII is dropped rather than sent to the console,
         * so a program cannot drive the terminal with escape
         * sequences. */
        char said[3 * 8 + 1], shown[3 * 8 + 1];
        u32 n = 0, k = 0;
        for (u32 i = 0; i < m.nwords && i < 3; i++)
            for (u32 b = 0; b < 8; b++) {
                char c = (char)((m.words[i] >> (b * 8)) & 0xFF);
                said[n++] = c;
                if (c >= 0x20 && c < 0x7F) shown[k++] = c;
            }
        said[n] = 0;
        shown[k] = 0;
        kprintf("user: %s\n", shown);
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
    "answer <n or v>  the value, in digits, to it\n"
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
    "line runs the new words.\n"
    "\n"
    "a text can also be asked of another machine: press\n"
    "ask beside send, and it runs over there. it arrives\n"
    "holding the way home as its first gift -- wait, then\n"
    "answer sends the result back. it must finish inside\n"
    "its budget, and the far machine only works at all\n"
    "when its settings say \"work | welcomed\".\n"
    "\n"
    "a first line \"split P from LO to HI\" divides such\n"
    "a task among the machines that answer the scan\n"
    "willing: each part runs with its own stretch -- the\n"
    "first wait says the low end as m, the second wait\n"
    "the high end -- and the parts' numbers are summed.\n"
    "the answer is written back into the task. point a\n"
    "task at the foreman program and it is seen through\n"
    "without another click; \"again N\" in the first line\n"
    "hands it in anew every n seconds.\n";

/* Finds a reference by petname and type, on the root or one list
 * below it -- the one search the shelved graph needs everywhere. */
static object *find_petnamed(object *root, const char *nm, type_id t,
                             object **holder, u64 *slot)
{
    for (u64 i = 0; i < obj_slots(root); i++) {
        object *s = obj_get_slot(root, i);
        const char *n = obj_slot_name(root, i);
        if (s && n && strcmp(n, nm) == 0 && obj_type(s) == t) {
            if (holder) *holder = root;
            if (slot) *slot = i;
            return s;
        }
    }
    for (u64 i = 0; i < obj_slots(root); i++) {
        object *l = obj_get_slot(root, i);
        if (!l || obj_type(l) != TYPE_LIST) continue;
        for (u64 k = 0; k < obj_slots(l); k++) {
            object *s = obj_get_slot(l, k);
            const char *n = obj_slot_name(l, k);
            if (s && n && strcmp(n, nm) == 0 && obj_type(s) == t) {
                if (holder) *holder = l;
                if (slot) *slot = k;
                return s;
            }
        }
    }
    return NULL;
}

/* Appends into the first empty slot, growing when there is none. */
static bool list_append(object *l, object *o, u32 rights, const char *nm)
{
    u64 n = obj_slots(l), at = n;
    for (u64 i = 0; i < n; i++)
        if (!obj_get_slot(l, i)) { at = i; break; }
    if (at == n && !obj_grow_slots(l, n + 1)) return false;
    obj_set_slot(l, at, o, rights);
    obj_set_slot_name(l, at, nm);
    return true;
}

/* The shelves. A home where every program, page and list lies on one
 * level reads like a drawer tipped out; two lists sort it: "programs"
 * holds what runs, "system" holds the machine's own pages -- the
 * time, the log, the settings, the activity. The person's material
 * stays on top. Old graphs are sorted the same way on the way in, so
 * the structure is the system's, not only the fresh start's. */
static void ensure_structure(object *root, object **progs_out,
                             object **sys_out)
{
    object *progs = NULL, *sys = NULL;

    for (u64 i = 0; i < obj_slots(root); i++) {
        object *s = obj_get_slot(root, i);
        const char *n = obj_slot_name(root, i);
        if (!s || !n || obj_type(s) != TYPE_LIST) continue;
        if (strcmp(n, "programs") == 0) progs = s;
        if (strcmp(n, "system") == 0)   sys = s;
    }

    if (!progs) {
        object *made = obj_create(TYPE_LIST, 0, 4);
        if (made) {
            obj_set_name(made, "programs");
            if (list_append(root, made,
                            CAP_READ | CAP_WRITE | CAP_GRANT, "programs"))
                progs = made;
            obj_release(made);
        }
    }
    if (!sys) {
        object *made = obj_create(TYPE_LIST, 0, 4);
        if (made) {
            obj_set_name(made, "system");
            if (list_append(root, made,
                            CAP_READ | CAP_WRITE | CAP_GRANT, "system"))
                sys = made;
            obj_release(made);
        }
    }

    /* Sorting in: programs -- running or records -- go to their
     * shelf, the machine's pages to theirs, each keeping its rights
     * and its petname. Everything else stays where the person put
     * it. */
    for (u64 i = 0; i < obj_slots(root); i++) {
        object *s = obj_get_slot(root, i);
        if (!s || s == progs || s == sys) continue;
        const char *nm = obj_slot_name(root, i);
        u32 r = obj_slot_rights(root, i);

        object *shelf = NULL;
        if (progs && obj_type(s) == TYPE_PROGRAM) shelf = progs;
        else if (sys && nm &&
                 (strcmp(nm, "the time") == 0 || strcmp(nm, "log") == 0 ||
                  strcmp(nm, "settings") == 0 ||
                  strcmp(nm, "activity") == 0))
            shelf = sys;
        if (!shelf) continue;

        if (list_append(shelf, s, r, nm)) {
            obj_set_slot(root, i, NULL, 0);
            obj_set_slot_name(root, i, NULL);
        }
    }

    *progs_out = progs;
    *sys_out = sys;
}

/* The machine, as a program sees it -- written so that the page
 * itself assembles: stand on it, press assemble, run what it made.
 * A program made on this machine starts with two handles and the
 * eight calls, and nothing else; this is the whole of it. */
static const char machine_text[] =
    "; the machine, as a program sees it.\n"
    ";\n"
    "; a program starts holding two things and nothing else:\n"
    ";   rdi  a handle to speak to the console (send, tag TEXT)\n"
    ";   rsi  a handle to its own letter box (receive)\n"
    "; the stack is ready at rsp. the code lies at 0x1000000,\n"
    "; read and run, never written; the data at 0x1100000, read\n"
    "; and written, never run -- one page at least.\n"
    ";\n"
    "; a system call: the number in rax, the arguments in\n"
    "; rdi rsi rdx r10 r8, the answer in rax. rcx and r11 are\n"
    "; lost across it.\n"
    ";   0 exit                1 yield\n"
    ";   2 send     handle, tag, w0, w1, w2\n"
    ";   3 receive  handle, buffer, no_wait   (72 bytes land)\n"
    ";   4 read     handle, offset  -> eight bytes\n"
    ";   5 write    handle, offset, value\n"
    ";   6 pass     port, tag, handle, mask, w0\n"
    ";   7 clock    -> the second of the day\n"
    "; answers: 0 done; -1 refused; -2 nothing there, or full.\n"
    ";\n"
    "; a message, as it lands in the buffer:\n"
    ";   0   tag      8 bytes      16  words    4 x 8\n"
    ";   8   nwords   4 bytes      48  handles  2 x 8\n"
    ";   12  ncaps    4 bytes      64  masks    2 x 4\n"
    "; a gift is tag 0x4556494721: the handle at 48, the rights\n"
    "; at 16, and a number that rode along at 24. TEXT is\n"
    "; 0x54584554: three words of eight letters to the console.\n"
    ";\n"
    "; the words: mov lea movzx add sub and or xor cmp test\n"
    "; shl shr sar inc dec neg not mul imul div idiv cqo push\n"
    "; pop call ret jmp je jne jl jge jle jg jb jae jbe ja\n"
    "; syscall nop. brackets hold addresses: [rax], [rbx + 8],\n"
    "; [name]. a width goes before them when nothing else says\n"
    "; it: byte [rdi], qword [rsp - 8]. section code, section\n"
    "; data; db dw dd dq lay values down, res n lays n zeros.\n"
    ";\n"
    "; stand on this page, press assemble, then run what it made.\n"
    "\n"
    "section data\n"
    "hello: db \"hello fr\", \"om the m\", \"achine  \"\n"
    "\n"
    "section code\n"
    "    mov rax, 2              ; send\n"
    "    mov rsi, 0x54584554     ; the TEXT tag\n"
    "    mov rdx, [hello]        ; eight letters each\n"
    "    mov r10, [hello + 8]\n"
    "    mov r8, [hello + 16]\n"
    "    syscall                 ; rdi still holds the console\n"
    "    mov rax, 0              ; exit\n"
    "    syscall\n";

/* Finds a reference page -- named on the root or one list below it
 * -- or makes one, preferring to live in the aside next to the other
 * explanations. Then brings its words up to date. Graphs from earlier
 * days gain the page this way too. */
static void ensure_page(object *root, const char *name,
                        const char *text, u64 text_size, u64 page_size)
{
    object *found = NULL;
    object *aside = NULL;
    object *place_of = NULL;             /* where the reference lives */
    u64     slot_of = 0;

    for (u64 i = 0; i < obj_slots(root) && !found; i++) {
        object *s = obj_get_slot(root, i);
        if (!s) continue;
        const char *nm = obj_slot_name(root, i);

        if (nm && strcmp(nm, name) == 0 && obj_type(s) == TYPE_TEXT) {
            found = s; place_of = root; slot_of = i;
            break;
        }

        if (obj_type(s) != TYPE_LIST) continue;
        if (nm && strcmp(nm, "aside") == 0) aside = s;

        for (u64 j = 0; j < obj_slots(s) && !found; j++) {
            object *t = obj_get_slot(s, j);
            const char *tn = obj_slot_name(s, j);
            if (t && tn && strcmp(tn, name) == 0 &&
                obj_type(t) == TYPE_TEXT) {
                found = t; place_of = s; slot_of = j;
            }
        }
    }

    if (!found) {
        object *made = obj_create(TYPE_TEXT, page_size, 0);
        if (!made) return;
        obj_set_name(made, name);

        object *place = aside ? aside : root;
        u64 n = obj_slots(place), at = n;
        for (u64 i = 0; i < n; i++)
            if (!obj_get_slot(place, i)) { at = i; break; }
        if (at == n && !obj_grow_slots(place, n + 1)) {
            obj_release(made);
            return;
        }
        obj_set_slot(place, at, made, CAP_READ);
        obj_set_slot_name(place, at, name);
        obj_release(made);
        found = made;                     /* the slot holds it now */
    }

    /* A page from an older day may be too small for today's words.
     * The words matter, the object does not: a bigger page takes the
     * old one's place in the graph. */
    if (obj_size(found) < text_size && place_of) {
        object *wider = obj_create(TYPE_TEXT, page_size, 0);
        if (!wider) return;
        obj_set_name(wider, name);
        obj_set_slot(place_of, slot_of, wider, CAP_READ);
        obj_set_slot_name(place_of, slot_of, name);
        obj_release(wider);
        found = wider;
    }

    u8 *d = (u8 *)obj_data(found);
    if (!d || obj_size(found) < text_size) return;

    bool same = true;
    for (u64 i = 0; i < text_size && same; i++)
        if (d[i] != (u8)text[i]) same = false;
    if (same) return;

    for (u64 i = 0; i < obj_size(found); i++)
        d[i] = (i < text_size) ? (u8)text[i] : 0;
    char note[64];
    u32 at = 0;
    for (u32 i = 0; name[i] && at < 30; i++) note[at++] = name[i];
    const char *tail = " page speaks today's words";
    for (u32 i = 0; tail[i] && at < sizeof(note) - 1; i++) note[at++] = tail[i];
    note[at] = 0;
    journal_says("system", note);
}

/* The compiler's page: c as this machine speaks it, and a program
 * in it. Stand on the page, press compile, run what it made. */
static const char compiler_text[] =
    "/* the compiler: c, the way this machine speaks it.\n"
    " *\n"
    " * a program is a text like this one. stand on it, press\n"
    " * compile: beside it lands the assembly it became, to be read,\n"
    " * and the image that runs. main is called with the two things\n"
    " * a program starts holding, and its answer is the exit code.\n"
    " *\n"
    " * types: char (1 byte), short (2), int (4), long (8), signed\n"
    " * or not; float and double; pointers, arrays, struct, union,\n"
    " * bit fields, typedef, enum, __attribute__((packed)). whole\n"
    " * arithmetic is done in 64 bits and cut to size on the way\n"
    " * into a variable. functions take up to sixteen arguments, six\n"
    " * in registers; pointers to them work; so do ... and va_arg.\n"
    " * if else while for do switch break continue return goto.\n"
    " * the operators of c, with c's precedence; sizeof; casts;\n"
    " * initializers with braces and .designators; static locals;\n"
    " * _Static_assert. #define with or without arguments, #if and\n"
    " * #elif with arithmetic and defined(), #include \"name\" (a\n"
    " * text beside this one), <stdarg.h> <stdbool.h> <stdint.h>\n"
    " * <stddef.h> from inside. comments both ways.\n"
    " * syscall(nr, a0, a1, a2, a3, a4) is the door to the kernel;\n"
    " * the machine page names the calls.\n"
    " *\n"
    " * inline assembly the way the kernel writes it: asm volatile\n"
    " * (\"...\" : outputs : inputs : clobbers) with a b c d S D r m i,\n"
    " * and register variables tied to a name. structs go in and come\n"
    " * back by value; (type){ ... } literals; a variadic call of any\n"
    " * length.\n"
    " *\n"
    " * a text without main becomes an object: bytes that still\n"
    " * wait for other texts' names. 'link' on a list of objects\n"
    " * joins them into one image -- or into a kernel, when one of\n"
    " * them lays down kmain. 'build' on a list of texts does all\n"
    " * of it: every .c through this compiler, every .S (the gnu\n"
    " * dialect the kernel's own assembly is written in) through\n"
    " * the translator, and the objects through the linker. the\n"
    " * kernel's own sources, taken in from the exchange disk, build\n"
    " * this way into a kernel.elf that boots.\n"
    " *\n"
    " * not here: a 128-bit type, and va_arg of a struct.\n"
    " */\n"
    "\n"
    "#define SEND 2\n"
    "#define TEXT 0x54584554\n"
    "\n"
    "long say(long console, char *s)\n"
    "{\n"
    "    long w[3];\n"
    "    char *b = (char *)w;\n"
    "    long i;\n"
    "    for (i = 0; i < 24; i++) b[i] = ' ';\n"
    "    for (i = 0; i < 24 && s[i]; i++) b[i] = s[i];\n"
    "    return syscall(SEND, console, TEXT, w[0], w[1], w[2]);\n"
    "}\n"
    "\n"
    "long sum_to(long n)\n"
    "{\n"
    "    long s = 0;\n"
    "    while (n > 0) s += n--;\n"
    "    return s;\n"
    "}\n"
    "\n"
    "long main(long console, long inbox)\n"
    "{\n"
    "    char digits[24];\n"
    "    double half = 0.5;             /* the vector unit, in ring 3 */\n"
    "    long v = sum_to(100) + (long)(half * 4.0) - 2;\n"
    "    long i = 23;\n"
    "    digits[i] = 0;\n"
    "    do { digits[--i] = '0' + v % 10; v /= 10; } while (v);\n"
    "    say(console, \"hello from c\");\n"
    "    say(console, digits + i);\n"
    "    return 0;\n"
    "}\n";

static void ensure_language(object *root)
{
    ensure_page(root, "the language", lang_text, sizeof(lang_text), 2048);
    ensure_page(root, "the machine", machine_text, sizeof(machine_text), 3000);
    ensure_page(root, "the compiler", compiler_text, sizeof(compiler_text), 4000);
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

/* The exchange disk's list, when a FAT disk is attached: where its
 * files landed as objects, and where objects lie to be written out. */
static object *disk_list;

object *system_disk(void) { return disk_list; }

/* The list this machine serves to the local net, when the person has
 * made one: a list named "the served" on home or one shelf below.
 * The reference is the whole switch -- no list, no serving. */
object *system_served(void)
{
    if (!persistent_root) return NULL;
    return find_petnamed(persistent_root, "the served", TYPE_LIST,
                         NULL, NULL);
}

/* The deliberate end: the graph goes to disk, the journal notes the
 * leaving, and the machine is asked to sleep at the ports the common
 * machines listen on. A machine that ignores them is told so instead
 * of left looking frozen. */
void system_off(void)
{
    journal_says("system", "going to rest");

    object *roots[2] = { persistent_root, shell_session() };
    if (persistent_root && blk_present())
        snap_save(roots, roots[1] ? 2 : 1);
    kprintf("system: off; generation %llu is on the disk\n",
            snap_generation());

    outw(0x604, 0x2000);              /* qemu q35 */
    outw(0xB004, 0x2000);             /* bochs, older qemu */
    outw(0x4004, 0x3400);             /* virtualbox */

    journal_says("system", "the machine would not sleep");
}

void system_restart(void)
{
    object *roots[2] = { persistent_root, shell_session() };
    if (persistent_root && blk_present())
        snap_save(roots, roots[1] ? 2 : 1);
    kprintf("system: restarting; generation %llu is on the disk\n",
            snap_generation());

    /* The keyboard controller's reset line, then the chipset's reset
     * register, and if neither is listened to, an interrupt with no
     * table to take it: the processor resets itself. */
    outb(0x64, 0xFE);
    for (volatile u32 i = 0; i < 1000000; i++) { }
    outb(0xCF9, 0x06);
    for (volatile u32 i = 0; i < 1000000; i++) { }
    struct { u16 limit; u64 base; } __attribute__((packed)) none = { 0, 0 };
    __asm__ volatile ("lidt %0\n\tint3" :: "m"(none));
    for (;;) __asm__ volatile ("hlt");
}

/* Writes the graph out once changes have stopped arriving.
 *
 * There is no save command, so something has to decide when. Waiting
 * for quiet rather than saving on every keystroke means a burst of
 * typing costs one write instead of thirty, and half a second of
 * stillness is far below the point where anyone would notice. */
static u32 taken_at_boot;         /* files the exchange disk handed over before this ran */

static void persist_thread(void *arg)
{
    (void)arg;

    /* What the boot took in from the exchange disk changed the graph
     * before anyone was counting; the first quiet moment writes it
     * down, or a machine turned off in the meantime would come up
     * without it. */
    u64 seen = shell_changes() + obj_touches();
    u64 written = taken_at_boot ? seen - 1 : seen;
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
#ifdef __erebus__
    /* The compiler the machine carries defines this; the one outside
     * does not. A kernel that says it was built here, was. */
    kprintf("boot: this kernel was built by the machine's own tools\n");
#endif

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

    /* Deep enough that a quick program's burst of lines survives the
     * console being slower than the sender; what does not fit is
     * refused, and a program that cares asks again. */
    object *console = port_create(256);
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

        /* The shelves come first, so records already sorted onto them
         * -- and records still lying flat in an old graph, which the
         * sorting moves -- are all found in the walk below. */
        object *progs_shelf = NULL, *sys_shelf = NULL;
        ensure_structure(root, &progs_shelf, &sys_shelf);

        /* Everywhere a record can lie: the root, and each list on it. */
        object *places[16];
        u32 nplaces = 0;
        places[nplaces++] = root;
        for (u64 i = 0; i < obj_slots(root) && nplaces < 16; i++) {
            object *s = obj_get_slot(root, i);
            if (s && obj_type(s) == TYPE_LIST) places[nplaces++] = s;
        }

        for (u32 pi = 0; pi < nplaces; pi++)
            for (u64 i = 0; i < obj_slots(places[pi]); i++) {
                object *s = obj_get_slot(places[pi], i);
                if (!s || obj_type(s) != TYPE_PROGRAM ||
                    proc_is_running(s))
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
         * slot on the programs shelf. Read and grant, never write:
         * nobody edits a running program by typing into it. */
        for (u32 pi = 0; pi < nplaces; pi++) {
            object *place = places[pi];
            for (u64 i = 0; i < obj_slots(place); i++) {
            object *s = obj_get_slot(place, i);
            if (!s || obj_type(s) != TYPE_PROGRAM || proc_is_running(s))
                continue;
            bool matched = false;
            for (u32 k = 0; k < STANDARD_COUNT; k++) {
                if (s != records[k] || !standard_obj[k]) continue;
                obj_set_slot(place, i, standard_obj[k],
                             obj_slot_rights(place, i));
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
            /* Only an unmatched record is still an object: a matched
             * one was just replaced in its slot, and that may have
             * been its last holder. */
            bool was_script = !matched && obj_name(s) &&
                              strcmp(obj_name(s), "script") == 0;
            bool was_code   = !matched && obj_name(s) &&
                              strcmp(obj_name(s), "code") == 0;
            if (was_script || was_code) {
                object *script_words = NULL;
                object *code_image = NULL;
                for (u64 j = 0; j < obj_slots(s); j++) {
                    object *t = obj_get_slot(s, j);
                    const char *jn = obj_slot_name(s, j);
                    if (t && jn && strcmp(jn, "its words") == 0 &&
                        obj_type(t) == TYPE_TEXT) { script_words = t; break; }
                    if (t && jn && strcmp(jn, "its code") == 0 &&
                        obj_type(t) == TYPE_BYTES) { code_image = t; break; }
                }

                /* A program built on this machine comes back the same
                 * way a script does: its image is all it ever was. */
                object *fresh = script_words ? runner_launch(script_words)
                              : code_image   ? code_launch(code_image)
                              : NULL;
                if (fresh) {
                    for (u64 j = 0; j < obj_slots(s); j++) {
                        object *t = obj_get_slot(s, j);
                        const char *jn = obj_slot_name(s, j);
                        if (!t || (jn && (strcmp(jn, "its words") == 0 ||
                                          strcmp(jn, "its code") == 0)))
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
                    obj_set_slot(place, i, fresh,
                                 obj_slot_rights(place, i));
                    obj_release(fresh);          /* the slot holds it now */
                    matched = true;
                }
            }

            if (!matched) {
                obj_set_slot(place, i, NULL, 0);
                obj_set_slot_name(place, i, NULL);
            }
            }
        }

        for (u32 k = 0; k < STANDARD_COUNT; k++) {
            if (!standard_obj[k] || placed[k]) continue;
            object *shelf = progs_shelf ? progs_shelf : root;
            list_append(shelf, standard_obj[k], CAP_READ | CAP_GRANT,
                        standard[k].petname);
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

                    list_append(sys_shelf ? sys_shelf : root, face,
                                CAP_READ, "the time");

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
        {
            object *j = find_petnamed(root, "log", TYPE_TEXT,
                                      NULL, NULL);
            if (j) journal_adopt(j);
        }
        if (!journal_object() && journal_create())
            list_append(sys_shelf ? sys_shelf : root, journal_object(),
                        CAP_READ, "log");

        /* The settings, found the same way as the journal or made
         * fresh. The reference the person holds is read AND write: how
         * the system is set is theirs to say, by writing sentences. */
        {
            object *s = find_petnamed(root, "settings", TYPE_TEXT,
                                      NULL, NULL);
            if (s) settings_adopt(s);
        }
        if (!settings_object() && settings_create())
            list_append(sys_shelf ? sys_shelf : root, settings_object(),
                        CAP_READ | CAP_WRITE, "settings");
        settings_apply();

        /* The activity table: what the machine is doing, rewritten
         * once a second by its own thread. Read-only for the person --
         * the machine reports, nobody edits the report. */
        {
            object *a = find_petnamed(root, "activity", TYPE_TEXT,
                                      NULL, NULL);
            if (a) activity_adopt(a);
        }
        if (!activity_object() && activity_create())
            list_append(sys_shelf ? sys_shelf : root, activity_object(),
                        CAP_READ, "activity");

        /* Where the pipe lays what other machines send: a plain list,
         * found again by its name or made fresh, held read-and-write
         * -- what arrived is the person's to keep, rename, or throw
         * out, and the kernel only ever appends. */
        {
            object *arr = find_petnamed(root, "arrivals", TYPE_LIST,
                                        NULL, NULL);
            if (!arr) {
                object *made = obj_create(TYPE_LIST, 0, 4);
                if (made) {
                    obj_set_name(made, "arrivals");
                    if (list_append(root, made, CAP_READ | CAP_WRITE,
                                    "arrivals"))
                        arr = made;
                    obj_release(made);
                }
            }
            if (arr) pipe_arrivals_set(arr);
        }

        /* The line: one running conversation with whoever else is on
         * the pipe. The kernel writes what is said; the person reads
         * it here and speaks through the shell's bottom row. Read-only
         * on purpose -- a talk, once had, is a record like any other. */
        {
            object *ln = find_petnamed(root, "the line", TYPE_TEXT,
                                       NULL, NULL);
            if (!ln) {
                object *made = obj_create(TYPE_TEXT, 4096, 0);
                if (made) {
                    obj_set_name(made, "the line");
                    if (list_append(sys_shelf ? sys_shelf : root, made,
                                    CAP_READ, "the line"))
                        ln = made;
                    obj_release(made);
                }
            }
            if (ln) pipe_line_set(ln);
        }

        /* The door's key: the host's ed25519 pair, made once and kept
         * in the graph like everything that must survive a boot. The
         * reference the person holds grants nothing -- it exists
         * where everything exists, and nobody reads it. Letting go
         * of it makes a fresh one at the next start, and the clients
         * will say so. */
        {
            object *dk = find_petnamed(root, "the door key", TYPE_BYTES,
                                       NULL, NULL);
            if (!dk) {
                object *made = obj_create(TYPE_BYTES, 64, 0);
                if (made) {
                    obj_set_name(made, "the door key");
                    ssh_make_key((u8 *)obj_data(made));
                    if (list_append(sys_shelf ? sys_shelf : root, made, 0,
                                    "the door key"))
                        dk = made;
                    obj_release(made);
                }
            }
            if (dk && obj_size(dk) >= 64) {
                ssh_init((const u8 *)obj_data(dk));
                char fp[64];
                ssh_fingerprint(fp);
                kprintf("ssh:  the door's key is %s\n", fp);
                char note[96];
                u32 at = 0;
                const char *a = "the door's key is ";
                while (a[at]) { note[at] = a[at]; at++; }
                for (u32 i = 0; fp[i] && at < sizeof(note) - 1; i++)
                    note[at++] = fp[i];
                note[at] = 0;
                journal_says("ssh", note);
            }
        }

        /* A restored clock face is the same heartbeat it always was;
         * the mark does not ride the snapshot, so it is set anew. */
        {
            object *face = find_petnamed(root, "the time", TYPE_TEXT,
                                         NULL, NULL);
            if (face) obj_set_fleeting(face, true);
        }

        /* The exchange disk: when a FAT disk stands beside the store,
         * its files come in as objects on a list of their own, laid
         * on the system shelf. Writing back stays a deliberate act,
         * a word on that list. */
        if (blk_aux_present() && fat_mount()) {
            object *dl = find_petnamed(root, "the disk", TYPE_LIST,
                                       NULL, NULL);
            if (!dl) {
                object *made = obj_create(TYPE_LIST, 0, 4);
                if (made) {
                    obj_set_name(made, "the disk");
                    if (list_append(sys_shelf ? sys_shelf : root, made,
                                    CAP_READ | CAP_WRITE, "the disk"))
                        dl = made;
                    obj_release(made);
                }
            }
            if (dl) {
                disk_list = dl;
                taken_at_boot = fat_take_in(dl);
            }
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

        /* The terminal walks the same graph from the same beginning,
         * with the same rights in hand. Its view is the shell's sixth
         * mode; the core it talks to knows nothing of screens, which
         * is what will let a remote line speak to it one day. */
        term_init(root, CAP_READ | CAP_WRITE | CAP_GRANT);
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

    /* The kernel is up: the loader's count of starts goes back to
     * zero. A start the loader had to rescue is said so, and so is
     * one that follows a start that never got this far. */
    {
        u32 was = fat_boot_settle();
        if (bi->flags & EB_BOOT_FELL_BACK) {
            kprintf("boot: the installed kernel did not come up twice; the previous one is back as kernel.elf\n");
            journal_says("system", "the new kernel did not come up; the previous one is back");
        } else if (was > 1) {
            journal_says("system", "the machine is up after a start that did not finish");
        }
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
