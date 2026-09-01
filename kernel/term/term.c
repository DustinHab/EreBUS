/*
 * term.c -- the system spoken to in lines.
 *
 * The terminal is a walk, the same walk the shell makes with clicks:
 * it stands somewhere in the graph, holds exactly the rights the
 * references on the way granted, and can only narrow them by going
 * further. There are no paths and no lookup by name from nowhere --
 * "go" follows a reference the standpoint can see, or it goes
 * nowhere.
 *
 * The grammar is one sentence shape, everywhere: a verb, then a
 * name, and when a second thing is involved, "to" or "at" joins it.
 * Names may contain spaces, which is why the joiner is a word and
 * not a character. Numbers count slots. There are no flags, no
 * options, no punctuation to remember.
 *
 * Nothing in here touches a screen. The transcript is a byte ring
 * and the commands come in as lines; the shell's terminal view feeds
 * keys into the gathering line, and a remote line arriving over the
 * network later will call term_line() directly. Both read the same
 * transcript back.
 */
#include <eb/term.h>
#include <eb/cap.h>
#include <eb/proc.h>
#include <eb/journal.h>
#include <eb/pipe.h>
#include <eb/standard.h>
#include <eb/settings.h>
#include <eb/time.h>
#include <eb/string.h>

#define TERM_DEPTH 16
#define TERM_OUT   16384
#define TERM_LINE  200
#define NAME_SHOWN 40

static object *node[TERM_DEPTH];
static u32     rights[TERM_DEPTH];
static char    names[TERM_DEPTH][NAME_SHOWN];
static u32     depth;

static char out[TERM_OUT];
static u64  out_len;
static u64  seq;

static char in_line[TERM_LINE];
static u32  in_len;
static char last_line[TERM_LINE];

/* ------------------------------------------------------------------ */
/* The transcript                                                      */
/* ------------------------------------------------------------------ */

const char *term_out(u64 *len)
{
    if (len) *len = out_len;
    return out;
}

u64 term_sequence(void) { return seq; }

static void t_putc(char c)
{
    /* Full: the older half makes room, cut at a line boundary. */
    if (out_len + 1 >= sizeof(out)) {
        u64 from = out_len / 2;
        while (from < out_len && out[from] != '\n') from++;
        if (from < out_len) from++;
        memmove(out, out + from, out_len - from);
        out_len -= from;
    }
    out[out_len++] = c;
}

static void t_puts(const char *s)
{
    while (*s) t_putc(*s++);
}

static void t_dec(u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) t_putc(d[--n]);
}

static void t_end(void)
{
    t_putc('\n');
    seq++;
}

static void t_say(const char *s)
{
    t_puts(s);
    t_end();
}

/* ------------------------------------------------------------------ */
/* The standpoint                                                      */
/* ------------------------------------------------------------------ */

static object *focus(void)       { return node[depth - 1]; }
static u32     focus_rights(void){ return rights[depth - 1]; }

void term_init(object *root, u32 root_rights)
{
    if (!root) return;
    obj_retain(root);
    node[0] = root;
    rights[0] = root_rights;
    u32 i = 0;
    const char *h = "home";
    while (h[i]) { names[0][i] = h[i]; i++; }
    names[0][i] = 0;
    depth = 1;
    t_say("the terminal.  'help' names the words it knows.");
}

static const char *kind_word(type_id t)
{
    switch (t) {
    case TYPE_TEXT:    return "text";
    case TYPE_BYTES:   return "bytes";
    case TYPE_LIST:    return "list";
    case TYPE_PROGRAM: return "program";
    case TYPE_PICTURE: return "picture";
    default:           return "thing";
    }
}

static void rights_word(u32 r, char b[4])
{
    b[0] = (r & CAP_READ)  ? 'r' : '-';
    b[1] = (r & CAP_WRITE) ? 'w' : '-';
    b[2] = (r & CAP_GRANT) ? 'g' : '-';
    b[3] = 0;
}

/* The name a slot shows: the petname if one was given, else the
 * object's own claimed name, else nothing. */
static const char *shown_name(object *holder, u64 slot)
{
    const char *pn = obj_slot_name(holder, slot);
    if (pn && pn[0]) return pn;
    object *t = obj_get_slot(holder, slot);
    const char *on = t ? obj_name(t) : NULL;
    return (on && on[0]) ? on : "";
}

static u64 text_len(const u8 *d, u64 size)
{
    u64 n = 0;
    while (n < size && d[n]) n++;
    return n;
}

