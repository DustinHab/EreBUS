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
 * Nothing in here touches a screen. A session's transcript is a byte
 * ring and the commands come in as lines; the shell's terminal view
 * feeds keys into the screen session's gathering line, and a visitor
 * through the door feeds whole lines into a session of their own.
 * Both read their transcripts back the same way.
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

#define TERM_SESSIONS 3
#define TERM_DEPTH    16
#define TERM_OUT      16384
#define TERM_LINE     200
#define NAME_SHOWN    40

struct term_session {
    bool    used;
    object *node[TERM_DEPTH];
    u32     rights[TERM_DEPTH];
    char    names[TERM_DEPTH][NAME_SHOWN];
    u32     depth;

    char out[TERM_OUT];
    u64  out_len;
    u64  total;
    u64  seq;

    char in_line[TERM_LINE];
    u32  in_len;
    char last_line[TERM_LINE];
};

static term_session sessions[TERM_SESSIONS];
static object *troot;
static u32     troot_rights;

/* ------------------------------------------------------------------ */
/* The transcript                                                      */
/* ------------------------------------------------------------------ */

const char *term_out(term_session *s, u64 *len)
{
    if (len) *len = s->out_len;
    return s->out;
}

u64 term_total(term_session *s)    { return s->total; }
u64 term_sequence(term_session *s) { return s->seq; }

static void t_putc(term_session *s, char c)
{
    /* Full: the older half makes room, cut at a line boundary. */
    if (s->out_len + 1 >= sizeof(s->out)) {
        u64 from = s->out_len / 2;
        while (from < s->out_len && s->out[from] != '\n') from++;
        if (from < s->out_len) from++;
        memmove(s->out, s->out + from, s->out_len - from);
        s->out_len -= from;
    }
    s->out[s->out_len++] = c;
    s->total++;
}

static void t_puts(term_session *s, const char *str)
{
    while (*str) t_putc(s, *str++);
}

static void t_dec(term_session *s, u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) t_putc(s, d[--n]);
}

static void t_end(term_session *s)
{
    t_putc(s, '\n');
    s->seq++;
}

static void t_say(term_session *s, const char *str)
{
    t_puts(s, str);
    t_end(s);
}

/* ------------------------------------------------------------------ */
/* Sessions and standpoints                                            */
/* ------------------------------------------------------------------ */

static object *focus(term_session *s)        { return s->node[s->depth - 1]; }
static u32     focus_rights(term_session *s) { return s->rights[s->depth - 1]; }

static void session_begin(term_session *s)
{
    memset(s, 0, sizeof(*s));
    s->used = true;
    obj_retain(troot);
    s->node[0] = troot;
    s->rights[0] = troot_rights;
    const char *h = "home";
    u32 i = 0;
    while (h[i]) { s->names[0][i] = h[i]; i++; }
    s->names[0][i] = 0;
    s->depth = 1;
    t_say(s, "the terminal.  'help' names the words it knows.");
}

void term_init(object *root, u32 root_rights)
{
    if (!root) return;
    troot = root;
    troot_rights = root_rights;
    session_begin(&sessions[0]);
}

term_session *term_screen(void) { return &sessions[0]; }

term_session *term_open(void)
{
    if (!troot) return NULL;
    for (u32 i = 1; i < TERM_SESSIONS; i++)
        if (!sessions[i].used) {
            session_begin(&sessions[i]);
            return &sessions[i];
        }
    return NULL;
}

