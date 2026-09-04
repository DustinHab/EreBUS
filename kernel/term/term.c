/*
 * term.c -- the terminal: the system spoken to in lines.
 * - a session is a standpoint in the graph with the rights the references on the way granted; "go" follows visible references only
 * - grammar: verb, name, optionally "to"/"at" and a second name; names may contain spaces; numbers count slots; no flags
 * - no screen here: the transcript is a byte ring; lines come from the shell's terminal view or from a door session
 * - "receive N bytes as NAME" switches the session to taking raw bytes into a new text (term_taking / term_take_bytes)
 */
#include <eb/term.h>
#include <eb/cap.h>
#include <eb/proc.h>
#include <eb/journal.h>
#include <eb/pipe.h>
#include <eb/standard.h>
#include <eb/settings.h>
#include <eb/asm.h>
#include <eb/cc.h>
#include <eb/ld.h>
#include <eb/lang.h>
#include <eb/fat.h>
#include <eb/settle.h>
#include <eb/version.h>
#include <eb/nodes.h>
#include <eb/wifi.h>
#include <eb/net.h>
#include <eb/fmt.h>
#include <eb/time.h>
#include <eb/thread.h>
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

    /* The next line is a passphrase for this network: not echoed,
     * not kept as the last line. */
    bool secret;
    char secret_for[33];

    /* Bytes coming in whole: 'receive <n> bytes as <name>' makes the
     * text and then the next n bytes of this session are its contents,
     * not words. The door feeds them straight in. */
    object *take_o;
    u64     take_left, take_size;
    char    take_name[NAME_SHOWN];
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
    /* A visitor who leaves with bytes still owed leaves a text half
     * filled; the text stays where it was laid, the hold on it goes. */
    if (s->take_o) { obj_release(s->take_o); s->take_o = NULL; s->take_left = 0; }
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

/* receive <n> bytes as <name>: a text made here, and then the next n
 * bytes of this session are its contents rather than words.
 *
 * This is how a file comes in through the door. A machine with no
 * exchange disk in it -- one whose disks are all its own or somebody
 * else's -- had no way to be handed a text except a line at a time,
 * and a kernel's sources are forty thousand lines. The count comes
 * first so the text can be made the right size and so the session
 * knows exactly when words begin again; nothing inside the bytes is
 * looked at, whatever it happens to say. */
#define RECEIVE_MAX (8u << 20)

static void cmd_receive(term_session *s, const char *what)
{
    const char *p = what;
    while (*p == ' ') p++;
    if (*p < '0' || *p > '9') { t_say(s, "receive <n> bytes as <name>: the count first."); return; }
    u64 n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (u64)(*p++ - '0');
    while (*p == ' ') p++;
    if (!(p[0] == 'b' && p[1] == 'y' && p[2] == 't' && p[3] == 'e' && p[4] == 's')) {
        t_say(s, "receive <n> bytes as <name>: say 'bytes' after the count."); return;
    }
    p += 5;
    while (*p == ' ') p++;
    if (!(p[0] == 'a' && p[1] == 's' && p[2] == ' ')) { t_say(s, "receive <n> bytes as <name>: 'as' and then the name."); return; }
    p += 3;
    while (*p == ' ') p++;
    if (!*p) { t_say(s, "name it."); return; }
    if (n == 0 || n > RECEIVE_MAX) { t_say(s, "between one byte and eight million, please."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "you may not lay things in here."); return; }
    if (s->take_o) { t_say(s, "bytes are still coming for the last one."); return; }

    object *made = obj_create(TYPE_TEXT, n + 1, 0);
    if (!made) { t_say(s, "nothing came of it; memory is short."); return; }
    memset(obj_data(made), 0, n + 1);
    i64 at = lay_here(s, made, CAP_READ | CAP_WRITE | CAP_GRANT, p);
    if (at < 0) { obj_release(made); t_say(s, "no room for another reference here."); return; }

    s->take_o = made;                 /* held until the last byte is in */
    s->take_left = s->take_size = n;
    u32 k = 0;
    while (p[k] && k < NAME_SHOWN - 1) { s->take_name[k] = p[k]; k++; }
    s->take_name[k] = 0;

    t_puts(s, "send ");
    t_dec(s, n);
    t_say(s, " bytes now.");
}

bool term_taking(term_session *s)
{
    return s && s->used && s->take_o != NULL;
}

