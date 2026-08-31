/*
 * shell.c -- moving through the object graph.
 */
#include <eb/shell.h>
#include <eb/fb.h>
#include <eb/ps2.h>
#include <eb/thread.h>
#include <eb/snapshot.h>
#include <eb/msg.h>
#include <eb/fmt.h>
#include <eb/proc.h>
#include <eb/journal.h>
#include <eb/settings.h>
#include <eb/time.h>

#define SNAP_HISTORY_MAX 32
#define TRAIL_MAX 12
#define LENS_MAX   3
#define GRAPH_MAX 48

#define PAD    10
#define ROW    (GLYPH_H + 6)

/* The colours live in a palette rather than in the constants, because
 * "colors are light" in the settings has to mean something. Every use
 * below goes through the palette, so switching it re-dresses the whole
 * shell at the next paint. */
typedef struct {
    color back, panel, panel_hi, edge, text, dim, faint,
          accent, write, readonly, bar;
} palette;

static const palette pal_dark = {
    RGB( 12,  14,  19), RGB( 22,  26,  34), RGB( 30,  36,  48),
    RGB( 48,  56,  72), RGB(214, 219, 230), RGB(120, 130, 150),
    RGB( 74,  82,  98), RGB(122, 172, 255), RGB(126, 200, 140),
    RGB(206, 158,  92), RGB(  8,  10,  14),
};

/* Paper, not a photograph negative: the light palette is chosen by
 * hand, since inverting the dark one produces glare, not light. */
static const palette pal_light = {
    RGB(232, 230, 224), RGB(219, 216, 208), RGB(205, 201, 191),
    RGB(178, 174, 164), RGB( 34,  37,  43), RGB( 92,  97, 106),
    RGB(150, 152, 155), RGB( 38,  86, 168), RGB( 26, 112,  53),
    RGB(158,  90,  18), RGB(210, 206, 197),
};

static palette pal = {
    RGB( 12,  14,  19), RGB( 22,  26,  34), RGB( 30,  36,  48),
    RGB( 48,  56,  72), RGB(214, 219, 230), RGB(120, 130, 150),
    RGB( 74,  82,  98), RGB(122, 172, 255), RGB(126, 200, 140),
    RGB(206, 158,  92), RGB(  8,  10,  14),
};

#define C_BACK      (pal.back)
#define C_PANEL     (pal.panel)
#define C_PANEL_HI  (pal.panel_hi)
#define C_EDGE      (pal.edge)
#define C_TEXT      (pal.text)
#define C_DIM       (pal.dim)
#define C_FAINT     (pal.faint)
#define C_ACCENT    (pal.accent)
#define C_WRITE     (pal.write)
#define C_READONLY  (pal.readonly)
#define C_BAR       (pal.bar)

/* ------------------------------------------------------------------ */
/* Where we are                                                        */
/* ------------------------------------------------------------------ */

static struct {
    domain *dom;

    /* The path. node[0] is where we started; the last entry is the
     * focus. rights[i] is what we hold at that step, which can only
     * ever be narrower than the step before. */
    object *node[TRAIL_MAX];
    u32     rights[TRAIL_MAX];
    u32     via[TRAIL_MAX];        /* the slot we followed to get here */
    u32     depth;

    lens_kind lens[LENS_MAX];
    u32       lens_count;

    u32        selected;           /* which outgoing reference is picked */
    shell_mode mode;

    /* Where typing lands in the focused text. Clicking into the text
     * puts it there; following a reference puts it at the end, which is
     * where appending minds expect it. Clamped to the text on every
     * use, so it survives the text shrinking under it. */
    u64 caret;

    u64  changes;
    bool redraw;
    i32  mouse_x, mouse_y;
    u8   buttons;

    /* The shell's own state, as an object in the graph. Persistence
     * writes it out with everything else, which is why coming back
     * lands where one left off without any special arrangement. */
    object *session;

    /* Looking at the past. The live path is set aside while a past
     * generation is on screen, and put back on the way out. */
    u64     history[SNAP_HISTORY_MAX];
    u32     history_count;
    u32     at_generation;         /* 0 = now, otherwise into history[] */
    object *past_root;
    object *past_session;

    object *live_node[TRAIL_MAX];
    u32     live_rights[TRAIL_MAX];
    u32     live_via[TRAIL_MAX];
    u32     live_depth;
    u32     live_selected;
} nav;

static object *focus(void)      { return nav.node[nav.depth - 1]; }
static u32     focus_rights(void){ return nav.rights[nav.depth - 1]; }

/* Declared here because the input handling comes before them in the
 * file and the drawing comes after, and both need them. */
static void session_store(void);
static void leave_past(void);
static void go_to_generation(u32 index);
static void follow(u64 slot);

/* ------------------------------------------------------------------ */
/* What can be pointed at                                              */
/* ------------------------------------------------------------------ */

/* Everything drawn that means something records where it was drawn, and
 * a click looks up what is underneath.
 *
 * The rule this enforces: if it is on the screen and it does something,
 * it can be clicked. Keyboard shortcuts are then genuinely shortcuts --
 * faster for whoever knows them, and never the only way to reach
 * something. A thing that can only be done by remembering a key is a
 * thing most people will never find. */
typedef enum {
    HOT_NONE,
    HOT_REFERENCE,   /* an outgoing reference: follow it */
    HOT_TRAIL,       /* a step on the path: go back to it */
    HOT_LENS,        /* a lens tab: turn it on or off */
    HOT_NODE,        /* a node in the graph */
    HOT_TIME,        /* a mark in the history */
    HOT_RIGHT,       /* one letter of a reference's rights */
    HOT_NAME,        /* a reference's name: rename it */
    HOT_CLEAR,       /* drop a reference */
    HOT_ADD,         /* make a new reference here */
    HOT_PALETTE,     /* one choice of what to add */
    HOT_JOURNAL,     /* the newest journal line: go to the journal */
    HOT_TEXT         /* the text itself: put the caret there */
} hot_kind;

typedef struct {
    i32      x, y, w, h;
    hot_kind kind;
    u32      index;
} hot_region;

#define HOT_MAX 96
static hot_region hots[HOT_MAX];
static u32        hot_count;
static i32        hovered = -1;

static void hot_reset(void) { hot_count = 0; }

static void hot_add(i32 x, i32 y, i32 w, i32 h, hot_kind k, u32 index)
{
    if (hot_count >= HOT_MAX) return;
    hots[hot_count++] = (hot_region){ x, y, w, h, k, index };
}

static i32 hot_at(i32 mx, i32 my)
{
    /* Last one wins: regions are added in drawing order, so whatever is
     * drawn on top is also what gets clicked. */
    for (i32 i = (i32)hot_count - 1; i >= 0; i--) {
        const hot_region *r = &hots[i];
        if (mx >= r->x && mx < r->x + r->w &&
            my >= r->y && my < r->y + r->h)
            return i;
    }
    return -1;
}

static bool is_hovered(hot_kind k, u32 index)
{
    if (hovered < 0 || (u32)hovered >= hot_count) return false;
    return hots[hovered].kind == k && hots[hovered].index == index;
}

/* ------------------------------------------------------------------ */
/* Making and shaping                                                  */
/* ------------------------------------------------------------------ */

/* Two small states where a keystroke means something other than
 * "change the object I am looking at".
 *
 * Modes are worth avoiding, and there are only these two: typing a
 * name, and choosing what to add. Both are entered by clicking the
 * thing they concern, both show a caret or a highlight exactly where
 * they apply, and both end on escape. Anything more elaborate would be
 * a mode nobody asked to be in. */
static struct {
    enum { EDIT_NONE, EDIT_NAME, EDIT_PICK } kind;
    u64  slot;
    char buf[OBJ_NAME_MAX];
    u32  len;
} edit;

/* What the palette offers. The fixed entries make something new; the
 * rest are objects already in hand, so pointing at something that
 * exists needs no dragging and no second window. */
#define PALETTE_FIXED 3
#define CARRY_MAX 24

/* What is being carried.
 *
 * Everything on the trail, and everything one step from anywhere on the
 * trail. That is not a search and not a listing of the system: it is
 * exactly the set of things that can be named from where we are
 * standing, which is exactly the set of things that can be handed to
 * anyone else. Each comes with the rights we hold on it, which is the
 * ceiling on what we could pass on -- authority given away is authority
 * held, never more. */
typedef struct {
    object *o;
    u32     rights;
    object *holder;      /* where it was seen, for the name on it */
    u64     slot;
} carried;

static u32 gather(carried *out)
{
    u32 n = 0;

    for (u32 d = 0; d < nav.depth && n < CARRY_MAX; d++) {
        out[n].o      = nav.node[d];
        out[n].rights = nav.rights[d];
        out[n].holder = d ? nav.node[d - 1] : NULL;
        out[n].slot   = d ? nav.via[d] : 0;
        n++;
    }

    for (u32 d = 0; d < nav.depth && n < CARRY_MAX; d++) {
        object *src = nav.node[d];
        for (u64 i = 0; i < obj_slots(src) && n < CARRY_MAX; i++) {
            object *t = obj_get_slot(src, i);
            if (!t) continue;

            u32 r = nav.rights[d] & obj_slot_rights(src, i);

            /* Seen already by another route. Two routes to the same
             * object are two separate holds, and what is held is both
             * of them together. */
            u32 j = 0;
            while (j < n && out[j].o != t) j++;
            if (j < n) { out[j].rights |= r; continue; }

            out[n].o = t; out[n].rights = r;
            out[n].holder = src; out[n].slot = i;
            n++;
        }
    }
    return n;
}