void term_close(term_session *s)
{
    if (!s || s == &sessions[0] || !s->used) return;
    while (s->depth > 0) {
        s->depth--;
        obj_release(s->node[s->depth]);
        s->node[s->depth] = NULL;
    }
    s->used = false;
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
 * object's own claimed name, else nothing. Points into the holder's
 * slot table, which any growing of that table frees -- copy it out
 * before laying anything new in the holder. */
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

/* ------------------------------------------------------------------ */
/* Finding what a word names                                           */
/* ------------------------------------------------------------------ */

/* A number names a slot by count; anything else is matched against
 * the names the slots show. Exact and whole, no guessing. */
static i64 slot_named(term_session *s, const char *what)
{
    object *f = focus(s);
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

static bool resolve(term_session *s, const char *what, spot *sp)
{
    if (!what[0]) {
        sp->o = focus(s);
        sp->r = focus_rights(s);
        sp->nm = s->names[s->depth - 1];
        sp->slot = -1;
        return true;
    }
    i64 i = slot_named(s, what);
    if (i < 0) {
        t_puts(s, "nothing here is called ");
        t_puts(s, what);
        t_say(s, ".  'look' shows the names.");
        return false;
    }
    sp->o = obj_get_slot(focus(s), (u64)i);
    sp->r = focus_rights(s) & obj_slot_rights(focus(s), (u64)i);
    sp->nm = shown_name(focus(s), (u64)i);
    sp->slot = i;
    if (sp->r == 0) {
        t_say(s, "that reference grants nothing in your hand.");
        return false;
    }
    return true;
}

/* Splits "<name> to <name>" at the last joiner, so names keep their
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

/* Lays a reference into the focused holder, first hole or a grown
 * one. The slot takes its own reference. Answers the slot, or -1
 * when there is no room. */
static i64 lay_here(term_session *s, object *made, u32 r, const char *nm)
{
    object *f = focus(s);
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

static void describe(term_session *s, object *o, u32 r, const char *nm)
{
    char rw[4];
    rights_word(r, rw);

    t_puts(s, nm[0] ? nm : "(unnamed)");
    t_puts(s, "  ");
    t_puts(s, kind_word(obj_type(o)));
    t_puts(s, "  ");
    t_puts(s, rw);
    t_puts(s, "  ");
    if (obj_type(o) == TYPE_LIST) {
        u64 filled = 0, n = obj_slots(o);
        for (u64 i = 0; i < n; i++) if (obj_get_slot(o, i)) filled++;
        t_dec(s, filled);
        t_puts(s, filled == 1 ? " thing" : " things");
    } else if (obj_type(o) == TYPE_TEXT) {
        t_dec(s, text_len((const u8 *)obj_data(o), obj_size(o)));
        t_puts(s, " letters");
    } else if (obj_type(o) == TYPE_PROGRAM) {
        t_puts(s, proc_is_running(o) ? "running" : "ended");
    } else {
        t_dec(s, obj_size(o));
        t_puts(s, " bytes");
    }
    t_end(s);

    u64 n = obj_slots(o);
    for (u64 i = 0; i < n; i++) {
        object *t = obj_get_slot(o, i);
        if (!t) continue;
        char sr[4];
        rights_word(r & obj_slot_rights(o, i), sr);
        t_puts(s, "  ");
        t_dec(s, i);
        t_puts(s, "  ");
        t_puts(s, sr);
        t_puts(s, "  ");
        const char *snm = shown_name(o, i);
        t_puts(s, snm[0] ? snm : "(unnamed)");
        t_puts(s, "  ");
        t_puts(s, kind_word(obj_type(t)));
        t_end(s);
    }
}

static void cmd_look(term_session *s, const char *what)
{
    spot sp;
    if (!resolve(s, what, &sp)) return;
    describe(s, sp.o, sp.r, sp.nm);
}

static void cmd_where(term_session *s)
{
    for (u32 i = 0; i < s->depth; i++) {
        if (i) t_puts(s, " > ");
        t_puts(s, s->names[i]);
    }
    t_end(s);
}

static void cmd_go(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "go where?  'look' shows the names."); return; }
    if (s->depth >= TERM_DEPTH) { t_say(s, "deep enough; 'back' first."); return; }

    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (sp.slot < 0) return;

    obj_retain(sp.o);
    s->node[s->depth] = sp.o;
    s->rights[s->depth] = sp.r;
    u32 i = 0;
    while (sp.nm[i] && i < NAME_SHOWN - 1) { s->names[s->depth][i] = sp.nm[i]; i++; }
    s->names[s->depth][i] = 0;
    s->depth++;
    cmd_look(s, "");
}

static void cmd_back(term_session *s)
{
    if (s->depth <= 1) { t_say(s, "this is the beginning."); return; }
    s->depth--;
    obj_release(s->node[s->depth]);
    s->node[s->depth] = NULL;
    cmd_where(s);
}

static void cmd_home(term_session *s)
{
    while (s->depth > 1) {
        s->depth--;
        obj_release(s->node[s->depth]);
        s->node[s->depth] = NULL;
    }
    t_say(s, "home.");
}

/* ------------------------------------------------------------------ */
/* Reading and writing                                                 */
/* ------------------------------------------------------------------ */

static void cmd_read(term_session *s, const char *what)
{
    spot sp;
    if (!resolve(s, what, &sp)) return;
    object *f = sp.o;
    type_id t = obj_type(f);

    if (t == TYPE_TEXT) {
        const u8 *d = (const u8 *)obj_data(f);
        u64 len = text_len(d, obj_size(f));
        u64 show = len > 2000 ? 2000 : len;
        for (u64 i = 0; i < show; i++)
            t_putc(s, d[i] >= 0x20 || d[i] == '\n' ? (char)d[i] : ' ');
        if (show && s->out[s->out_len - 1] != '\n') t_putc(s, '\n');
        if (len > show) {
            t_puts(s, "...and ");
            t_dec(s, len - show);
            t_say(s, " more letters");
        }
        if (len == 0) t_say(s, "(empty)");
        s->seq++;
        return;
    }
    if (t == TYPE_BYTES) {
        const u8 *d = (const u8 *)obj_data(f);
        u64 size = obj_size(f);
        t_dec(s, size);
        t_say(s, " bytes; the first rows of them:");
        static const char hx[] = "0123456789abcdef";
        for (u64 row = 0; row < 8 && row * 16 < size; row++) {
            t_puts(s, "  ");
            for (u64 c = 0; c < 16 && row * 16 + c < size; c++) {
                u8 b = d[row * 16 + c];
                t_putc(s, hx[b >> 4]);
                t_putc(s, hx[b & 15]);
                t_putc(s, ' ');
            }
            t_end(s);
        }
        return;
    }
    if (t == TYPE_PICTURE) {
        const u8 *d = (const u8 *)obj_data(f);
        u32 w = 0, h = 0;
        for (u32 i = 0; i < 4; i++) w |= (u32)d[i] << (i * 8);
        for (u32 i = 0; i < 4; i++) h |= (u32)d[4 + i] << (i * 8);
        t_puts(s, "a picture, ");
        t_dec(s, w);
        t_puts(s, " by ");
        t_dec(s, h);
        t_say(s, ".  the screen's picture lens shows it.");
        return;
    }
    if (t == TYPE_PROGRAM) {
        t_puts(s, "a program, ");
        t_puts(s, proc_is_running(f) ? "running" : "ended");
        t_say(s, ".  'look' shows what it holds.");
        return;
    }
    t_say(s, "'look' is the way to read this kind.");
}

static void cmd_write(term_session *s, const char *words)
{
    object *f = focus(s);
    if (obj_type(f) != TYPE_TEXT) { t_say(s, "only a text takes writing; 'go' to one first."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "this one is read-only in your hand."); return; }
    if (!words[0]) { t_say(s, "write what?"); return; }

    u8 *d = (u8 *)obj_data(f);
    u64 size = obj_size(f);
    u64 len = text_len(d, size);
    u64 need = 0;
    while (words[need]) need++;
    if (len + need + 2 >= size) { t_say(s, "it has no room left."); return; }

    if (len && d[len - 1] != '\n') d[len++] = '\n';
    for (u64 i = 0; i < need; i++) d[len++] = (u8)words[i];
    d[len++] = '\n';
    d[len] = 0;
    obj_touch(f);

    /* A line written into the settings means something the moment
     * it is written, here as on the screen. */
    if (f == settings_object()) settings_apply();
    t_say(s, "written.");
}

/* ------------------------------------------------------------------ */
/* Making, shaping, letting go                                         */
/* ------------------------------------------------------------------ */

static void cmd_make(term_session *s, const char *what)
{
    const char *nm;
    type_id t;
    if      (word_starts(what, "text", &nm)) t = TYPE_TEXT;
    else if (word_starts(what, "list", &nm)) t = TYPE_LIST;
    else { t_say(s, "make text <name>, or make list <name>."); return; }
    if (!*nm) { t_say(s, "name it."); return; }

    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "you may not lay things in here."); return; }

    object *made = obj_create(t, t == TYPE_TEXT ? 3000 : 0,
                              t == TYPE_LIST ? 4 : 0);
    if (!made) { t_say(s, "nothing came of it; memory is short."); return; }
    i64 at = lay_here(s, made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);                /* the slot holds it now */
    if (at < 0) { t_say(s, "no room for another reference here."); return; }

    t_puts(s, nm);
    t_puts(s, "  lies here now, slot ");
    t_dec(s, (u64)at);
    t_end(s);
}