u32 term_take_bytes(term_session *s, const u8 *d, u32 n)
{
    if (!s || !s->take_o || !n) return 0;
    u64 done = s->take_size - s->take_left;
    u32 take = (u64)n < s->take_left ? n : (u32)s->take_left;
    u8 *dst = (u8 *)obj_data(s->take_o);
    memcpy(dst + done, d, take);
    s->take_left -= take;
    if (s->take_left == 0) {
        dst[s->take_size] = 0;
        obj_touch(s->take_o);
        t_puts(s, s->take_name);
        t_puts(s, "  lies here now, ");
        t_dec(s, s->take_size);
        t_say(s, " bytes.");
        obj_release(s->take_o);
        s->take_o = NULL;
    }
    return take;
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

/* A text becomes a program through the assembler: the image lies
 * beside it, named after it, ready to run. What went wrong is said
 * with its line. */
/* A name that ends in .S is a text in the gnu dialect, the way the
 * kernel's own assembly files are written. */
static bool gnu_named(const char *nm)
{
    u32 n = (u32)strlen(nm);
    return n > 2 && nm[n - 2] == '.' && nm[n - 1] == 'S';
}

/* Lays what a text became -- an image, or an object that waits for
 * other texts' names -- here, named after the text, and says so. */
static void lay_made(term_session *s, const char *base, const u8 *bytes, u64 len, u32 kind)
{
    object *made = obj_create(TYPE_BYTES, len, 0);
    if (!made) { t_say(s, "nothing came of it; memory is short."); return; }
    memcpy(obj_data(made), bytes, len);
    char nm[NAME_SHOWN + 8];
    u32 n = 0;
    while (base[n] && n < 19) { nm[n] = base[n]; n++; }
    const char *tail = kind == LANG_IMAGE ? " code" : " object";
    for (u32 i = 0; tail[i]; i++) nm[n++] = tail[i];
    nm[n] = 0;
    i64 at = lay_here(s, made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);
    if (at < 0) { t_say(s, "no room to lay it here."); return; }
    t_puts(s, nm);
    t_puts(s, "  lies beside it: ");
    t_dec(s, len);
    if (kind == LANG_IMAGE) { t_say(s, " bytes of image.  'run' it."); return; }
    char wants[120];
    ld_object_wants(bytes, len, wants, sizeof(wants));
    t_say(s, " bytes of object.  it waits for other texts' names:");
    t_puts(s, "  ");
    t_say(s, wants);
    t_say(s, "  'link' joins objects; 'build' makes them from a list of texts.");
}

static void cmd_assemble(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "assemble which text?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "only a text can be assembled."); return; }
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "the image would lie here, and you may not lay things in here."); return; }
    if (term_building()) { t_say(s, "a build is running; the tools are busy until it is done."); return; }

    u8 *out = lang_out_buffer();
    if (!out) { t_say(s, "there is no room for the tools' tables."); return; }
    char err[120];
    u32 kind = 0;
    const u8 *src = (const u8 *)obj_data(sp.o);
    u64 slen = text_len(src, obj_size(sp.o));
    i64 got = lang_build_text(src, slen, gnu_named(sp.nm) || gnu_looks(src, slen),
                              out, LANG_OUT_MAX, &kind, err, sizeof(err));
    if (got < 0) { t_say(s, err); return; }
    lay_made(s, sp.nm, out, (u64)got, kind);
}

/* #include "name" reaches the texts lying beside the source: the
 * holder the standpoint is on, searched by petname. */
static bool find_beside(void *ctx, const char *name, const u8 **text, u64 *len)
{
    object *holder = (object *)ctx;
    u64 n = obj_slots(holder);
    for (u64 i = 0; i < n; i++) {
        object *t = obj_get_slot(holder, i);
        if (!t || obj_type(t) != TYPE_TEXT) continue;
        const char *nm = shown_name(holder, i);
        /* <eb/types.h> asks for types.h: the way there is not a thing here */
        const char *want = name;
        for (const char *q = name; *q; q++) if (*q == '/') want = q + 1;
        u32 j = 0;
        while (nm[j] && want[j] && low(nm[j]) == low(want[j])) j++;
        if (nm[j] || want[j]) continue;
        *text = (const u8 *)obj_data(t);
        *len = text_len(*text, obj_size(t));
        return true;
    }
    return false;
}

/* A text of c becomes a text of assembly beside it, and that becomes
 * an image beside it too. The assembly stays: what the compiler
 * made is there to be read. */
static void cmd_compile(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "compile which text?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "only a text can be compiled."); return; }
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "what it makes would lie here, and you may not lay things in here."); return; }
    if (term_building()) { t_say(s, "a build is running; the tools are busy until it is done."); return; }

    char base[NAME_SHOWN];
    u32 n = 0;
    while (sp.nm[n] && n < 19) { base[n] = sp.nm[n]; n++; }
    base[n] = 0;

    char *text = lang_text_buffer();
    u8 *out = lang_out_buffer();
    if (!text || !out) { t_say(s, "there is no room for the tools' tables."); return; }
    char err[120];
    const u8 *src = (const u8 *)obj_data(sp.o);
    i64 got = cc_compile(src, text_len(src, obj_size(sp.o)), base,
                         find_beside, focus(s), text, LANG_TEXT_MAX,
                         err, sizeof(err));
    if (got < 0) { t_say(s, err); return; }

    object *made = obj_create(TYPE_TEXT, (u64)got + 16, 0);
    if (!made) { t_say(s, "nothing came of it; memory is short."); return; }
    memcpy(obj_data(made), text, (u64)got);
    char nm[NAME_SHOWN];
    n = 0;
    while (base[n]) { nm[n] = base[n]; n++; }
    const char *t1 = " asm";
    for (u32 i = 0; t1[i]; i++) nm[n++] = t1[i];
    nm[n] = 0;
    i64 at = lay_here(s, made, CAP_READ | CAP_WRITE | CAP_GRANT, nm);
    obj_release(made);
    if (at < 0) { t_say(s, "no room to lay the assembly here."); return; }
    t_puts(s, nm);
    t_puts(s, "  lies beside it: ");
    t_dec(s, (u64)got);
    t_say(s, " letters of assembly.");

    u32 kind = 0;
    i64 img = lang_build_text((const u8 *)text, (u64)got, false, out, LANG_OUT_MAX,
                              &kind, err, sizeof(err));
    if (img < 0) {
        t_say(s, "the assembler refused what the compiler made:");
        t_say(s, err);
        return;
    }
    lay_made(s, base, out, (u64)img, kind);
}