/* What we hold on something we can name from here -- which is not the
 * same as what we hold on the thing pointing at it. */
static u32 held_on(object *t)
{
    carried c[CARRY_MAX];
    u32 n = gather(c);
    for (u32 i = 0; i < n; i++) if (c[i].o == t) return c[i].rights;
    return focus_rights();
}

/* What it takes to change the outgoing references of the thing in
 * focus.
 *
 * For most objects that is the right to write it. For a running program
 * it is the right to grant: a program's references are not contents to
 * be edited, they are the list of what it has been handed, and adding
 * to that list is giving something away. Writing into a program is a
 * different act entirely, and not one this shell offers. */
static bool can_shape(void)
{
    object *f = focus();
    if (!f || nav.at_generation != 0) return false;
    if (obj_type(f) == TYPE_PROGRAM) return (focus_rights() & CAP_GRANT) != 0;
    return (focus_rights() & CAP_WRITE) != 0;
}

static void edit_cancel(void)
{
    edit.kind = EDIT_NONE;
    edit.len = 0;
    nav.redraw = true;
}

u64        shell_changes(void)      { return nav.changes; }
object    *shell_focus(void)        { return focus(); }
shell_mode shell_current_mode(void) { return nav.mode; }

/* ------------------------------------------------------------------ */
/* Small drawing helpers                                               */
/* ------------------------------------------------------------------ */

static void text_at(i32 x, i32 y, i32 limit, const char *s, color c)
{
    while (*s && x + GLYPH_W <= limit) {
        fb_glyph(x, y, (u8)*s++, c, 0, false);
        x += GLYPH_W;
    }
}

/* One muted colour per kind of thing, used wherever a type is named.
 * The eye learns them without being told: green things run, amber
 * things are raw, blue things are for reading and writing. */
static color type_color(type_id t)
{
    switch (t) {
    case TYPE_TEXT:    return C_ACCENT;
    case TYPE_BYTES:   return C_READONLY;
    case TYPE_PROGRAM: return C_WRITE;
    default:           return C_DIM;
    }
}

static u32 put(char *buf, u32 at, const char *s)
{
    while (*s) buf[at++] = *s++;
    return at;
}

static u32 put_dec(char *buf, u32 at, u64 v)
{
    char tmp[24];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) buf[at++] = tmp[--n];
    return at;
}

static u64 text_len(const u8 *d, u64 size)
{
    u64 n = 0;
    while (n < size && d[n]) n++;
    return n;
}

/* What to call something.
 *
 * Three sources, in order of how much they can be trusted.
 *
 * First the name on the reference we came through. That one was written
 * by whoever holds the referencing object -- by us, in practice -- and
 * nothing else can set it. An object handed over by a program cannot
 * announce itself as something reassuring, because it has no say in
 * what we wrote down about it.
 *
 * Then the name the object carries for itself, which is a convenience
 * and marked as a claim rather than shown as a fact.
 *
 * Only if there is neither, a description of the contents, so that
 * something unnamed is still recognisable rather than blank. */
static void describe(object *o, char *out, u32 max)
{
    u32 at = 0;
    if (!o) { out[0] = 0; put(out, 0, "empty"); out[5] = 0; return; }

    switch (obj_type(o)) {
    case TYPE_TEXT: {
        const u8 *d = (const u8 *)obj_data(o);
        u64 len = d ? text_len(d, obj_size(o)) : 0;
        if (len == 0) { at = put(out, at, "(empty)"); break; }
        for (u64 i = 0; i < len && at < max - 1; i++) {
            if (d[i] == '\n') break;
            out[at++] = (char)d[i];
        }
        break;
    }
    case TYPE_LIST:
        at = put(out, at, "list of ");
        at = put_dec(out, at, obj_slots(o));
        break;
    case TYPE_BYTES:
        at = put_dec(out, at, obj_size(o));
        at = put(out, at, " bytes");
        break;
    default:
        at = put(out, at, type_name(obj_type(o)));
        break;
    }
    if (at > max - 1) at = max - 1;
    out[at] = 0;
}

/* The label to show, given the object and, where there is one, the
 * reference we reached it through. */
static void label_of(object *holder, u64 slot, object *o, char *out, u32 max)
{
    const char *petname = holder ? obj_slot_name(holder, slot) : NULL;
    if (petname) { u32 n = put(out, 0, petname); out[n] = 0; return; }

    const char *claim = o ? obj_name(o) : NULL;
    if (claim) {
        u32 n = put(out, 0, claim);
        out[n] = 0;
        return;
    }
    describe(o, out, max);
}

/* True when the label is the object's own claim rather than ours, so it
 * can be drawn differently. */
static bool label_is_claim(object *holder, u64 slot, object *o)
{
    if (holder && obj_slot_name(holder, slot)) return false;
    return o && obj_name(o) != NULL;
}

static void rights_text(u32 r, char *out)
{
    u32 at = 0;
    out[at++] = (r & CAP_READ)    ? 'r' : '-';
    out[at++] = (r & CAP_WRITE)   ? 'w' : '-';
    out[at++] = (r & CAP_GRANT)   ? 'g' : '-';
    out[at] = 0;
}

/* ------------------------------------------------------------------ */
/* Lenses                                                              */
/* ------------------------------------------------------------------ */

/* Where the text lens was last drawn, so a click into it can be turned
 * back into a place in the text. One text lens is on screen at a time;
 * when none is, the width is zero and no region is offered. */
static struct {
    i32 x, y, cols, rows;
} text_area;

static void lens_text(object *o, i32 x, i32 y, i32 w, i32 h, bool caret)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(nothing to show)", C_FAINT); return; }

    u64 len = text_len(d, obj_size(o));
    i32 cols = w / GLYPH_W;
    i32 cx = 0, cy = 0;

    if (nav.caret > len) nav.caret = len;

    if (caret) {
        text_area.x = x;
        text_area.y = y;
        text_area.cols = cols;
        text_area.rows = h / GLYPH_H;
        hot_add(x, y, cols * GLYPH_W, h, HOT_TEXT, 0);
    }

    /* The settings read as a table, and they are drawn as one: the
     * matter column dimmed, the bar fainter still, the value in full
     * ink -- the eye goes to what can be changed. The characters stay
     * exactly where plain drawing would put them, so the caret and the
     * click arithmetic need no special case. */
    bool table = (o == settings_object());
    bool past_bar = false;

    for (u64 i = 0; i <= len && cy * GLYPH_H < h; i++) {
        u8 ch = (i < len) ? d[i] : 0;

        if (caret && i == nav.caret)
            fb_rect(x + cx * GLYPH_W - 1, y + cy * GLYPH_H, 2, GLYPH_H,
                    C_ACCENT);
        if (i == len) break;

        if (ch == '\n' || cx >= cols) {
            cx = 0; cy++;
            past_bar = false;
            if (ch == '\n') continue;
        }
        if (cy * GLYPH_H >= h) break;

        color c = C_TEXT;
        if (table) {
            if (ch == '|') { c = C_FAINT; past_bar = true; }
            else if (!past_bar) c = C_DIM;
        }
        fb_glyph(x + cx * GLYPH_W, y + cy * GLYPH_H, ch, c, 0, false);
        cx++;
    }
}

/* The walk above, run backwards: which place in the text sits at this
 * row and column. A click past the end of a line lands at the end of
 * that line; a click below the text lands at the end of the text. */
static u64 text_index_at(object *o, i32 row, i32 col)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) return 0;

    u64 len = text_len(d, obj_size(o));
    i32 cx = 0, cy = 0;

    for (u64 i = 0; i < len; i++) {
        u8 ch = d[i];

        if (ch == '\n') {
            if (cy == row && col >= cx) return i;   /* right of the line */
            cx = 0; cy++;
            continue;
        }
        if (cx >= text_area.cols) { cx = 0; cy++; }

        if (cy == row && cx == col) return i;
        if (cy > row) return i;
        cx++;
    }
    return len;
}

static void lens_bytes(object *o, i32 x, i32 y, i32 w, i32 h)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(no payload)", C_FAINT); return; }

    u64 size = obj_size(o);
    i32 per = (w / GLYPH_W - 7) / 3;
    if (per > 16) per = 16;
    if (per < 4) per = 4;

    const char *hex = "0123456789abcdef";
    for (i32 row = 0; row * GLYPH_H < h; row++) {
        u64 base = (u64)row * per;
        if (base >= size) break;

        char line[96];
        u32 at = 0;
        line[at++] = hex[(base >> 8) & 0xF];
        line[at++] = hex[(base >> 4) & 0xF];
        line[at++] = hex[base & 0xF];
        line[at++] = ' ';
        line[at++] = ' ';
        for (i32 i = 0; i < per && base + (u64)i < size; i++) {
            u8 b = d[base + i];
            line[at++] = hex[b >> 4];
            line[at++] = hex[b & 0xF];
            line[at++] = ' ';
        }
        line[at] = 0;
        text_at(x, y + row * GLYPH_H, x + w, line, C_DIM);
    }
}