static void cmd_copy(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "copy which?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (sp.slot < 0) return;
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "the copy would lie here, and you may not lay things in here."); return; }

    type_id t = obj_type(sp.o);
    object *made = NULL;
    if (t == TYPE_LIST) {
        u64 n = obj_slots(sp.o);
        made = obj_create(TYPE_LIST, 0, n ? n : 4);
        if (made) for (u64 i = 0; i < n; i++) {
            object *m = obj_get_slot(sp.o, i);
            if (!m) continue;
            obj_set_slot(made, i, m, obj_slot_rights(sp.o, i));
            obj_set_slot_name(made, i, obj_slot_name(sp.o, i));
        }
    } else if (t == TYPE_TEXT || t == TYPE_BYTES || t == TYPE_PICTURE) {
        made = obj_create(t, obj_size(sp.o), 0);
        if (made && obj_data(sp.o))
            memcpy(obj_data(made), obj_data(sp.o), obj_size(sp.o));
    } else {
        t_say(s, "this kind cannot be copied.");
        return;
    }
    if (!made) { t_say(s, "nothing came of it; memory is short."); return; }

    char nm[NAME_SHOWN];
    u32 n = 0;
    while (sp.nm[n] && n < 19) { nm[n] = sp.nm[n]; n++; }
    const char *tail = " copy";
    for (u32 i = 0; tail[i]; i++) nm[n++] = tail[i];
    nm[n] = 0;

    i64 at = lay_here(s, made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);
    if (at < 0) { t_say(s, "no room to lay the copy here."); return; }
    t_puts(s, nm);
    t_say(s, "  lies beside it.");
}