/* ------------------------------------------------------------------ */
/* Linking and building                                                */
/* ------------------------------------------------------------------ */

/* The objects of a build, kept while it runs: their bytes in one
 * arena, asked for once. */
#define ARENA_MAX (24u * 1024 * 1024)
#define UNITS_MAX 128
static u8 *arena;
static ld_unit units[UNITS_MAX];
static char unit_names[UNITS_MAX][NAME_SHOWN];

/* Lays o into the list, replacing what carries the name already. */
static bool lay_into(object *holder, object *o, const char *nm)
{
    u64 n = obj_slots(holder), at = n;
    for (u64 i = 0; i < n; i++) {
        const char *have = obj_slot_name(holder, i);
        if (obj_get_slot(holder, i) && have && strcmp(have, nm) == 0) { at = i; break; }
    }
    if (at == n) for (u64 i = 0; i < n; i++) if (!obj_get_slot(holder, i)) { at = i; break; }
    if (at == n && !obj_grow_slots(holder, n + 1)) return false;
    obj_set_slot(holder, at, o, CAP_READ | CAP_WRITE | CAP_GRANT);
    obj_set_slot_name(holder, at, nm);
    obj_touch(holder);
    return true;
}

/* A line for the report, built from pieces and numbers. */
static void ap(char *line, u32 *at, const char *s)
{
    while (*s && *at < 158) line[(*at)++] = *s++;
    line[*at] = 0;
}

static void apd(char *line, u32 *at, u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n && *at < 158) line[(*at)++] = d[--n];
    line[*at] = 0;
}

/* Links the gathered units, into the list: a kernel when one of them
 * lays kmain down, an image otherwise. */
static bool link_units(object *into, const char *listname, u32 n, term_say_fn say, void *ctx)
{
    char line[160];
    u32 at = 0;
    if (n == 0) { say(ctx, "there is nothing to link in it."); return false; }
    bool kernel = false;
    for (u32 i = 0; i < n; i++) if (ld_object_defines(units[i].data, units[i].len, "kmain")) kernel = true;

    u8 *out = lang_out_buffer();
    if (!out) { say(ctx, "there is no room for the tools' tables."); return false; }
    char err[160];
    i64 got = ld_link(units, n, kernel ? LD_KERNEL : LD_PROGRAM, out, LANG_OUT_MAX, err, sizeof(err));
    if (got < 0) { say(ctx, err); return false; }

    char nm[NAME_SHOWN + 8];
    if (kernel) {
        const char *k = "kernel.elf";
        u32 q = 0;
        while (k[q]) { nm[q] = k[q]; q++; }
        nm[q] = 0;
    } else {
        u32 k = 0;
        while (listname[k] && k < 19) { nm[k] = listname[k]; k++; }
        const char *tail = " code";
        for (u32 i = 0; tail[i]; i++) nm[k++] = tail[i];
        nm[k] = 0;
    }
    object *made = obj_create(TYPE_BYTES, (u64)got, 0);
    if (!made) { say(ctx, "nothing came of it; memory is short."); return false; }
    memcpy(obj_data(made), out, (u64)got);
    /* A kernel is far too large for the snapshot and lives until the
     * next boot -- write out keeps it; a program's image is small and
     * stays, like one compile makes. */
    if (kernel) obj_set_transient(made, true);
    bool ok = lay_into(into, made, nm);
    obj_release(made);
    if (!ok) { say(ctx, "no room to lay it in the list."); return false; }
    kprintf("build: %s, %llu bytes from %u objects\n", nm, (u64)got, n);
    ap(line, &at, nm);
    ap(line, &at, "  lies in the list: ");
    apd(line, &at, (u64)got);
    ap(line, &at, " bytes, ");
    apd(line, &at, n);
    ap(line, &at, kernel ? " objects, the kernel's shape.  'write out' carries it to the disk."
                         : " objects.  'run' it.");
    say(ctx, line);
    return true;
}

bool term_link_list(object *list, const char *name, term_say_fn say, void *ctx)
{
    u32 n = 0;
    for (u64 i = 0; i < obj_slots(list) && n < UNITS_MAX; i++) {
        object *t = obj_get_slot(list, i);
        if (!t || obj_type(t) != TYPE_BYTES) continue;
        if (!(obj_slot_rights(list, i) & CAP_READ)) continue;
        if (!ld_object_ok((const u8 *)obj_data(t), obj_size(t))) continue;
        const char *nm = shown_name(list, i);
        u32 k = 0;
        while (nm[k] && k < NAME_SHOWN - 1) { unit_names[n][k] = nm[k]; k++; }
        unit_names[n][k] = 0;
        units[n].data = (const u8 *)obj_data(t);
        units[n].len = obj_size(t);
        units[n].name = unit_names[n];
        n++;
    }
    return link_units(list, name, n, say, ctx);
}