static void lens_structure(object *o, i32 x, i32 y, i32 w, i32 h)
{
    char line[96];
    i32 ty = y;
    u32 at;

    text_at(x, ty, x + w, "type", C_DIM);
    text_at(x + 9 * GLYPH_W, ty, x + w, type_name(obj_type(o)),
            type_color(obj_type(o)));
    ty += ROW;

    at = put(line, 0, "identity ");
    at = put_dec(line, at, obj_id(o));
    line[at] = 0;
    text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    at = put(line, 0, "held by  ");
    at = put_dec(line, at, obj_refs(o));
    at = put(line, at, " references");
    line[at] = 0;
    text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    at = put(line, 0, "payload  ");
    at = put_dec(line, at, obj_size(o));
    at = put(line, at, " bytes");
    line[at] = 0;
    text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    /* For a program, the one fact that changes everything about it. */
    if (obj_type(o) == TYPE_PROGRAM) {
        bool alive = proc_is_running(o);
        text_at(x, ty, x + w, "state", C_DIM);
        text_at(x + 9 * GLYPH_W, ty, x + w, alive ? "running" : "ended",
                alive ? C_WRITE : C_READONLY);
        ty += ROW;
    }
    ty += ROW / 2;

    /* What it points at. Empty slots are counted, not listed: eight
     * lines of "empty" say nothing that one quiet line does not. */
    u64 filled = 0, vacant = 0;
    for (u64 i = 0; i < obj_slots(o); i++)
        if (obj_get_slot(o, i)) filled++; else vacant++;

    if (filled == 0) {
        text_at(x, ty, x + w, "no references", C_FAINT);
        ty += ROW;
    } else {
        text_at(x, ty, x + w, "references", C_DIM);
        ty += ROW;
        for (u64 i = 0; i < obj_slots(o) && ty < y + h; i++) {
            object *t = obj_get_slot(o, i);
            if (!t) continue;
            char what[48], r[4];
            label_of(o, i, t, what, sizeof(what));
            rights_text(obj_slot_rights(o, i), r);

            at = put(line, 0, "  ");
            at = put_dec(line, at, i);
            at = put(line, at, "  ");
            at = put(line, at, r);
            at = put(line, at, "  ");
            at = put(line, at, what);
            line[at] = 0;
            text_at(x, ty, x + w, line, C_DIM);
            ty += ROW;
        }
    }
    if (vacant > 0 && ty < y + h) {
        at = put(line, 0, "  ");
        at = put_dec(line, at, vacant);
        at = put(line, at, filled ? " empty slots" : " empty slots");
        line[at] = 0;
        text_at(x, ty, x + w, line, C_FAINT);
        ty += ROW;
    }

    /* For a running program: what it actually holds.
     *
     * Not the same list as "references". The slots above are what YOU
     * pointed it at -- your record of the giving. This is the program's
     * capability table -- the kernel's record of what it can reach,
     * narrowings and withdrawals included. Whoever delegates can see
     * what came of it; authority handed out and forgotten is how every
     * other system rots. */
    domain *pd = proc_domain_of(o);
    if (pd) {
        ty += ROW / 2;
        text_at(x, ty, x + w, "capabilities", C_DIM);
        ty += ROW;

        u64 held = 0;
        for (u64 i = 1; i <= domain_capacity(pd) && ty < y + h; i++) {
            u32 hr = 0;
            object *t = domain_cap_at(pd, i, &hr);
            if (!t) continue;
            held++;

            char r[4];
            rights_text(hr, r);
            const char *claim = obj_name(t);

            at = put(line, 0, "  ");
            at = put(line, at, r);
            if (hr & CAP_CALL) { line[at - 1] = 'c'; }
            at = put(line, at, "  ");
            at = put(line, at, type_name(obj_type(t)));
            if (claim) {
                at = put(line, at, "  ");
                at = put(line, at, claim);
            }
            line[at] = 0;
            text_at(x, ty, x + w, line, C_DIM);
            ty += ROW;
        }
        if (held == 0 && ty < y + h)
            text_at(x, ty, x + w, "  none", C_FAINT);
    }
}

static void draw_lens(lens_kind k, object *o, i32 x, i32 y, i32 w, i32 h,
                      bool caret)
{
    switch (k) {
    case LENS_TEXT:      lens_text(o, x, y, w, h, caret); break;
    case LENS_BYTES:     lens_bytes(o, x, y, w, h); break;
    case LENS_STRUCTURE: lens_structure(o, x, y, w, h); break;
    default: break;
    }
}

static const char *lens_name(lens_kind k)
{
    switch (k) {
    case LENS_TEXT:      return "text";
    case LENS_BYTES:     return "bytes";
    case LENS_STRUCTURE: return "structure";
    default:             return "?";
    }
}

/* A sensible first lens for a type. The system knows what an object is,
 * so nobody has to be asked how to open it. */
static lens_kind default_lens(object *o)
{
    switch (obj_type(o)) {
    case TYPE_TEXT:  return LENS_TEXT;
    case TYPE_BYTES: return LENS_BYTES;
    default:         return LENS_STRUCTURE;
    }
}

/* ------------------------------------------------------------------ */
/* Shell one: focus and path                                           */
/* ------------------------------------------------------------------ */