static void cmd_rename(term_session *s, const char *rest)
{
    char a[TERM_LINE], b[TERM_LINE];
    if (!split_at(rest, " to ", a, b)) {
        t_say(s, "rename <name> to <new name>.");
        return;
    }
    spot sp;
    if (!resolve(s, a, &sp)) return;
    if (sp.slot < 0) return;
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "the name lives on this holder, and you may not change it."); return; }
    obj_set_slot_name(focus(s), (u64)sp.slot, b);
    obj_touch(focus(s));
    t_puts(s, a);
    t_puts(s, "  is now called  ");
    t_say(s, b);
}

/* Letting go steps the reference into the bin first, petname and
 * rights along; inside the bin, or without a writable home, it is
 * final -- the shell's manner, word for word. */
static void cmd_letgo(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "let go of which?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (sp.slot < 0) { t_say(s, "stand beside it, not on it: 'back' first."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "you may not take things out of here."); return; }

    /* The name goes with it; copied before anything grows. */
    char nm[NAME_SHOWN];
    u32 n = 0;
    while (sp.nm[n] && n < NAME_SHOWN - 1) { nm[n] = sp.nm[n]; n++; }
    nm[n] = 0;
    u32 had = obj_slot_rights(focus(s), (u64)sp.slot);

    if (obj_type(focus(s)) == TYPE_PROGRAM)
        proc_revoke(focus(s), sp.o);

    object *home = s->node[0];
    object *bin = NULL;
    if (s->rights[0] & CAP_WRITE) {
        for (u64 i = 0; i < obj_slots(home); i++) {
            object *b = obj_get_slot(home, i);
            const char *bn = obj_slot_name(home, i);
            if (b && bn && strcmp(bn, "bin") == 0 && obj_type(b) == TYPE_LIST)
                { bin = b; break; }
        }
    }

    bool final = focus(s) == bin || sp.o == bin || !(s->rights[0] & CAP_WRITE);

    if (!final && !bin) {
        object *made = obj_create(TYPE_LIST, 0, 4);
        if (made) {
            obj_set_name(made, "bin");
            u64 hn = obj_slots(home), at = hn;
            for (u64 i = 0; i < hn; i++)
                if (!obj_get_slot(home, i)) { at = i; break; }
            if (at < hn || obj_grow_slots(home, hn + 1)) {
                obj_set_slot(home, at, made, CAP_READ | CAP_WRITE);
                obj_set_slot_name(home, at, "bin");
                bin = made;
            }
            obj_release(made);
        }
    }

    if (!final && bin) {
        u64 bn = obj_slots(bin), at = bn;
        for (u64 i = 0; i < bn; i++)
            if (!obj_get_slot(bin, i)) { at = i; break; }
        if (at == bn && !obj_grow_slots(bin, bn + 1)) final = true;
        else {
            obj_set_slot(bin, at, sp.o, had);
            obj_set_slot_name(bin, at, nm);
            obj_touch(bin);
        }
    }

    obj_set_slot(focus(s), (u64)sp.slot, NULL, 0);
    obj_set_slot_name(focus(s), (u64)sp.slot, NULL);
    obj_touch(focus(s));
    t_say(s, final ? "let go, for good." : "it lies in the bin now.");
}