bool term_build_list(object *list, const char *name, term_say_fn say, void *ctx)
{
    if (!arena) arena = (u8 *)lang_big_alloc(ARENA_MAX);
    char *text = lang_text_buffer();
    u8 *out = lang_out_buffer();
    if (!arena || !text || !out) { say(ctx, "there is no room for the tools' tables."); return false; }

    u32 n = 0, used = 0, texts = 0;
    char err[160];
    char line[160];
    for (u64 i = 0; i < obj_slots(list) && n < UNITS_MAX; i++) {
        object *t = obj_get_slot(list, i);
        if (!t || obj_type(t) != TYPE_TEXT) continue;
        if (!(obj_slot_rights(list, i) & CAP_READ)) continue;
        const char *nm = shown_name(list, i);
        u32 nl = (u32)strlen(nm);
        bool is_c = nl > 2 && nm[nl - 2] == '.' && nm[nl - 1] == 'c';
        bool is_S = nl > 2 && nm[nl - 2] == '.' && (nm[nl - 1] == 'S' || nm[nl - 1] == 's');
        if (!is_c && !is_S) continue;
        texts++;

        const u8 *src = (const u8 *)obj_data(t);
        u64 slen = text_len(src, obj_size(t));
        u8 *at = arena + used;
        u64 room = ARENA_MAX - used;
        i64 got;
        if (is_c) {
            i64 al = cc_compile(src, slen, nm, find_beside, list, text, LANG_TEXT_MAX, err, sizeof(err));
            if (al < 0) { u32 q = 0; ap(line, &q, nm); ap(line, &q, ": "); ap(line, &q, err); say(ctx, line); return false; }
            got = asm_assemble((const u8 *)text, (u64)al, at, room, err, sizeof(err));
        } else {
            /* the kernel's own assembly is in the gnu dialect; the
             * text says so itself, whatever its name's case */
            bool gnu = nm[nl - 1] == 'S' || gnu_looks(src, slen);
            got = gnu ? asm_assemble_gnu(src, slen, at, room, err, sizeof(err))
                      : asm_assemble(src, slen, at, room, err, sizeof(err));
        }
        if (got < 0) { u32 q = 0; ap(line, &q, nm); ap(line, &q, ": "); ap(line, &q, err); say(ctx, line); return false; }

        u32 k = 0;
        while (nm[k] && k < NAME_SHOWN - 1) { unit_names[n][k] = nm[k]; k++; }
        unit_names[n][k] = 0;
        units[n].data = at;
        units[n].len = (u64)got;
        units[n].name = unit_names[n];
        n++;
        used += (u32)((got + 15) & ~15);
        u32 q = 0;
        ap(line, &q, "  ");
        ap(line, &q, nm);
        ap(line, &q, "  ");
        apd(line, &q, (u64)got);
        ap(line, &q, " bytes of object");
        say(ctx, line);
    }
    if (texts == 0) { say(ctx, "there is no text of c or assembly in it."); return false; }
    return link_units(list, name, n, say, ctx);
}

static void say_to_session(void *ctx, const char *line)
{
    t_say((term_session *)ctx, line);
}

/* ------------------------------------------------------------------ */
/* A build in the background                                           */
/* ------------------------------------------------------------------ */

static void say_to_journal_line(void *ctx, const char *line)
{
    (void)ctx;
    journal_says("build", line);
}

static struct { object *list; char name[NAME_SHOWN]; } job;
static volatile bool building;

static void build_thread(void *arg)
{
    (void)arg;
    term_build_list(job.list, job.name, say_to_journal_line, NULL);
    obj_release(job.list);
    job.list = NULL;
    building = false;
}

bool term_building(void) { return building; }

bool term_build_start(object *list, const char *name)
{
    if (building || !list) return false;
    building = true;
    job.list = list;
    obj_retain(list);
    u32 k = 0;
    while (name && name[k] && k < NAME_SHOWN - 1) { job.name[k] = name[k]; k++; }
    job.name[k] = 0;
    if (!thread_create("build", build_thread, NULL, thread_domain(sched_current()))) {
        obj_release(list);
        job.list = NULL;
        building = false;
        return false;
    }
    return true;
}

/* link <list>: every object in the list, joined. */
static void cmd_link(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "link which list?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_LIST) { t_say(s, "link takes a list of objects."); return; }
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    if (!(sp.r & CAP_WRITE)) { t_say(s, "what it makes would lie in the list, and you may not lay things in there."); return; }
    if (building) { t_say(s, "a build is running; the tools are busy until it is done."); return; }
    term_link_list(sp.o, sp.nm, say_to_session, s);
}

/* build <list>: every text in the list that is c or assembly becomes
 * an object, and the objects are linked. The headers the c includes
 * lie in the same list. */
static void cmd_build(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "build which list?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_LIST) { t_say(s, "build takes a list of texts."); return; }
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    if (!(sp.r & CAP_WRITE)) { t_say(s, "what it makes would lie in the list, and you may not lay things in there."); return; }
    if (!term_build_start(sp.o, sp.nm)) { t_say(s, "a build is running already; the journal says how it goes."); return; }
    t_say(s, "building, in the background: the journal names each text as it is done,");
    t_say(s, "and what lies in the list at the end.");
}

/* take in <list>, write out <list>: the exchange disk's files into the
 * list, and the list's things onto the disk -- the same two acts the
 * chips on the disk offer. */
static void cmd_take_in(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "take in to which list?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_LIST || !(sp.r & CAP_WRITE)) { t_say(s, "take in fills a list you may write."); return; }
    if (!fat_present()) { t_say(s, "there is no exchange disk."); return; }
    u32 n = fat_take_in(sp.o);
    t_dec(s, n);
    t_say(s, " files came in.");
}

/* install <name>: the kernel in those bytes becomes the one the next
 * start runs; the running one stays beside it as kernel.old, and the
 * loader comes back to it if the new one does not come up twice. */
static bool words_are(const char *s, const char *w)
{
    u32 i = 0;
    while (s[i] && w[i] && s[i] == w[i]) i++;
    return s[i] == 0 && w[i] == 0;
}