static void draw_focus_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sh;
    i32 left_w = 300;
    i32 right_w = 340;
    i32 mid_x = PAD + left_w + PAD;
    i32 mid_w = sw - mid_x - right_w - 2 * PAD;

    object *f = focus();

    /* --- the path we took --------------------------------------- */
    fb_rect(PAD, top, left_w, bottom - top, C_PANEL);
    text_at(PAD + PAD, top + PAD, PAD + left_w, "path", C_FAINT);
    fb_rect(PAD + PAD, top + PAD + ROW - 2, left_w - 2 * PAD, 1, C_EDGE);

    i32 ty = top + PAD + ROW + 4;
    for (u32 i = 0; i < nav.depth; i++) {
        char what[40], line[80];
        label_of(i ? nav.node[i - 1] : NULL, i ? nav.via[i] : 0,
                 nav.node[i], what, sizeof(what));

        /* Plain ASCII on purpose: this panel draws a byte at a time, so
         * a box-drawing character would arrive as three separate
         * glyphs. The console elsewhere decodes UTF-8; this does not,
         * and pretending otherwise showed up as line noise. */
        u32 at = 0;
        for (u32 ind = 0; ind < i && ind < 6; ind++) line[at++] = ' ';
        if (i > 0) at = put(line, at, "+- ");
        at = put(line, at, what);
        line[at] = 0;

        bool here = (i + 1 == nav.depth);
        bool hot = is_hovered(HOT_TRAIL, i);
        if (here || hot)
            fb_rect(PAD, ty - 3, left_w, ROW, here ? C_PANEL_HI : C_EDGE);
        text_at(PAD + PAD, ty, PAD + left_w - PAD, line,
                (here || hot) ? C_TEXT : C_DIM);
        hot_add(PAD, ty - 3, left_w, ROW, HOT_TRAIL, i);
        ty += ROW;
    }

    /* --- the object itself, through its lenses -------------------- */
    fb_rect(mid_x, top, mid_w, bottom - top, C_PANEL);

    i32 tab_x = mid_x + PAD;
    for (u32 i = 0; i < LENS_COUNT; i++) {
        bool on = false;
        for (u32 j = 0; j < nav.lens_count; j++) if (nav.lens[j] == i) on = true;

        const char *n = lens_name((lens_kind)i);
        i32 w = (i32)(10 * GLYPH_W);
        bool hot = is_hovered(HOT_LENS, i);

        if (on || hot)
            fb_rect(tab_x - 4, top + PAD - 4, w, ROW,
                    on ? C_PANEL_HI : C_EDGE);
        text_at(tab_x, top + PAD, mid_x + mid_w, n,
                on ? C_ACCENT : (hot ? C_TEXT : C_FAINT));
        hot_add(tab_x - 4, top + PAD - 4, w, ROW, HOT_LENS, i);
        tab_x += w + 8;
    }
    fb_rect(mid_x + PAD, top + PAD + ROW + 2, mid_w - 2 * PAD, 1, C_EDGE);

    i32 cy = top + PAD + ROW + PAD + 4;
    i32 ch = bottom - cy - PAD;
    i32 each = nav.lens_count ? (mid_w - 2 * PAD) / (i32)nav.lens_count : 0;

    for (u32 i = 0; i < nav.lens_count; i++) {
        i32 lx = mid_x + PAD + (i32)i * each;
        if (i > 0) fb_rect(lx - PAD / 2, cy, 1, ch, C_EDGE);
        draw_lens(nav.lens[i], f, lx, cy, each - PAD, ch,
                  nav.lens[i] == LENS_TEXT &&
                  (focus_rights() & CAP_WRITE) != 0);
    }

    /* --- where it leads ------------------------------------------ */
    i32 rx = sw - PAD - right_w;
    fb_rect(rx, top, right_w, bottom - top, C_PANEL);
    text_at(rx + PAD, top + PAD, sw - PAD, "contents", C_FAINT);
    fb_rect(rx + PAD, top + PAD + ROW - 2, right_w - 2 * PAD, 1, C_EDGE);

    ty = top + PAD + ROW + 4;
    bool may_shape = can_shape();

    u64 slots = obj_slots(f);
    u64 used = 0;
    for (u64 i = 0; i < slots; i++) if (obj_get_slot(f, i)) used++;

    if (used == 0 && !may_shape)
        text_at(rx + PAD, ty, sw - PAD, "no references", C_FAINT);

    i32 col_rights = rx + PAD + 2 * GLYPH_W;
    i32 col_name   = rx + PAD + 7 * GLYPH_W;
    i32 col_clear  = rx + right_w - PAD - 2 * GLYPH_W;

    for (u64 i = 0; i < slots && ty < bottom - 2 * ROW; i++) {
        object *t = obj_get_slot(f, i);
        if (!t) continue;                     /* empty slots are not shown */

        char what[40], r[4];
        label_of(f, i, t, what, sizeof(what));
        u32 slot_rights = obj_slot_rights(f, i);
        rights_text(slot_rights, r);

        bool picked = (i == nav.selected);
        bool hot = is_hovered(HOT_REFERENCE, (u32)i);
        if (picked || hot)
            fb_rect(rx, ty - 3, right_w, ROW, picked ? C_PANEL_HI : C_EDGE);
        hot_add(rx, ty - 3, right_w, ROW, HOT_REFERENCE, (u32)i);

        text_at(rx + PAD, ty, sw, picked ? ">" : " ", C_ACCENT);

        /* The three rights, one letter each, each its own target.
         *
         * This is where authority is actually handed on: whoever
         * follows this reference gets these and nothing more. Putting
         * it here rather than behind a dialogue is deliberate -- it is
         * the same row as the thing it governs, and it is never more
         * than one click from being seen to being changed. */
        for (u32 b = 0; b < 3; b++) {
            static const u32 bit[3] = { CAP_READ, CAP_WRITE, CAP_GRANT };
            static const char letter[3] = { 'r', 'w', 'g' };

            /* A letter is clickable when we could actually put it
             * there, which is a question about our hold on the target,
             * not about the reference. Letters that cannot be granted
             * are shown but not offered -- better than offering them and
             * refusing afterwards. */
            bool on = (slot_rights & bit[b]) != 0;
            bool can = may_shape && (on || (held_on(t) & bit[b]));
            bool lit = is_hovered(HOT_RIGHT, (u32)(i * 8 + b));

            char one[2] = { on ? letter[b] : '-', 0 };
            i32 lx = col_rights + (i32)b * GLYPH_W;

            if (lit && can) fb_rect(lx - 1, ty - 2, GLYPH_W + 2, ROW - 2,
                                    C_EDGE);
            text_at(lx, ty, lx + GLYPH_W, one,
                    on ? (b == 1 ? C_WRITE : C_DIM)
                       : (can && lit ? C_TEXT : C_FAINT));
            if (can) hot_add(lx - 1, ty - 2, GLYPH_W + 2, ROW - 2,
                             HOT_RIGHT, (u32)(i * 8 + b));
        }

        /* The name, which is ours to write and nothing else's. */
        if (edit.kind == EDIT_NAME && edit.slot == i) {
            fb_rect(col_name - 3, ty - 3, col_clear - col_name, ROW, C_EDGE);
            edit.buf[edit.len] = 0;
            text_at(col_name, ty, col_clear, edit.buf, C_TEXT);
            fb_rect(col_name + (i32)edit.len * GLYPH_W, ty, 2, GLYPH_H,
                    C_ACCENT);
        } else {
            bool lit = is_hovered(HOT_NAME, (u32)i);
            if (lit && may_shape)
                fb_rect(col_name - 3, ty - 3, col_clear - col_name, ROW,
                        C_EDGE);
            text_at(col_name, ty, col_clear, what,
                    !t ? C_FAINT : ((picked || hot || lit) ? C_TEXT : C_DIM));
            if (may_shape)
                hot_add(col_name - 3, ty - 3, col_clear - col_name, ROW,
                        HOT_NAME, (u32)i);
        }

        if (may_shape) {
            bool lit = is_hovered(HOT_CLEAR, (u32)i);
            text_at(col_clear, ty, sw, "x", lit ? C_READONLY : C_FAINT);
            hot_add(col_clear - 2, ty - 2, 2 * GLYPH_W, ROW - 2,
                    HOT_CLEAR, (u32)i);
        }

        ty += ROW;
    }

    /* Adding. There is no separate command for making an object,
     * because making one without pointing at it from somewhere would
     * produce something unreachable that vanishes immediately. The two
     * are one act, and this is where it happens. */
    if (may_shape && ty < bottom - ROW) {
        ty += 4;
        bool lit = is_hovered(HOT_ADD, 0);
        if (lit) fb_rect(rx, ty - 3, right_w, ROW, C_EDGE);
        text_at(rx + PAD, ty, sw - PAD, "+  add",
                lit ? C_TEXT : C_FAINT);
        hot_add(rx, ty - 3, right_w, ROW, HOT_ADD, 0);
        ty += ROW;

        if (edit.kind == EDIT_PICK) {
            static const char *fixed[PALETTE_FIXED] = {
                "  text", "  bytes", "  list"
            };
            for (u32 p = 0; p < PALETTE_FIXED && ty < bottom - ROW; p++) {
                bool on = is_hovered(HOT_PALETTE, p);
                if (on) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);
                text_at(rx + PAD, ty, sw - PAD, fixed[p],
                        on ? C_TEXT : C_DIM);
                hot_add(rx, ty - 3, right_w, ROW, HOT_PALETTE, p);
                ty += ROW;
            }

            carried carry[CARRY_MAX];
            u32 carried_count = gather(carry);

            if (carried_count > 0 && ty < bottom - ROW) {
                text_at(rx + PAD, ty, sw - PAD,
                        "  or something you already have:", C_FAINT);
                ty += ROW;
            }
            for (u32 c = 0; c < carried_count && ty < bottom - ROW; c++) {
                char nm[40], r[4];
                label_of(carry[c].holder, carry[c].slot, carry[c].o,
                         nm, sizeof(nm));
                rights_text(carry[c].rights, r);

                bool on = is_hovered(HOT_PALETTE, PALETTE_FIXED + c);
                if (on) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);

                /* The rights come along on the label. What is being
                 * offered is not the object but a particular hold on
                 * it, and the difference is the whole point. */
                text_at(rx + PAD + 2 * GLYPH_W, ty, sw - PAD, r,
                        (carry[c].rights & CAP_WRITE) ? C_WRITE : C_READONLY);
                text_at(rx + PAD + 7 * GLYPH_W, ty, sw - PAD, nm,
                        on ? C_TEXT : C_DIM);
                hot_add(rx, ty - 3, right_w, ROW, HOT_PALETTE,
                        PALETTE_FIXED + c);
                ty += ROW;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Shell two: the graph                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    object *o;
    u32     depth;
    i32     x, y;
    i32     parent;   /* index of the node we came from, -1 for the start */
} graph_node;

static graph_node gnodes[GRAPH_MAX];
static u32        gcount;

static i32 find_node(object *o)
{
    for (u32 i = 0; i < gcount; i++) if (gnodes[i].o == o) return (i32)i;
    return -1;
}

/* Breadth first from the start of the path. Bounded in both directions:
 * a graph can be deep and it can be wide, and a picture is useless once
 * it is either. */
static void build_graph(void)
{
    gcount = 0;
    if (nav.depth == 0) return;

    gnodes[gcount].o = nav.node[0];
    gnodes[gcount].depth = 0;
    gnodes[gcount].parent = -1;
    gcount++;

    for (u32 i = 0; i < gcount && gcount < GRAPH_MAX; i++) {
        if (gnodes[i].depth >= 3) continue;
        object *o = gnodes[i].o;

        for (u64 s = 0; s < obj_slots(o) && gcount < GRAPH_MAX; s++) {
            object *t = obj_get_slot(o, s);
            if (!t) continue;
            if (find_node(t) >= 0) continue;   /* already placed; cycles end here */

            gnodes[gcount].o = t;
            gnodes[gcount].depth = gnodes[i].depth + 1;
            gnodes[gcount].parent = (i32)i;
            gcount++;
        }
    }

    /* Lay out in columns by distance from the start, and centre the
     * whole thing: a small graph pinned to the left corner reads as a
     * mistake rather than as a small graph. */
    u32 widest = 0;
    for (u32 i = 0; i < gcount; i++)
        if (gnodes[i].depth > widest) widest = gnodes[i].depth;

    i32 column = 320;
    i32 span = (i32)(widest + 1) * column - (column - 250);
    i32 x0 = ((i32)fb_width() - span) / 2;
    if (x0 < 40) x0 = 40;

    for (u32 d = 0; d <= widest; d++) {
        u32 n = 0;
        for (u32 i = 0; i < gcount; i++) if (gnodes[i].depth == d) n++;
        if (n == 0) continue;

        i32 spacing = 92;
        i32 total = (i32)n * spacing;
        i32 y = (((i32)fb_height() - 260) - total) / 2 + 90;

        for (u32 i = 0; i < gcount; i++) {
            if (gnodes[i].depth != d) continue;
            gnodes[i].x = x0 + (i32)d * column;
            gnodes[i].y = y;
            y += spacing;
        }
    }
}