/* ------------------------------------------------------------------ */
/* Programs                                                            */
/* ------------------------------------------------------------------ */

static void cmd_run(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "run which text?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "only a text can run."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "the running one would lie here, and you may not lay things in here."); return; }

    /* The name is copied out first: laying the program here can grow
     * the slot table, and sp.nm points into the old one -- freed the
     * moment it grows. */
    char nm[NAME_SHOWN];
    u32 n = 0;
    while (sp.nm[n] && n < NAME_SHOWN - 1) { nm[n] = sp.nm[n]; n++; }
    nm[n] = 0;

    /* runner_launch lends the program object -- the live table holds
     * the reference. The slot below takes its own; releasing here
     * would give away what was never ours, and the reaper would then
     * free the object under the slot at the program's end. */
    object *prog = runner_launch(sp.o);
    if (!prog) { t_say(s, "it would not start."); return; }
    i64 at = lay_here(s, prog, CAP_READ | CAP_GRANT, nm);
    if (at < 0) { t_say(s, "it runs, but there was no room to lay it here."); return; }
    t_say(s, "it runs; the journal carries what it says.");
}

static void cmd_give(term_session *s, const char *rest)
{
    char a[TERM_LINE], b[TERM_LINE];
    if (!split_at(rest, " to ", a, b)) {
        t_say(s, "give <name> to <program>.");
        return;
    }
    spot thing, prog;
    if (!resolve(s, a, &thing) || !resolve(s, b, &prog)) return;
    if (obj_type(prog.o) != TYPE_PROGRAM || !proc_is_running(prog.o)) {
        t_say(s, "only a running program can be given to.");
        return;
    }
    if (!(prog.r & CAP_GRANT)) { t_say(s, "you may not give to that program."); return; }

    if (!proc_grant(prog.o, thing.o, thing.r)) {
        t_say(s, "it could not be handed over.");
        return;
    }
    t_puts(s, prog.nm);
    t_say(s, "  holds it now, with what you held.");
}