static void cmd_install(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "install which kernel?  a built one by name, or 'this kernel' for the one running."); return; }

    /* 'install this kernel': the loader and kernel this machine started
     * with -- from a stick, most often -- go onto the boot disk. That is
     * how an installed machine gets a newer system without being
     * emptied: boot the new stick, and say so. The store is not
     * touched; what was made stays made. */
    if (words_are(what, "this kernel") || words_are(what, "this system")) {
        const u8 *l, *k;
        u64 ls, ks;
        if (!system_boot_files(&l, &ls, &k, &ks)) {
            t_say(s, "the loader did not hand its files over at start, so there is nothing to install from.");
            return;
        }
        char why[120];
        if (!fat_install_kernel(k, ks, why, sizeof(why))) { t_say(s, why); return; }
        if (!fat_install_loader(l, ls, why, sizeof(why))) {
            t_say(s, why);
            t_say(s, "the kernel went on all the same; the next start runs it with the old loader.");
        }
        kprintf("boot: the running loader and kernel (%llu and %llu bytes) are installed on the boot disk\n", ls, ks);
        journal_says("system", "the running system is installed on the boot disk for the next start");
        t_say(s, "installed.  the boot disk now starts with this loader and this kernel;");
        t_say(s, "the kernel it had stays beside it as kernel.old, and the store is as it was.");
        t_say(s, "take the stick out and 'restart'.");
        return;
    }

    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_BYTES) { t_say(s, "a kernel is bytes: the kernel.elf a build makes."); return; }
    if (!(sp.r & CAP_READ)) { t_say(s, "you may not read that."); return; }
    char why[120];
    if (!fat_install_kernel((const u8 *)obj_data(sp.o), obj_size(sp.o), why, sizeof(why))) {
        t_say(s, why);
        return;
    }
    kprintf("boot: a kernel of %llu bytes is installed; the previous one is kernel.old\n", obj_size(sp.o));
    journal_says("system", "a new kernel is installed for the next start");
    t_say(s, "installed.  the next start runs it; the running kernel stays as kernel.old,");
    t_say(s, "and the loader returns to that one if the new kernel does not come up twice.");
    t_say(s, "'restart' starts it now.");
}

static void cmd_restart(term_session *s)
{
    t_say(s, "starting again.");
    system_restart();
}

static void cmd_write_out(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "write out which list?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (obj_type(sp.o) != TYPE_LIST || !(sp.r & CAP_READ)) { t_say(s, "write out takes a list you may read."); return; }
    if (!fat_present()) { t_say(s, "there is no exchange disk."); return; }
    u32 n = fat_write_out(sp.o);
    t_dec(s, n);
    t_say(s, " files went out.");
}

static void cmd_run(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "run which text, or which image?"); return; }
    spot sp;
    if (!resolve(s, what, &sp)) return;
    if (!(focus_rights(s) & CAP_WRITE)) { t_say(s, "the running one would lie here, and you may not lay things in here."); return; }

    if (obj_type(sp.o) == TYPE_BYTES) {
        if (!code_image_ok((const u8 *)obj_data(sp.o), obj_size(sp.o),
                           NULL, NULL, NULL)) {
            t_say(s, "those bytes are no program image; 'assemble' makes one.");
            return;
        }
        char nm[NAME_SHOWN];
        u32 n = 0;
        while (sp.nm[n] && n < NAME_SHOWN - 1) { nm[n] = sp.nm[n]; n++; }
        nm[n] = 0;
        object *prog = code_launch(sp.o);
        if (!prog) { t_say(s, "it would not start."); return; }
        i64 at = lay_here(s, prog, CAP_READ | CAP_GRANT, nm);
        obj_release(prog);                       /* the slot holds it now */
        if (at < 0) { t_say(s, "it runs, but there was no room to lay it here."); return; }
        t_say(s, "it runs; the journal carries what it says.");
        return;
    }
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "only a text or an image can run."); return; }

    /* The name is copied out first: laying the program here can grow
     * the slot table, and sp.nm points into the old one -- freed the
     * moment it grows. */
    char nm[NAME_SHOWN];
    u32 n = 0;
    while (sp.nm[n] && n < NAME_SHOWN - 1) { nm[n] = sp.nm[n]; n++; }
    nm[n] = 0;

    /* The launcher hands the program back with a hold of our own: a
     * short program can run to its end and be reaped before the slot
     * takes its hold, and ours is what keeps the object real across
     * that gap. Once it lies here, the slot's hold is enough. */
    object *prog = runner_launch(sp.o);
    if (!prog) { t_say(s, "it would not start."); return; }
    i64 at = lay_here(s, prog, CAP_READ | CAP_GRANT, nm);
    obj_release(prog);
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

/* "ask <task>", or "ask <task> with <object>": the object goes ahead
 * of the work to every machine that gets a part. */