static void line_between(i32 x0, i32 y0, i32 x1, i32 y1, color c)
{
    /* Straight enough: step along whichever axis is longer. */
    i32 dx = x1 - x0, dy = y1 - y0;
    i32 steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
              ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (steps == 0) return;

    for (i32 i = 0; i <= steps; i++)
        fb_rect(x0 + dx * i / steps, y0 + dy * i / steps, 2, 2, c);
}

static void draw_graph_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sw; (void)sh;
    build_graph();

    i32 node_w = 250, node_h = 46;

    /* Every reference, not only the ones the walk came in by.
     *
     * Drawing just the tree would hide precisely what makes this a
     * graph: an object reachable by two different routes appears once,
     * and a loop back to somewhere already on screen appears not at
     * all. Those are the two shapes a directory tree cannot have, so
     * they are the two worth seeing. The extra edges are drawn fainter,
     * because the difference between "this is where it was found" and
     * "this also points here" is worth keeping. */
    for (u32 i = 0; i < gcount; i++) {
        object *from = gnodes[i].o;

        for (u64 s = 0; s < obj_slots(from); s++) {
            object *to = obj_get_slot(from, s);
            if (!to) continue;
            i32 j = find_node(to);
            if (j < 0) continue;

            bool tree_edge = (gnodes[j].parent == (i32)i);

            i32 x0 = gnodes[i].x + node_w, y0 = gnodes[i].y + node_h / 2;
            i32 x1 = gnodes[j].x,          y1 = gnodes[j].y + node_h / 2;

            /* An edge that goes backwards or sideways would cut through
             * the column it starts in, so it leaves from the left. */
            if (gnodes[j].depth <= gnodes[i].depth) {
                x0 = gnodes[i].x;
                x1 = gnodes[j].x + node_w;
            }
            line_between(x0, y0, x1, y1, tree_edge ? C_EDGE : C_FAINT);
        }
    }

    for (u32 i = 0; i < gcount; i++) {
        object *o = gnodes[i].o;
        bool is_focus = (o == focus());
        bool on_path = false;
        for (u32 d = 0; d < nav.depth; d++) if (nav.node[d] == o) on_path = true;

        i32 x = gnodes[i].x, y = gnodes[i].y;
        if (y < top || y + node_h > bottom) continue;

        bool hot = is_hovered(HOT_NODE, i);
        fb_rect(x - 2, y - 2, node_w + 4, node_h + 4,
                is_focus ? C_ACCENT : (hot ? C_DIM
                                           : (on_path ? C_EDGE : C_PANEL)));
        fb_rect(x, y, node_w, node_h,
                (is_focus || hot) ? C_PANEL_HI : C_PANEL);
        hot_add(x - 2, y - 2, node_w + 4, node_h + 4, HOT_NODE, i);

        char what[40];
        graph_node *par = gnodes[i].parent >= 0 ? &gnodes[gnodes[i].parent]
                                                : NULL;
        u64 via = 0;
        if (par) for (u64 s = 0; s < obj_slots(par->o); s++)
            if (obj_get_slot(par->o, s) == o) { via = s; break; }
        label_of(par ? par->o : NULL, via, o, what, sizeof(what));
        text_at(x + 10, y + 7, x + node_w - 8, what,
                is_focus ? C_TEXT : C_DIM);
        text_at(x + 10, y + 7 + GLYPH_H + 2, x + node_w - 8,
                type_name(obj_type(o)), C_FAINT);
    }

    /* The focused object's content, below the map. Even here one is
     * looking at something, not only at a diagram of it. */
    i32 py = bottom - 220;
    fb_rect(PAD, py, (i32)fb_width() - 2 * PAD, 220 - PAD, C_PANEL);
    draw_lens(nav.lens[0], focus(), PAD * 2, py + PAD,
              (i32)fb_width() - 4 * PAD, 220 - 3 * PAD, false);
}

/* ------------------------------------------------------------------ */
/* Shell three: the path as columns                                    */
/* ------------------------------------------------------------------ */

static void draw_tiles_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sh;
    u32 shown = nav.depth;
    if (shown > 5) shown = 5;            /* only the last few fit */
    u32 first = nav.depth - shown;

    i32 gap = 6;
    i32 total = sw - 2 * PAD - (i32)(shown - 1) * gap;

    /* The focus gets the room: the columns behind it are context. */
    i32 narrow = (shown > 1) ? total / ((i32)shown + 2) : total;
    i32 wide = total - narrow * (i32)(shown - 1);

    i32 x = PAD;
    for (u32 i = 0; i < shown; i++) {
        u32 idx = first + i;
        bool last = (i + 1 == shown);
        i32 w = last ? wide : narrow;

        fb_rect(x, top, w, bottom - top, last ? C_PANEL_HI : C_PANEL);
        fb_rect(x, top, w, 2, last ? C_ACCENT : C_EDGE);

        char what[48], r[4];
        label_of(idx ? nav.node[idx - 1] : NULL, idx ? nav.via[idx] : 0,
                 nav.node[idx], what, sizeof(what));
        rights_text(nav.rights[idx], r);

        text_at(x + PAD, top + PAD, x + w - PAD, what,
                last ? C_TEXT : C_DIM);
        text_at(x + w - PAD - 3 * GLYPH_W, top + PAD, x + w, r,
                (nav.rights[idx] & CAP_WRITE) ? C_WRITE : C_READONLY);
        fb_rect(x + PAD, top + PAD + ROW, w - 2 * PAD, 1, C_EDGE);

        draw_lens(last ? nav.lens[0] : default_lens(nav.node[idx]),
                  nav.node[idx],
                  x + PAD, top + PAD + ROW + PAD,
                  w - 2 * PAD, bottom - top - ROW - 3 * PAD,
                  last && (nav.rights[idx] & CAP_WRITE) != 0);

        x += w + gap;
    }
}

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */

static const char *mode_name(shell_mode m)
{
    switch (m) {
    case SHELL_FOCUS: return "focus";
    case SHELL_GRAPH: return "graph";
    case SHELL_TILES: return "columns";
    default:          return "?";
    }
}

static void draw_cursor(void)
{
    for (i32 i = 0; i < 17; i++) {
        i32 len = 1 + i / 2;
        fb_rect(nav.mouse_x, nav.mouse_y + i, len + 1, 1, RGB(0, 0, 0));
        fb_rect(nav.mouse_x + 1, nav.mouse_y + i, len - 1, 1, RGB(255, 255, 255));
    }
}