static void cmd_end(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "end which program?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_PROGRAM) { t_say(s, "only a program can be ended."); return; }
    if (!(sp.r & CAP_GRANT)) { t_say(s, "you may not end that one."); return; }
    if (!proc_end(sp.o)) { t_say(s, "it was not running."); return; }
    journal_says("system", "a program was ended by hand");
    t_say(s, "ended.  it finishes at its next step into the kernel.");
}

/* ------------------------------------------------------------------ */
/* The wire                                                            */
/* ------------------------------------------------------------------ */

static void cmd_send(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "send which?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that, so you may not send it."); return; }
    if (pipe_post(sp.o))
        t_say(s, "on its way to the peer.");
    else
        t_say(s, "the pipe would not take it.  is a peer named?  'scan' and 'point at' set one.");
}

static void cmd_ask(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "ask with which task?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "a task is a text."); return; }
    if (pipe_ask(sp.o, (sp.r & CAP_WRITE) != 0))
        t_say(s, "the desk has it.  the answer lands in the task itself, or in arrivals.");
    else
        t_say(s, "the desk would not take it.");
}

static void cmd_say(term_session *s, const char *words)
{
    if (!words[0]) { t_say(s, "say what?"); return; }
    if (pipe_say(words))
        t_say(s, "said; it stands on the line.");
    else
        t_say(s, "nobody is on the line, and no peer is named in the settings.");
}

static void cmd_scan(term_session *s)
{
    pipe_scan();
    t_say(s, "the call is out.  'found' shows who answered.");
}

static void put_ip(term_session *s, const u8 ip[4])
{
    for (u32 i = 0; i < 4; i++) {
        if (i) t_putc(s, '.');
        t_dec(s, ip[i]);
    }
}

static void cmd_found(term_session *s)
{
    u32 n = pipe_found_count();
    if (n == 0) {
        t_say(s, pipe_scanning() ? "no answers yet; the call is still out."
                                 : "nobody has answered.  'scan' calls again.");
        return;
    }
    for (u32 i = 0; i < n; i++) {
        u8 ip[4];
        char nm[24];
        bool works;
        u32 mib;
        if (!pipe_found_at(i, ip, nm, &works, &mib)) continue;
        t_puts(s, "  ");
        put_ip(s, ip);
        t_puts(s, "  ");
        t_puts(s, nm[0] ? nm : "(no name)");
        if (works) {
            t_puts(s, "  takes work, ");
            t_dec(s, mib);
            t_puts(s, "M free");
        }
        t_end(s);
    }
}

/* Points the pipe at a machine: by the name it answered the scan
 * with, or by its address. Written into the settings as the peer
 * line, the same honest way a click on a found machine writes it --
 * no hidden switch, just the sentence. */