static char low(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Does the line begin with this word, whole? Then rest is what
 * follows it, spaces skipped. Two-word verbs match the same way. */
static bool word_starts(const char *line, const char *word,
                        const char **rest);

/* ------------------------------------------------------------------ */
/* Finding what a word names                                           */
/* ------------------------------------------------------------------ */

/* A number names a slot by count; anything else is matched against
 * the names the slots show. Exact and whole, no guessing. */
static i64 slot_named(const char *what)
{
    object *f = focus();
    u64 n = obj_slots(f);

    bool digits = what[0] != 0;
    for (u32 i = 0; what[i]; i++)
        if (what[i] < '0' || what[i] > '9') { digits = false; break; }
    if (digits) {
        u64 v = 0;
        for (u32 i = 0; what[i]; i++) v = v * 10 + (u64)(what[i] - '0');
        if (v < n && obj_get_slot(f, v)) return (i64)v;
        return -1;
    }

    for (u64 i = 0; i < n; i++) {
        if (!obj_get_slot(f, i)) continue;
        const char *nm = shown_name(f, i);
        u32 j = 0;
        while (nm[j] && what[j] && low(nm[j]) == low(what[j])) j++;
        if (!nm[j] && !what[j]) return (i64)i;
    }
    return -1;
}

/* A place a command acts on: the standpoint itself when no name is
 * given, else the named reference -- looked at, not walked to. */
typedef struct {
    object     *o;
    u32         r;                    /* narrowed, as a walk would */
    const char *nm;
    i64         slot;                 /* -1 when it is the focus */
} spot;

static bool resolve(const char *what, spot *s)
{
    if (!what[0]) {
        s->o = focus();
        s->r = focus_rights();
        s->nm = names[depth - 1];
        s->slot = -1;
        return true;
    }
    i64 i = slot_named(what);
    if (i < 0) {
        t_puts("nothing here is called ");
        t_puts(what);
        t_say(".  'look' shows the names.");
        return false;
    }
    s->o = obj_get_slot(focus(), (u64)i);
    s->r = focus_rights() & obj_slot_rights(focus(), (u64)i);
    s->nm = shown_name(focus(), (u64)i);
    s->slot = i;
    if (s->r == 0) {
        t_say("that reference grants nothing in your hand.");
        return false;
    }
    return true;
}

/* Splits "<name> to <name>" at the last " to ", so names keep their
 * spaces. Answers false when there is no joiner. */
static bool split_at(const char *rest, const char *joiner,
                     char left[TERM_LINE], char right[TERM_LINE])
{
    i64 cut = -1;
    u32 jl = 0;
    while (joiner[jl]) jl++;
    for (u32 i = 0; rest[i]; i++) {
        u32 j = 0;
        while (j < jl && rest[i + j] == joiner[j]) j++;
        if (j == jl) cut = (i64)i;
    }
    if (cut < 0) return false;
    u32 n = 0;
    for (i64 i = 0; i < cut && n < TERM_LINE - 1; i++)
        left[n++] = rest[i];
    while (n && left[n - 1] == ' ') n--;
    left[n] = 0;
    n = 0;
    for (u32 i = (u32)cut + jl; rest[i] && n < TERM_LINE - 1; i++)
        right[n++] = rest[i];
    right[n] = 0;
    return left[0] && right[0];
}

/* Lays a fresh reference into the focused holder, first hole or a
 * grown one. Answers the slot, or -1 when there is no room. */
static i64 lay_here(object *made, u32 r, const char *nm)
{
    object *f = focus();
    u64 slots = obj_slots(f), at = slots;
    for (u64 i = 0; i < slots; i++)
        if (!obj_get_slot(f, i)) { at = i; break; }
    if (at == slots && !obj_grow_slots(f, slots + 1)) return -1;
    obj_set_slot(f, at, made, r);
    obj_set_slot_name(f, at, nm);
    obj_touch(f);
    return (i64)at;
}

/* ------------------------------------------------------------------ */
/* Looking and walking                                                 */
/* ------------------------------------------------------------------ */

static void describe(object *o, u32 r, const char *nm)
{
    char rw[4];
    rights_word(r, rw);

    t_puts(nm[0] ? nm : "(unnamed)");
    t_puts("  ");
    t_puts(kind_word(obj_type(o)));
    t_puts("  ");
    t_puts(rw);
    t_puts("  ");
    if (obj_type(o) == TYPE_LIST) {
        u64 filled = 0, n = obj_slots(o);
        for (u64 i = 0; i < n; i++) if (obj_get_slot(o, i)) filled++;
        t_dec(filled);
        t_puts(filled == 1 ? " thing" : " things");
    } else if (obj_type(o) == TYPE_TEXT) {
        t_dec(text_len((const u8 *)obj_data(o), obj_size(o)));
        t_puts(" letters");
    } else if (obj_type(o) == TYPE_PROGRAM) {
        t_puts(proc_is_running(o) ? "running" : "ended");
    } else {
        t_dec(obj_size(o));
        t_puts(" bytes");
    }
    t_end();

    u64 n = obj_slots(o);
    for (u64 i = 0; i < n; i++) {
        object *t = obj_get_slot(o, i);
        if (!t) continue;
        char sr[4];
        rights_word(r & obj_slot_rights(o, i), sr);
        t_puts("  ");
        t_dec(i);
        t_puts("  ");
        t_puts(sr);
        t_puts("  ");
        const char *snm = shown_name(o, i);
        t_puts(snm[0] ? snm : "(unnamed)");
        t_puts("  ");
        t_puts(kind_word(obj_type(t)));
        t_end();
    }
}

static void cmd_look(const char *what)
{
    spot s;
    if (!resolve(what, &s)) return;
    describe(s.o, s.r, s.nm);
}

static void cmd_where(void)
{
    for (u32 i = 0; i < depth; i++) {
        if (i) t_puts(" > ");
        t_puts(names[i]);
    }
    t_end();
}

static void cmd_go(const char *what)
{
    if (!what[0]) { t_say("go where?  'look' shows the names."); return; }
    if (depth >= TERM_DEPTH) { t_say("deep enough; 'back' first."); return; }

    spot s;
    if (!resolve(what, &s)) return;
    if (s.slot < 0) return;

    obj_retain(s.o);
    node[depth] = s.o;
    rights[depth] = s.r;
    u32 i = 0;
    while (s.nm[i] && i < NAME_SHOWN - 1) { names[depth][i] = s.nm[i]; i++; }
    names[depth][i] = 0;
    depth++;
    cmd_look("");
}

static void cmd_back(void)
{
    if (depth <= 1) { t_say("this is the beginning."); return; }
    depth--;
    obj_release(node[depth]);
    node[depth] = NULL;
    cmd_where();
}

static void cmd_home(void)
{
    while (depth > 1) {
        depth--;
        obj_release(node[depth]);
        node[depth] = NULL;
    }
    t_say("home.");
}

/* ------------------------------------------------------------------ */
/* Reading and writing                                                 */
/* ------------------------------------------------------------------ */

static void cmd_read(const char *what)
{
    spot s;
    if (!resolve(what, &s)) return;
    object *f = s.o;
    type_id t = obj_type(f);

    if (t == TYPE_TEXT) {
        const u8 *d = (const u8 *)obj_data(f);
        u64 len = text_len(d, obj_size(f));
        u64 show = len > 2000 ? 2000 : len;
        for (u64 i = 0; i < show; i++)
            t_putc(d[i] >= 0x20 || d[i] == '\n' ? (char)d[i] : ' ');
        if (show && out[out_len - 1] != '\n') t_putc('\n');
        if (len > show) {
            t_puts("...and ");
            t_dec(len - show);
            t_say(" more letters");
        }
        if (len == 0) t_say("(empty)");
        seq++;
        return;
    }
    if (t == TYPE_BYTES) {
        const u8 *d = (const u8 *)obj_data(f);
        u64 size = obj_size(f);
        t_dec(size);
        t_say(" bytes; the first rows of them:");
        static const char hx[] = "0123456789abcdef";
        for (u64 row = 0; row < 8 && row * 16 < size; row++) {
            t_puts("  ");
            for (u64 c = 0; c < 16 && row * 16 + c < size; c++) {
                u8 b = d[row * 16 + c];
                t_putc(hx[b >> 4]);
                t_putc(hx[b & 15]);
                t_putc(' ');
            }
            t_end();
        }
        return;
    }
    if (t == TYPE_PICTURE) {
        const u8 *d = (const u8 *)obj_data(f);
        u32 w = 0, h = 0;
        for (u32 i = 0; i < 4; i++) w |= (u32)d[i] << (i * 8);
        for (u32 i = 0; i < 4; i++) h |= (u32)d[4 + i] << (i * 8);
        t_puts("a picture, ");
        t_dec(w);
        t_puts(" by ");
        t_dec(h);
        t_say(".  the screen's picture lens shows it.");
        return;
    }
    if (t == TYPE_PROGRAM) {
        t_puts("a program, ");
        t_puts(proc_is_running(f) ? "running" : "ended");
        t_say(".  'look' shows what it holds.");
        return;
    }
    t_say("'look' is the way to read this kind.");
}

static void cmd_write(const char *words)
{
    object *f = focus();
    if (obj_type(f) != TYPE_TEXT) { t_say("only a text takes writing; 'go' to one first."); return; }
    if (!(focus_rights() & CAP_WRITE)) { t_say("this one is read-only in your hand."); return; }
    if (!words[0]) { t_say("write what?"); return; }

    u8 *d = (u8 *)obj_data(f);
    u64 size = obj_size(f);
    u64 len = text_len(d, size);
    u64 need = 0;
    while (words[need]) need++;
    if (len + need + 2 >= size) { t_say("it has no room left."); return; }

    if (len && d[len - 1] != '\n') d[len++] = '\n';
    for (u64 i = 0; i < need; i++) d[len++] = (u8)words[i];
    d[len++] = '\n';
    d[len] = 0;
    obj_touch(f);
    t_say("written.");
}

/* ------------------------------------------------------------------ */
/* Making, shaping, letting go                                         */
/* ------------------------------------------------------------------ */

static void cmd_make(const char *what)
{
    const char *nm;
    type_id t;
    if      (word_starts(what, "text", &nm)) t = TYPE_TEXT;
    else if (word_starts(what, "list", &nm)) t = TYPE_LIST;
    else { t_say("make text <name>, or make list <name>."); return; }
    if (!*nm) { t_say("name it."); return; }

    if (!(focus_rights() & CAP_WRITE)) { t_say("you may not lay things in here."); return; }

    object *made = obj_create(t, t == TYPE_TEXT ? 3000 : 0,
                              t == TYPE_LIST ? 4 : 0);
    if (!made) { t_say("nothing came of it; memory is short."); return; }
    i64 at = lay_here(made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);
    if (at < 0) { t_say("no room for another reference here."); return; }

    t_puts(nm);
    t_puts("  lies here now, slot ");
    t_dec((u64)at);
    t_end();
}

static void cmd_copy(const char *what)
{
    if (!what[0]) { t_say("copy which?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (s.slot < 0) return;
    if (!(s.r & CAP_READ)) { t_say("you may not read that."); return; }
    if (!(focus_rights() & CAP_WRITE)) { t_say("the copy would lie here, and you may not lay things in here."); return; }

    type_id t = obj_type(s.o);
    object *made = NULL;
    if (t == TYPE_LIST) {
        u64 n = obj_slots(s.o);
        made = obj_create(TYPE_LIST, 0, n ? n : 4);
        if (made) for (u64 i = 0; i < n; i++) {
            object *m = obj_get_slot(s.o, i);
            if (!m) continue;
            obj_set_slot(made, i, m, obj_slot_rights(s.o, i));
            obj_set_slot_name(made, i, obj_slot_name(s.o, i));
        }
    } else if (t == TYPE_TEXT || t == TYPE_BYTES || t == TYPE_PICTURE) {
        made = obj_create(t, obj_size(s.o), 0);
        if (made && obj_data(s.o))
            memcpy(obj_data(made), obj_data(s.o), obj_size(s.o));
    } else {
        t_say("this kind cannot be copied.");
        return;
    }
    if (!made) { t_say("nothing came of it; memory is short."); return; }

    char nm[NAME_SHOWN];
    u32 n = 0;
    while (s.nm[n] && n < 19) { nm[n] = s.nm[n]; n++; }
    const char *tail = " copy";
    for (u32 i = 0; tail[i]; i++) nm[n++] = tail[i];
    nm[n] = 0;

    i64 at = lay_here(made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);
    if (at < 0) { t_say("no room to lay the copy here."); return; }
    t_puts(nm);
    t_say("  lies beside it.");
}

static void cmd_rename(const char *rest)
{
    char a[TERM_LINE], b[TERM_LINE];
    if (!split_at(rest, " to ", a, b)) {
        t_say("rename <name> to <new name>.");
        return;
    }
    spot s;
    if (!resolve(a, &s)) return;
    if (s.slot < 0) return;
    if (!(focus_rights() & CAP_WRITE)) { t_say("the name lives on this holder, and you may not change it."); return; }
    obj_set_slot_name(focus(), (u64)s.slot, b);
    obj_touch(focus());
    t_puts(a);
    t_puts("  is now called  ");
    t_say(b);
}

/* Letting go steps the reference into the bin first, petname and
 * rights along; inside the bin, or without a writable home, it is
 * final -- the shell's manner, word for word. */
static void cmd_letgo(const char *what)
{
    if (!what[0]) { t_say("let go of which?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (s.slot < 0) { t_say("stand beside it, not on it: 'back' first."); return; }
    if (!(focus_rights() & CAP_WRITE)) { t_say("you may not take things out of here."); return; }

    if (obj_type(focus()) == TYPE_PROGRAM)
        proc_revoke(focus(), s.o);

    object *home = node[0];
    object *bin = NULL;
    if (rights[0] & CAP_WRITE) {
        for (u64 i = 0; i < obj_slots(home); i++) {
            object *b = obj_get_slot(home, i);
            const char *n = obj_slot_name(home, i);
            if (b && n && strcmp(n, "bin") == 0 && obj_type(b) == TYPE_LIST)
                { bin = b; break; }
        }
    }

    bool final = focus() == bin || s.o == bin || !(rights[0] & CAP_WRITE);

    if (!final && !bin) {
        object *made = obj_create(TYPE_LIST, 0, 4);
        if (made) {
            obj_set_name(made, "bin");
            u64 n = obj_slots(home), at = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(home, i)) { at = i; break; }
            if (at < n || obj_grow_slots(home, n + 1)) {
                obj_set_slot(home, at, made, CAP_READ | CAP_WRITE);
                obj_set_slot_name(home, at, "bin");
                bin = made;
            }
            obj_release(made);
        }
    }

    if (!final && bin) {
        u64 n = obj_slots(bin), at = n;
        for (u64 i = 0; i < n; i++)
            if (!obj_get_slot(bin, i)) { at = i; break; }
        if (at == n && !obj_grow_slots(bin, n + 1)) final = true;
        else {
            obj_set_slot(bin, at, s.o,
                         obj_slot_rights(focus(), (u64)s.slot));
            obj_set_slot_name(bin, at, s.nm);
            obj_touch(bin);
        }
    }

    obj_set_slot(focus(), (u64)s.slot, NULL, 0);
    obj_set_slot_name(focus(), (u64)s.slot, NULL);
    obj_touch(focus());
    t_say(final ? "let go, for good." : "it lies in the bin now.");
}

/* ------------------------------------------------------------------ */
/* Programs                                                            */
/* ------------------------------------------------------------------ */

static void cmd_run(const char *what)
{
    if (!what[0]) { t_say("run which text?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (obj_type(s.o) != TYPE_TEXT) { t_say("only a text can run."); return; }
    if (!(focus_rights() & CAP_WRITE)) { t_say("the running one would lie here, and you may not lay things in here."); return; }

    /* The name is copied out first: laying the program here can grow
     * the slot table, and s.nm points into the old one -- freed the
     * moment it grows. */
    char nm[NAME_SHOWN];
    u32 n = 0;
    while (s.nm[n] && n < NAME_SHOWN - 1) { nm[n] = s.nm[n]; n++; }
    nm[n] = 0;

    /* runner_launch lends the program object -- the live table holds
     * the reference. The slot below takes its own; releasing here
     * would give away what was never ours, and the reaper would then
     * free the object under the slot at the program's end. */
    object *prog = runner_launch(s.o);
    if (!prog) { t_say("it would not start."); return; }
    i64 at = lay_here(prog, CAP_READ | CAP_GRANT, nm);
    if (at < 0) { t_say("it runs, but there was no room to lay it here."); return; }
    t_say("it runs; the journal carries what it says.");
}

static void cmd_give(const char *rest)
{
    char a[TERM_LINE], b[TERM_LINE];
    if (!split_at(rest, " to ", a, b)) {
        t_say("give <name> to <program>.");
        return;
    }
    spot thing, prog;
    if (!resolve(a, &thing) || !resolve(b, &prog)) return;
    if (obj_type(prog.o) != TYPE_PROGRAM || !proc_is_running(prog.o)) {
        t_say("only a running program can be given to.");
        return;
    }
    if (!(prog.r & CAP_GRANT)) { t_say("you may not give to that program."); return; }

    if (!proc_grant(prog.o, thing.o, thing.r)) {
        t_say("it could not be handed over.");
        return;
    }
    t_puts(prog.nm);
    t_say("  holds it now, with what you held.");
}

static void cmd_end(const char *what)
{
    if (!what[0]) { t_say("end which program?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (obj_type(s.o) != TYPE_PROGRAM) { t_say("only a program can be ended."); return; }
    if (!(s.r & CAP_GRANT)) { t_say("you may not end that one."); return; }
    if (!proc_end(s.o)) { t_say("it was not running."); return; }
    journal_says("system", "a program was ended by hand");
    t_say("ended.  it finishes at its next step into the kernel.");
}

/* ------------------------------------------------------------------ */
/* The wire                                                            */
/* ------------------------------------------------------------------ */

static void cmd_send(const char *what)
{
    if (!what[0]) { t_say("send which?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (!(s.r & CAP_READ)) { t_say("you may not read that, so you may not send it."); return; }
    if (pipe_post(s.o))
        t_say("on its way to the peer.");
    else
        t_say("the pipe would not take it.  is a peer named?  'scan' and 'point at' set one.");
}

static void cmd_ask(const char *what)
{
    if (!what[0]) { t_say("ask with which task?"); return; }
    spot s;
    if (!resolve(what, &s)) return;
    if (obj_type(s.o) != TYPE_TEXT) { t_say("a task is a text."); return; }
    if (pipe_ask(s.o, (s.r & CAP_WRITE) != 0))
        t_say("the desk has it.  the answer lands in the task itself, or in arrivals.");
    else
        t_say("the desk would not take it.");
}

static void cmd_say(const char *words)
{
    if (!words[0]) { t_say("say what?"); return; }
    if (pipe_say(words))
        t_say("said; it stands on the line.");
    else
        t_say("nobody is on the line, and no peer is named in the settings.");
}

static void cmd_scan(void)
{
    pipe_scan();
    t_say("the call is out.  'found' shows who answered.");
}

static void cmd_found(void)
{
    u32 n = pipe_found_count();
    if (n == 0) {
        t_say(pipe_scanning() ? "no answers yet; the call is still out."
                              : "nobody has answered.  'scan' calls again.");
        return;
    }
    for (u32 i = 0; i < n; i++) {
        u8 ip[4];
        char nm[24];
        bool works;
        u32 mib;
        if (!pipe_found_at(i, ip, nm, &works, &mib)) continue;
        t_puts("  ");
        t_dec(ip[0]); t_putc('.');
        t_dec(ip[1]); t_putc('.');
        t_dec(ip[2]); t_putc('.');
        t_dec(ip[3]);
        t_puts("  ");
        t_puts(nm[0] ? nm : "(no name)");
        if (works) {
            t_puts("  takes work, ");
            t_dec(mib);
            t_puts("M free");
        }
        t_end();
    }
}

/* Points the pipe at a machine: by the name it answered the scan
 * with, or by its address. Written into the settings as the peer
 * line, the same honest way a click on a found machine writes it --
 * no hidden switch, just the sentence. */
static void cmd_point(const char *what)
{
    if (!what[0]) { t_say("point at whom?  a found name, or an address."); return; }

    u8 ip[4] = { 0, 0, 0, 0 };
    u32 port = 7800;
    bool have = false;

    u32 n = pipe_found_count();
    for (u32 i = 0; i < n && !have; i++) {
        u8 fip[4];
        char nm[24];
        if (!pipe_found_at(i, fip, nm, NULL, NULL)) continue;
        u32 j = 0;
        while (nm[j] && what[j] && low(nm[j]) == low(what[j])) j++;
        if (!nm[j] && !what[j]) {
            for (u32 k = 0; k < 4; k++) ip[k] = fip[k];
            have = true;
        }
    }

    if (!have) {
        /* Four numbers with dots, and a fifth for the port if given. */
        u32 v = 0, part = 0, i = 0;
        bool any = false;
        for (;; i++) {
            char c = what[i];
            if (c >= '0' && c <= '9') { v = v * 10 + (u32)(c - '0'); any = true; continue; }
            if (part < 4) {
                if (!any || v > 255) break;
                ip[part++] = (u8)v;
            } else {
                if (any && v >= 1 && v <= 65535) port = v;
                break;
            }
            v = 0; any = false;
            if (c == 0) break;
            if (c != '.' && c != ' ') break;
        }
        have = part == 4;
    }

    if (!have) { t_say("that names no machine i can see."); return; }

    object *st = settings_object();
    if (!st) { t_say("no settings stand."); return; }
    u8 *d = (u8 *)obj_data(st);
    u64 size = obj_size(st);
    u64 len = text_len(d, size);
    char line[64];
    u32 at = 0;
    const char *k = "peer     | ";
    while (k[at]) { line[at] = k[at]; at++; }
    for (u32 i = 0; i < 4; i++) {
        u32 v = ip[i];
        char dg[4];
        u32 nd = 0;
        if (v == 0) dg[nd++] = '0';
        while (v) { dg[nd++] = (char)('0' + v % 10); v /= 10; }
        while (nd) line[at++] = dg[--nd];
        line[at++] = i < 3 ? '.' : ' ';
    }
    u32 pv = port;
    char dg[8];
    u32 nd = 0;
    while (pv) { dg[nd++] = (char)('0' + pv % 10); pv /= 10; }
    while (nd) line[at++] = dg[--nd];
    if (len + at + 2 >= size) { t_say("the settings page has no room left."); return; }
    if (len && d[len - 1] != '\n') d[len++] = '\n';
    for (u32 i = 0; i < at; i++) d[len++] = (u8)line[i];
    d[len++] = '\n';
    d[len] = 0;
    obj_touch(st);
    settings_apply();

    t_puts("the pipe points at ");
    t_dec(ip[0]); t_putc('.');
    t_dec(ip[1]); t_putc('.');
    t_dec(ip[2]); t_putc('.');
    t_dec(ip[3]);
    t_say(" now; send, ask and say reach it.");
}

/* ------------------------------------------------------------------ */
/* Searching everything reachable                                      */
/* ------------------------------------------------------------------ */

#define FIND_MOST 256
#define FIND_HITS 16

static bool contains_ci(const u8 *hay, u64 hlen, const char *needle)
{
    u32 nl = 0;
    while (needle[nl]) nl++;
    if (nl == 0 || nl > hlen) return false;
    for (u64 i = 0; i + nl <= hlen; i++) {
        u32 j = 0;
        while (j < nl && low((char)hay[i + j]) == low(needle[j])) j++;
        if (j == nl) return true;
    }
    return false;
}

/* Walks everything reachable from home, breadth first, and names
 * every reference whose name or text holds the words. The walk is
 * bounded, and says so when it hits the bound -- a search that
 * silently gives up is a search that lies. */
static void cmd_find(const char *words)
{
    if (!words[0]) { t_say("find what?"); return; }

    static object *seen[FIND_MOST];
    static u32 parent[FIND_MOST];
    static char label[FIND_MOST][24];
    u32 count = 0, hits = 0;
    bool full = false;

    seen[0] = node[0];
    parent[0] = 0;
    label[0][0] = 0;
    count = 1;

    for (u32 at = 0; at < count; at++) {
        object *o = seen[at];
        u64 n = obj_slots(o);
        for (u64 i = 0; i < n; i++) {
            object *t = obj_get_slot(o, i);
            if (!t) continue;
            bool old = false;
            for (u32 j = 0; j < count; j++)
                if (seen[j] == t) { old = true; break; }
            if (old) continue;
            if (count >= FIND_MOST) { full = true; break; }

            u32 me = count++;
            seen[me] = t;
            parent[me] = at;
            const char *nm = shown_name(o, i);
            u32 c = 0;
            while (nm[c] && c < 23) { label[me][c] = nm[c]; c++; }
            label[me][c] = 0;

            bool hit = contains_ci((const u8 *)label[me], c, words);
            if (!hit && obj_type(t) == TYPE_TEXT) {
                const u8 *d = (const u8 *)obj_data(t);
                u64 tl = text_len(d, obj_size(t));
                if (tl > 4096) tl = 4096;
                hit = contains_ci(d, tl, words);
            }
            if (hit && hits < FIND_HITS) {
                hits++;
                /* The way there, walked back and said forward. */
                u32 chain[TERM_DEPTH];
                u32 cn = 0, w = me;
                while (w && cn < TERM_DEPTH) { chain[cn++] = w; w = parent[w]; }
                t_puts("  home");
                while (cn) {
                    t_puts(" > ");
                    t_puts(label[chain[--cn]][0]
                           ? label[chain[cn]] : "(unnamed)");
                }
                t_end();
            }
        }
        if (full) break;
    }

    if (hits == 0) t_say("nothing holds those words.");
    if (hits >= FIND_HITS) t_say("...and maybe more; the first sixteen are shown.");
    if (full) t_say("(the walk was cut short; the graph is larger than the search.)");
}

/* ------------------------------------------------------------------ */
/* Small things                                                        */
/* ------------------------------------------------------------------ */

static void cmd_journal(void)
{
    object *j = journal_object();
    const u8 *d = j ? (const u8 *)obj_data(j) : NULL;
    if (!d) { t_say("no journal stands."); return; }
    u64 len = text_len(d, obj_size(j));
    if (len == 0) { t_say("nothing has happened yet."); return; }

    u64 from = len, lines = 0;
    while (from > 0 && lines < 12) {
        from--;
        if (from && d[from - 1] == '\n') lines++;
    }
    for (u64 i = from; i < len; i++)
        t_putc(d[i] >= 0x20 || d[i] == '\n' ? (char)d[i] : ' ');
    if (out_len && out[out_len - 1] != '\n') t_putc('\n');
    seq++;
}

static void cmd_time(void)
{
    u32 h, m, s;
    time_wall(&h, &m, &s);
    if (h < 10) t_putc('0');
    t_dec(h);
    t_putc(':');
    if (m < 10) t_putc('0');
    t_dec(m);
    t_putc(':');
    if (s < 10) t_putc('0');
    t_dec(s);
    t_puts("  --  up ");
    t_dec(time_ns() / 1000000000ULL);
    t_say(" seconds");
}

static void cmd_help(void)
{
    t_say("one shape, always: a verb, a name, and 'to' or 'at' when");
    t_say("two things meet.  names may have spaces; numbers count slots.");
    t_end();
    t_say("looking around");
    t_say("  look [name]      what stands here, or what that points at");
    t_say("  go <name>        follow a reference");
    t_say("  back             one step back;  home  returns to the start");
    t_say("  where            the walk so far");
    t_say("  find <words>     search names and texts, everywhere you reach");
    t_end();
    t_say("things");
    t_say("  read [name]      the thing itself: letters, bytes, size");
    t_say("  write <words>    add a line to the text you stand on");
    t_say("  make text <name>     a fresh text, laid in here");
    t_say("  make list <name>     a fresh list, laid in here");
    t_say("  copy <name>      a copy, laid beside it");
    t_say("  rename <name> to <new name>");
    t_say("  let go <name>    into the bin; in the bin, for good");
    t_end();
    t_say("programs");
    t_say("  run <name>       run that text as a program, here");
    t_say("  give <name> to <program>   hand it a reference");
    t_say("  end <name>       end a running program");
    t_end();
    t_say("the other machines");
    t_say("  scan             call out: who else is on the wire?");
    t_say("  found            who answered");
    t_say("  point at <name or address>   choose the peer");
    t_say("  send <name>      carry a thing to the peer");
    t_say("  ask <name>       have the machines work a task text");
    t_say("  say <words>      speak on the line");
    t_end();
    t_say("the machine");
    t_say("  journal          the last things that happened");
    t_say("  time             the wall clock, and how long it has run");
}

/* ------------------------------------------------------------------ */
/* One line in                                                         */
/* ------------------------------------------------------------------ */

static bool word_starts(const char *line, const char *word,
                        const char **rest)
{
    u32 i = 0;
    while (word[i] && line[i] == word[i]) i++;
    if (word[i]) return false;
    if (line[i] != 0 && line[i] != ' ') return false;
    while (line[i] == ' ') i++;
    if (rest) *rest = line + i;
    return true;
}

void term_line(const char *line)
{
    if (!depth) return;
    while (*line == ' ') line++;

    /* The command echoes first, so the transcript reads as the talk
     * it is. */
    t_puts("> ");
    t_say(line);
    if (!*line) return;

    const char *rest = "";
    if      (word_starts(line, "help", NULL))     cmd_help();
    else if (word_starts(line, "look", &rest))    cmd_look(rest);
    else if (word_starts(line, "where", NULL))    cmd_where();
    else if (word_starts(line, "go", &rest))      cmd_go(rest);
    else if (word_starts(line, "back", NULL))     cmd_back();
    else if (word_starts(line, "home", NULL))     cmd_home();
    else if (word_starts(line, "find", &rest))    cmd_find(rest);
    else if (word_starts(line, "read", &rest))    cmd_read(rest);
    else if (word_starts(line, "write", &rest))   cmd_write(rest);
    else if (word_starts(line, "make", &rest))    cmd_make(rest);
    else if (word_starts(line, "copy", &rest))    cmd_copy(rest);
    else if (word_starts(line, "rename", &rest))  cmd_rename(rest);
    else if (word_starts(line, "let go", &rest))  cmd_letgo(rest);
    else if (word_starts(line, "run", &rest))     cmd_run(rest);
    else if (word_starts(line, "give", &rest))    cmd_give(rest);
    else if (word_starts(line, "end", &rest))     cmd_end(rest);
    else if (word_starts(line, "send", &rest))    cmd_send(rest);
    else if (word_starts(line, "ask", &rest))     cmd_ask(rest);
    else if (word_starts(line, "say", &rest))     cmd_say(rest);
    else if (word_starts(line, "scan", NULL))     cmd_scan();
    else if (word_starts(line, "found", NULL))    cmd_found();
    else if (word_starts(line, "point at", &rest))cmd_point(rest);
    else if (word_starts(line, "journal", NULL))  cmd_journal();
    else if (word_starts(line, "time", NULL))     cmd_time();
    else {
        t_puts("i do not know '");
        t_puts(line);
        t_say("'.  'help' names the words.");
    }
}

/* ------------------------------------------------------------------ */
/* The gathering line, for the screen's view                           */
/* ------------------------------------------------------------------ */

const char *term_gather(u32 *len)
{
    if (len) *len = in_len;
    return in_line;
}

void term_key(char c)
{
    if (c < 0x20 || c > 0x7E) return;
    if (in_len < sizeof(in_line) - 1) in_line[in_len++] = c;
}

void term_rub(void)
{
    if (in_len) in_len--;
}

void term_clear_line(void)
{
    in_len = 0;
}

void term_recall(void)
{
    u32 i = 0;
    while (last_line[i] && i < sizeof(in_line) - 1) {
        in_line[i] = last_line[i];
        i++;
    }
    in_len = i;
}

void term_enter(void)
{
    in_line[in_len] = 0;
    char run[TERM_LINE];
    for (u32 i = 0; i <= in_len; i++) {
        last_line[i] = in_line[i];
        run[i] = in_line[i];
    }
    in_len = 0;
    term_line(run);
}