static void draw_all(void)
{
    i32 sw = (i32)fb_width(), sh = (i32)fb_height();
    i32 top = 66, bottom = sh - 34;

    /* The newest journal line gets a quiet row above the footer. What
     * programs and the system do would otherwise be invisible until
     * somebody thought to go and look. */
    char newest[104];
    bool have_news = journal_latest(newest, sizeof(newest));
    if (have_news) bottom -= ROW + 6;

    pal = settings_light() ? pal_light : pal_dark;

    hot_reset();
    fb_rect(0, 0, sw, sh, C_BACK);

    /* Header: what is in focus and what may be done with it. Rights are
     * not hidden in a dialogue somewhere -- they are the first thing on
     * the screen, because they are the first thing that matters. */
    fb_rect(0, 0, sw, top - 8, C_BAR);

    char what[64], r[4];
    u32 d = nav.depth - 1;
    object *holder = d ? nav.node[d - 1] : NULL;
    u64 came_by = d ? nav.via[d] : 0;

    label_of(holder, came_by, focus(), what, sizeof(what));
    bool claimed = label_is_claim(holder, came_by, focus());
    rights_text(focus_rights(), r);

    text_at(PAD * 2, 12, sw / 2, what, claimed ? C_READONLY : C_TEXT);
    text_at(PAD * 2, 12 + ROW, sw / 2,
            claimed ? "the object's own name"
                    : "named by you",
            C_FAINT);

    /* Room for the footer, which is the longest line anywhere on the
     * screen. It was 96 for a long time, and the footer was longer than
     * that for exactly as long -- put() does not check, so the hints
     * quietly wrote over whatever the compiler had placed next. Found
     * the day the text grew and the corruption finally landed somewhere
     * visible. */
    char line[192];
    i32 tx = sw / 2 + PAD;
    const char *tn = type_name(obj_type(focus()));
    text_at(tx, 14, sw - PAD, tn, type_color(obj_type(focus())));
    while (*tn++) tx += GLYPH_W;
    tx += 2 * GLYPH_W;

    u32 at = put(line, 0, r);
    /* What can be done here, said plainly. A running program is the one
     * case where the answer is neither of the usual two: its contents
     * are not for editing, but what it holds is for giving. And a
     * program that has ended is a record, not a recipient. */
    if (obj_type(focus()) == TYPE_PROGRAM)
        at = put(line, at, !proc_is_running(focus())
                           ? "  ended"
                           : (focus_rights() & CAP_GRANT)
                           ? "  you may give it things"
                           : "  read only");
    else
        at = put(line, at, (focus_rights() & CAP_WRITE)
                           ? "  writable"
                           : "  read only");
    line[at] = 0;
    text_at(tx, 14, sw - PAD, line,
            (focus_rights() & CAP_WRITE) ? C_WRITE : C_READONLY);

    /* Far right, quietly: which generation is on the disk, and how long
     * the system has been up. Not a status bar -- two facts. */
    at = put(line, 0, "generation ");
    at = put_dec(line, at, snap_generation());
    at = put(line, at, "   up ");
    u64 ups = time_ns() / 1000000000ULL;
    if (ups >= 60) {
        at = put_dec(line, at, ups / 60);
        at = put(line, at, "m ");
    }
    at = put_dec(line, at, ups % 60);
    at = put(line, at, "s");
    line[at] = 0;
    text_at(sw - PAD * 2 - (i32)at * GLYPH_W, 12 + ROW, sw - PAD,
            line, C_FAINT);

    /* The history strip.
     *
     * A row of marks, oldest at the left, the present at the right.
     * It is not a scrollbar and not an undo list: each mark is a whole
     * state of the system that is still on the disk, and standing on
     * one shows everything as it was, not one document. */
    i32 strip_h = 30;
    if (nav.history_count > 0) bottom -= strip_h;

    switch (nav.mode) {
    case SHELL_FOCUS: draw_focus_shell(sw, sh, top, bottom); break;
    case SHELL_GRAPH: draw_graph_shell(sw, sh, top, bottom); break;
    case SHELL_TILES: draw_tiles_shell(sw, sh, top, bottom); break;
    default: break;
    }

    if (nav.history_count > 0) {
        i32 sy = bottom + 6;
        i32 tick = 14, gap = 5;
        i32 count = (i32)nav.history_count;
        i32 x = PAD * 2;

        text_at(x, sy + 2, sw, "history", C_FAINT);
        x += 9 * GLYPH_W;

        /* Oldest on the left, so time runs the way it is read. Index 0
         * of the history is the newest, hence the reversal. */
        for (i32 i = count - 1; i >= 0; i--) {
            /* Mark i is history entry i, and entry zero is the present.
             * Clicking one goes there; clicking the rightmost comes
             * back to now. */
            bool here = (nav.at_generation == 0) ? (i == 0)
                                                 : ((u32)i == nav.at_generation);
            bool present = (i == 0);
            color c = here ? C_ACCENT : (present ? C_DIM : C_FAINT);

            if (is_hovered(HOT_TIME, (u32)i) && !here) c = C_TEXT;
            fb_rect(x, sy + (here ? 0 : 3), tick, here ? 20 : 14, c);
            hot_add(x - 2, sy, tick + 4, 24, HOT_TIME, (u32)i);
            x += tick + gap;
        }

        x += PAD * 2;
        at = 0;
        if (nav.at_generation == 0) {
            at = put(line, 0, "now  --  generation ");
            at = put_dec(line, at, snap_generation());
            at = put(line, at, "");
        } else {
            at = put(line, 0, "generation ");
            at = put_dec(line, at, nav.history[nav.at_generation]);
            at = put(line, at, "  --  the past, read only.  "
                               "escape returns to now");
        }
        line[at] = 0;
        text_at(x, sy + 2, sw - PAD, line,
                nav.at_generation ? C_READONLY : C_DIM);
    }

    /* The newest thing that happened, clickable: it leads to the whole
     * record. Dim on purpose -- it should be findable, not insistent. */
    if (have_news) {
        i32 jy = sh - 28 - ROW - 6;
        bool lit = is_hovered(HOT_JOURNAL, 0);
        if (lit) fb_rect(0, jy - 2, sw, ROW + 4, C_PANEL);
        text_at(PAD * 2, jy, sw - PAD, newest, lit ? C_TEXT : C_FAINT);
        hot_add(0, jy - 2, sw, ROW + 4, HOT_JOURNAL, 0);
    }

    /* Footer: the mode, and what the keys do. No menu bar -- there is
     * nothing to put in one that is not already visible. The last hint
     * follows the focus, because what typing does depends on what is
     * under it. Whoever knows their way around turns the hints off in
     * the settings, and the line keeps only the mode. */
    fb_rect(0, sh - 28, sw, 28, C_BAR);
    at = put(line, 0, mode_name(nav.mode));
    if (settings_hints()) {
        at = put(line, at, "   click anything you can see.   "
                           "tab: switch view.   arrows also move.   ");
        if (obj_type(focus()) == TYPE_PROGRAM && proc_is_running(focus()))
            at = put(line, at, "point it at something to hand it over.");
        else if (obj_type(focus()) == TYPE_TEXT &&
                 (focus_rights() & CAP_WRITE) && nav.at_generation == 0)
            at = put(line, at, "typing goes where the caret is.");
        else
            at = put(line, at, "typing changes the object.");
    }
    line[at] = 0;
    text_at(PAD * 2, sh - 28 + 6, sw - PAD, line, C_FAINT);

    draw_cursor();
}

/* ------------------------------------------------------------------ */
/* Moving                                                              */
/* ------------------------------------------------------------------ */

static void set_lenses_for(object *o)
{
    nav.lens[0] = default_lens(o);
    nav.lens_count = 1;
}

static void follow(u64 slot)
{
    if (nav.depth >= TRAIL_MAX) return;

    object *f = focus();
    if (slot >= obj_slots(f)) return;

    object *t = obj_get_slot(f, slot);
    if (!t) return;

    /* Following a reference can only narrow what one holds. The step is
     * the intersection of what we had with what the reference permits;
     * there is no path through this function that widens anything. */
    u32 gained = focus_rights() & obj_slot_rights(f, slot);
    if (gained == 0) return;             /* a reference we may not use */

    nav.via[nav.depth] = (u32)slot;
    nav.node[nav.depth] = t;
    nav.rights[nav.depth] = gained;
    nav.depth++;
    nav.selected = 0;
    nav.caret = (u64)-1;                 /* the end, once clamped */
    set_lenses_for(t);
    nav.changes++;
    nav.redraw = true;
}

static void go_back(void)
{
    if (nav.depth <= 1) return;
    nav.depth--;
    nav.selected = nav.via[nav.depth];
    nav.caret = (u64)-1;
    set_lenses_for(focus());
    nav.changes++;
    nav.redraw = true;
}

static void toggle_lens(lens_kind k)
{
    for (u32 i = 0; i < nav.lens_count; i++) {
        if (nav.lens[i] != k) continue;
        if (nav.lens_count == 1) return;            /* never zero lenses */
        for (u32 j = i; j + 1 < nav.lens_count; j++) nav.lens[j] = nav.lens[j + 1];
        nav.lens_count--;
        nav.redraw = true;
        return;
    }
    if (nav.lens_count >= LENS_MAX) return;
    nav.lens[nav.lens_count++] = k;
    nav.redraw = true;
}

static void type_into_focus(u32 codepoint)
{
    object *f = focus();
    if (!(focus_rights() & CAP_WRITE)) return;
    if (obj_type(f) != TYPE_TEXT) return;

    u8 *d = (u8 *)obj_data(f);
    if (!d) return;
    u64 size = obj_size(f);
    u64 len = text_len(d, size);
    if (nav.caret > len) nav.caret = len;

    /* Typing happens at the caret, and the caret goes where it is
     * clicked -- so a line in the middle can be reworked in place,
     * which is what makes a text of one-line facts editable at all. */
    if (codepoint == '\b') {
        if (nav.caret == 0) return;
        for (u64 i = nav.caret - 1; i < len; i++) d[i] = d[i + 1];
        d[len - 1] = 0;
        nav.caret--;
    } else if (len + 1 < size) {
        for (u64 i = len + 1; i > nav.caret; i--) d[i] = d[i - 1];
        d[nav.caret] = (u8)codepoint;
        nav.caret++;
    } else {
        return;
    }
    nav.changes++;
    nav.redraw = true;
}