static void cmd_point(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "point at whom?  a found name, or an address."); return; }

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

    if (!have) { t_say(s, "that names no machine i can see."); return; }

    object *st = settings_object();
    if (!st) { t_say(s, "no settings stand."); return; }
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
    if (len + at + 2 >= size) { t_say(s, "the settings page has no room left."); return; }
    if (len && d[len - 1] != '\n') d[len++] = '\n';
    for (u32 i = 0; i < at; i++) d[len++] = (u8)line[i];
    d[len++] = '\n';
    d[len] = 0;
    obj_touch(st);
    settings_apply();

    t_puts(s, "the pipe points at ");
    put_ip(s, ip);
    t_say(s, " now; send, ask and say reach it.");
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
static void cmd_find(term_session *s, const char *words)
{
    if (!words[0]) { t_say(s, "find what?"); return; }

    static object *seen[FIND_MOST];
    static u32 parent[FIND_MOST];
    static char label[FIND_MOST][24];
    u32 count = 0, hits = 0;
    bool full = false;

    seen[0] = s->node[0];
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
                u32 chain[TERM_DEPTH];
                u32 cn = 0, w = me;
                while (w && cn < TERM_DEPTH) { chain[cn++] = w; w = parent[w]; }
                t_puts(s, "  home");
                while (cn) {
                    t_puts(s, " > ");
                    t_puts(s, label[chain[--cn]][0]
                              ? label[chain[cn]] : "(unnamed)");
                }
                t_end(s);
            }
        }
        if (full) break;
    }

    if (hits == 0) t_say(s, "nothing holds those words.");
    if (hits >= FIND_HITS) t_say(s, "...and maybe more; the first sixteen are shown.");
    if (full) t_say(s, "(the walk was cut short; the graph is larger than the search.)");
}

/* ------------------------------------------------------------------ */
/* Small things                                                        */
/* ------------------------------------------------------------------ */

static void cmd_journal(term_session *s)
{
    object *j = journal_object();
    const u8 *d = j ? (const u8 *)obj_data(j) : NULL;
    if (!d) { t_say(s, "no journal stands."); return; }
    u64 len = text_len(d, obj_size(j));
    if (len == 0) { t_say(s, "nothing has happened yet."); return; }

    u64 from = len, lines = 0;
    while (from > 0 && lines < 12) {
        from--;
        if (from && d[from - 1] == '\n') lines++;
    }
    for (u64 i = from; i < len; i++)
        t_putc(s, d[i] >= 0x20 || d[i] == '\n' ? (char)d[i] : ' ');
    if (s->out_len && s->out[s->out_len - 1] != '\n') t_putc(s, '\n');
    s->seq++;
}

static void cmd_time(term_session *s)
{
    u32 h, m, sec;
    time_wall(&h, &m, &sec);
    if (h < 10) t_putc(s, '0');
    t_dec(s, h);
    t_putc(s, ':');
    if (m < 10) t_putc(s, '0');
    t_dec(s, m);
    t_putc(s, ':');
    if (sec < 10) t_putc(s, '0');
    t_dec(s, sec);
    t_puts(s, "  --  up ");
    t_dec(s, time_ns() / 1000000000ULL);
    t_say(s, " seconds");
}

static void cmd_help(term_session *s)
{
    t_say(s, "one shape, always: a verb, a name, and 'to' or 'at' when");
    t_say(s, "two things meet.  names may have spaces; numbers count slots.");
    t_end(s);
    t_say(s, "looking around");
    t_say(s, "  look [name]      what stands here, or what that points at");
    t_say(s, "  go <name>        follow a reference");
    t_say(s, "  back             one step back;  home  returns to the start");
    t_say(s, "  where            the walk so far");
    t_say(s, "  find <words>     search names and texts, everywhere you reach");
    t_end(s);
    t_say(s, "things");
    t_say(s, "  read [name]      the thing itself: letters, bytes, size");
    t_say(s, "  write <words>    add a line to the text you stand on");
    t_say(s, "  make text <name>     a fresh text, laid in here");
    t_say(s, "  make list <name>     a fresh list, laid in here");
    t_say(s, "  copy <name>      a copy, laid beside it");
    t_say(s, "  rename <name> to <new name>");
    t_say(s, "  let go <name>    into the bin; in the bin, for good");
    t_end(s);
    t_say(s, "programs");
    t_say(s, "  run <name>       run that text as a program, here");
    t_say(s, "  give <name> to <program>   hand it a reference");
    t_say(s, "  end <name>       end a running program");
    t_end(s);
    t_say(s, "the other machines");
    t_say(s, "  scan             call out: who else is on the wire?");
    t_say(s, "  found            who answered");
    t_say(s, "  point at <name or address>   choose the peer");
    t_say(s, "  send <name>      carry a thing to the peer");
    t_say(s, "  ask <name>       have the machines work a task text");
    t_say(s, "  say <words>      speak on the line");
    t_end(s);
    t_say(s, "the machine");
    t_say(s, "  journal          the last things that happened");
    t_say(s, "  time             the wall clock, and how long it has run");
}