static void cmd_ask(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "ask with which task?  'ask <task>', or 'ask <task> with <object>' to send an input along."); return; }

    char task[64];
    const char *with = NULL;
    u32 tl = 0;
    for (u32 i = 0; what[i] && tl < sizeof(task) - 1; i++) {
        if (what[i] == ' ' && what[i+1] == 'w' && what[i+2] == 'i' && what[i+3] == 't' &&
            what[i+4] == 'h' && what[i+5] == ' ') {
            with = what + i + 6;
            break;
        }
        task[tl++] = what[i];
    }
    while (tl > 0 && task[tl - 1] == ' ') tl--;
    task[tl] = 0;
    while (with && *with == ' ') with++;

    spot sp;
    if (!resolve(s, task, &sp)) return;
    if (obj_type(sp.o) != TYPE_TEXT) { t_say(s, "a task is a text."); return; }

    bool ok;
    if (with && *with) {
        spot in;
        if (!resolve(s, with, &in)) return;
        if (!(in.r & CAP_READ)) { t_say(s, "you may not read that, so it cannot go along."); return; }
        ok = pipe_ask_with(sp.o, (sp.r & CAP_WRITE) != 0, in.o);
    } else {
        ok = pipe_ask(sp.o, (sp.r & CAP_WRITE) != 0);
    }
    if (ok)
        t_say(s, "the desk has it.  the answer lands in the task itself, or in arrivals.");
    else
        t_say(s, "the desk would not take it.  the journal says why.");
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

/* The nodes table, one line each: name, address, version, what it may
 * do here, and when it was last heard. */
static void cmd_nodes(term_session *s)
{
    nodes_apply();
    u32 n = nodes_count();
    if (n == 0) {
        t_say(s, "no node has been met yet.  'scan' calls out; the first sealed knock writes the first row.");
        return;
    }
    for (u32 i = 0; i < n; i++) {
        char nm[24], ver[24], may[24];
        u8 ip[4];
        u16 port;
        nodes_name_at(i, nm);
        nodes_version_at(i, ver);
        nodes_may_words(nodes_may_at(i), may);
        bool has = nodes_address_at(i, ip, &port);
        t_puts(s, "  ");
        t_puts(s, nm);
        t_puts(s, "  ");
        if (has) { put_ip(s, ip); t_putc(s, ' '); t_dec(s, port); }
        else t_puts(s, "no address");
        t_puts(s, "  ");
        t_puts(s, ver[0] ? ver : "version unknown");
        t_puts(s, "  may: ");
        t_puts(s, may);
        if (has) {
            u64 ago = pipe_seen_ago_s(ip);
            if (ago == ~0ULL) t_puts(s, "  not heard since the start");
            else { t_puts(s, "  heard "); t_dec(s, ago); t_puts(s, "s ago"); }
        }
        t_end(s);
    }
    t_say(s, "the table itself lies in system as 'nodes'; 'allow' writes the may column.");
}

/* "allow <node> work update", "allow <node> all", "allow <node> nothing":
 * the rights are the last words, the name is whatever stands before. */
static void cmd_allow(term_session *s, const char *rest)
{
    if (!rest[0]) {
        t_say(s, "allow whom what?  'allow <node> work', 'allow <node> update', 'allow <node> all', 'allow <node> nothing'.");
        return;
    }
    char name[64];
    u32 nl = 0;
    while (rest[nl] && nl < sizeof(name) - 1) { name[nl] = rest[nl]; nl++; }
    name[nl] = 0;

    u32 may = 0;
    bool any = false;
    for (;;) {
        while (nl > 0 && name[nl - 1] == ' ') nl--;
        u32 start = nl;
        while (start > 0 && name[start - 1] != ' ') start--;
        if (start == nl) break;
        const char *w = name + start;
        u32 wl = nl - start;
        u32 bit = 0;
        bool nothing = false;
        if (wl == 4 && memcmp(w, "work", 4) == 0) bit = NODE_MAY_WORK;
        else if (wl == 6 && memcmp(w, "update", 6) == 0) bit = NODE_MAY_UPDATE;
        else if (wl == 3 && memcmp(w, "all", 3) == 0) bit = NODE_MAY_WORK | NODE_MAY_UPDATE;
        else if (wl == 7 && memcmp(w, "nothing", 7) == 0) nothing = true;
        else if (wl == 3 && memcmp(w, "and", 3) == 0) { nl = start; continue; }
        else break;
        may |= bit;
        (void)nothing;
        any = true;
        nl = start;
    }
    while (nl > 0 && name[nl - 1] == ' ') nl--;
    name[nl] = 0;
    if (!any || !nl) {
        t_say(s, "say which node and what: 'allow <node> work', 'update', 'all' or 'nothing'.");
        return;
    }

    nodes_apply();
    i32 i = nodes_by_name(name);
    if (i < 0) {
        t_puts(s, "no node called '");
        t_puts(s, name);
        t_say(s, "' in nodes.  'nodes' lists them.");
        return;
    }
    nodes_allow((u32)i, may);
    char words[24];
    nodes_may_words(may, words);
    t_puts(s, name);
    t_puts(s, " may now: ");
    t_say(s, may ? words : "nothing");
}