static void handle_keys(void)
{
    key_event k;
    while (ps2_poll_key(&k)) {
        if (!k.down || k.codepoint == 0) continue;

        /* While a name is being written, the keys belong to the name.
         * The row shows a caret exactly where the letters are going, so
         * there is no question about where typing lands. */
        if (edit.kind == EDIT_NAME) {
            if (k.codepoint == KEY_ESCAPE) { edit_cancel(); continue; }
            if (k.codepoint == KEY_ENTER) {
                edit.buf[edit.len] = 0;
                obj_set_slot_name(focus(), edit.slot,
                                  edit.len ? edit.buf : NULL);
                edit_cancel();
                nav.changes++;
                continue;
            }
            if (k.codepoint == '\b') {
                if (edit.len) edit.len--;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint >= 0x20 && k.codepoint < 0x7F &&
                edit.len < OBJ_NAME_MAX - 1) {
                edit.buf[edit.len++] = (char)k.codepoint;
                nav.redraw = true;
            }
            continue;
        }

        if (edit.kind == EDIT_PICK && k.codepoint == KEY_ESCAPE) {
            edit_cancel();
            continue;
        }

        switch (k.codepoint) {
        case KEY_TAB:
            nav.mode = (shell_mode)((nav.mode + 1) % SHELL_MODE_COUNT);
            nav.changes++;
            nav.redraw = true;
            continue;

        /* Time. Page up steps back through the generations the ring
         * still holds, page down comes forward again, escape returns to
         * the present. Nothing is undone by going back -- an older
         * state is read, and the present is still where it was. */
        case KEY_PGUP:
            go_to_generation(nav.at_generation + 1);
            continue;
        case KEY_PGDN:
            if (nav.at_generation > 0) go_to_generation(nav.at_generation - 1);
            continue;
        case KEY_ESCAPE:
            leave_past();
            continue;
        case KEY_UP:
            if (nav.selected > 0) { nav.selected--; nav.redraw = true; }
            continue;
        case KEY_DOWN: {
            u64 n = obj_slots(focus());
            if (n && nav.selected + 1 < n) { nav.selected++; nav.redraw = true; }
            continue;
        }
        case KEY_RIGHT:
        case KEY_ENTER:
            follow(nav.selected);
            continue;
        case KEY_LEFT:
            go_back();
            continue;
        default: break;
        }

        /* Lenses are switched by clicking their tabs, or with control
         * held. Plain digits used to do it, which meant a digit could
         * not be typed into a text -- a shortcut that quietly takes a
         * character away from the thing one is actually doing is worse
         * than no shortcut. */
        if (k.ctrl) {
            if (k.codepoint == '1') { toggle_lens(LENS_TEXT); continue; }
            if (k.codepoint == '2') { toggle_lens(LENS_BYTES); continue; }
            if (k.codepoint == '3') { toggle_lens(LENS_STRUCTURE); continue; }
            continue;
        }

        if (k.codepoint < 0x110000u) type_into_focus(k.codepoint);
    }
}

/* Retreats to an earlier point on the path. */
static void go_back_to(u32 index)
{
    if (index + 1 >= nav.depth) return;
    while (nav.depth > index + 1) go_back();
}

/* Acts on whatever was clicked.
 *
 * One gesture, one meaning: pointing at a thing and pressing goes to
 * it, or turns it on. Nothing here opens a menu, and nothing needs a
 * second click to confirm -- every one of these is undone by clicking
 * somewhere else, so there is nothing to be careful about. */