/* ------------------------------------------------------------------ */
/* One line in                                                         */
/* ------------------------------------------------------------------ */

void term_line(term_session *s, const char *line)
{
    if (!s || !s->used || !s->depth) return;
    while (*line == ' ') line++;

    /* The command echoes first, so the transcript reads as the talk
     * it is. */
    t_puts(s, "> ");
    t_say(s, line);
    if (!*line) return;

    const char *rest = "";
    if      (word_starts(line, "help", NULL))     cmd_help(s);
    else if (word_starts(line, "look", &rest))    cmd_look(s, rest);
    else if (word_starts(line, "where", NULL))    cmd_where(s);
    else if (word_starts(line, "go", &rest))      cmd_go(s, rest);
    else if (word_starts(line, "back", NULL))     cmd_back(s);
    else if (word_starts(line, "home", NULL))     cmd_home(s);
    else if (word_starts(line, "find", &rest))    cmd_find(s, rest);
    else if (word_starts(line, "read", &rest))    cmd_read(s, rest);
    else if (word_starts(line, "write", &rest))   cmd_write(s, rest);
    else if (word_starts(line, "make", &rest))    cmd_make(s, rest);
    else if (word_starts(line, "copy", &rest))    cmd_copy(s, rest);
    else if (word_starts(line, "rename", &rest))  cmd_rename(s, rest);
    else if (word_starts(line, "let go", &rest))  cmd_letgo(s, rest);
    else if (word_starts(line, "run", &rest))     cmd_run(s, rest);
    else if (word_starts(line, "give", &rest))    cmd_give(s, rest);
    else if (word_starts(line, "end", &rest))     cmd_end(s, rest);
    else if (word_starts(line, "send", &rest))    cmd_send(s, rest);
    else if (word_starts(line, "ask", &rest))     cmd_ask(s, rest);
    else if (word_starts(line, "say", &rest))     cmd_say(s, rest);
    else if (word_starts(line, "scan", NULL))     cmd_scan(s);
    else if (word_starts(line, "found", NULL))    cmd_found(s);
    else if (word_starts(line, "point at", &rest))cmd_point(s, rest);
    else if (word_starts(line, "journal", NULL))  cmd_journal(s);
    else if (word_starts(line, "time", NULL))     cmd_time(s);
    else {
        t_puts(s, "i do not know '");
        t_puts(s, line);
        t_say(s, "'.  'help' names the words.");
    }
}

/* ------------------------------------------------------------------ */
/* The gathering line, for the screen's view                           */
/* ------------------------------------------------------------------ */

const char *term_gather(term_session *s, u32 *len)
{
    if (len) *len = s->in_len;
    return s->in_line;
}

void term_key(term_session *s, char c)
{
    if (c < 0x20 || c > 0x7E) return;
    if (s->in_len < sizeof(s->in_line) - 1) s->in_line[s->in_len++] = c;
}

void term_rub(term_session *s)
{
    if (s->in_len) s->in_len--;
}

void term_clear_line(term_session *s)
{
    s->in_len = 0;
}

void term_recall(term_session *s)
{
    u32 i = 0;
    while (s->last_line[i] && i < sizeof(s->in_line) - 1) {
        s->in_line[i] = s->last_line[i];
        i++;
    }
    s->in_len = i;
}

void term_enter(term_session *s)
{
    s->in_line[s->in_len] = 0;
    char run[TERM_LINE];
    for (u32 i = 0; i <= s->in_len; i++) {
        s->last_line[i] = s->in_line[i];
        run[i] = s->in_line[i];
    }
    s->in_len = 0;
    term_line(s, run);
}