/* "update <node>", "update <node> with <kernel.elf>", "update all". */
static void cmd_update(term_session *s, const char *rest)
{
    if (!rest[0]) {
        t_say(s, "update which node?  'update <node>' sends the kernel this machine runs;");
        t_say(s, "'update <node> with <kernel.elf>' a built one; 'update all' every node with an address.");
        return;
    }
    char who[64];
    const char *with = NULL;
    u32 wl = 0;
    for (u32 i = 0; rest[i] && wl < sizeof(who) - 1; i++) {
        if (rest[i] == ' ' && rest[i+1] == 'w' && rest[i+2] == 'i' && rest[i+3] == 't' &&
            rest[i+4] == 'h' && rest[i+5] == ' ') {
            with = rest + i + 6;
            break;
        }
        who[wl++] = rest[i];
    }
    while (wl > 0 && who[wl - 1] == ' ') wl--;
    who[wl] = 0;
    while (with && *with == ' ') with++;

    object *image = NULL;
    if (with && *with) {
        spot sp;
        if (!resolve(s, with, &sp)) return;
        if (obj_type(sp.o) != TYPE_BYTES || !(sp.r & CAP_READ)) {
            t_say(s, "a kernel is bytes you may read: the kernel.elf a build makes.");
            return;
        }
        image = sp.o;
    }

    char why[120];
    if (words_are(who, "all")) {
        u32 n = pipe_update_all(image, why, sizeof(why));
        if (!n) { t_say(s, why); return; }
        t_dec(s, n);
        t_say(s, n == 1 ? " node queued; the journal says how it took it."
                        : " nodes queued, one after the other; the journal says how each took it.");
        return;
    }
    if (!pipe_update(who, image, why, sizeof(why))) { t_say(s, why); return; }
    t_puts(s, "the kernel is on its way to ");
    t_puts(s, who);
    t_say(s, ".  the journal says whether it was taken; the node installs it and restarts.");
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
    t_say(s, "  run <name>       run that text, or that image, as a program, here");
    t_say(s, "  assemble <name>  turn that text of instructions into an image");
    t_say(s, "  compile <name>   turn that text of c into assembly, and that into an image");
    t_say(s, "  link <list>      join the objects in a list into one image, or a kernel");
    t_say(s, "  build <list>     compile and assemble every text in a list, then link");
    t_say(s, "  take in <list>   the exchange disk's files, into the list");
    t_say(s, "  write out <list> the list's texts and bytes, onto the exchange disk");
    t_say(s, "  install <name>   make that kernel the one the next start runs");
    t_say(s, "  install this kernel   put the loader and kernel this machine started with");
    t_say(s, "                   onto the boot disk; the store stays as it is");
    t_say(s, "  restart          start the machine again");
    t_say(s, "  disks            the disks on the bus, and what is on them");
    t_say(s, "  settle on disk N                     make that disk this machine's: boot volume and store");
    t_say(s, "  settle in partition P of disk N      make that partition the store");
    t_say(s, "  settle in the free space of disk N   make a store in the room left, touching nothing else");
    t_say(s, "  yes              go ahead with what 'settle' offered");
    t_say(s, "  networks         the wireless networks in the air");
    t_say(s, "  join <name> [with <passphrase>]   join one; the passphrase is asked for if not given");
    t_say(s, "  leave            leave the wireless network");
    t_say(s, "  wifi             the station: joined where, how, and its address");
    t_say(s, "  address          which card carries the traffic, and the address it holds");
    t_say(s, "  version          what the running kernel calls itself");
    t_say(s, "  nodes            the machines met through the pipe, and what each may do here");
    t_say(s, "  allow <node> work|update|all|nothing   what that node may ask of this machine");
    t_say(s, "  update <node>    send this kernel to that node; it installs and restarts.  'update all'; '... with <kernel.elf>'");
    t_say(s, "  receive <n> bytes as <name>   a text made here, filled with the next n bytes");
    t_say(s, "                   of this session as they are; how a file comes in through the door");
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

/* The settling words answer through this: one line at a time, into
 * the session that asked. */
static void say_to(void *ctx, const char *line)
{
    t_say((term_session *)ctx, line);
}

/* ------------------------------------------------------------------ */
/* The air                                                             */
/* ------------------------------------------------------------------ */

static const char *security_word(u8 s)
{
    return s == WIFI_OPEN ? "open" : s == WIFI_WPA2 ? "wpa2" : "not one this station speaks";
}

static void cmd_networks(term_session *s)
{
    if (!wifi_radio_present()) { t_say(s, "no radio, and no wire to carry the test bench's air."); return; }
    wifi_scan();
    wifi_net list[16];
    u32 n = wifi_networks(list, 16);
    if (n == 0) { t_say(s, "nothing heard yet; the radio listens.  ask again in a moment."); return; }
    for (u32 i = 0; i < n; i++) {
        t_puts(s, "  ");
        t_puts(s, list[i].ssid);
        t_puts(s, "  channel ");
        t_dec(s, list[i].channel);
        t_puts(s, "  signal ");
        if (list[i].rssi < 0) { t_putc(s, '-'); t_dec(s, (u64)(-list[i].rssi)); }
        else t_dec(s, (u64)list[i].rssi);
        t_puts(s, " dBm  ");
        t_puts(s, security_word(list[i].security));
        if (list[i].joined) t_puts(s, "  (joined)");
        else if (list[i].remembered) t_puts(s, "  (remembered)");
        t_end(s);
    }
    t_say(s, "join <name> joins one; a passphrase is asked for when the network wants one.");
}

static void cmd_join(term_session *s, const char *what)
{
    if (!what[0]) { t_say(s, "join which network?  'networks' names them."); return; }
    char a[TERM_LINE], b[TERM_LINE];
    const char *name = what, *pass = NULL;
    if (split_at(what, " with ", a, b)) { name = a; pass = b; }

    wifi_net list[16];
    u32 n = wifi_networks(list, 16);
    wifi_net *found = NULL;
    for (u32 i = 0; i < n && !found; i++)
        if (strcmp(list[i].ssid, name) == 0) found = &list[i];
    if (!found) { t_puts(s, "no network called "); t_puts(s, name); t_say(s, " has been heard; 'networks' lists them."); return; }
    if (found->security == WIFI_OTHER) { t_say(s, "that network's protection is not one this station speaks; wpa2 with a passphrase is."); return; }

    char had[64];
    if (pass) wifi_join(name, pass);
    else if (found->security == WIFI_OPEN) wifi_join(name, "");
    else if (settings_wlan(name, had, sizeof(had))) wifi_join(name, had);
    else {
        s->secret = true;
        u32 i = 0;
        while (name[i] && i < 32) { s->secret_for[i] = name[i]; i++; }
        s->secret_for[i] = 0;
        t_puts(s, "the passphrase for ");
        t_puts(s, name);
        t_say(s, "?  (the letters will not show)");
        return;
    }
    t_puts(s, "joining ");
    t_puts(s, name);
    t_say(s, "; the journal says how it went, and so does 'wifi'.");
}

static void cmd_leave(term_session *s)
{
    if (!wifi_up()) { t_say(s, "not on any network."); return; }
    wifi_leave();
    t_say(s, "leaving the network.");
}

static void cmd_wifi(term_session *s)
{
    char line[160];
    t_say(s, wifi_radio_name());
    t_say(s, wifi_state(line, sizeof(line)));
    u8 ip[4];
    if (wifi_up() && net_own_address(ip)) {
        t_puts(s, "address ");
        put_ip(s, ip);
        t_end(s);
    }
}

/* address: which card carries the traffic, its name on the wire, and
 * the address it holds. The boot log says all of this once and then
 * the desktop covers it, and a person who wants to reach the machine
 * from elsewhere needs the number without restarting to read it. */
static void cmd_address(term_session *s)
{
    if (!nic_up()) { t_say(s, "no card carries traffic; the wire goes nowhere."); return; }
    const u8 *m = nic_mac();
    char line[96];
    u32 at = 0;
    const char *p = nic_name();
    while (*p && at < 40) line[at++] = *p++;
    static const char hex[] = "0123456789abcdef";
    const char *q = " on the wire as ";
    while (*q) line[at++] = *q++;
    for (u32 i = 0; i < 6; i++) {
        if (i) line[at++] = ':';
        line[at++] = hex[m[i] >> 4];
        line[at++] = hex[m[i] & 15];
    }
    line[at] = 0;
    t_say(s, line);

    u8 ip[4];
    if (net_own_address(ip)) {
        t_puts(s, "address ");
        put_ip(s, ip);
        t_end(s);
    } else {
        t_say(s, "no address yet; nobody has leased one and none was claimed.");
    }
    if (wifi_up()) t_say(s, wifi_state(line, sizeof(line)));
}

/* What the running kernel calls itself: the text the build gave it. */
static void cmd_version(term_session *s)
{
    t_puts(s, "EreBUS ");
    t_say(s, erebus_version);
}

bool term_secret(term_session *s) { return s && s->secret; }

void term_line(term_session *s, const char *line)
{
    if (!s || !s->used || !s->depth) return;
    while (*line == ' ') line++;

    /* A passphrase asked for: taken, and never written down here. */
    if (s->secret) {
        s->secret = false;
        s->last_line[0] = 0;
        t_say(s, "> (a passphrase, not shown)");
        if (!*line) { t_say(s, "nothing given; the network stays unjoined."); return; }
        wifi_join(s->secret_for, line);
        t_puts(s, "joining ");
        t_puts(s, s->secret_for);
        t_say(s, "; the journal says how it went, and so does 'wifi'.");
        return;
    }

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
    else if (word_starts(line, "write out", &rest)) cmd_write_out(s, rest);
    else if (word_starts(line, "take in", &rest)) cmd_take_in(s, rest);
    else if (word_starts(line, "install", &rest)) cmd_install(s, rest);
    else if (word_starts(line, "restart", NULL))  cmd_restart(s);
    else if (word_starts(line, "disks", NULL))    settle_disks(say_to, s);
    else if (word_starts(line, "settle", &rest))  settle_plan(rest, say_to, s);
    else if (word_starts(line, "yes", NULL))      settle_yes(say_to, s);
    else if (word_starts(line, "networks", NULL)) cmd_networks(s);
    else if (word_starts(line, "join", &rest))    cmd_join(s, rest);
    else if (word_starts(line, "leave", NULL))    cmd_leave(s);
    else if (word_starts(line, "wifi", NULL))     cmd_wifi(s);
    else if (word_starts(line, "address", NULL))  cmd_address(s);
    else if (word_starts(line, "receive", &rest))  cmd_receive(s, rest);
    else if (word_starts(line, "write", &rest))   cmd_write(s, rest);
    else if (word_starts(line, "make", &rest))    cmd_make(s, rest);
    else if (word_starts(line, "copy", &rest))    cmd_copy(s, rest);
    else if (word_starts(line, "rename", &rest))  cmd_rename(s, rest);
    else if (word_starts(line, "let go", &rest))  cmd_letgo(s, rest);
    else if (word_starts(line, "run", &rest))     cmd_run(s, rest);
    else if (word_starts(line, "assemble", &rest))cmd_assemble(s, rest);
    else if (word_starts(line, "compile", &rest)) cmd_compile(s, rest);
    else if (word_starts(line, "link", &rest))    cmd_link(s, rest);
    else if (word_starts(line, "build", &rest))   cmd_build(s, rest);
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
    else if (word_starts(line, "version", NULL))  cmd_version(s);
    else if (word_starts(line, "nodes", NULL))    cmd_nodes(s);
    else if (word_starts(line, "allow", &rest))   cmd_allow(s, rest);
    else if (word_starts(line, "update", &rest))  cmd_update(s, rest);
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