static void act_on(const hot_region *r)
{
    switch (r->kind) {
    case HOT_REFERENCE:
        nav.selected = r->index;
        follow(r->index);
        break;

    case HOT_TRAIL:
        go_back_to(r->index);
        break;

    case HOT_LENS:
        toggle_lens((lens_kind)r->index);
        break;

    case HOT_NODE: {
        /* Walk to it from where we are, one reference at a time, so the
         * path stays a real chain of references rather than a jump that
         * skipped whatever the steps would have permitted. */
        if (r->index >= gcount) break;
        object *want = gnodes[r->index].o;
        if (want == focus()) break;

        for (u64 s = 0; s < obj_slots(focus()); s++) {
            if (obj_get_slot(focus(), s) == want) { follow(s); return; }
        }
        /* Not a neighbour of where we stand. Retrace: if it is on the
         * path we came by, go back to it. */
        for (u32 i = 0; i < nav.depth; i++)
            if (nav.node[i] == want) { go_back_to(i); return; }
        break;
    }

    case HOT_TIME:
        go_to_generation(r->index);
        break;

    case HOT_TEXT: {
        /* A click into the writing puts the caret there. The pixel is
         * turned back into a row and a column, and the column walk
         * finds which letter that is. */
        if (!(focus_rights() & CAP_WRITE)) break;
        i32 row = (nav.mouse_y - text_area.y) / GLYPH_H;
        i32 col = (nav.mouse_x - text_area.x + GLYPH_W / 2) / GLYPH_W;
        if (row < 0) row = 0;
        if (col < 0) col = 0;
        nav.caret = text_index_at(focus(), row, col);
        nav.redraw = true;
        break;
    }

    case HOT_JOURNAL: {
        /* The line leads to the record. The walk is a real walk -- back
         * to the start, then through the reference that holds the
         * journal -- so what one arrives holding is what that path
         * grants, which for the journal is reading and nothing else. */
        object *j = journal_object();
        if (!j) break;
        if (nav.at_generation != 0) leave_past();
        go_back_to(0);
        for (u64 i = 0; i < obj_slots(focus()); i++)
            if (obj_get_slot(focus(), i) == j) {
                nav.selected = (u32)i;
                follow(i);
                break;
            }
        break;
    }

    case HOT_RIGHT: {
        /* One letter, one right. Turning it off narrows what anyone
         * following this reference will get; turning it on can only
         * reach as far as what we hold ourselves, which is why the
         * letters one cannot grant are not clickable at all rather
         * than clickable and refused. */
        static const u32 bit[3] = { CAP_READ, CAP_WRITE, CAP_GRANT };
        u64 slot = r->index / 8;
        u32 which = r->index % 8;
        if (which > 2 || slot >= obj_slots(focus())) break;
        if (!can_shape()) break;

        object *target = obj_get_slot(focus(), slot);
        u32 ceiling = held_on(target);

        u32 now = obj_slot_rights(focus(), slot);
        u32 next = (now & bit[which]) ? (now & ~bit[which])
                                      : (now | (bit[which] & ceiling));
        obj_set_slot(focus(), slot, target, next);

        /* A running program is holding this right now, so narrowing the
         * reference has to reach it. Otherwise the letters on the screen
         * would say one thing and the capability in its table another,
         * and the screen would be the one that was lying. */
        if (obj_type(focus()) == TYPE_PROGRAM && target)
            proc_grant(focus(), target, next);

        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_NAME: {
        if (!can_shape()) break;
        edit.kind = EDIT_NAME;
        edit.slot = r->index;
        edit.len = 0;
        const char *had = obj_slot_name(focus(), r->index);
        if (had) while (had[edit.len] && edit.len < OBJ_NAME_MAX - 1) {
            edit.buf[edit.len] = had[edit.len];
            edit.len++;
        }
        nav.redraw = true;
        break;
    }

    case HOT_CLEAR: {
        if (!can_shape()) break;

        /* Taking it back from a program, before the reference that
         * recorded the giving disappears. */
        object *target = obj_get_slot(focus(), r->index);
        if (obj_type(focus()) == TYPE_PROGRAM && target)
            proc_revoke(focus(), target);

        /* Letting go of a reference. If it was the last one the object
         * is gone, and if it was not, the object is still perfectly
         * reachable by whoever else points at it. Nothing here has to
         * ask whether it is "in use" -- the count already knows. */
        obj_set_slot(focus(), r->index, NULL, 0);
        obj_set_slot_name(focus(), r->index, NULL);
        edit_cancel();
        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_ADD:
        edit.kind = (edit.kind == EDIT_PICK) ? EDIT_NONE : EDIT_PICK;
        nav.redraw = true;
        break;

    case HOT_PALETTE: {
        if (!can_shape()) break;

        object *f = focus();
        u64 slot = 0;
        bool found = false;
        for (u64 i = 0; i < obj_slots(f); i++)
            if (!obj_get_slot(f, i)) { slot = i; found = true; break; }
        if (!found) {
            slot = obj_slots(f);
            if (!obj_grow_slots(f, slot + 1)) break;
        }

        object *made = NULL;
        const char *suggest = "";
        bool created = false;
        char borrowed[40];

        /* Something made here is held outright -- there is nobody it
         * could have been narrowed by. */
        u32 give = CAP_READ | CAP_WRITE | CAP_GRANT;

        if (r->index == 0)      { made = obj_create(TYPE_TEXT, 512, 0);
                                  suggest = "note"; created = true; }
        else if (r->index == 1) { made = obj_create(TYPE_BYTES, 64, 0);
                                  suggest = "bytes"; created = true; }
        else if (r->index == 2) { made = obj_create(TYPE_LIST, 0, 4);
                                  suggest = "list"; created = true; }
        else {
            carried carry[CARRY_MAX];
            u32 n = gather(carry);
            u32 c = r->index - PALETTE_FIXED;
            if (c >= n) break;

            made = carry[c].o;
            give = carry[c].rights;
            label_of(carry[c].holder, carry[c].slot, made,
                     borrowed, sizeof(borrowed));
            suggest = borrowed;
        }
        if (!made) break;

        /* What the new reference permits cannot exceed what we hold on
         * the thing it points at. Not on the thing doing the pointing --
         * those are different objects and the confusion between them is
         * how authority leaks. */
        obj_set_slot(f, slot, made, give);

        /* If what we just pointed at something is a running program,
         * pointing at it is exactly how it comes to hold the thing.
         * There is no separate act of sharing -- the reference is the
         * grant, and the program is told about it the only way anything
         * reaches a program, which is by message. */
        if (obj_type(f) == TYPE_PROGRAM)
            proc_grant(f, made, give);

        if (created) obj_release(made);      /* the slot holds it now */

        edit.kind = EDIT_NAME;
        edit.slot = slot;
        edit.len = 0;
        while (suggest[edit.len] && edit.len < OBJ_NAME_MAX - 1) {
            edit.buf[edit.len] = suggest[edit.len];
            edit.len++;
        }
        nav.selected = slot;
        nav.changes++;
        nav.redraw = true;
        break;
    }

    default:
        break;
    }
}

static void handle_mouse(void)
{
    mouse_event m;
    bool moved = false;

    i32 num = 1, den = 1;
    settings_pointer_scale(&num, &den);

    while (ps2_poll_mouse(&m)) {
        nav.mouse_x += (i32)m.dx * num / den;
        nav.mouse_y += (i32)m.dy * num / den;
        if (nav.mouse_x < 0) nav.mouse_x = 0;
        if (nav.mouse_y < 0) nav.mouse_y = 0;
        if (nav.mouse_x > (i32)fb_width() - 2)  nav.mouse_x = (i32)fb_width() - 2;
        if (nav.mouse_y > (i32)fb_height() - 2) nav.mouse_y = (i32)fb_height() - 2;
        moved = true;

        bool was = (nav.buttons & 1) != 0;
        bool is = (m.buttons & 1) != 0;
        nav.buttons = m.buttons;

        if (is && !was) {
            i32 h = hot_at(nav.mouse_x, nav.mouse_y);
            if (h >= 0) act_on(&hots[h]);
        }
    }

    if (!moved) return;

    /* Whatever is under the pointer lights up. Without that one has to
     * click to find out whether anything was there at all, and guessing
     * is not the same as knowing. */
    i32 now_over = hot_at(nav.mouse_x, nav.mouse_y);
    if (now_over != hovered) hovered = now_over;
    nav.redraw = true;
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* The session, as an object                                           */
/* ------------------------------------------------------------------ */

/* Everything about where somebody is that is not itself an object. The
 * path is kept in the session object's own reference slots, which is
 * both the natural place for it and the reason the snapshot picks it up
 * without being told. */
typedef struct {
    u32 mode;
    u32 lens_count;
    u32 lens[LENS_MAX];
    u32 depth;
    u32 selected;
    u32 via[TRAIL_MAX];
} session_state;

static void session_store(void)
{
    if (!nav.session) return;
    if (nav.at_generation != 0) return;   /* the past is not written down */

    session_state *s = (session_state *)obj_data(nav.session);
    if (!s) return;

    s->mode = (u32)nav.mode;
    s->lens_count = nav.lens_count;
    for (u32 i = 0; i < LENS_MAX; i++) s->lens[i] = (u32)nav.lens[i];
    s->depth = nav.depth;
    s->selected = nav.selected;
    for (u32 i = 0; i < TRAIL_MAX; i++) s->via[i] = nav.via[i];

    /* The path itself, as references. The rights each step conferred go
     * on the reference, so coming back restores not only where one was
     * but what one was allowed to do there. */
    for (u32 i = 0; i < TRAIL_MAX; i++)
        obj_set_slot(nav.session, i,
                     i < nav.depth ? nav.node[i] : NULL,
                     i < nav.depth ? nav.rights[i] : 0);
}

static bool session_restore(object *session)
{
    if (!session || obj_type(session) != TYPE_SESSION) return false;

    const session_state *s = (const session_state *)obj_data(session);
    if (!s || s->depth == 0 || s->depth > TRAIL_MAX) return false;

    for (u32 i = 0; i < s->depth; i++) {
        object *n = obj_get_slot(session, i);
        if (!n) return false;
        nav.node[i] = n;
        nav.rights[i] = obj_slot_rights(session, i);
        nav.via[i] = s->via[i];
    }
    nav.depth = s->depth;
    nav.selected = s->selected;
    nav.mode = (shell_mode)(s->mode % SHELL_MODE_COUNT);
    nav.lens_count = s->lens_count ? (s->lens_count > LENS_MAX ? 1
                                                              : s->lens_count)
                                   : 1;
    for (u32 i = 0; i < nav.lens_count; i++)
        nav.lens[i] = (lens_kind)(s->lens[i] % LENS_COUNT);
    return true;
}

static bool resumed;

object *shell_session(void) { return nav.session; }
bool    shell_resumed(void) { return resumed; }

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

static void leave_past(void)
{
    if (nav.at_generation == 0) return;

    if (nav.past_session) { obj_release(nav.past_session); nav.past_session = NULL; }
    if (nav.past_root)    { obj_release(nav.past_root);    nav.past_root = NULL; }

    for (u32 i = 0; i < nav.live_depth; i++) {
        nav.node[i] = nav.live_node[i];
        nav.rights[i] = nav.live_rights[i];
        nav.via[i] = nav.live_via[i];
    }
    nav.depth = nav.live_depth;
    nav.selected = nav.live_selected;
    nav.at_generation = 0;
    set_lenses_for(focus());
    nav.redraw = true;
}

/* Steps to a generation in the history. Index 0 is the present.
 *
 * The past is loaded as its own graph, entirely separate from the live
 * one, and every step through it is read-only. That is not a policy
 * decision that had to be enforced anywhere: the rights are simply
 * masked down on the way in, and everything downstream already refuses
 * to write without them. */
static void go_to_generation(u32 index)
{
    if (index == 0) { leave_past(); return; }

    nav.history_count = snap_history(nav.history, SNAP_HISTORY_MAX);
    if (index >= nav.history_count) return;

    /* history[0] is the newest generation, which is the present as it
     * was last written down -- stepping back has to skip it, or the
     * first step would go nowhere. */
    u64 want = nav.history[index];

    object *roots[2] = { NULL, NULL };
    if (snap_load_generation(want, roots, 2) < 1) return;

    if (nav.at_generation == 0) {
        /* First step away from the present: put the live path aside. */
        for (u32 i = 0; i < nav.depth; i++) {
            nav.live_node[i] = nav.node[i];
            nav.live_rights[i] = nav.rights[i];
            nav.live_via[i] = nav.via[i];
        }
        nav.live_depth = nav.depth;
        nav.live_selected = nav.selected;
    } else {
        if (nav.past_session) obj_release(nav.past_session);
        if (nav.past_root) obj_release(nav.past_root);
    }

    nav.past_root = roots[0];
    nav.past_session = roots[1];
    nav.at_generation = index;

    /* If that generation recorded where somebody was, go back to
     * exactly there rather than merely to the same data. */
    bool placed = nav.past_session && session_restore(nav.past_session);
    if (!placed) {
        nav.node[0] = nav.past_root;
        nav.via[0] = 0;
        nav.depth = 1;
        nav.selected = 0;
    }

    for (u32 i = 0; i < nav.depth; i++) nav.rights[i] &= CAP_READ;
    set_lenses_for(focus());
    nav.redraw = true;
}

/* ------------------------------------------------------------------ */

void shell_init(domain *d, object *root, u32 rights, object *session)
{
    nav.dom = d;
    nav.node[0] = root;
    nav.rights[0] = rights;
    nav.via[0] = 0;
    nav.depth = 1;
    nav.selected = 0;
    nav.mode = SHELL_FOCUS;
    nav.at_generation = 0;
    nav.mouse_x = (i32)fb_width() / 2;
    nav.mouse_y = (i32)fb_height() / 2;
    set_lenses_for(root);

    /* Whether the restore actually worked, rather than whether one was
     * offered. A session that failed to load and a fresh start look the
     * same on screen, and saying "resumed" for both would hide exactly
     * the failure worth noticing. */
    resumed = (session != NULL) && session_restore(session);

    if (resumed) {
        nav.session = session;
        obj_retain(session);
    } else {
        if (session) obj_release(session);
        nav.session = obj_create(TYPE_SESSION, sizeof(session_state),
                                 TRAIL_MAX);
        if (nav.session) obj_set_name(nav.session, "where you were");
    }

    nav.history_count = snap_history(nav.history, SNAP_HISTORY_MAX);
    session_store();
    nav.redraw = true;
}

void shell_run(void *arg)
{
    (void)arg;
    u64 seen_journal = journal_sequence();
    u64 settings_seen = (u64)-1;       /* apply once on the way in */
    u64 last_tick = time_ns();

    for (;;) {
        handle_mouse();
        handle_keys();

        /* Two things redraw the screen without anybody touching it:
         * something happened (the journal grew), and time passed (the
         * clock in the corner would otherwise only be right while one
         * is doing something, which is exactly when nobody looks). */
        u64 seq = journal_sequence();
        if (seq != seen_journal) { seen_journal = seq; nav.redraw = true; }

        /* The settings apply as they are typed: the moment a line comes
         * to mean something, it takes effect. Reading a page of text is
         * cheap enough to do on every change. */
        if (nav.changes != settings_seen) {
            settings_seen = nav.changes;
            settings_apply();
        }

        u64 now = time_ns();
        if (now - last_tick >= 1000000000ULL) {
            last_tick = now;
            nav.redraw = true;
        }

        if (nav.redraw) {
            nav.redraw = false;
            session_store();
            draw_all();
            fb_present();
        }
        sched_yield();
    }
}
