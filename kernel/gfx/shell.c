/*
 * shell.c -- moving through the object graph.
 */
#include <eb/shell.h>
#include <eb/fb.h>
#include <eb/version.h>
#include <eb/ps2.h>
#include <eb/thread.h>
#include <eb/snapshot.h>
#include <eb/msg.h>
#include <eb/fmt.h>
#include <eb/proc.h>
#include <eb/journal.h>
#include <eb/settings.h>
#include <eb/standard.h>
#include <eb/net.h>
#include <eb/pipe.h>
#include <eb/bundle.h>
#include <eb/fat.h>
#include <eb/term.h>
#include <eb/asm.h>
#include <eb/cc.h>
#include <eb/ld.h>
#include <eb/lang.h>
#include <eb/html.h>
#include <eb/string.h>
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

    /* The marked stretch of the focused text: the anchor where the
     * button went down, and the moving end. Equal means nothing is
     * marked. Byte places, clamped like the caret. */
    u64  sel_a, sel_b;
    bool sel_drag;

    /* Whether the button went down on a canvas, so moving keeps
     * painting until it comes back up. */
    bool paint_drag;

    /* How far down a rendered page one has read. */
    u32 html_scroll;

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
static void index_walk_to(u32 row);
static void build_index(void);
static bool picture_shape(object *o, u32 *w, u32 *h);

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
    HOT_TEXT,        /* the text itself: put the caret there */
    HOT_INDEX,       /* a row of the index: walk there */
    HOT_TAKE,        /* lift the marked letters out */
    HOT_PUT,         /* set the lifted letters down at the caret */
    HOT_DROP,        /* let the lifted letters go */
    HOT_CAPCUT,      /* one capability of a running program: take it back */
    HOT_MODE,        /* a view's name in the footer: switch to it */
    HOT_FINDX,       /* empty the search */
    HOT_INK,         /* choose an ink */
    HOT_CANVAS,      /* a cell of a picture: paint it */
    HOT_RUN,         /* run the focused text as a program */
    HOT_GO,          /* ask the network for the page the first line names */
    HOT_LINK,        /* a link in a rendered page: go there */
    HOT_SCROLL,      /* the page's edge: up or down a window */
    HOT_FIELD,       /* a form field: put the writing there */
    HOT_SUBMIT,      /* a form's button: send it */
    HOT_BACK,        /* step back through where the browser has been */
    HOT_FWD,         /* and forward again */
    HOT_ADDR,        /* the address itself: type a new one */
    HOT_SEND,        /* give the focused object to the pipe's peer */
    HOT_SCAN,        /* call out on the wire for other machines */
    HOT_PEERPICK,    /* a found machine: point the pipe at it */
    HOT_SENDPICK,    /* a found machine, chosen mid-send: point and go */
    HOT_ASK,         /* run the focused text on the peer's machine */
    HOT_OFF,         /* save everything and put the machine to sleep */
    HOT_END,         /* end the focused running program */
    HOT_PACK,        /* fold the focused list into one plain thing */
    HOT_UNPACK,      /* build the list back out of a bundle */
    HOT_P2REF,       /* the second pane: follow a reference */
    HOT_P2TRAIL,     /* the second pane: go back to a step */
    HOT_FATIN,       /* read the exchange disk's files in again */
    HOT_FATOUT,      /* write the disk list's new things out */
    HOT_COPY,        /* lay a copy of the focused thing beside it */
    HOT_TFIND,       /* search inside the focused text */
    HOT_LINE_SAY,    /* send what the bottom row has gathered */
    HOT_ASSEMBLE,    /* turn the focused text into a program image */
    HOT_COMPILE,     /* turn the focused text of c into assembly and an image */
    HOT_BUILD,       /* compile, assemble and link everything in the focused list */
    HOT_INSTALL,     /* make the focused kernel the one the next start runs */
    HOT_RESTART      /* save everything and start the machine again */
} hot_kind;

typedef struct {
    i32      x, y, w, h;
    hot_kind kind;
    u32      index;
} hot_region;

#define HOT_MAX 256
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
/* Scrolling                                                           */
/* ------------------------------------------------------------------ */

/* Every area that can hold more than it shows scrolls the same way:
 * the wheel over it, the clickable edge beside it, and a thumb that
 * says where one is. One table holds them all, keyed by which area,
 * so a page, a text, a listing and a panel are not four inventions. */
enum {
    SCR_HTML,        /* the rendered page (kept in nav.html_scroll) */
    SCR_TEXT,        /* the focused text */
    SCR_BYTES,       /* the byte view */
    SCR_STRUCT,      /* the structure lens */
    SCR_CONTENTS,    /* the references panel on the right */
    SCR_INDEX,       /* the index listing */
    SCR_TEXT2,       /* the second pane's text, in the split */
    SCR_TERM,        /* the terminal's transcript */
    SCR_COUNT
};

/* Which area the text lens scrolls in: the second pane redirects
 * this while it draws, so both halves of a split keep their own
 * place in their own texts. */
static u8 cur_text_area = SCR_TEXT;

static u32 scrolls[SCR_COUNT];      /* SCR_HTML aliases nav.html_scroll */
static u32 scr_rows[SCR_COUNT];
static u32 scr_vis[SCR_COUNT];

/* Where each area was drawn this frame, for the wheel to find. */
typedef struct { i32 x, y, w, h; u8 area; } scr_target;
static scr_target scr_targets[8];
static u32 scr_target_count;

static u32 *scroll_var(u8 area)
{
    return area == SCR_HTML ? &nav.html_scroll : &scrolls[area];
}

/* The journal reads newest-last, so it opens at its end and stays
 * there while new lines come -- until one scrolls up to read back,
 * which unpins it. */
static bool log_pinned = true;

static void scroll_by(u8 area, i32 delta_rows)
{
    if (area >= SCR_COUNT) return;
    u32 *s = scroll_var(area);
    u32 max = scr_rows[area] > scr_vis[area]
            ? scr_rows[area] - scr_vis[area] : 0;
    i64 v = (i64)*s + delta_rows;
    if (v < 0) v = 0;
    if (v > (i64)max) v = (i64)max;
    if ((u32)v != *s) {
        *s = (u32)v;
        nav.redraw = true;
    }

    /* Scrolling up in the journal means reading back: stop following
     * the newest line until one returns to the end. */
    if (area == SCR_TEXT && delta_rows < 0 &&
        focus() == journal_object())
        log_pinned = false;
}

/* Notes an area's size and place, clamps its scroll, and -- when there
 * is more than fits -- draws the thumb and offers the edge: upper half
 * back, lower half onward, the same gesture everywhere. */
static void scroll_area(u8 area, i32 x, i32 y, i32 w, i32 h,
                        u32 rows, u32 vis)
{
    if (area >= SCR_COUNT) return;
    scr_rows[area] = rows;
    scr_vis[area] = vis;

    u32 *s = scroll_var(area);
    u32 max = rows > vis ? rows - vis : 0;
    if (*s > max) *s = max;

    if (scr_target_count < 8)
        scr_targets[scr_target_count++] = (scr_target){ x, y, w, h, area };

    if (rows <= vis || h <= 0) return;

    i32 tx = x + w - 6;
    fb_rect(tx, y, 2, h, C_EDGE);
    i32 th = (i32)((u64)h * vis / rows);
    if (th < 12) th = 12;
    i32 tp = (i32)((u64)(h - th) * *s / max);
    fb_rect(tx - 1, y + tp, 4, th, C_DIM);

    hot_add(x + w - 2 * GLYPH_W, y, 2 * GLYPH_W, h / 2,
            HOT_SCROLL, (u32)area * 2);
    hot_add(x + w - 2 * GLYPH_W, y + h / 2, 2 * GLYPH_W, h - h / 2,
            HOT_SCROLL, (u32)area * 2 + 1);
}

/* The wheel scrolls whatever it is over. Last drawn wins, like the
 * clicks. */
static void wheel_at(i32 mx, i32 my, i32 dz)
{
    for (i32 i = (i32)scr_target_count - 1; i >= 0; i--) {
        if (mx < scr_targets[i].x ||
            mx >= scr_targets[i].x + scr_targets[i].w ||
            my < scr_targets[i].y ||
            my >= scr_targets[i].y + scr_targets[i].h)
            continue;
        scroll_by(scr_targets[i].area, dz * 3);
        return;
    }
}

/* Typing and pasting pull the window to the caret; plain redraws do
 * not, so reading far from the caret is not snatched away. */
static bool caret_chase;

/* Searching inside the focused text: the needle, whether the search
 * is open, and how many places answered last frame. */
static bool tfind_on;
static char tfind_buf[40];
static u32  tfind_len;
static u32  tfind_count;
static char to_lower(char c);

/* Speaking on the line: the letters gathered in the bottom row while
 * the line is in focus, sent whole on enter. The pipe caps a word at
 * two hundred letters, and so does this. */
static char say_buf[201];
static u32  say_len;

static u64 text_len(const u8 *d, u64 size);

/* Lays a fresh object into a holder: first hole or a grown one. The
 * slot takes its own reference. */
static bool lay_beside(object *holder, object *made, u32 rights, const char *nm)
{
    u64 slots = obj_slots(holder), spot = slots;
    for (u64 i = 0; i < slots; i++)
        if (!obj_get_slot(holder, i)) { spot = i; break; }
    if (spot == slots && !obj_grow_slots(holder, slots + 1)) return false;
    obj_set_slot(holder, spot, made, rights);
    obj_set_slot_name(holder, spot, nm);
    obj_touch(holder);
    return true;
}

/* Whether these bytes begin the way an elf does: a kernel, as far as
 * the shell can tell; the installer looks closer. */
static bool elf_smells(object *o)
{
    if (!o || obj_type(o) != TYPE_BYTES || obj_size(o) < 64) return false;
    const u8 *d = (const u8 *)obj_data(o);
    return d && d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F';
}

/* Whether a list holds anything the tools would build: a text of c
 * or assembly, or an object. */
static bool list_buildable(object *l)
{
    for (u64 i = 0; i < obj_slots(l); i++) {
        object *t = obj_get_slot(l, i);
        if (!t) continue;
        if (obj_type(t) == TYPE_BYTES && ld_object_ok((const u8 *)obj_data(t), obj_size(t))) return true;
        if (obj_type(t) != TYPE_TEXT) continue;
        const char *nm = obj_slot_name(l, i);
        if (!nm) nm = obj_name(t);
        if (!nm) continue;
        u32 n = (u32)strlen(nm);
        if (n > 2 && nm[n - 2] == '.' && (nm[n - 1] == 'c' || nm[n - 1] == 'S' || nm[n - 1] == 's')) return true;
    }
    return false;
}

/* #include "name" reaches the texts lying beside the source, in the
 * holder one came through, by petname. */
static bool find_beside_in(void *ctx, const char *name, const u8 **text, u64 *len)
{
    object *holder = (object *)ctx;
    for (u64 i = 0; i < obj_slots(holder); i++) {
        object *t = obj_get_slot(holder, i);
        if (!t || obj_type(t) != TYPE_TEXT) continue;
        const char *nm = obj_slot_name(holder, i);
        if (!nm) nm = obj_name(t);
        if (!nm) continue;
        /* <eb/types.h> asks for types.h: the way there is not a thing here */
        const char *want = name;
        for (const char *q = name; *q; q++) if (*q == '/') want = q + 1;
        u32 j = 0;
        while (nm[j] && want[j] && to_lower(nm[j]) == to_lower(want[j])) j++;
        if (nm[j] || want[j]) continue;
        *text = (const u8 *)obj_data(t);
        *len = text_len(*text, obj_size(t));
        return true;
    }
    return false;
}

/* Says the gathered word and clears the row. Enter and the send word
 * both land here, so the click and the key cannot drift apart. */
static void say_commit(void)
{
    if (!say_len) return;
    say_buf[say_len] = 0;
    pipe_say(say_buf);
    say_len = 0;
    nav.redraw = true;
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

/* Letters lifted out of one text, waiting to be set down in another.
 * The shell carries them, not any object, so they survive every step
 * of the walk -- including a walk into the past, which is how a line
 * from an old generation gets back into the present. */
static char held_text[256];
static u32  held_len;

/* The marked stretch, ordered and clamped to the text in focus. */
static void marked_range(u64 len, u64 *lo, u64 *hi)
{
    *lo = nav.sel_a < nav.sel_b ? nav.sel_a : nav.sel_b;
    *hi = nav.sel_a < nav.sel_b ? nav.sel_b : nav.sel_a;
    if (*lo > len) *lo = len;
    if (*hi > len) *hi = len;
}

/* Removes the marked letters, closes the gap, and leaves the caret
 * where they stood. Answers whether anything was removed. */
static bool cut_marked(u8 *d, u64 size, u64 *len)
{
    u64 lo, hi;
    marked_range(*len, &lo, &hi);
    if (lo >= hi) return false;

    u64 gap = hi - lo;
    for (u64 i = lo; i + gap < size; i++) d[i] = d[i + gap];
    for (u64 i = size - gap; i < size; i++) d[i] = 0;

    *len -= gap;
    nav.caret = lo;
    nav.sel_a = nav.sel_b = 0;
    return true;
}

/* What the palette offers. The fixed entries make something new; the
 * rest are objects already in hand, so pointing at something that
 * exists needs no dragging and no second window. */
#define PALETTE_FIXED 7
#define CARRY_MAX 24

/* What a picture is born as: room enough to draw in, small enough to
 * snapshot without a thought. */
#define PICTURE_W 160
#define PICTURE_H 100

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
    case TYPE_PICTURE: {
        u32 pw, ph;
        if (picture_shape(o, &pw, &ph)) {
            at = put_dec(out, at, pw);
            at = put(out, at, " x ");
            at = put_dec(out, at, ph);
        } else {
            at = put(out, at, "picture");
        }
        break;
    }
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
    u32 scroll;
} text_area;

static void lens_text(object *o, i32 x, i32 y, i32 w, i32 h, bool caret)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(nothing to show)", C_FAINT); return; }

    u64 len = text_len(d, obj_size(o));
    i32 cols = w / GLYPH_W;
    i32 vis = h / GLYPH_H;
    if (cols < 1) cols = 1;
    if (vis < 1) vis = 1;

    if (nav.caret > len) nav.caret = len;

    /* One quick pass for the shape of the flow: how many wrapped rows
     * there are, and which one the caret is on. */
    u32 rows, crow = 0;
    {
        i32 mx = 0;
        u32 my = 0;
        for (u64 i = 0; i < len; i++) {
            if (caret && i == nav.caret) crow = my;
            u8 ch = d[i];
            if (ch == '\n' || mx >= cols) {
                mx = 0; my++;
                if (ch == '\n') continue;
            }
            mx++;
        }
        if (caret && nav.caret == len) crow = my;
        rows = my + 1;
    }

    bool scrollable = caret || cur_text_area == SCR_TEXT2;
    u32 scroll = 0;
    if (scrollable) {
        u32 *s = &scrolls[cur_text_area];
        u32 max = rows > (u32)vis ? rows - (u32)vis : 0;

        /* The journal opens at its end and stays pinned there while
         * lines come, until one scrolls back to read. */
        if (o == journal_object()) {
            if (log_pinned) *s = max;
            else if (*s >= max) log_pinned = true;
        } else if (caret && caret_chase) {
            if (crow < *s) *s = crow;
            else if (crow >= *s + (u32)vis) *s = crow - (u32)vis + 1;
        }
        if (caret) caret_chase = false;
        if (*s > max) *s = max;
        scroll = *s;
    }

    u64 lo = 0, hi = 0;
    if (caret) {
        text_area.x = x;
        text_area.y = y;
        text_area.cols = cols;
        text_area.rows = vis;
        text_area.scroll = scroll;
        hot_add(x, y, cols * GLYPH_W, h, HOT_TEXT, 0);
        marked_range(len, &lo, &hi);
    }

    /* Where the search's needle lies in this text, found fresh each
     * frame so edits and the needle stay in step. */
    u32 fstarts[128];
    u32 fnum = 0, fk = 0;
    if (caret && tfind_on && tfind_len) {
        for (u64 i = 0; i + tfind_len <= len && fnum < 128; i++) {
            u32 j = 0;
            while (j < tfind_len &&
                   to_lower((char)d[i + j]) == to_lower(tfind_buf[j]))
                j++;
            if (j == tfind_len) fstarts[fnum++] = (u32)i;
        }
        tfind_count = fnum;
    } else if (caret) {
        tfind_count = 0;
    }

    /* The caret bar is drawn only where typing would land something --
     * marking and lifting work in any readable text, the bar does not
     * pretend more than that. */
    bool bar = caret && (focus_rights() & CAP_WRITE) &&
               nav.at_generation == 0;

    /* The settings read as a table, and they are drawn as one: the
     * matter column dimmed, the bar fainter still, the value in full
     * ink -- the eye goes to what can be changed. The characters stay
     * exactly where plain drawing would put them, so the caret and the
     * click arithmetic need no special case. */
    bool table = (o == settings_object());
    bool past_bar = false;

    i32 cx = 0;
    u32 cy = 0;
    for (u64 i = 0; i <= len && cy < scroll + (u32)vis; i++) {
        u8 ch = (i < len) ? d[i] : 0;
        bool on = (cy >= scroll);
        i32 py = y + (i32)(cy - scroll) * GLYPH_H;

        if (bar && i == nav.caret && on)
            fb_rect(x + cx * GLYPH_W - 1, py, 2, GLYPH_H, C_ACCENT);
        if (i == len) break;

        if (ch == '\n' || cx >= cols) {
            if (i >= lo && i < hi && ch == '\n' && on)
                fb_rect(x + cx * GLYPH_W, py, GLYPH_W / 2, GLYPH_H,
                        C_EDGE);
            cx = 0; cy++;
            past_bar = false;
            if (ch == '\n') continue;
            on = (cy >= scroll);
            py = y + (i32)(cy - scroll) * GLYPH_H;
        }
        if (cy >= scroll + (u32)vis) break;

        if (i >= lo && i < hi && on)
            fb_rect(x + cx * GLYPH_W, py, GLYPH_W, GLYPH_H, C_EDGE);

        if (fnum) {
            while (fk < fnum && i >= fstarts[fk] + tfind_len) fk++;
            if (fk < fnum && i >= fstarts[fk] && on)
                fb_rect(x + cx * GLYPH_W, py, GLYPH_W, GLYPH_H, C_EDGE);
        }

        color c = C_TEXT;
        if (table) {
            if (ch == '|') { c = C_FAINT; past_bar = true; }
            else if (!past_bar) c = C_DIM;
        }
        if (on) fb_glyph(x + cx * GLYPH_W, py, ch, c, 0, false);
        cx++;
    }

    if (scrollable) scroll_area(cur_text_area, x, y, w, h, rows,
                                (u32)vis);
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

/* The sixteen inks live in fb.c now, shared with the web server's
 * bmp writer -- a picture keeps its colours whatever the shell is
 * wearing, and whoever serves it says the same colours. */
#define inks fb_inks

static u8 ink_current = 1;               /* dark ink on paper */

/* Where the canvas was last drawn, for turning a pointer position back
 * into a cell. One canvas is on screen at a time, like the text. */
static struct {
    i32 x, y, scale;
    u32 w, h;
} canvas;

static bool picture_shape(object *o, u32 *w, u32 *h)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d || obj_size(o) < PICTURE_HEADER) return false;
    u32 pw = (u32)d[0] | ((u32)d[1] << 8) |
             ((u32)d[2] << 16) | ((u32)d[3] << 24);
    u32 ph = (u32)d[4] | ((u32)d[5] << 8) |
             ((u32)d[6] << 16) | ((u32)d[7] << 24);
    if (pw == 0 || ph == 0) return false;
    if ((u64)pw * ph + PICTURE_HEADER > obj_size(o)) return false;
    *w = pw;
    *h = ph;
    return true;
}

static void lens_paint(object *o, i32 x, i32 y, i32 w, i32 h, bool live)
{
    u32 pw, ph;
    if (!picture_shape(o, &pw, &ph)) {
        text_at(x, y, x + w, "(no picture in these bytes)", C_FAINT);
        return;
    }
    const u8 *d = (const u8 *)obj_data(o);

    bool may = live && (focus_rights() & CAP_WRITE) &&
               nav.at_generation == 0;

    /* The inks, offered where the hand already is. Only when drawing
     * is possible: a read-only picture is a picture, not a tool. */
    i32 py = y;
    if (may) {
        for (u32 i = 0; i < 16; i++) {
            i32 cx = x + (i32)i * 26;
            bool on = (ink_current == i);
            bool lit = is_hovered(HOT_INK, i);
            if (on || lit)
                fb_rect(cx - 2, py - 2, 24, 24, on ? C_ACCENT : C_DIM);
            fb_rect(cx, py, 20, 20, inks[i]);
            hot_add(cx - 2, py - 2, 24, 24, HOT_INK, i);
        }
        py += 32;
    }

    i32 scale_x = w / (i32)pw;
    i32 scale_y = (h - (py - y) - 4) / (i32)ph;
    i32 scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1) scale = 1;
    if (scale > 6) scale = 6;

    fb_rect(x - 1, py - 1, (i32)pw * scale + 2, (i32)ph * scale + 2,
            C_EDGE);
    for (u32 row = 0; row < ph; row++) {
        if (py + (i32)(row + 1) * scale > y + h) break;
        for (u32 col = 0; col < pw; col++)
            fb_rect(x + (i32)col * scale, py + (i32)row * scale,
                    scale, scale,
                    inks[d[PICTURE_HEADER + (u64)row * pw + col] & 15]);
    }

    if (may) {
        canvas.x = x;
        canvas.y = py;
        canvas.scale = scale;
        canvas.w = pw;
        canvas.h = ph;
        hot_add(x, py, (i32)pw * scale, (i32)ph * scale, HOT_CANVAS, 0);
    }
}

/* Where the last stroke left off, so a fast hand draws a line rather
 * than a trail of separate cells -- the pointer reports positions, but
 * the gesture between them was a stroke. */
static i32 paint_last_col = -1, paint_last_row = -1;

static void paint_cell(u8 *d, u64 size, i32 col, i32 row)
{
    if (col < 0 || row < 0 || col >= (i32)canvas.w || row >= (i32)canvas.h)
        return;
    u64 at = PICTURE_HEADER + (u64)row * canvas.w + (u64)col;
    if (at >= size || d[at] == ink_current) return;
    d[at] = ink_current;
    nav.changes++;
    nav.redraw = true;
}

/* Paints under the pointer, and everything between here and the last
 * cell of the same stroke. */
static void paint_at_pointer(void)
{
    object *f = focus();
    if (obj_type(f) != TYPE_PICTURE) return;
    if (!(focus_rights() & CAP_WRITE) || nav.at_generation != 0) return;

    u8 *d = (u8 *)obj_data(f);
    if (!d || canvas.scale == 0) return;

    i32 col = (nav.mouse_x - canvas.x) / canvas.scale;
    i32 row = (nav.mouse_y - canvas.y) / canvas.scale;

    i32 c0 = nav.paint_drag ? paint_last_col : col;
    i32 r0 = nav.paint_drag ? paint_last_row : row;
    if (c0 < 0 || r0 < 0) { c0 = col; r0 = row; }

    i32 dc = col - c0, dr = row - r0;
    i32 steps = (dc < 0 ? -dc : dc) > (dr < 0 ? -dr : dr)
              ? (dc < 0 ? -dc : dc) : (dr < 0 ? -dr : dr);
    if (steps == 0) steps = 1;
    for (i32 i = 0; i <= steps; i++)
        paint_cell(d, obj_size(f), c0 + dc * i / steps, r0 + dr * i / steps);

    paint_last_col = col;
    paint_last_row = row;
}

/* ------------------------------------------------------------------ */
/* The html lens: a fetched page as prose                              */
/* ------------------------------------------------------------------ */

/* What the last render found, for turning clicks back into links. */
static char      link_urls[HTML_LINKS_MAX][HTML_URL_MAX];
static u32       link_count;
static html_spot link_spots[HTML_SPOTS_MAX];
static u32       link_spot_count;
static u32       html_rows;         /* the whole flow */
static u32       html_vis;          /* rows the window holds */

/* Forms found in the page, and what has been typed into them. The
 * values persist across redraws so typing survives the clock's tick;
 * they reset to the markup's defaults when the page itself changes,
 * which is noticed by the shape of the fields changing. */
static html_form  form_defs[HTML_FORMS_MAX];
static u32         form_count;
static html_field  field_defs[HTML_FIELDS_MAX];
static u32         field_count;
static char        field_val[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static char        field_init[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static html_spot   field_spots[HTML_SPOTS_MAX];
static u32         field_spot_count;
static i32         field_focus = -1;
static u32         field_shape;     /* a print of the fields, to spot change */
static bool        addr_edit;       /* the address line is being typed */

/* The recipient chooser: opens under the send word when no peer is
 * set yet, scanning as it opens, so choosing whom happens where the
 * sending was asked for. When it was ask that opened it, the chosen
 * machine gets the work instead of the copy. */
static bool sendto_open;
static bool sendto_ask;
static i32  sendto_x;

/* Where the browser has been, so it can go back. Addresses only; the
 * bodies are re-fetched, which is honest -- the page may have moved
 * on, and a browser that lied about that would be worse than one that
 * asked again. */
#define HISTORY_DEPTH 24
static char browse_back[HISTORY_DEPTH][HTML_URL_MAX];
static u32  browse_back_count;
static char browse_fwd[HISTORY_DEPTH][HTML_URL_MAX];
static u32  browse_fwd_count;

static void ask_the_wire(void);

/* The current ask, copied out. */
static u32 current_ask(object *o, char *out, u32 max)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { out[0] = 0; return 0; }
    u64 len = text_len(d, obj_size(o));
    u32 n = 0;
    while (n < len && d[n] != '\n' && n < max - 1) { out[n] = (char)d[n]; n++; }
    out[n] = 0;
    return n;
}

/* Replaces the focused page's ask line and asks the wire for it. The
 * body below the line is cleared; the scroll returns to the top. */
static void browse_to(const char *addr)
{
    object *f = focus();
    if (obj_type(f) != TYPE_TEXT || nav.at_generation != 0) return;
    if (!(focus_rights() & CAP_WRITE)) return;
    u8 *d = (u8 *)obj_data(f);
    u64 size = obj_size(f);
    if (!d) return;

    u32 n = 0;
    while (addr[n] && n < size - 2) { d[n] = (u8)addr[n]; n++; }
    d[n++] = '\n';
    for (u64 i = n; i < size; i++) d[i] = 0;

    nav.html_scroll = 0;
    field_focus = -1;
    addr_edit = false;
    nav.changes++;
    ask_the_wire();
}

/* Remembers where we stand before stepping somewhere new. */
static void browse_remember(void)
{
    object *f = focus();
    if (obj_type(f) != TYPE_TEXT) return;
    char here[HTML_URL_MAX];
    if (current_ask(f, here, sizeof(here)) == 0) return;

    if (browse_back_count &&
        strcmp(browse_back[browse_back_count - 1], here) == 0) return;
    if (browse_back_count >= HISTORY_DEPTH) {
        for (u32 i = 1; i < HISTORY_DEPTH; i++)
            memcpy(browse_back[i - 1], browse_back[i], HTML_URL_MAX);
        browse_back_count--;
    }
    memcpy(browse_back[browse_back_count++], here, HTML_URL_MAX);
}

/* Whether a text reads like a page from outside: an address alone on
 * the first line, markup somewhere below. The lens is offered either
 * way; this only decides what a text opens as. */
static bool smells_like_page(object *o)
{
    if (obj_type(o) != TYPE_TEXT) return false;
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) return false;

    u64 len = text_len(d, obj_size(o));
    u64 i = 0;
    bool dot = false;
    while (i < len && d[i] != '\n') {
        if (d[i] == ' ') return false;
        if (d[i] == '.') dot = true;
        i++;
    }
    if (!dot || i == 0 || i + 1 >= len) return false;
    for (u64 j = i; j < len; j++) if (d[j] == '<') return true;
    return false;
}

/* The current ask, split for resolving relative links. */
static void ask_parts(const u8 *d, u64 ask, char *host, u32 hmax,
                      u32 *hlen, char *path, u32 pmax, u32 *plen)
{
    u64 at = 0;
    if (ask >= 7 && d[4] == ':' && d[5] == '/' && d[6] == '/') at = 7;
    else if (ask >= 8 && d[5] == ':' && d[6] == '/' && d[7] == '/') at = 8;

    *hlen = 0;
    *plen = 0;
    while (at < ask && d[at] != '/' && *hlen < hmax - 1)
        host[(*hlen)++] = (char)d[at++];
    while (at < ask && *plen < pmax - 1)
        path[(*plen)++] = (char)d[at++];
}

/* A cheap print of the field set, so a page changing under us resets
 * what was typed while typing within one page survives a redraw. */
static u32 fields_print(void)
{
    u32 p = field_count * 2654435761u;
    for (u32 i = 0; i < field_count; i++) {
        p ^= field_defs[i].kind + 1u;
        for (u32 j = 0; field_defs[i].name[j]; j++)
            p = p * 31u + (u8)field_defs[i].name[j];
    }
    return p;
}

/* A small clickable word in the address strip. Returns its right edge. */
static i32 strip_word(i32 x, i32 y, const char *s, bool on, hot_kind k)
{
    i32 n = 0;
    while (s[n]) n++;
    bool lit = is_hovered(k, 0);
    if (lit) fb_rect(x - 3, y - 3, n * GLYPH_W + 6, ROW, C_EDGE);
    text_at(x, y, x + (n + 1) * GLYPH_W, s, on ? (lit ? C_TEXT : C_WRITE)
                                              : C_FAINT);
    if (on) hot_add(x - 3, y - 3, n * GLYPH_W + 6, ROW, k, 0);
    return x + n * GLYPH_W + 2 * GLYPH_W;
}

/* Turns a link -- absolute, host-relative, or relative to the room the
 * page was in -- into the "host/path" an ask line holds. Schemes are
 * dropped: the wire adds its own, and https means "the same place,
 * asked plainly" here. */
static void resolve_url(const char *base, const char *href,
                        char *out, u32 max)
{
    char host[128], path[256];
    u32 hlen, plen;
    u32 blen = 0;
    while (base[blen]) blen++;
    ask_parts((const u8 *)base, blen, host, sizeof(host), &hlen,
              path, sizeof(path), &plen);

    u32 nn = 0;
    const char *u = href;
    if (u[0]=='h' && u[1]=='t' && u[2]=='t' && u[3]=='p') {
        u32 skip = (u[4] == 's') ? 8 : 7;
        while (u[skip] && nn < max - 1) out[nn++] = u[skip++];
    } else if (u[0] == '/' && u[1] == '/') {
        u32 skip = 2;
        while (u[skip] && nn < max - 1) out[nn++] = u[skip++];
    } else if (u[0] == '/') {
        for (u32 i = 0; i < hlen && nn < max - 1; i++) out[nn++] = host[i];
        for (u32 i = 0; u[i] && nn < max - 1; i++) out[nn++] = u[i];
    } else {
        u32 dir = 0;
        for (u32 i = 0; i < plen; i++) if (path[i] == '/') dir = i + 1;
        for (u32 i = 0; i < hlen && nn < max - 1; i++) out[nn++] = host[i];
        if (dir == 0 && nn < max - 1) out[nn++] = '/';
        for (u32 i = 0; i < dir && nn < max - 1; i++) out[nn++] = path[i];
        for (u32 i = 0; u[i] && nn < max - 1; i++) out[nn++] = u[i];
    }
    out[nn] = 0;
}

/* Sends a form: gathers its fields into a query, hangs it off the
 * action, and browses there. GET only, which is what search boxes
 * speak; a page that insists on POST is answered plainly and may not
 * like it, and that limit is the readme's to own. */
static void submit_form(u32 field_index)
{
    object *f = focus();
    if (obj_type(f) != TYPE_TEXT || nav.at_generation != 0) return;
    if (!(focus_rights() & CAP_WRITE)) return;
    if (field_index >= field_count) return;
    u32 form = field_defs[field_index].form;

    char query[512];
    u32 q = 0;
    bool first = true;
    for (u32 i = 0; i < field_count && q < sizeof(query) - 8; i++) {
        if (field_defs[i].form != form) continue;
        if (field_defs[i].kind == FIELD_SUBMIT) continue;
        if (field_defs[i].name[0] == 0) continue;

        if (!first) query[q++] = '&';
        first = false;
        for (u32 j = 0; field_defs[i].name[j] && q < sizeof(query) - 4; j++)
            query[q++] = field_defs[i].name[j];
        query[q++] = '=';
        const char *val = field_val[i];
        for (u32 j = 0; val[j] && q < sizeof(query) - 4; j++) {
            char c = val[j];
            bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') ||
                         c == '-' || c == '_' || c == '.' || c == '~';
            if (plain) query[q++] = c;
            else if (c == ' ') query[q++] = '+';
            else {
                static const char hex[] = "0123456789ABCDEF";
                query[q++] = '%';
                query[q++] = hex[(u8)c >> 4];
                query[q++] = hex[(u8)c & 15];
            }
        }
    }
    query[q] = 0;

    char here[HTML_URL_MAX], dest[HTML_URL_MAX];
    current_ask(f, here, sizeof(here));

    const char *action = form < form_count ? form_defs[form].action : "";
    if (action[0])
        resolve_url(here, action, dest, sizeof(dest));
    else
        { u32 i = 0; while (here[i] && i < sizeof(dest) - 1) { dest[i] = here[i]; i++; } dest[i] = 0; }

    /* Trim any query the action or page already carried, then hang
     * ours on. */
    u32 dl = 0;
    while (dest[dl] && dest[dl] != '?') dl++;
    if (dl < sizeof(dest) - 1) {
        dest[dl++] = '?';
        for (u32 i = 0; query[i] && dl < sizeof(dest) - 1; i++)
            dest[dl++] = query[i];
        dest[dl] = 0;
    }

    browse_remember();
    browse_fwd_count = 0;
    browse_to(dest);
}

static void lens_html(object *o, i32 x, i32 y, i32 w, i32 h, bool live)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(nothing to show)", C_FAINT); return; }

    u64 len = text_len(d, obj_size(o));
    u64 ask = 0;
    while (ask < len && d[ask] != '\n') ask++;

    bool may_ask = live && nav.at_generation == 0 && net_up() &&
                   (focus_rights() & CAP_READ) &&
                   (focus_rights() & CAP_WRITE);

    /* The strip: back, forward, the address, and go. */
    i32 sx = x;
    if (live) {
        sx = strip_word(sx, y, "<", browse_back_count > 0, HOT_BACK);
        sx = strip_word(sx, y, ">", browse_fwd_count > 0, HOT_FWD);
    }

    char addr[96];
    u32 an = 0;
    while (an < sizeof(addr) - 1 && an < ask) { addr[an] = (char)d[an]; an++; }
    addr[an] = 0;
    i32 addr_w = x + w - sx - 4 * GLYPH_W;
    if (addr_edit) fb_rect(sx - 3, y - 3, addr_w, ROW, C_PANEL_HI);
    text_at(sx, y, sx + addr_w, addr, addr_edit ? C_TEXT : C_ACCENT);
    if (addr_edit)
        fb_rect(sx + (i32)an * GLYPH_W, y, 2, GLYPH_H, C_ACCENT);
    if (may_ask)
        hot_add(sx - 3, y - 3, addr_w, ROW, HOT_ADDR, 0);

    if (may_ask) {
        i32 gx = x + w - 2 * GLYPH_W - 4;
        bool lit = is_hovered(HOT_GO, 0);
        if (lit) fb_rect(gx - 4, y - 3, 2 * GLYPH_W + 8, ROW, C_EDGE);
        text_at(gx, y, x + w, "go", lit ? C_TEXT : C_WRITE);
        hot_add(gx - 4, y - 3, 2 * GLYPH_W + 8, ROW, HOT_GO, 0);
    }

    /* How the last page came: sealed says the channel was encrypted;
     * the reminder that the seal is not yet an identity is the readme's
     * to keep, and the word here is deliberately "sealed", not "safe". */
    if (net_last_secure()) {
        const char *m = "sealed";
        i32 mw = 6 * GLYPH_W;
        text_at(x + w - 4 * GLYPH_W - mw - 8, y, x + w, m, C_WRITE);
    }
    fb_rect(x, y + ROW - 4, w, 1, C_EDGE);

    i32 by = y + ROW + 2;
    i32 bh = h - ROW - 2;

    if (ask + 1 >= len) {
        text_at(x, by, x + w, may_ask
                ? "nothing here yet. the first line asks; go fetches."
                : "nothing here yet.", C_FAINT);
        html_rows = 0;
        if (live) { link_spot_count = 0; field_spot_count = 0; }
        return;
    }

    html_sink sink = {
        .urls = link_urls, .url_count = &link_count,
        .link_spots = link_spots, .link_spot_count = &link_spot_count,
        .forms = form_defs, .form_count = &form_count,
        .fields = field_defs, .field_count = &field_count,
        .field_spots = field_spots, .field_spot_count = &field_spot_count,
        .field_values = field_val, .field_init = field_init,
    };

    html_view v = {
        .src = d + ask + 1,
        .len = len - ask - 1,
        .x = x, .y = by, .w = w - 2 * GLYPH_W, .h = bh,
        .scroll = nav.html_scroll,
        .col = { C_TEXT, C_DIM, C_FAINT, C_ACCENT, C_EDGE },
    };

    html_rows = html_render(&v, live ? &sink : NULL);
    html_vis = (u32)(bh / GLYPH_H);

    if (live) {
        /* A page that changed shape is a new page: take its defaults,
         * and let go of any field the writing was in. */
        u32 print = fields_print();
        if (print != field_shape) {
            field_shape = print;
            field_focus = -1;
            for (u32 i = 0; i < field_count; i++)
                memcpy(field_val[i], field_init[i], HTML_VALUE_MAX);
        }
    }

    /* Links and buttons carry only as far as writing does: following
     * one rewrites the ask, and a page held read-only is a page, not a
     * doorway. */
    if (live && may_ask) {
        for (u32 i = 0; i < link_spot_count; i++)
            hot_add(link_spots[i].x, link_spots[i].y - 2,
                    link_spots[i].w, link_spots[i].h + 4, HOT_LINK, i);
        for (u32 i = 0; i < field_spot_count; i++) {
            html_spot *sp = &field_spots[i];
            u32 fi = sp->ref;
            bool submit = fi < field_count &&
                          field_defs[fi].kind == FIELD_SUBMIT;
            hot_add(sp->x, sp->y, sp->w, sp->h,
                    submit ? HOT_SUBMIT : HOT_FIELD, i);

            /* The caret sits after the writing in the field one is in. */
            if (!submit && (i32)i == field_focus) {
                u32 vl = 0;
                while (field_val[fi][vl]) vl++;
                i32 cx = sp->x + 2 + (i32)vl * GLYPH_W;
                if (cx < sp->x + sp->w - 2)
                    fb_rect(cx, sp->y + 3, 2, GLYPH_H, C_ACCENT);
            }
        }
    } else if (live) {
        link_spot_count = 0;
        field_spot_count = 0;
    }

    scroll_area(SCR_HTML, x, by, w, bh, html_rows, html_vis);
}

static void lens_bytes(object *o, i32 x, i32 y, i32 w, i32 h)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(no payload)", C_FAINT); return; }

    u64 size = obj_size(o);
    i32 per = (w / GLYPH_W - 7) / 3;
    if (per > 16) per = 16;
    if (per < 4) per = 4;

    u32 total = (u32)((size + (u64)per - 1) / (u64)per);
    u32 vis = (u32)(h / GLYPH_H);
    if (vis < 1) vis = 1;
    u32 scroll = scrolls[SCR_BYTES];
    if (total > vis && scroll > total - vis) scroll = total - vis;
    if (total <= vis) scroll = 0;

    const char *hex = "0123456789abcdef";
    for (u32 row = 0; row < vis; row++) {
        u64 base = (u64)(scroll + row) * (u64)per;
        if (base >= size) break;

        char line[96];
        u32 at = 0;
        line[at++] = hex[(base >> 12) & 0xF];
        line[at++] = hex[(base >> 8) & 0xF];
        line[at++] = hex[(base >> 4) & 0xF];
        line[at++] = hex[base & 0xF];
        line[at++] = ' ';
        for (i32 i = 0; i < per && base + (u64)i < size; i++) {
            u8 b = d[base + i];
            line[at++] = hex[b >> 4];
            line[at++] = hex[b & 0xF];
            line[at++] = ' ';
        }
        line[at] = 0;
        text_at(x, y + (i32)row * GLYPH_H, x + w, line, C_DIM);
    }

    scroll_area(SCR_BYTES, x, y, w, h, total, vis);
}

static void lens_structure(object *o, i32 x, i32 y, i32 w, i32 h)
{
    char line[96];
    u32 scroll = scrolls[SCR_STRUCT];
    i32 ty = y - (i32)scroll * ROW;
    u32 at;

    /* Everything below advances ty whether or not the row lands in
     * the window; drawing and click regions are offered only where it
     * does. That one rule is the whole scrolling. */
#define ROW_ON (ty >= y && ty + GLYPH_H <= y + h)

    if (ROW_ON) {
        text_at(x, ty, x + w, "type", C_DIM);
        text_at(x + 9 * GLYPH_W, ty, x + w, type_name(obj_type(o)),
                type_color(obj_type(o)));
    }
    ty += ROW;

    at = put(line, 0, "identity ");
    at = put_dec(line, at, obj_id(o));
    line[at] = 0;
    if (ROW_ON) text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    at = put(line, 0, "held by  ");
    at = put_dec(line, at, obj_refs(o));
    at = put(line, at, " references");
    line[at] = 0;
    if (ROW_ON) text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    at = put(line, 0, "payload  ");
    at = put_dec(line, at, obj_size(o));
    at = put(line, at, " bytes");
    line[at] = 0;
    if (ROW_ON) text_at(x, ty, x + w, line, C_DIM);
    ty += ROW;

    /* For a program, the one fact that changes everything about it. */
    if (obj_type(o) == TYPE_PROGRAM) {
        bool alive = proc_is_running(o);
        if (ROW_ON) {
            text_at(x, ty, x + w, "state", C_DIM);
            text_at(x + 9 * GLYPH_W, ty, x + w,
                    alive ? "running" : "ended",
                    alive ? C_WRITE : C_READONLY);
        }
        ty += ROW;
    }
    ty += ROW / 2;

    /* What it points at. Empty slots are counted, not listed: eight
     * lines of "empty" say nothing that one quiet line does not. */
    u64 filled = 0, vacant = 0;
    for (u64 i = 0; i < obj_slots(o); i++)
        if (obj_get_slot(o, i)) filled++; else vacant++;

    if (filled == 0) {
        if (ROW_ON) text_at(x, ty, x + w, "no references", C_FAINT);
        ty += ROW;
    } else {
        if (ROW_ON) text_at(x, ty, x + w, "references", C_DIM);
        ty += ROW;
        for (u64 i = 0; i < obj_slots(o); i++) {
            object *t = obj_get_slot(o, i);
            if (!t) continue;

            if (ROW_ON) {
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
            }
            ty += ROW;
        }
    }
    if (vacant > 0) {
        at = put(line, 0, "  ");
        at = put_dec(line, at, vacant);
        at = put(line, at, " empty slots");
        line[at] = 0;
        if (ROW_ON) text_at(x, ty, x + w, line, C_FAINT);
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
    /* The arrivals list is the pipe's face, so standing on it shows
     * the pipe whole: where it points, who else is on the wire, and
     * the one word that looks for them. Connecting is a click on a
     * found machine -- it writes the peer line into the settings the
     * way a hand would, and the journal says so. */
    if (o == pipe_arrivals() && o == focus() && nav.at_generation == 0) {
        ty += ROW / 2;
        if (ROW_ON) {
            text_at(x, ty, x + w, "the pipe", C_DIM);

            bool lit = is_hovered(HOT_SCAN, 0);
            i32 sx2 = x + 10 * GLYPH_W;
            if (lit) fb_rect(sx2 - 4, ty - 3, 4 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(sx2, ty, x + w, "scan", lit ? C_TEXT : C_ACCENT);
            hot_add(sx2 - 4, ty - 3, 4 * GLYPH_W + 8, ROW, HOT_SCAN, 0);
        }
        ty += ROW;

        u8 pip[4];
        u16 ppt;
        if (ROW_ON) {
            if (settings_peer(pip, &ppt)) {
                at = put(line, 0, "  points at ");
                at = put_dec(line, at, pip[0]); line[at++] = '.';
                at = put_dec(line, at, pip[1]); line[at++] = '.';
                at = put_dec(line, at, pip[2]); line[at++] = '.';
                at = put_dec(line, at, pip[3]);
                line[at] = 0;
                text_at(x, ty, x + w, line, C_TEXT);
            } else {
                text_at(x, ty, x + w, "  points at nobody", C_FAINT);
            }
        }
        ty += ROW;

        u32 nfound = pipe_found_count();
        if (pipe_scanning() && nfound == 0) {
            if (ROW_ON) text_at(x, ty, x + w, "  listening...", C_FAINT);
            ty += ROW;
        }
        for (u32 i = 0; i < nfound; i++) {
            u8 fip[4];
            char fname[24];
            bool fworks;
            u32 fmib;
            if (!pipe_found_at(i, fip, fname, &fworks, &fmib)) break;

            if (ROW_ON) {
                at = put(line, 0, "  ");
                at = put(line, at, fname);
                while (at < 20) line[at++] = ' ';
                at = put_dec(line, at, fip[0]); line[at++] = '.';
                at = put_dec(line, at, fip[1]); line[at++] = '.';
                at = put_dec(line, at, fip[2]); line[at++] = '.';
                at = put_dec(line, at, fip[3]);
                if (fworks) {
                    while (at < 40) line[at++] = ' ';
                    at = put(line, at, "takes work, ");
                    at = put_dec(line, at, fmib);
                    at = put(line, at, "M free");
                }
                line[at] = 0;

                bool plit = is_hovered(HOT_PEERPICK, i);
                if (plit) fb_rect(x, ty - 3, w, ROW, C_EDGE);
                text_at(x, ty, x + w, line, plit ? C_TEXT : C_ACCENT);
                hot_add(x, ty - 3, w, ROW, HOT_PEERPICK, i);
            }
            ty += ROW;
        }
    }

    domain *pd = proc_domain_of(o);
    if (pd) {
        ty += ROW / 2;
        if (ROW_ON) text_at(x, ty, x + w, "capabilities", C_DIM);
        ty += ROW;

        /* Whoever may give may also take back -- any of it, the voice
         * and the letter box included. The x sits on the same row as
         * the authority it withdraws, like everywhere else. */
        bool may_cut = (o == focus()) && nav.at_generation == 0 &&
                       (focus_rights() & CAP_GRANT);

        u64 held = 0;
        for (u64 i = 1; i <= domain_capacity(pd); i++) {
            u32 hr = 0;
            object *t = domain_cap_at(pd, i, &hr);
            if (!t) continue;
            held++;

            if (ROW_ON) {
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

                if (may_cut) {
                    bool lit = is_hovered(HOT_CAPCUT, (u32)i);
                    text_at(x + w - 2 * GLYPH_W, ty, x + w, "x",
                            lit ? C_READONLY : C_FAINT);
                    hot_add(x + w - 2 * GLYPH_W - 2, ty - 2, 2 * GLYPH_W,
                            ROW - 2, HOT_CAPCUT, (u32)i);
                }
            }
            ty += ROW;
        }
        if (held == 0) {
            if (ROW_ON) text_at(x, ty, x + w, "  none", C_FAINT);
            ty += ROW;
        }
    }

    u32 total = (u32)((ty - (y - (i32)scroll * ROW) + ROW - 1) / ROW);
    scroll_area(SCR_STRUCT, x, y, w, h, total, (u32)(h / ROW));
#undef ROW_ON
}

static void draw_lens(lens_kind k, object *o, i32 x, i32 y, i32 w, i32 h,
                      bool caret)
{
    switch (k) {
    case LENS_TEXT:      lens_text(o, x, y, w, h, caret); break;
    case LENS_BYTES:     lens_bytes(o, x, y, w, h); break;
    case LENS_STRUCTURE: lens_structure(o, x, y, w, h); break;
    case LENS_PAINT:     lens_paint(o, x, y, w, h, caret); break;
    case LENS_HTML:      lens_html(o, x, y, w, h, caret); break;
    default: break;
    }
}

static const char *lens_name(lens_kind k)
{
    switch (k) {
    case LENS_TEXT:      return "text";
    case LENS_BYTES:     return "bytes";
    case LENS_STRUCTURE: return "structure";
    case LENS_PAINT:     return "picture";
    case LENS_HTML:      return "html";
    default:             return "?";
    }
}

/* A sensible first lens for a type. The system knows what an object is,
 * so nobody has to be asked how to open it. */
static lens_kind default_lens(object *o)
{
    switch (obj_type(o)) {
    case TYPE_TEXT:
        /* A text that is a fetched page opens as the page it is;
         * the raw markup stays one lens tab away. */
        return smells_like_page(o) ? LENS_HTML : LENS_TEXT;
    case TYPE_BYTES:   return LENS_BYTES;
    case TYPE_PICTURE: return LENS_PAINT;
    default:           return LENS_STRUCTURE;
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
    /* The search into the focused text, at the tab row's right: a
     * word to open it, the needle and its count while it is open. */
    if (obj_type(f) == TYPE_TEXT) {
        char fword[64];
        u32 fa = put(fword, 0, "find");
        if (tfind_on) {
            fa = put(fword, fa, " ");
            for (u32 i = 0; i < tfind_len && fa < 56; i++)
                fword[fa++] = tfind_buf[i];
            fa = put(fword, fa, "_");
            if (tfind_len) {
                fa = put(fword, fa, "  ");
                fa = put_dec(fword, fa, tfind_count);
            }
        }
        fword[fa] = 0;
        i32 fw = (i32)fa * GLYPH_W;
        i32 fx = mid_x + mid_w - PAD - fw;
        bool flit = is_hovered(HOT_TFIND, 0);
        if (flit || tfind_on)
            fb_rect(fx - 4, top + PAD - 4, fw + 8, ROW,
                    tfind_on ? C_PANEL_HI : C_EDGE);
        text_at(fx, top + PAD, mid_x + mid_w, fword,
                tfind_on ? C_TEXT : (flit ? C_TEXT : C_FAINT));
        hot_add(fx - 4, top + PAD - 4, fw + 8, ROW, HOT_TFIND, 0);
    }

    fb_rect(mid_x + PAD, top + PAD + ROW + 2, mid_w - 2 * PAD, 1, C_EDGE);

    i32 cy = top + PAD + ROW + PAD + 4;
    i32 ch = bottom - cy - PAD;
    i32 each = nav.lens_count ? (mid_w - 2 * PAD) / (i32)nav.lens_count : 0;

    for (u32 i = 0; i < nav.lens_count; i++) {
        i32 lx = mid_x + PAD + (i32)i * each;
        if (i > 0) fb_rect(lx - PAD / 2, cy, 1, ch, C_EDGE);
        draw_lens(nav.lens[i], f, lx, cy, each - PAD, ch,
                  nav.lens[i] == LENS_TEXT || nav.lens[i] == LENS_PAINT ||
                  nav.lens[i] == LENS_HTML);
    }

    /* --- where it leads ------------------------------------------ */
    i32 rx = sw - PAD - right_w;
    fb_rect(rx, top, right_w, bottom - top, C_PANEL);
    text_at(rx + PAD, top + PAD, sw - PAD, "contents", C_FAINT);
    fb_rect(rx + PAD, top + PAD + ROW - 2, right_w - 2 * PAD, 1, C_EDGE);

    i32 list_top = top + PAD + ROW + 4;
    ty = list_top - (i32)scrolls[SCR_CONTENTS] * ROW;
    bool may_shape = can_shape();

    u64 slots = obj_slots(f);
    u64 used = 0;
    for (u64 i = 0; i < slots; i++) if (obj_get_slot(f, i)) used++;

    if (used == 0 && !may_shape)
        text_at(rx + PAD, list_top, sw - PAD, "no references", C_FAINT);

    i32 col_rights = rx + PAD + 2 * GLYPH_W;
    i32 col_name   = rx + PAD + 7 * GLYPH_W;
    i32 col_clear  = rx + right_w - PAD - 2 * GLYPH_W;

/* Rows advance whether drawn or not; only the visible get ink and
 * click regions. The same rule as every other scrolling area. */
#define CROW_ON (ty >= list_top && ty + GLYPH_H <= bottom - 4)

    for (u64 i = 0; i < slots; i++) {
        object *t = obj_get_slot(f, i);
        if (!t) continue;                     /* empty slots are not shown */
        if (!CROW_ON) { ty += ROW; continue; }

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

            if (lit && can) fb_rect(lx - 2, ty - 3, GLYPH_W + 4, ROW,
                                    C_EDGE);
            text_at(lx, ty, lx + GLYPH_W, one,
                    on ? (b == 1 ? C_WRITE : C_DIM)
                       : (can && lit ? C_TEXT : C_FAINT));
            /* As tall as the row it governs: a letter that can only
             * be hit on its exact pixels is a letter most pointers
             * miss. */
            if (can) hot_add(lx - 2, ty - 3, GLYPH_W + 4, ROW,
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
    if (may_shape) {
        ty += 4;
        if (CROW_ON) {
            bool lit = is_hovered(HOT_ADD, 0);
            if (lit) fb_rect(rx, ty - 3, right_w, ROW, C_EDGE);
            text_at(rx + PAD, ty, sw - PAD, "+  add",
                    lit ? C_TEXT : C_FAINT);
            hot_add(rx, ty - 3, right_w, ROW, HOT_ADD, 0);
        }
        ty += ROW;

        if (edit.kind == EDIT_PICK) {
            static const char *fixed[PALETTE_FIXED] = {
                "  text", "  bytes", "  list", "  picture", "  script",
                "  page", "  task"
            };
            for (u32 p = 0; p < PALETTE_FIXED; p++) {
                if (CROW_ON) {
                    bool on = is_hovered(HOT_PALETTE, p);
                    if (on) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);
                    text_at(rx + PAD, ty, sw - PAD, fixed[p],
                            on ? C_TEXT : C_DIM);
                    hot_add(rx, ty - 3, right_w, ROW, HOT_PALETTE, p);
                }
                ty += ROW;
            }

            /* The programs the system ships with, each ready to start.
             * Making one here is the same act as making a text: a fresh
             * thing comes to exist, and the reference being added is
             * how it is held. It starts with a voice and a letter box;
             * everything beyond that is given the ordinary way. */
            u32 sc = standard_count();
            if (sc > 0) {
                if (CROW_ON)
                    text_at(rx + PAD, ty, sw - PAD,
                            "  or a program, started fresh:", C_FAINT);
                ty += ROW;
            }
            for (u32 p = 0; p < sc; p++) {
                if (CROW_ON) {
                    bool on = is_hovered(HOT_PALETTE, PALETTE_FIXED + p);
                    if (on) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);

                    char nm[40];
                    u32 n = put(nm, 0, "  ");
                    n = put(nm, n, standard_name(p));
                    nm[n] = 0;
                    text_at(rx + PAD + 2 * GLYPH_W, ty, sw - PAD, nm,
                            on ? C_TEXT : C_WRITE);
                    hot_add(rx, ty - 3, right_w, ROW, HOT_PALETTE,
                            PALETTE_FIXED + p);
                }
                ty += ROW;
            }

            carried carry[CARRY_MAX];
            u32 carried_count = gather(carry);

            if (carried_count > 0) {
                if (CROW_ON)
                    text_at(rx + PAD, ty, sw - PAD,
                            "  or something you already have:", C_FAINT);
                ty += ROW;
            }
            for (u32 c = 0; c < carried_count; c++) {
                if (CROW_ON) {
                    char nm[40], r[4];
                    label_of(carry[c].holder, carry[c].slot, carry[c].o,
                             nm, sizeof(nm));
                    rights_text(carry[c].rights, r);

                    bool on = is_hovered(HOT_PALETTE,
                                         PALETTE_FIXED + standard_count() + c);
                    if (on) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);

                    /* The rights come along on the label. What is being
                     * offered is not the object but a particular hold on
                     * it, and the difference is the whole point. */
                    text_at(rx + PAD + 2 * GLYPH_W, ty, sw - PAD, r,
                            (carry[c].rights & CAP_WRITE) ? C_WRITE
                                                          : C_READONLY);
                    text_at(rx + PAD + 7 * GLYPH_W, ty, sw - PAD, nm,
                            on ? C_TEXT : C_DIM);
                    hot_add(rx, ty - 3, right_w, ROW, HOT_PALETTE,
                            PALETTE_FIXED + standard_count() + c);
                }
                ty += ROW;
            }
        }
    }

    u32 ctotal = (u32)((ty - (list_top - (i32)scrolls[SCR_CONTENTS] * ROW)
                        + ROW - 1) / ROW);
    scroll_area(SCR_CONTENTS, rx, list_top, right_w,
                bottom - 4 - list_top, ctotal,
                (u32)((bottom - 4 - list_top) / ROW));
#undef CROW_ON
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

/* The map moves under the hand: dragging empty ground pans it, the
 * wheel zooms around the pointer. A graph larger than the screen is
 * the normal case, not a failure, once it can be walked this way. */
static i32  graph_zoom = 100;        /* percent */
static i32  graph_pan_x, graph_pan_y;
static bool graph_drag;
static i32  graph_last_x, graph_last_y;
static i32  graph_top, graph_bot;    /* where the map was last drawn */

static void graph_zoom_at(i32 mx, i32 my, i32 dz)
{
    i32 nz = graph_zoom;
    i32 steps = dz < 0 ? -dz : dz;
    for (i32 i = 0; i < steps; i++)
        nz = (dz < 0) ? nz * 11 / 10 : nz * 10 / 11;
    if (nz < 30)  nz = 30;
    if (nz > 250) nz = 250;
    if (nz == graph_zoom) return;

    /* The point under the pointer stays under the pointer. */
    graph_pan_x = mx - (i32)((i64)(mx - graph_pan_x) * nz / graph_zoom);
    graph_pan_y = my - (i32)((i64)(my - graph_pan_y) * nz / graph_zoom);
    graph_zoom = nz;
    nav.redraw = true;
}

static void draw_graph_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sw; (void)sh;
    build_graph();

    i32 z = graph_zoom;
    i32 node_w = 250 * z / 100, node_h = 46 * z / 100;
    if (node_h < 8) node_h = 8;

    graph_top = top;
    graph_bot = bottom - 220;

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

            i32 ax = gnodes[i].x * z / 100 + graph_pan_x;
            i32 ay = gnodes[i].y * z / 100 + graph_pan_y;
            i32 bx = gnodes[j].x * z / 100 + graph_pan_x;
            i32 by = gnodes[j].y * z / 100 + graph_pan_y;

            i32 x0 = ax + node_w, y0 = ay + node_h / 2;
            i32 x1 = bx,          y1 = by + node_h / 2;

            /* An edge that goes backwards or sideways would cut through
             * the column it starts in, so it leaves from the left. */
            if (gnodes[j].depth <= gnodes[i].depth) {
                x0 = ax;
                x1 = bx + node_w;
            }
            if ((y0 < top && y1 < top) ||
                (y0 > graph_bot && y1 > graph_bot))
                continue;
            line_between(x0, y0, x1, y1, tree_edge ? C_EDGE : C_FAINT);
        }
    }

    for (u32 i = 0; i < gcount; i++) {
        object *o = gnodes[i].o;
        bool is_focus = (o == focus());
        bool on_path = false;
        for (u32 d = 0; d < nav.depth; d++) if (nav.node[d] == o) on_path = true;

        i32 x = gnodes[i].x * z / 100 + graph_pan_x;
        i32 y = gnodes[i].y * z / 100 + graph_pan_y;
        if (y + node_h < top || y > graph_bot) continue;
        if (x + node_w < 0 || x > sw) continue;

        bool hot = is_hovered(HOT_NODE, i);
        fb_rect(x - 2, y - 2, node_w + 4, node_h + 4,
                is_focus ? C_ACCENT : (hot ? C_DIM
                                           : (on_path ? C_EDGE : C_PANEL)));
        fb_rect(x, y, node_w, node_h,
                (is_focus || hot) ? C_PANEL_HI : C_PANEL);
        hot_add(x - 2, y - 2, node_w + 4, node_h + 4, HOT_NODE, i);

        /* Labels only where they fit: a shrunken box with a full
         * label would be noise wearing letters. */
        if (z >= 55) {
            char what[40];
            graph_node *par = gnodes[i].parent >= 0
                            ? &gnodes[gnodes[i].parent] : NULL;
            u64 via = 0;
            if (par) for (u64 s = 0; s < obj_slots(par->o); s++)
                if (obj_get_slot(par->o, s) == o) { via = s; break; }
            label_of(par ? par->o : NULL, via, o, what, sizeof(what));
            text_at(x + 10, y + 7, x + node_w - 8, what,
                    is_focus ? C_TEXT : C_DIM);
            if (z >= 85 && node_h >= 40)
                text_at(x + 10, y + 7 + GLYPH_H + 2, x + node_w - 8,
                        type_name(obj_type(o)), C_FAINT);
        }
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
                  last && (nav.lens[0] == LENS_TEXT ||
                           nav.lens[0] == LENS_PAINT ||
                           nav.lens[0] == LENS_HTML));

        x += w + gap;
    }
}

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */
/* Shell five: the split -- two independent walks                      */
/* ------------------------------------------------------------------ */

/* The second walk. Its references are retained while it stands on
 * them -- the collector treats a held reference the graph does not
 * explain as an outside holder, which is exactly what this is. The
 * second pane reads and walks; writing, running and sending stay
 * with the first, which is where the caret and the header's words
 * already live. */
static struct {
    object *node[TRAIL_MAX];
    u32     rights[TRAIL_MAX];
    u32     via[TRAIL_MAX];
    u32     depth;
    u32     selected;
} p2;

static void p2_reset(void)
{
    for (u32 i = 1; i < p2.depth; i++) obj_release(p2.node[i]);
    p2.node[0] = nav.node[0];
    p2.rights[0] = nav.rights[0];
    p2.depth = 1;
    p2.selected = 0;
    scrolls[SCR_TEXT2] = 0;
}

static void p2_follow(u64 slot)
{
    if (p2.depth >= TRAIL_MAX) return;
    object *f = p2.node[p2.depth - 1];
    if (slot >= obj_slots(f)) return;
    object *t = obj_get_slot(f, slot);
    if (!t) return;

    u32 gained = p2.rights[p2.depth - 1] & obj_slot_rights(f, slot);
    if (gained == 0) return;

    obj_retain(t);
    p2.via[p2.depth] = (u32)slot;
    p2.node[p2.depth] = t;
    p2.rights[p2.depth] = gained;
    p2.depth++;
    p2.selected = 0;
    scrolls[SCR_TEXT2] = 0;
    nav.redraw = true;
}

static void p2_back_to(u32 index)
{
    while (p2.depth > index + 1 && p2.depth > 1) {
        p2.depth--;
        obj_release(p2.node[p2.depth]);
        p2.selected = p2.via[p2.depth];
    }
    scrolls[SCR_TEXT2] = 0;
    nav.redraw = true;
}

/* One half of the split: the walked path as a line of words, the
 * references as a narrow column, and the object through a lens. */
static void draw_pane(i32 px, i32 pw, i32 top, i32 bottom, bool primary)
{
    u32 depth = primary ? nav.depth : p2.depth;
    u32 sel   = primary ? nav.selected : p2.selected;

    fb_rect(px, top, pw, bottom - top, C_PANEL);
    if (primary)
        fb_rect(px, top, pw, 2, C_ACCENT);

    /* The path as one line: every step a word, every word a way
     * back. */
    i32 x = px + PAD;
    i32 ty = top + PAD;
    for (u32 i = 0; i < depth; i++) {
        object *n = primary ? nav.node[i] : p2.node[i];
        object *h = i ? (primary ? nav.node[i - 1] : p2.node[i - 1])
                      : NULL;
        u32 via = i ? (primary ? nav.via[i] : p2.via[i]) : 0;

        char what[32];
        label_of(h, via, n, what, sizeof(what));
        i32 wl = 0;
        while (what[wl]) wl++;

        hot_kind hk = primary ? HOT_TRAIL : HOT_P2TRAIL;
        bool lit = is_hovered(hk, i);
        bool here = (i + 1 == depth);
        if (lit && !here)
            fb_rect(x - 3, ty - 3, wl * GLYPH_W + 6, ROW, C_EDGE);
        text_at(x, ty, px + pw - PAD, what,
                here ? C_TEXT : (lit ? C_TEXT : C_DIM));
        hot_add(x - 3, ty - 3, wl * GLYPH_W + 6, ROW, hk, i);
        x += wl * GLYPH_W;
        if (!here) {
            text_at(x + GLYPH_W, ty, px + pw, ">", C_FAINT);
            x += 3 * GLYPH_W;
        }
        if (x > px + pw - 12 * GLYPH_W) break;   /* a long walk clips */
    }
    fb_rect(px + PAD, top + PAD + ROW - 2, pw - 2 * PAD, 1, C_EDGE);

    /* The narrow column of references, and the lens beside it. */
    object *f = primary ? focus() : p2.node[p2.depth - 1];
    i32 list_w = 24 * GLYPH_W;
    i32 ly = top + PAD + ROW + 6;
    u32 shown = 0, total_refs = 0;

    for (u64 i = 0; i < obj_slots(f); i++) {
        object *t = obj_get_slot(f, i);
        if (!t) continue;
        total_refs++;
        if (ly >= bottom - ROW) continue;

        char what[24];
        label_of(f, i, t, what, sizeof(what));

        hot_kind hk = primary ? HOT_REFERENCE : HOT_P2REF;
        bool picked = (i == sel);
        bool lit = is_hovered(hk, (u32)i);
        if (picked || lit)
            fb_rect(px, ly - 3, list_w + PAD, ROW,
                    picked ? C_PANEL_HI : C_EDGE);
        text_at(px + PAD, ly, px + PAD + list_w, what,
                (picked || lit) ? C_TEXT : C_DIM);
        hot_add(px, ly - 3, list_w + PAD, ROW, hk, (u32)i);
        ly += ROW;
        shown++;
    }
    if (shown < total_refs && ly < bottom) {
        char line[32];
        u32 at = put(line, 0, "  and ");
        at = put_dec(line, at, total_refs - shown);
        at = put(line, at, " more");
        line[at] = 0;
        text_at(px + PAD, ly, px + PAD + list_w, line, C_FAINT);
    }

    i32 lens_x = px + PAD + list_w + PAD + 6;
    i32 lens_w = pw - list_w - 3 * PAD - 6;
    i32 lens_y = top + PAD + ROW + 6;
    fb_rect(lens_x - 4, lens_y, 1, bottom - lens_y - PAD, C_EDGE);

    if (primary) {
        draw_lens(nav.lens[0], f, lens_x, lens_y, lens_w,
                  bottom - lens_y - PAD,
                  nav.lens[0] == LENS_TEXT || nav.lens[0] == LENS_PAINT ||
                  nav.lens[0] == LENS_HTML);
    } else {
        cur_text_area = SCR_TEXT2;
        draw_lens(default_lens(f), f, lens_x, lens_y, lens_w,
                  bottom - lens_y - PAD, false);
        cur_text_area = SCR_TEXT;
    }
}

static void draw_split_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sh;

    /* The second walk starts at home, and starts over when the world
     * under it changed -- another generation, another root. */
    if (p2.depth == 0 || p2.node[0] != nav.node[0]) p2_reset();
    if (p2.depth > 0) {
        object *f2 = p2.node[p2.depth - 1];
        if (p2.selected >= obj_slots(f2)) p2.selected = 0;
    }

    i32 half = sw / 2;
    draw_pane(PAD, half - PAD - PAD / 2, top, bottom - PAD, true);
    draw_pane(half + PAD / 2, sw - half - PAD - PAD / 2, top,
              bottom - PAD, false);
}

/* ------------------------------------------------------------------ */
/* Shell four: the index                                               */
/* ------------------------------------------------------------------ */

/* Every object reachable from home, one line each: what a file manager
 * is for, without pretending there are files. The walk starts at the
 * beginning of the path and follows references breadth first, so the
 * list is ordered by distance from where one lives.
 *
 * What the list does NOT show is the point at which this system parts
 * ways with a file manager: objects held only by the kernel and the
 * programs -- letter boxes, the console, the session -- are counted at
 * the bottom, not listed. They exist, but no reference of yours leads
 * to them, and an overview of what you hold is not a window into what
 * you do not. */
#define INDEX_MAX 192

typedef struct {
    object *o;
    i32     parent;    /* row of the discoverer, -1 for home */
    u32     via;       /* the slot on the discoverer */
} index_row;

static index_row irows[INDEX_MAX];
static u32       icount;

/* The search. There are no paths to remember here, so finding a thing
 * again means asking for what it says or what it was called. Typing in
 * the index feeds this; escape empties it. */
static char find_buf[48];
static u32  find_len;

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Whether the needle occurs in these bytes, case aside, and where. */
static bool bytes_contain(const u8 *d, u64 len, u64 *where)
{
    if (find_len == 0 || !d) return false;
    if (len < find_len) return false;
    for (u64 i = 0; i + find_len <= len; i++) {
        u32 j = 0;
        while (j < find_len &&
               to_lower((char)d[i + j]) == to_lower(find_buf[j])) j++;
        if (j == find_len) { if (where) *where = i; return true; }
    }
    return false;
}

/* Whether a row answers the search: by the name it is shown under, or
 * by what the text itself says. A content match hands back the line it
 * was found in, so the list can show not only what matched but why. */
static bool row_matches(const index_row *r, char *why, u32 why_max)
{
    why[0] = 0;
    if (find_len == 0) return true;

    char label[48];
    label_of(r->parent >= 0 ? irows[r->parent].o : NULL, r->via,
             r->o, label, sizeof(label));
    u64 llen = 0;
    while (label[llen]) llen++;
    if (bytes_contain((const u8 *)label, llen, NULL)) return true;

    if (obj_type(r->o) != TYPE_TEXT) return false;

    const u8 *d = (const u8 *)obj_data(r->o);
    if (!d) return false;
    u64 len = text_len(d, obj_size(r->o));
    u64 at = 0;
    if (!bytes_contain(d, len, &at)) return false;

    u64 start = at;
    while (start > 0 && d[start - 1] != '\n') start--;
    u32 n = 0;
    while (start + n < len && d[start + n] != '\n' && n < why_max - 1) {
        why[n] = (char)d[start + n];
        n++;
    }
    why[n] = 0;
    return true;
}

static bool index_has(object *o)
{
    for (u32 i = 0; i < icount; i++) if (irows[i].o == o) return true;
    return false;
}

static void build_index(void)
{
    icount = 0;
    if (nav.depth == 0) return;

    irows[icount].o = nav.node[0];
    irows[icount].parent = -1;
    irows[icount].via = 0;
    icount++;

    for (u32 i = 0; i < icount && icount < INDEX_MAX; i++) {
        object *o = irows[i].o;
        for (u64 s = 0; s < obj_slots(o) && icount < INDEX_MAX; s++) {
            object *t = obj_get_slot(o, s);
            if (!t || index_has(t)) continue;
            irows[icount].o = t;
            irows[icount].parent = (i32)i;
            irows[icount].via = (u32)s;
            icount++;
        }
    }
}

static void draw_index_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sh;
    build_index();

    fb_rect(PAD, top, sw - 2 * PAD, bottom - top, C_PANEL);

    i32 x = PAD * 2;
    i32 ty = top + PAD;
    char line[96];
    u32 at;

    /* The sum first: how much there is, and how much of what exists is
     * yours to see at all. */
    u64 bytes = 0;
    for (u32 i = 0; i < icount; i++) bytes += obj_size(irows[i].o);
    u64 unseen = obj_live_count() > icount ? obj_live_count() - icount : 0;

    at = put(line, 0, "reachable from ");
    char home_name[40];
    label_of(NULL, 0, nav.node[0], home_name, sizeof(home_name));
    at = put(line, at, home_name);
    at = put(line, at, ":  ");
    at = put_dec(line, at, icount);
    at = put(line, at, " objects, ");
    at = put_dec(line, at, bytes / 1024);
    at = put(line, at, " KiB of payload");
    line[at] = 0;
    text_at(x, ty, sw - PAD, line, C_TEXT);
    ty += ROW;

    at = put(line, 0, "and ");
    at = put_dec(line, at, unseen);
    at = put(line, at, " more exist that no reference of yours reaches");
    line[at] = 0;
    text_at(x, ty, sw - PAD, line, C_FAINT);
    ty += ROW + ROW / 2;

    /* The search row. Typing lands here while the index is open --
     * there is no little box to click first, because there is nothing
     * else typing could mean in a list. Escape empties it. */
    text_at(x, ty, sw - PAD, "find", C_DIM);
    if (find_len) {
        find_buf[find_len] = 0;
        text_at(x + 6 * GLYPH_W, ty, sw - PAD, find_buf, C_TEXT);
        fb_rect(x + (6 + (i32)find_len) * GLYPH_W, ty, 2, GLYPH_H,
                C_ACCENT);

        bool litx = is_hovered(HOT_FINDX, 0);
        i32 fx = x + (9 + (i32)find_len) * GLYPH_W;
        text_at(fx, ty, sw, "x", litx ? C_READONLY : C_FAINT);
        hot_add(fx - 2, ty - 3, 2 * GLYPH_W, ROW, HOT_FINDX, 0);
    } else {
        fb_rect(x + 6 * GLYPH_W, ty, 2, GLYPH_H, C_ACCENT);
        text_at(x + 8 * GLYPH_W, ty, sw - PAD,
                "type to search names and words", C_FAINT);
    }
    ty += ROW + ROW / 2;

    text_at(x, ty, sw - PAD,
            "  id  kind       bytes   held  way   name", C_DIM);
    fb_rect(x, ty + ROW - 4, sw - 4 * PAD, 1, C_EDGE);
    ty += ROW + 4;

    i32 list_y = ty;
    u32 ivis = (u32)((bottom - 2 * ROW - list_y) / ROW);
    if (ivis < 1) ivis = 1;

    u32 shown = 0, matched = 0;
    u32 skip = scrolls[SCR_INDEX];
    for (u32 i = 0; i < icount; i++) {
        object *o = irows[i].o;
        index_row *r = &irows[i];

        char why[44];
        if (!row_matches(r, why, sizeof(why))) continue;
        matched++;
        if (matched <= skip) continue;        /* scrolled past, above */
        if (ty >= bottom - 2 * ROW) continue;
        shown++;

        bool here = (o == focus());
        bool hot = is_hovered(HOT_INDEX, i);
        if (here || hot)
            fb_rect(PAD, ty - 3, sw - 2 * PAD, ROW,
                    here ? C_PANEL_HI : C_EDGE);

        at = 0;
        at = put_dec(line, at, obj_id(o));
        line[at] = 0;
        text_at(x + (4 - (i32)at) * GLYPH_W, ty, sw, line, C_FAINT);

        text_at(x + 6 * GLYPH_W, ty, x + 16 * GLYPH_W,
                type_name(obj_type(o)), type_color(obj_type(o)));

        at = 0;
        at = put_dec(line, at, obj_size(o));
        line[at] = 0;
        text_at(x + (22 - (i32)at) * GLYPH_W, ty, sw, line, C_DIM);

        at = 0;
        at = put_dec(line, at, obj_refs(o));
        line[at] = 0;
        text_at(x + (28 - (i32)at) * GLYPH_W, ty, sw, line, C_DIM);

        /* The rights on the reference this row was found through: what
         * the best-known way in actually grants. Home was not found
         * through anything; it is simply held. */
        char rt[4];
        if (r->parent >= 0) {
            rights_text(obj_slot_rights(irows[r->parent].o, r->via), rt);
            text_at(x + 30 * GLYPH_W, ty, sw, rt, C_DIM);
        } else {
            text_at(x + 30 * GLYPH_W, ty, sw, "---", C_FAINT);
        }

        char what[48];
        label_of(r->parent >= 0 ? irows[r->parent].o : NULL, r->via,
                 o, what, sizeof(what));
        text_at(x + 36 * GLYPH_W, ty, sw - PAD, what,
                (here || hot) ? C_TEXT : C_DIM);

        /* Why the search found it: the line it says it in. */
        if (why[0]) {
            at = put(line, 0, "says \"");
            at = put(line, at, why);
            at = put(line, at, "\"");
            line[at] = 0;
            text_at(x + 62 * GLYPH_W, ty, sw - PAD * 2 - 10 * GLYPH_W,
                    line, C_FAINT);
        }

        /* For a program, whether it runs, at the right edge. */
        if (obj_type(o) == TYPE_PROGRAM) {
            bool alive = proc_is_running(o);
            text_at(sw - PAD * 2 - 8 * GLYPH_W, ty, sw - PAD,
                    alive ? "running" : "ended",
                    alive ? C_WRITE : C_READONLY);
        }

        hot_add(PAD, ty - 3, sw - 2 * PAD, ROW, HOT_INDEX, i);
        ty += ROW;
    }

    if (skip + shown < matched) {
        at = put(line, 0, "  and ");
        at = put_dec(line, at, matched - skip - shown);
        at = put(line, at, " more below; the wheel or the edge scrolls");
        line[at] = 0;
        text_at(x, ty, sw - PAD, line, C_FAINT);
        ty += ROW;
    }

    scroll_area(SCR_INDEX, PAD, list_y, sw - 2 * PAD,
                bottom - 2 * ROW - list_y, matched, ivis);

    if (find_len && ty < bottom) {
        at = put(line, 0, "  ");
        at = put_dec(line, at, matched);
        at = put(line, at, matched == 1 ? " answer, of " : " answers, of ");
        at = put_dec(line, at, icount);
        at = put(line, at, " asked");
        line[at] = 0;
        text_at(x, ty, sw - PAD, line, C_DIM);
    }
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* The terminal                                                        */
/* ------------------------------------------------------------------ */

/* The transcript, and under it the line being gathered. The core
 * behind this knows nothing of the screen: this view feeds keys in
 * and draws the words back out, and a remote line arriving over the
 * network later will speak through the same doors. */
static void draw_term_shell(i32 sw, i32 sh, i32 top, i32 bottom)
{
    (void)sh;
    i32 x = PAD * 2;
    i32 y0 = top + 10;
    i32 w = sw - PAD * 4;
    i32 h = bottom - y0 - ROW - 14;      /* room for the gather line */

    term_session *ts = term_screen();
    u64 tlen;
    const char *t = term_out(ts, &tlen);

    u32 rows = 0;
    for (u64 i = 0; i < tlen; i++) if (t[i] == '\n') rows++;
    u32 vis = h > 0 ? (u32)(h / ROW) : 0;

    /* Stay at the end while new words come, unless one scrolled up
     * to read back -- the journal's manner. */
    static u64 seen;
    static bool pinned = true;
    u32 max = rows > vis ? rows - vis : 0;
    if (term_sequence(ts) != seen) {
        seen = term_sequence(ts);
        if (pinned) scrolls[SCR_TERM] = max;
    }
    if (scrolls[SCR_TERM] > max) scrolls[SCR_TERM] = max;

    scroll_area(SCR_TERM, x, y0, w, h, rows, vis);
    pinned = scrolls[SCR_TERM] >= max;

    u32 skip = scrolls[SCR_TERM];
    i32 ty = y0;
    u64 i = 0;
    while (i < tlen && skip) { if (t[i] == '\n') skip--; i++; }
    char line[220];
    while (i < tlen && ty + ROW <= y0 + h) {
        u32 n = 0;
        while (i < tlen && t[i] != '\n') {
            if (n < sizeof(line) - 1) line[n++] = t[i];
            i++;
        }
        if (i < tlen) i++;
        line[n] = 0;
        text_at(x, ty, x + w - 12, line,
                line[0] == '>' ? C_ACCENT : C_TEXT);
        ty += ROW;
    }

    /* The gathering line, with the caret where the letters land. */
    i32 gy = bottom - ROW - 4;
    fb_rect(x, gy - 4, w, 1, C_EDGE);
    u32 gl;
    const char *g = term_gather(ts, &gl);
    char gline[220];
    u32 at = 0;
    gline[at++] = '>';
    gline[at++] = ' ';
    bool hush = term_secret(ts);                     /* a passphrase: dots, not letters */
    for (u32 k = 0; k < gl && at < sizeof(gline) - 2; k++)
        gline[at++] = hush ? '*' : g[k];
    gline[at++] = '_';
    gline[at] = 0;
    text_at(x, gy, x + w, gline, C_TEXT);
}

static const char *mode_name(shell_mode m)
{
    switch (m) {
    case SHELL_FOCUS: return "focus";
    case SHELL_GRAPH: return "graph";
    case SHELL_TILES: return "columns";
    case SHELL_INDEX: return "index";
    case SHELL_SPLIT: return "split";
    case SHELL_TERM:  return "terminal";
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

    /* The newest journal line gets a quiet row above the footer -- and
     * only while it is news. What programs and the system do would
     * otherwise be invisible until somebody thought to go and look; but
     * a line that stays for hours stops reading as an event and starts
     * reading as a status, which it is not. Half a minute, then the row
     * retires and the log keeps it. The age comes from the line itself:
     * every entry begins with the second it happened in. */
    char newest[104];
    bool have_news = journal_latest(newest, sizeof(newest));
    if (have_news) {
        u64 said_at = 0, p = 0;
        while (newest[p] == ' ') p++;
        while (newest[p] >= '0' && newest[p] <= '9')
            said_at = said_at * 10 + (u64)(newest[p++] - '0');
        u64 now_s = time_ns() / 1000000000ULL;
        if (now_s > said_at + 30) have_news = false;
    }
    if (have_news) bottom -= ROW + 6;

    pal = settings_light() ? pal_light : pal_dark;

    hot_reset();
    scr_target_count = 0;
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
     * visible. Grown again for the line's mouth: "say: " and two
     * hundred letters and the caret must fit. */
    char line[256];
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

    /* Any readable text can be run, provided there is somewhere to
     * put the running program: the object one came through. The word
     * sits beside the rights because running is the one thing rights
     * alone do not announce. */
    {
        /* A text runs through the interpreter; bytes that are an image
         * run through the loader. The same word, because it is the
         * same act. And a text can be assembled first -- the image
         * lands beside it, the way a running program does. */
        bool is_image = obj_type(focus()) == TYPE_BYTES &&
                        code_image_ok((const u8 *)obj_data(focus()),
                                      obj_size(focus()), NULL, NULL, NULL);
        bool can_lay = (focus_rights() & CAP_READ) &&
                       nav.at_generation == 0 && nav.depth >= 2;
        if (can_lay) {
            object *through = nav.node[nav.depth - 2];
            u32 hr = nav.rights[nav.depth - 2];
            can_lay = (obj_type(through) == TYPE_PROGRAM)
                    ? (hr & CAP_GRANT) != 0
                    : (hr & CAP_WRITE) != 0;
        }
        bool can_run = can_lay && (obj_type(focus()) == TYPE_TEXT || is_image);
        bool can_asm = can_lay && obj_type(focus()) == TYPE_TEXT &&
                       obj_type(nav.node[nav.depth - 2]) != TYPE_PROGRAM;
        i32 chip_x = tx + (i32)at * GLYPH_W + 3 * GLYPH_W;
        if (can_run) {
            bool lit = is_hovered(HOT_RUN, 0);
            if (lit) fb_rect(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "run", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, HOT_RUN, 0);
            chip_x += 5 * GLYPH_W;
        }

        /* The pipe's offer: any plain readable thing can be sent to
         * another machine. A system function, so it stands in the
         * header like run does, on everything it applies to -- and
         * with nobody chosen yet, pressing it opens the choosing,
         * rather than the word hiding and leaving no trail to why. */
        type_id ft = obj_type(focus());
        bool can_send = (ft == TYPE_TEXT || ft == TYPE_BYTES ||
                         ft == TYPE_PICTURE) &&
                        (focus_rights() & CAP_READ) &&
                        nav.at_generation == 0 && net_up();
        if (can_send) {
            bool lit = is_hovered(HOT_SEND, 0);
            if (lit) fb_rect(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "send", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, HOT_SEND, 0);
            sendto_x = chip_x;
            chip_x += 6 * GLYPH_W;
        } else {
            sendto_open = false;
        }

        /* Far work: a text can be asked of another machine. It runs
         * over there under its own clock, and the answer comes home.
         * The word stands beside send because asking is sending with
         * a request attached. */
        if (can_send && ft == TYPE_TEXT) {
            bool lit = is_hovered(HOT_ASK, 0);
            if (lit) fb_rect(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "ask", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, HOT_ASK, 0);
            chip_x += 5 * GLYPH_W;
        }

        /* A running program can be ended by whoever may give to it:
         * the same authority that hands things over may close the
         * hand. It ends at its next step and is reaped like any
         * other end. */
        if (ft == TYPE_PROGRAM && proc_is_running(focus()) &&
            (focus_rights() & CAP_GRANT) && nav.at_generation == 0) {
            bool lit = is_hovered(HOT_END, 0);
            if (lit) fb_rect(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "end",
                    lit ? C_TEXT : C_READONLY);
            hot_add(chip_x - 4, 11, 3 * GLYPH_W + 8, ROW, HOT_END, 0);
            chip_x += 5 * GLYPH_W;
        }

        /* A list folds into one plain thing the pipe can carry; a
         * bundle unfolds back into a list. The words stand only
         * where they apply, like everything in this header. */
        if (ft == TYPE_LIST && (focus_rights() & CAP_READ) &&
            nav.at_generation == 0) {
            bool lit = is_hovered(HOT_PACK, 0);
            if (lit) fb_rect(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "pack", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, HOT_PACK, 0);
            chip_x += 6 * GLYPH_W;
        }
        if (ft == TYPE_BYTES && bundle_smells(focus()) &&
            (focus_rights() & CAP_READ) && nav.at_generation == 0) {
            bool lit = is_hovered(HOT_UNPACK, 0);
            if (lit) fb_rect(chip_x - 4, 11, 6 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "unpack",
                    lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 6 * GLYPH_W + 8, ROW, HOT_UNPACK, 0);
            chip_x += 8 * GLYPH_W;
        }

        /* Anything readable copies: the payload for the plain kinds,
         * the same references for a list. The copy lies down next to
         * the original, named after it. */
        if ((ft == TYPE_TEXT || ft == TYPE_BYTES || ft == TYPE_PICTURE ||
             ft == TYPE_LIST) &&
            (focus_rights() & CAP_READ) && nav.at_generation == 0) {
            bool lit = is_hovered(HOT_COPY, 0);
            if (lit) fb_rect(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "copy", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 4 * GLYPH_W + 8, ROW, HOT_COPY, 0);
            chip_x += 6 * GLYPH_W;
        }

        /* Last in the row, so the words before it keep their places:
         * a text of instructions becomes an image that runs. */
        if (can_asm) {
            bool lit = is_hovered(HOT_ASSEMBLE, 0);
            if (lit) fb_rect(chip_x - 4, 11, 8 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "assemble", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 8 * GLYPH_W + 8, ROW, HOT_ASSEMBLE, 0);
            chip_x += 10 * GLYPH_W;

            lit = is_hovered(HOT_COMPILE, 0);
            if (lit) fb_rect(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "compile", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, HOT_COMPILE, 0);
            chip_x += 9 * GLYPH_W;
        }

        /* The exchange disk's list carries its two acts: reading the
         * disk in again, and writing the new things out. */
        if (focus() == system_disk() && focus() != NULL &&
            nav.at_generation == 0) {
            bool lit = is_hovered(HOT_FATIN, 0);
            if (lit) fb_rect(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "take in",
                    lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, HOT_FATIN, 0);
            chip_x += 9 * GLYPH_W;

            bool lit2 = is_hovered(HOT_FATOUT, 0);
            if (lit2) fb_rect(chip_x - 4, 11, 9 * GLYPH_W + 8, ROW,
                              C_EDGE);
            text_at(chip_x, 14, sw - PAD, "write out",
                    lit2 ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 9 * GLYPH_W + 8, ROW, HOT_FATOUT, 0);
            chip_x += 11 * GLYPH_W;
        }

        /* Last of all: a list with sources or objects in it builds. */
        if (ft == TYPE_LIST && (focus_rights() & CAP_WRITE) &&
            nav.at_generation == 0 && list_buildable(focus())) {
            bool lit = is_hovered(HOT_BUILD, 0);
            if (lit) fb_rect(chip_x - 4, 11, 5 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "build", lit ? C_TEXT : C_ACCENT);
            hot_add(chip_x - 4, 11, 5 * GLYPH_W + 8, ROW, HOT_BUILD, 0);
            chip_x += 7 * GLYPH_W;
        }

        /* Bytes that begin the way an elf does are a kernel to install:
         * the next start runs it, the running one stays as kernel.old. */
        if (ft == TYPE_BYTES && (focus_rights() & CAP_READ) &&
            nav.at_generation == 0 && elf_smells(focus()) && fat_boot_present()) {
            bool lit = is_hovered(HOT_INSTALL, 0);
            if (lit) fb_rect(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(chip_x, 14, sw - PAD, "install", lit ? C_TEXT : C_READONLY);
            hot_add(chip_x - 4, 11, 7 * GLYPH_W + 8, ROW, HOT_INSTALL, 0);
            chip_x += 9 * GLYPH_W;
        }
    }

    /* Marked letters, and letters being carried. The mark offers
     * "take"; what was taken travels with the shell and offers "put"
     * wherever writing is allowed. Both live in the header because
     * they concern the walk, not any one object. */
    {
        u64 flen = 0;
        const u8 *fd = (const u8 *)obj_data(focus());
        if (fd && obj_type(focus()) == TYPE_TEXT)
            flen = text_len(fd, obj_size(focus()));

        u64 mlo, mhi;
        marked_range(flen, &mlo, &mhi);

        i32 hx = sw / 2 + PAD;
        i32 hy = 12 + ROW;

        if (mhi > mlo) {
            at = put(line, 0, "");
            at = put_dec(line, at, mhi - mlo);
            at = put(line, at, mhi - mlo == 1 ? " letter marked"
                                              : " letters marked");
            line[at] = 0;
            text_at(hx, hy, sw - PAD, line, C_DIM);
            hx += (i32)at * GLYPH_W + 2 * GLYPH_W;

            bool lit = is_hovered(HOT_TAKE, 0);
            if (lit) fb_rect(hx - 4, hy - 3, 4 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(hx, hy, sw, "take", lit ? C_TEXT : C_ACCENT);
            hot_add(hx - 4, hy - 3, 4 * GLYPH_W + 8, ROW, HOT_TAKE, 0);
        } else if (held_len) {
            char ex[20];
            u32 exn = 0;
            while (exn < 18 && exn < held_len) {
                char c = held_text[exn];
                ex[exn++] = (c == '\n' || (u8)c < 0x20) ? ' ' : c;
            }
            ex[exn] = 0;

            at = put(line, 0, "carrying \"");
            at = put(line, at, ex);
            if (held_len > 18) at = put(line, at, "...");
            at = put(line, at, "\"");
            line[at] = 0;
            text_at(hx, hy, sw - PAD, line, C_DIM);
            hx += (i32)at * GLYPH_W + 2 * GLYPH_W;

            bool may_put = obj_type(focus()) == TYPE_TEXT &&
                           (focus_rights() & CAP_WRITE) &&
                           nav.at_generation == 0;
            if (may_put) {
                bool lit = is_hovered(HOT_PUT, 0);
                if (lit) fb_rect(hx - 4, hy - 3, 3 * GLYPH_W + 8, ROW,
                                 C_EDGE);
                text_at(hx, hy, sw, "put", lit ? C_TEXT : C_ACCENT);
                hot_add(hx - 4, hy - 3, 3 * GLYPH_W + 8, ROW, HOT_PUT, 0);
                hx += 3 * GLYPH_W + 2 * GLYPH_W;
            }

            bool litx = is_hovered(HOT_DROP, 0);
            text_at(hx, hy, sw, "x", litx ? C_READONLY : C_FAINT);
            hot_add(hx - 2, hy - 3, 2 * GLYPH_W, ROW, HOT_DROP, 0);
        }
    }

    /* Far right, quietly: which kernel this is, which generation is on
     * the disk, and how long the system has been up. Not a status bar
     * -- three facts. The first is there because a machine that keeps
     * its graph looks the same whichever kernel it starts with, and
     * somebody who has just booted a stick deserves to know whether
     * they are looking at it. */
    at = put(line, 0, erebus_version);
    at = put(line, at, "   generation ");
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
    case SHELL_INDEX: draw_index_shell(sw, sh, top, bottom); break;
    case SHELL_SPLIT: draw_split_shell(sw, sh, top, bottom); break;
    case SHELL_TERM:  draw_term_shell(sw, sh, top, bottom); break;
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

    /* The four ways of looking, each its own word, each clickable --
     * tab cycles them, but a key alone would make them a secret. The
     * one in use is lit. */
    i32 mx = PAD * 2;
    for (u32 mi = 0; mi < SHELL_MODE_COUNT; mi++) {
        const char *mn = mode_name((shell_mode)mi);
        i32 ml = 0;
        while (mn[ml]) ml++;

        bool on = (nav.mode == (shell_mode)mi);
        bool lit = is_hovered(HOT_MODE, mi);
        text_at(mx, sh - 28 + 6, sw, mn,
                on ? C_ACCENT : (lit ? C_TEXT : C_FAINT));
        hot_add(mx - 2, sh - 28, ml * GLYPH_W + 4, 28, HOT_MODE, mi);
        mx += (ml + 2) * GLYPH_W;
    }
    mx += 2 * GLYPH_W;

    at = 0;

    /* With the line in focus the footer is a mouth, not a hint: what
     * is being typed stands here until enter or the send word says
     * it. It shows even with hints off -- it is not advice, it is the
     * only place the unsent word exists. */
    bool mouth = pipe_line() && focus() == pipe_line() &&
                 nav.at_generation == 0;
    if (mouth) {
        at = put(line, at, "say: ");
        for (u32 i = 0; i < say_len && at < sizeof(line) - 4; i++)
            line[at++] = say_buf[i];
        at = put(line, at, "_");
    } else if (settings_hints()) {
        at = put(line, at, "click anything you can see.  arrows move; "
                           "the wheel scrolls.  ");
        if (sendto_open && sendto_ask)
            at = put(line, at, "click a machine and it does the work. "
                               "esc keeps it here.");
        else if (sendto_open)
            at = put(line, at, "click a machine and it goes there. "
                               "esc keeps it here.");
        else if (focus() == pipe_arrivals() && nav.at_generation == 0)
            at = put(line, at, "scan finds machines; "
                               "click one and send reaches it.");
        else if (nav.mode == SHELL_GRAPH)
            at = put(line, at, "drag the empty ground to move the map; "
                               "the wheel zooms.");
        else if (nav.mode == SHELL_SPLIT)
            at = put(line, at, "two walks: the left one writes, "
                               "the right one reads.");
        else if (nav.mode == SHELL_TERM)
            at = put(line, at, "the keys go to the terminal; "
                               "'help' names its words.");
        else if (nav.mode == SHELL_INDEX)
            at = put(line, at, "typing searches what you can reach.");
        else if (obj_type(focus()) == TYPE_PROGRAM && proc_is_running(focus()))
            at = put(line, at, "point it at something to hand it over.");
        else if (obj_type(focus()) == TYPE_PICTURE &&
                 (focus_rights() & CAP_WRITE) && nav.at_generation == 0)
            at = put(line, at, "pick an ink, then draw.");
        else if (nav.sel_a != nav.sel_b)
            at = put(line, at, "typing replaces the marked letters.");
        else if (obj_type(focus()) == TYPE_TEXT &&
                 (focus_rights() & CAP_WRITE) && nav.at_generation == 0)
            at = put(line, at, "typing goes where the caret is. "
                               "drag to mark.");
        else
            at = put(line, at, "typing changes the object.");
    }
    /* The one deliberate end, far right where nothing reaches for it
     * by accident: everything is saved, then the machine sleeps. */
    i32 off_x = sw - PAD - 8 * GLYPH_W;
    {
        bool lit = is_hovered(HOT_OFF, 0);
        if (lit) fb_rect(off_x - 4, sh - 28 + 3, 8 * GLYPH_W + 8,
                         ROW, C_EDGE);
        text_at(off_x, sh - 28 + 6, sw, "turn off",
                lit ? C_READONLY : C_FAINT);
        hot_add(off_x - 4, sh - 28, 8 * GLYPH_W + 8, 28, HOT_OFF, 0);
    }
    /* And the other end that is a beginning: saved, then started
     * again -- the way a newly installed kernel is tried. */
    i32 again_x = off_x - 10 * GLYPH_W;
    {
        bool lit = is_hovered(HOT_RESTART, 0);
        if (lit) fb_rect(again_x - 4, sh - 28 + 3, 7 * GLYPH_W + 8,
                         ROW, C_EDGE);
        text_at(again_x, sh - 28 + 6, sw, "restart",
                lit ? C_READONLY : C_FAINT);
        hot_add(again_x - 4, sh - 28, 7 * GLYPH_W + 8, 28, HOT_RESTART, 0);
    }

    line[at] = 0;
    text_at(mx, sh - 28 + 6, again_x - 2 * GLYPH_W, line,
            mouth ? C_TEXT : C_FAINT);

    /* The send word, right after the gathered letters: the same act
     * as enter, for whoever speaks with the mouse. */
    if (mouth && say_len) {
        i32 sx2 = mx + ((i32)at + 2) * GLYPH_W;
        if (sx2 + 4 * GLYPH_W < off_x - 2 * GLYPH_W) {
            bool lit = is_hovered(HOT_LINE_SAY, 0);
            text_at(sx2, sh - 28 + 6, sw, "send",
                    lit ? C_ACCENT : C_TEXT);
            hot_add(sx2 - 3, sh - 28, 4 * GLYPH_W + 6, 28,
                    HOT_LINE_SAY, 0);
        }
    }

    /* The recipient chooser: send was pressed with nobody set, so the
     * choosing happens under the word itself. The scan is already
     * out; whoever answers stands here as a line, and a click on a
     * line points the pipe and lets the thing go in the same breath.
     * Drawn last so it lies over the panels and its clicks win. */
    if (sendto_open) {
        u32 nfound = pipe_found_count();
        i32 pw2 = 50 * GLYPH_W;
        i32 px = sendto_x - 4;
        if (px + pw2 > sw - PAD) px = sw - PAD - pw2;
        i32 py = 38;
        i32 ph = (i32)(1 + (nfound ? nfound : 1)) * ROW + 12;

        fb_rect(px - 1, py - 1, pw2 + 2, ph + 2, C_EDGE);
        fb_rect(px, py, pw2, ph, C_PANEL_HI);

        i32 ty2 = py + 8;
        text_at(px + 8, ty2, px + pw2,
                sendto_ask ? "ask whom" : "send to", C_DIM);
        if (!pipe_scanning()) {
            bool alit = is_hovered(HOT_SCAN, 1);
            i32 ax = px + pw2 - 10 * GLYPH_W - 8;
            if (alit) fb_rect(ax - 4, ty2 - 3, 10 * GLYPH_W + 8, ROW, C_EDGE);
            text_at(ax, ty2, px + pw2, "look again",
                    alit ? C_TEXT : C_ACCENT);
            hot_add(ax - 4, ty2 - 3, 10 * GLYPH_W + 8, ROW, HOT_SCAN, 1);
        }
        ty2 += ROW;

        if (nfound == 0) {
            text_at(px + 8, ty2, px + pw2,
                    pipe_scanning() ? "listening..."
                                    : "nobody answered -- is the other "
                                      "machine on?",
                    C_FAINT);
            ty2 += ROW;
        }
        for (u32 i = 0; i < nfound; i++) {
            u8 fip[4];
            char fname[24];
            bool fworks;
            if (!pipe_found_at(i, fip, fname, &fworks, NULL)) break;

            at = put(line, 0, "  ");
            at = put(line, at, fname);
            while (at < 20) line[at++] = ' ';
            at = put_dec(line, at, fip[0]); line[at++] = '.';
            at = put_dec(line, at, fip[1]); line[at++] = '.';
            at = put_dec(line, at, fip[2]); line[at++] = '.';
            at = put_dec(line, at, fip[3]);
            if (fworks) {
                while (at < 38) line[at++] = ' ';
                at = put(line, at, "takes work");
            }
            line[at] = 0;

            bool plit = is_hovered(HOT_SENDPICK, i);
            if (plit) fb_rect(px, ty2 - 3, pw2, ROW, C_EDGE);
            text_at(px + 8, ty2, px + pw2, line, plit ? C_TEXT : C_ACCENT);
            hot_add(px, ty2 - 3, pw2, ROW, HOT_SENDPICK, i);
            ty2 += ROW;
        }
    }

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
    nav.sel_a = nav.sel_b = 0;
    nav.sel_drag = false;
    nav.html_scroll = 0;
    scrolls[SCR_TEXT] = scrolls[SCR_BYTES] = 0;
    scrolls[SCR_STRUCT] = scrolls[SCR_CONTENTS] = 0;
    log_pinned = true;
    caret_chase = false;
    tfind_on = false;
    field_focus = -1;
    field_shape = 0;
    addr_edit = false;
    sendto_open = false;
    browse_back_count = browse_fwd_count = 0;
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
    nav.sel_a = nav.sel_b = 0;
    nav.sel_drag = false;
    nav.html_scroll = 0;
    scrolls[SCR_TEXT] = scrolls[SCR_BYTES] = 0;
    scrolls[SCR_STRUCT] = scrolls[SCR_CONTENTS] = 0;
    log_pinned = true;
    caret_chase = false;
    tfind_on = false;
    field_focus = -1;
    field_shape = 0;
    addr_edit = false;
    sendto_open = false;
    browse_back_count = browse_fwd_count = 0;
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

    /* A marked stretch goes first: erasing erases it, and a letter
     * takes its place. That is the whole meaning of marking something
     * and then typing. */
    bool struck = cut_marked(d, size, &len);

    /* Typing happens at the caret, and the caret goes where it is
     * clicked -- so a line in the middle can be reworked in place,
     * which is what makes a text of one-line facts editable at all. */
    if (codepoint == '\b') {
        if (struck) { nav.changes++; nav.redraw = true; return; }
        if (nav.caret == 0) return;
        for (u64 i = nav.caret - 1; i + 1 < size; i++) d[i] = d[i + 1];
        d[size - 1] = 0;
        nav.caret--;
    } else if (codepoint == KEY_DELETE) {
        if (struck) { nav.changes++; nav.redraw = true; return; }
        if (nav.caret >= len) return;
        for (u64 i = nav.caret; i + 1 < size; i++) d[i] = d[i + 1];
        d[size - 1] = 0;
    } else if (len + 1 < size) {
        for (u64 i = len + 1; i > nav.caret; i--) d[i] = d[i - 1];
        d[nav.caret] = (u8)codepoint;
        nav.caret++;
    } else if (!struck) {
        return;
    }
    caret_chase = true;
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

        /* The terminal takes the letters whole. Tab still turns the
         * modes; everything else is the talk. Up brings the last line
         * back, the pages leaf through the transcript. */
        if (nav.mode == SHELL_TERM && k.codepoint != KEY_TAB) {
            term_session *ts = term_screen();
            if      (k.codepoint == KEY_ENTER)  term_enter(ts);
            else if (k.codepoint == '\b')       term_rub(ts);
            else if (k.codepoint == KEY_ESCAPE) term_clear_line(ts);
            else if (k.codepoint == KEY_UP)     term_recall(ts);
            else if (k.codepoint == KEY_DOWN)   term_clear_line(ts);
            else if (k.codepoint == KEY_PGUP)   scroll_by(SCR_TERM, -10);
            else if (k.codepoint == KEY_PGDN)   scroll_by(SCR_TERM, 10);
            else if (k.codepoint >= 0x20 && k.codepoint < 0x7F && !k.ctrl)
                term_key(ts, (char)k.codepoint);
            nav.redraw = true;
            continue;
        }

        /* Typing a new address. The first line of the page is the ask;
         * this appends to it and, on enter, sends it. A backspace eats
         * the last letter, escape leaves the line as it was. */
        if (addr_edit) {
            object *f = focus();
            u8 *d = (u8 *)obj_data(f);
            if (obj_type(f) != TYPE_TEXT || !d ||
                !(focus_rights() & CAP_WRITE)) {
                addr_edit = false;
            } else if (k.codepoint == KEY_ESCAPE) {
                addr_edit = false; nav.redraw = true; continue;
            } else if (k.codepoint == KEY_ENTER) {
                addr_edit = false;
                browse_remember();
                browse_fwd_count = 0;
                ask_the_wire();
                continue;
            } else if (k.codepoint == '\b') {
                u64 e = 0, size = obj_size(f);
                while (e < size && d[e] && d[e] != '\n') e++;
                if (e > 0) {
                    for (u64 i = e - 1; i + 1 < size; i++) d[i] = d[i + 1];
                    d[size - 1] = 0;
                    nav.changes++;
                }
                nav.redraw = true; continue;
            } else if (k.codepoint >= 0x20 && k.codepoint < 0x7F) {
                u64 e = 0, size = obj_size(f);
                while (e < size && d[e] && d[e] != '\n') e++;
                u64 len = text_len(d, size);
                if (len + 1 < size) {
                    for (u64 i = len + 1; i > e; i--) d[i] = d[i - 1];
                    d[e] = (u8)k.codepoint;
                    nav.changes++;
                }
                nav.redraw = true; continue;
            } else {
                continue;
            }
        }

        /* Writing into a form field, when one holds the writing. Enter
         * sends the form, escape steps out, and everything else lands
         * in the field rather than the page. */
        if (field_focus >= 0 && (u32)field_focus < field_spot_count) {
            u32 fi = field_spots[field_focus].ref;
            if (fi >= field_count || field_defs[fi].kind == FIELD_SUBMIT) {
                field_focus = -1;
            } else if (k.codepoint == KEY_ESCAPE) {
                field_focus = -1; nav.redraw = true; continue;
            } else if (k.codepoint == KEY_ENTER) {
                u32 saved = fi; field_focus = -1; submit_form(saved); continue;
            } else if (k.codepoint == '\b') {
                u32 l = 0; while (field_val[fi][l]) l++;
                if (l) field_val[fi][l - 1] = 0;
                nav.redraw = true; continue;
            } else if (k.codepoint >= 0x20 && k.codepoint < 0x7F) {
                u32 l = 0; while (field_val[fi][l]) l++;
                if (l < HTML_VALUE_MAX - 1) {
                    field_val[fi][l] = (char)k.codepoint;
                    field_val[fi][l + 1] = 0;
                }
                nav.redraw = true; continue;
            } else {
                continue;
            }
        }

        /* While the in-text search is open, the letters feed the
         * needle; enter walks to the next place it lies, escape
         * closes it. Everything else falls through unchanged. */
        if (tfind_on && obj_type(focus()) == TYPE_TEXT) {
            if (k.codepoint == KEY_ESCAPE) {
                tfind_on = false;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint == KEY_ENTER) {
                object *f = focus();
                const u8 *d = (const u8 *)obj_data(f);
                if (d && tfind_len) {
                    u64 len = text_len(d, obj_size(f));
                    u64 from = nav.caret + 1;
                    i64 hit = -1, first = -1;
                    for (u64 i = 0; i + tfind_len <= len; i++) {
                        u32 j = 0;
                        while (j < tfind_len &&
                               to_lower((char)d[i + j]) ==
                               to_lower(tfind_buf[j]))
                            j++;
                        if (j == tfind_len) {
                            if (first < 0) first = (i64)i;
                            if (i >= from) { hit = (i64)i; break; }
                        }
                    }
                    if (hit < 0) hit = first;
                    if (hit >= 0) {
                        nav.caret = (u64)hit;
                        caret_chase = true;
                        nav.redraw = true;
                    }
                }
                continue;
            }
            if (k.codepoint == '\b') {
                if (tfind_len) tfind_len--;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint >= 0x20 && k.codepoint < 0x100 && !k.ctrl &&
                tfind_len < sizeof(tfind_buf) - 1) {
                tfind_buf[tfind_len++] = (char)k.codepoint;
                nav.redraw = true;
                continue;
            }
        }

        /* With the line in focus the bottom row is a mouth: letters
         * gather there, enter says them, escape swallows them. The
         * text itself is read-only, so nothing else wanted the keys.
         * Arrows and the rest fall through and still walk. */
        if (pipe_line() && focus() == pipe_line() &&
            nav.at_generation == 0) {
            if (k.codepoint == KEY_ENTER) {
                say_commit();
                continue;
            }
            if (k.codepoint == '\b') {
                if (say_len) say_len--;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint == KEY_ESCAPE && say_len) {
                say_len = 0;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint >= 0x20 && k.codepoint < 0x7F && !k.ctrl &&
                say_len < sizeof(say_buf) - 1) {
                say_buf[say_len++] = (char)k.codepoint;
                nav.redraw = true;
                continue;
            }
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
            if (sendto_open) {
                sendto_open = false;
                nav.redraw = true;
            } else if (nav.mode == SHELL_INDEX && find_len) {
                find_len = 0;
                nav.redraw = true;
            } else {
                leave_past();
            }
            continue;
        case KEY_UP:
        case KEY_DOWN: {
            /* With a page open, the arrows read it; everywhere else
             * they pick among the references. */
            bool page_open = false;
            for (u32 li = 0; li < nav.lens_count; li++)
                if (nav.lens[li] == LENS_HTML) page_open = true;
            if (page_open && nav.mode == SHELL_FOCUS &&
                html_rows > html_vis) {
                u32 max = html_rows - html_vis;
                if (k.codepoint == KEY_UP)
                    nav.html_scroll = nav.html_scroll > 3
                                    ? nav.html_scroll - 3 : 0;
                else
                    nav.html_scroll = nav.html_scroll + 3 > max
                                    ? max : nav.html_scroll + 3;
                nav.redraw = true;
                continue;
            }
            if (k.codepoint == KEY_UP) {
                if (nav.selected > 0) { nav.selected--; nav.redraw = true; }
            } else {
                u64 n = obj_slots(focus());
                if (n && nav.selected + 1 < n) {
                    nav.selected++;
                    nav.redraw = true;
                }
            }
            continue;
        }
        case KEY_RIGHT:
            follow(nav.selected);
            continue;
        case KEY_ENTER:
            /* In the index with a search on, enter goes to the first
             * answer. In a text one is writing, it makes a line;
             * anywhere else it follows, like the right arrow. */
            if (nav.mode == SHELL_INDEX) {
                if (find_len) {
                    build_index();
                    for (u32 i = 0; i < icount; i++) {
                        char why[44];
                        if (row_matches(&irows[i], why, sizeof(why))) {
                            index_walk_to(i);
                            find_len = 0;
                            break;
                        }
                    }
                }
                continue;
            }
            if (obj_type(focus()) == TYPE_TEXT &&
                (focus_rights() & CAP_WRITE) && nav.at_generation == 0)
                type_into_focus('\n');
            else
                follow(nav.selected);
            continue;
        case KEY_LEFT:
            go_back();
            continue;
        case KEY_DELETE:
            type_into_focus(KEY_DELETE);
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
            if (k.codepoint == 'f' && obj_type(focus()) == TYPE_TEXT) {
                tfind_on = !tfind_on;
                nav.redraw = true;
                continue;
            }
            continue;
        }

        /* While the index is open, letters ask the index. There is
         * nothing else typing could mean in a list of everything. */
        if (nav.mode == SHELL_INDEX) {
            if (k.codepoint == '\b') {
                if (find_len) { find_len--; nav.redraw = true; }
            } else if (k.codepoint >= 0x20 && k.codepoint < 0x7F &&
                       find_len < sizeof(find_buf) - 1) {
                find_buf[find_len++] = (char)k.codepoint;
                nav.redraw = true;
            }
            continue;
        }

        if (k.codepoint < 0x110000u) type_into_focus(k.codepoint);
    }
}

/* Points the pipe at a machine by writing the peer line into the
 * settings, exactly as a hand would -- so it persists, the journal
 * notes it, and the settings text stays the one honest record. The
 * new line takes effect at once, since choosing a recipient and then
 * sending must not race the next redraw. */
static void peer_write(const u8 ip[4])
{
    object *s = settings_object();
    if (!s) return;
    u8 *d = (u8 *)obj_data(s);
    if (!d) return;
    u64 size = obj_size(s);
    u64 len = text_len(d, size);

    char pl[64];
    u32 at = put(pl, 0, "peer     | ");
    at = put_dec(pl, at, ip[0]); pl[at++] = '.';
    at = put_dec(pl, at, ip[1]); pl[at++] = '.';
    at = put_dec(pl, at, ip[2]); pl[at++] = '.';
    at = put_dec(pl, at, ip[3]);
    at = put(pl, at, " 7800");
    pl[at++] = '\n';
    pl[at] = 0;

    /* A full settings page would swallow the click without a trace;
     * saying so beats writing half a line. */
    if (len + (u64)at + 2 > size) {
        journal_says("settings", "the page is full; make room first");
        return;
    }

    if (len && d[len - 1] != '\n' && len + 1 < size) d[len++] = '\n';
    for (u32 i = 0; pl[i] && len + 1 < size; i++) d[len++] = (u8)pl[i];
    d[len] = 0;

    settings_apply();
    nav.changes++;
    nav.redraw = true;
}

/* Sends the focused text to the network service, read-and-write --
 * the kernel-side twin of what fetch does from ring 3, taken when the
 * person themselves presses go or follows a link. */
static void ask_the_wire(void)
{
    object *f = focus();
    if (obj_type(f) != TYPE_TEXT || nav.at_generation != 0) return;
    if (!(focus_rights() & CAP_READ) || !(focus_rights() & CAP_WRITE))
        return;
    if (!net_port() || !net_up()) return;

    message m = { 0 };
    m.tag = 0x45474150ULL;              /* the same word fetch says */
    object *what = f;
    u32 rr = CAP_READ | CAP_WRITE;
    port_post(net_port(), &m, &what, &rr, 1, "you");
    nav.redraw = true;
}

/* Retreats to an earlier point on the path. */
static void go_back_to(u32 index)
{
    if (index + 1 >= nav.depth) return;
    while (nav.depth > index + 1) go_back();
}

/* Walks to an index row, the honest way: back to home, then reference
 * by reference along the way the row was found. Every step passes
 * through follow(), so what one arrives holding is what the path
 * grants -- the list is an overview, not a side door. */
static void index_walk_to(u32 row)
{
    if (row >= icount) return;

    u32 chain[64];
    u32 n = 0;
    for (i32 i = (i32)row;
         irows[i].parent >= 0 && n < 64;
         i = irows[i].parent)
        chain[n++] = irows[i].via;

    go_back_to(0);
    while (n) follow(chain[--n]);
    nav.mode = SHELL_FOCUS;
    nav.redraw = true;
}

/* Acts on whatever was clicked.
 *
 * One gesture, one meaning: pointing at a thing and pressing goes to
 * it, or turns it on. Nothing here opens a menu, and nothing needs a
 * second click to confirm -- every one of these is undone by clicking
 * somewhere else, so there is nothing to be careful about. */
static void act_on(const hot_region *r)
{
    /* An open recipient chooser closes on any click that is not its
     * own; a second press on the word that opened it is the closing
     * itself. */
    if (sendto_open && r->kind != HOT_SENDPICK && r->kind != HOT_SCAN) {
        sendto_open = false;
        nav.redraw = true;
        if (r->kind == HOT_SEND || r->kind == HOT_ASK) return;
    }

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

    case HOT_INDEX:
        index_walk_to(r->index);
        break;

    case HOT_TEXT: {
        /* A click into the writing puts the caret there, and starts a
         * mark: holding the button and moving stretches it. The pixel
         * is turned back into a row and a column, and the column walk
         * finds which letter that is. Reading is enough -- marking and
         * lifting take nothing a reader does not have. */
        i32 row = (nav.mouse_y - text_area.y) / GLYPH_H;
        i32 col = (nav.mouse_x - text_area.x + GLYPH_W / 2) / GLYPH_W;
        if (row < 0) row = 0;
        if (col < 0) col = 0;
        row += (i32)text_area.scroll;
        nav.caret = text_index_at(focus(), row, col);
        nav.sel_a = nav.sel_b = nav.caret;
        nav.sel_drag = true;
        nav.redraw = true;
        break;
    }

    case HOT_TAKE: {
        object *f = focus();
        const u8 *d = (const u8 *)obj_data(f);
        if (!d) break;
        u64 len = text_len(d, obj_size(f));
        u64 lo, hi;
        marked_range(len, &lo, &hi);
        held_len = 0;
        for (u64 i = lo; i < hi && held_len < sizeof(held_text); i++)
            held_text[held_len++] = (char)d[i];
        nav.sel_a = nav.sel_b = 0;
        nav.redraw = true;
        break;
    }

    case HOT_PUT: {
        object *f = focus();
        if (obj_type(f) != TYPE_TEXT) break;
        if (!(focus_rights() & CAP_WRITE) || nav.at_generation != 0) break;
        u8 *d = (u8 *)obj_data(f);
        if (!d || held_len == 0) break;

        u64 size = obj_size(f);
        u64 len = text_len(d, size);
        if (nav.caret > len) nav.caret = len;
        if (len + 1 >= size) break;

        u64 room = size - 1 - len;
        u64 n = held_len > room ? room : held_len;
        for (u64 j = len; j + 1 > nav.caret; j--) {
            d[j + n] = d[j];
            if (j == 0) break;
        }
        for (u64 i = 0; i < n; i++) d[nav.caret + i] = (u8)held_text[i];
        nav.caret += n;
        caret_chase = true;
        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_DROP:
        held_len = 0;
        nav.redraw = true;
        break;

    case HOT_CAPCUT: {
        /* Taking one capability back from a running program. The row
         * indexes the program's own table, so this reaches what was
         * passed to it from anywhere -- by the shell, or by another
         * program -- and the withdrawing is the same act either way. */
        object *f = focus();
        if (!(focus_rights() & CAP_GRANT) || nav.at_generation != 0) break;
        domain *pd = proc_domain_of(f);
        if (!pd) break;

        u32 hr = 0;
        object *t = domain_cap_at(pd, r->index, &hr);
        if (!t) break;

        proc_revoke(f, t);
        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_MODE:
        nav.mode = (shell_mode)(r->index % SHELL_MODE_COUNT);
        nav.changes++;
        nav.redraw = true;
        break;

    case HOT_FINDX:
        find_len = 0;
        nav.redraw = true;
        break;

    case HOT_INK:
        ink_current = (u8)(r->index & 15);
        nav.redraw = true;
        break;

    case HOT_CANVAS:
        paint_last_col = paint_last_row = -1;
        paint_at_pointer();
        nav.paint_drag = true;
        break;

    case HOT_ADDR:
        addr_edit = true;
        field_focus = -1;
        nav.redraw = true;
        break;

    case HOT_SEND:
        /* Off to the peer -- and with no peer named yet, the press
         * opens the choosing right here: a scan goes out, whoever
         * answers stands below as a line, and clicking a line sends.
         * The substance is copied on the spot; what became of it the
         * journal will say, since the carrying is the network
         * thread's walk, not this click's wait. */
        if (focus_rights() & CAP_READ) {
            if (settings_peer(NULL, NULL)) {
                pipe_post(focus());
            } else {
                sendto_open = true;
                sendto_ask = false;
                pipe_scan();
            }
        }
        nav.redraw = true;
        break;

    case HOT_ASK:
        /* The same gesture with a request attached: the text runs on
         * the peer's machine and its answer comes home. Without a
         * peer, the same chooser opens -- picking a machine asks it. */
        if (focus_rights() & CAP_READ) {
            if (settings_peer(NULL, NULL)) {
                pipe_ask(focus(), (focus_rights() & CAP_WRITE) &&
                                  nav.at_generation == 0);
            } else {
                sendto_open = true;
                sendto_ask = true;
                pipe_scan();
            }
        }
        nav.redraw = true;
        break;

    case HOT_SCAN:
        pipe_scan();
        nav.redraw = true;
        break;

    case HOT_PEERPICK: {
        /* Connecting is one click: the found machine's address goes
         * into the settings as a written line. */
        u8 fip[4];
        char fname[24];
        if (!pipe_found_at(r->index, fip, fname, NULL, NULL)) break;
        peer_write(fip);
        break;
    }

    case HOT_SENDPICK: {
        /* Chosen mid-send: point the pipe at the machine and let the
         * thing go -- or the work -- in the same breath. */
        u8 fip[4];
        char fname[24];
        if (!pipe_found_at(r->index, fip, fname, NULL, NULL)) break;
        peer_write(fip);
        if (focus_rights() & CAP_READ) {
            if (sendto_ask)
                pipe_ask(focus(), (focus_rights() & CAP_WRITE) &&
                                  nav.at_generation == 0);
            else
                pipe_post(focus());
        }
        sendto_open = false;
        nav.redraw = true;
        break;
    }

    case HOT_GO:
        addr_edit = false;
        browse_remember();
        browse_fwd_count = 0;
        ask_the_wire();
        break;

    case HOT_LINK: {
        /* Going where the link points: remember where we stood, then
         * resolve the link against it and go. The same steps a person
         * would take by hand, in one press. */
        object *f = focus();
        if (obj_type(f) != TYPE_TEXT || nav.at_generation != 0) break;
        if (!(focus_rights() & CAP_WRITE)) break;
        if (r->index >= link_spot_count) break;
        u32 li = link_spots[r->index].ref;
        if (li >= link_count) break;

        char here[HTML_URL_MAX], dest[HTML_URL_MAX];
        current_ask(f, here, sizeof(here));
        resolve_url(here, link_urls[li], dest, sizeof(dest));
        if (dest[0] == 0) break;

        browse_remember();
        browse_fwd_count = 0;
        browse_to(dest);
        break;
    }

    case HOT_FIELD:
        if (r->index < field_spot_count) field_focus = (i32)r->index;
        nav.redraw = true;
        break;

    case HOT_SUBMIT:
        if (r->index < field_spot_count)
            submit_form(field_spots[r->index].ref);
        break;

    case HOT_BACK:
        if (browse_back_count > 0) {
            object *f = focus();
            char here[HTML_URL_MAX];
            if (obj_type(f) == TYPE_TEXT &&
                current_ask(f, here, sizeof(here)) > 0 &&
                browse_fwd_count < HISTORY_DEPTH)
                memcpy(browse_fwd[browse_fwd_count++], here, HTML_URL_MAX);
            browse_to(browse_back[--browse_back_count]);
        }
        break;

    case HOT_FWD:
        if (browse_fwd_count > 0) {
            browse_remember();
            browse_to(browse_fwd[--browse_fwd_count]);
        }
        break;

    case HOT_SCROLL: {
        /* The edge pages: upper half back, lower half onward, in any
         * area that scrolls. Which one is in the index. */
        u8 area = (u8)(r->index >> 1);
        if (area >= SCR_COUNT) break;
        u32 vis = scr_vis[area];
        i32 step = (i32)(vis > 4 ? vis - 2 : 3);
        scroll_by(area, (r->index & 1) ? step : -step);
        break;
    }

    case HOT_RUN: {
        /* The text becomes a program. The running instance lands next
         * to its words, in the object one came through, named like
         * them -- the text stays a text, and stays editable, which is
         * what makes this an editor: the next pass through a line
         * runs whatever the line says by then. */
        object *f = focus();
        if (nav.at_generation != 0 || nav.depth < 2) break;
        bool image = obj_type(f) == TYPE_BYTES &&
                     code_image_ok((const u8 *)obj_data(f), obj_size(f),
                                   NULL, NULL, NULL);
        if (obj_type(f) != TYPE_TEXT && !image) break;

        object *holder = nav.node[nav.depth - 2];
        u32 hr = nav.rights[nav.depth - 2];
        bool can = (obj_type(holder) == TYPE_PROGRAM)
                 ? (hr & CAP_GRANT) != 0
                 : (hr & CAP_WRITE) != 0;
        if (!can) break;

        object *prog = image ? code_launch(f) : runner_launch(f);
        if (!prog) break;

        u64 slots = obj_slots(holder), spot = slots;
        for (u64 i = 0; i < slots; i++)
            if (!obj_get_slot(holder, i)) { spot = i; break; }
        if (spot == slots && !obj_grow_slots(holder, slots + 1)) {
            obj_release(prog);              /* it runs, but lies nowhere */
            break;
        }

        char nm[40];
        label_of(holder, nav.via[nav.depth - 1], f, nm, sizeof(nm));

        obj_set_slot(holder, spot, prog, CAP_READ | CAP_GRANT);
        obj_set_slot_name(holder, spot, nm);
        if (obj_type(holder) == TYPE_PROGRAM)
            proc_grant(holder, prog, CAP_READ | CAP_GRANT);
        obj_release(prog);                  /* the slot holds it now */

        nav.changes++;
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

        bool found_j = false;
        for (u64 i = 0; i < obj_slots(focus()) && !found_j; i++)
            if (obj_get_slot(focus(), i) == j) {
                nav.selected = (u32)i;
                follow(i);
                found_j = true;
            }

        /* The log lives on the system shelf now; one list down is
         * still a real walk, reference by reference. */
        for (u64 i = 0; i < obj_slots(focus()) && !found_j; i++) {
            object *l = obj_get_slot(focus(), i);
            if (!l || obj_type(l) != TYPE_LIST) continue;
            for (u64 k = 0; k < obj_slots(l); k++) {
                if (obj_get_slot(l, k) != j) continue;
                nav.selected = (u32)i;
                follow(i);
                nav.selected = (u32)k;
                follow(k);
                found_j = true;
                break;
            }
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

        /* What is let go of steps into the bin first, petname and
         * rights along -- a slip of the pointer should cost a walk to
         * the bin, not the thing. Letting go inside the bin itself is
         * final, and so is letting go of the bin. The bin is made the
         * first time it is needed, next to everything else on home. */
        object *home = nav.node[0];
        object *bin = NULL;
        if (nav.rights[0] & CAP_WRITE) {
            for (u64 i = 0; i < obj_slots(home); i++) {
                object *s = obj_get_slot(home, i);
                const char *n = obj_slot_name(home, i);
                if (s && n && strcmp(n, "bin") == 0 &&
                    obj_type(s) == TYPE_LIST) { bin = s; break; }
            }
        }

        bool final = !target || focus() == bin || target == bin ||
                     !(nav.rights[0] & CAP_WRITE);

        if (!final && !bin) {
            object *made = obj_create(TYPE_LIST, 0, 4);
            if (made) {
                obj_set_name(made, "bin");
                u64 n = obj_slots(home), at2 = n;
                for (u64 i = 0; i < n; i++)
                    if (!obj_get_slot(home, i)) { at2 = i; break; }
                if (at2 < n || obj_grow_slots(home, n + 1)) {
                    obj_set_slot(home, at2, made, CAP_READ | CAP_WRITE);
                    obj_set_slot_name(home, at2, "bin");
                    bin = made;
                }
                obj_release(made);
            }
        }

        if (!final && bin) {
            u64 n = obj_slots(bin), at2 = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(bin, i)) { at2 = i; break; }
            if (at2 < n || obj_grow_slots(bin, n + 1)) {
                obj_set_slot(bin, at2, target,
                             obj_slot_rights(focus(), r->index));
                obj_set_slot_name(bin, at2,
                                  obj_slot_name(focus(), r->index));
            }
        }

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

    case HOT_OFF:
        system_off();
        nav.redraw = true;
        break;

    case HOT_RESTART:
        system_restart();
        nav.redraw = true;
        break;

    case HOT_INSTALL: {
        object *f = focus();
        if (!(focus_rights() & CAP_READ) || nav.at_generation != 0 || !elf_smells(f)) break;
        char why[120];
        if (!fat_install_kernel((const u8 *)obj_data(f), obj_size(f), why, sizeof(why))) {
            journal_says("install", why);
        } else {
            kprintf("boot: a kernel of %llu bytes is installed; the previous one is kernel.old\n", obj_size(f));
            journal_says("system", "a new kernel is installed for the next start; restart runs it");
        }
        nav.redraw = true;
        break;
    }

    case HOT_END:
        if ((focus_rights() & CAP_GRANT) && nav.at_generation == 0 &&
            proc_end(focus())) {
            journal_says("system", "a program was ended by hand");
            nav.changes++;
        }
        nav.redraw = true;
        break;

    case HOT_TFIND:
        tfind_on = !tfind_on;
        nav.redraw = true;
        break;

    case HOT_LINE_SAY:
        say_commit();
        break;

    case HOT_BUILD: {
        /* Everything in the focused list that the tools read becomes
         * one thing that runs -- or the kernel, when kmain is among
         * it. The report goes to the journal, line by line. */
        object *f = focus();
        if (obj_type(f) != TYPE_LIST || nav.at_generation != 0) break;
        if (!(focus_rights() & CAP_WRITE)) break;
        char base[40];
        if (nav.depth >= 2) label_of(nav.node[nav.depth - 2], nav.via[nav.depth - 1], f, base, sizeof(base));
        else put(base, 0, "home");
        base[19] = 0;
        if (term_build_start(f, base))
            journal_says("build", "building in the background; each text is named here as it is done");
        else
            journal_says("build", "a build is running already");
        nav.redraw = true;
        break;
    }

    case HOT_ASSEMBLE:
    case HOT_COMPILE: {
        /* The text becomes an image, laid beside it in the holder one
         * came through, named after it -- through the compiler first
         * when it is c, and then the assembly lies there too, to be
         * read. What went wrong is said in the journal with its line,
         * and nothing is laid. */
        object *f = focus();
        if (obj_type(f) != TYPE_TEXT || nav.at_generation != 0) break;
        if (nav.depth < 2) break;
        object *holder = nav.node[nav.depth - 2];
        if (!(nav.rights[nav.depth - 2] & CAP_WRITE)) break;
        if (obj_type(holder) == TYPE_PROGRAM) break;

        char base[40];
        label_of(holder, nav.via[nav.depth - 1], f, base, sizeof(base));
        base[19] = 0;

        if (term_building()) { journal_says("compile", "a build is running; the tools are busy until it is done"); break; }
        static char *text;
        if (!text) text = (char *)lang_big_alloc(4u << 20);
        u8 *image = lang_out_buffer();
        if (!text || !image) { journal_says("compile", "there is no room for the tools' tables"); break; }
        char err[112];
        const u8 *src = (const u8 *)obj_data(f);
        u64 slen = text_len(src, obj_size(f));
        bool gnu = false;

        if (r->kind == HOT_COMPILE) {
            i64 got = cc_compile(src, slen, base, find_beside_in, holder,
                                 text, 4u << 20, err, sizeof(err));
            if (got < 0) { journal_says("compile", err); nav.redraw = true; break; }
            object *asm_text = obj_create(TYPE_TEXT, (u64)got + 16, 0);
            if (!asm_text) break;
            memcpy(obj_data(asm_text), text, (u64)got);
            char an[40];
            u32 n = put(an, 0, base);
            put(an, n, " asm");
            if (!lay_beside(holder, asm_text, CAP_READ | CAP_WRITE | CAP_GRANT, an)) {
                obj_release(asm_text);
                break;
            }
            obj_release(asm_text);
            src = (const u8 *)text;
            slen = (u64)got;
        } else {
            /* the kernel's own assembly files are in the gnu dialect;
             * the text says so itself */
            u32 bl = (u32)strlen(base);
            gnu = (bl > 2 && base[bl - 2] == '.' && base[bl - 1] == 'S') || gnu_looks(src, slen);
        }

        u32 kind = 0;
        i64 got = lang_build_text(src, slen, gnu, image, LANG_OUT_MAX, &kind, err, sizeof(err));
        if (got < 0) {
            journal_says(r->kind == HOT_COMPILE ? "the assembler refused what the compiler made"
                                                : "assemble", err);
            nav.redraw = true;
            break;
        }

        object *made = obj_create(TYPE_BYTES, (u64)got, 0);
        if (!made) break;
        memcpy(obj_data(made), image, (u64)got);
        char nm[40];
        u32 n = put(nm, 0, base);
        put(nm, n, kind == LANG_IMAGE ? " code" : " object");
        if (!lay_beside(holder, made, CAP_READ | CAP_WRITE | CAP_GRANT, nm)) {
            obj_release(made);
            break;
        }
        obj_release(made);
        journal_says(r->kind == HOT_COMPILE ? "compile" : "assemble",
                     kind == LANG_IMAGE ? "an image lies beside the text; run it"
                                        : "an object lies beside the text; it waits for other texts' names -- link joins them");
        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_COPY: {
        object *f = focus();
        if (!(focus_rights() & CAP_READ) || nav.at_generation != 0)
            break;

        type_id t = obj_type(f);
        object *made = NULL;
        if (t == TYPE_LIST) {
            u64 n = obj_slots(f);
            made = obj_create(TYPE_LIST, 0, n ? n : 4);
            if (made) for (u64 i = 0; i < n; i++) {
                object *s = obj_get_slot(f, i);
                if (!s) continue;
                obj_set_slot(made, i, s, obj_slot_rights(f, i));
                obj_set_slot_name(made, i, obj_slot_name(f, i));
            }
        } else if (t == TYPE_TEXT || t == TYPE_BYTES ||
                   t == TYPE_PICTURE) {
            made = obj_create(t, obj_size(f), 0);
            if (made && obj_data(f))
                memcpy(obj_data(made), obj_data(f), obj_size(f));
        }
        if (!made) break;
        if (obj_name(f)) obj_set_name(made, obj_name(f));

        /* Named after the original, laid down beside it. */
        char nm[OBJ_NAME_MAX];
        char base[20];
        label_of(nav.depth >= 2 ? nav.node[nav.depth - 2] : NULL,
                 nav.depth >= 2 ? nav.via[nav.depth - 1] : 0,
                 f, base, sizeof(base));
        u32 at2 = 0;
        while (base[at2] && at2 < sizeof(base) - 1) { nm[at2] = base[at2]; at2++; }
        at2 = put(nm, at2, " copy");
        nm[at2] = 0;

        object *place = NULL;
        if (nav.depth >= 2 &&
            (nav.rights[nav.depth - 2] & CAP_WRITE) &&
            obj_type(nav.node[nav.depth - 2]) != TYPE_PROGRAM)
            place = nav.node[nav.depth - 2];
        else if ((focus_rights() & CAP_WRITE) && t == TYPE_LIST)
            place = f;

        bool placed_it = false;
        if (place) {
            u64 n = obj_slots(place), spot = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(place, i)) { spot = i; break; }
            if (spot < n || obj_grow_slots(place, n + 1)) {
                obj_set_slot(place, spot, made,
                             CAP_READ | CAP_WRITE | CAP_GRANT);
                obj_set_slot_name(place, spot, nm);
                placed_it = true;
            }
        }
        obj_release(made);
        journal_says("system", placed_it ? "a copy lies beside it"
                             : "nowhere writable to lay the copy");
        nav.changes++;
        nav.redraw = true;
        break;
    }

    case HOT_FATIN:
        if (focus() == system_disk() && system_disk()) {
            u32 got = fat_take_in(system_disk());
            journal_says("system", got ? "the disk's files came in"
                                       : "nothing new on the disk");
            nav.changes++;
        }
        nav.redraw = true;
        break;

    case HOT_FATOUT:
        if (focus() == system_disk() && system_disk()) {
            u32 wrote = fat_write_out(system_disk());
            journal_says("system", wrote ? "written out to the disk"
                                         : "nothing new to write out");
        }
        nav.redraw = true;
        break;

    case HOT_P2REF:
        p2.selected = r->index;
        p2_follow(r->index);
        break;

    case HOT_P2TRAIL:
        p2_back_to(r->index);
        break;

    case HOT_PACK:
    case HOT_UNPACK: {
        if (!(focus_rights() & CAP_READ) || nav.at_generation != 0)
            break;

        object *made = (r->kind == HOT_PACK)
                     ? bundle_pack(focus())
                     : bundle_unpack(focus());
        if (!made) {
            journal_says("system", r->kind == HOT_PACK
                         ? "it does not fit into one bundle"
                         : "these bytes do not unfold");
            nav.redraw = true;
            break;
        }

        /* The result lies down next to what it came from -- in the
         * holder one came through -- or inside the focus when there
         * is no holder to write into. */
        object *place = NULL;
        if (nav.depth >= 2 &&
            (nav.rights[nav.depth - 2] & CAP_WRITE) &&
            obj_type(nav.node[nav.depth - 2]) != TYPE_PROGRAM)
            place = nav.node[nav.depth - 2];
        else if ((focus_rights() & CAP_WRITE) &&
                 obj_type(focus()) == TYPE_LIST)
            place = focus();

        bool placed_it = false;
        if (place) {
            u64 n = obj_slots(place), at2 = n;
            for (u64 i = 0; i < n; i++)
                if (!obj_get_slot(place, i)) { at2 = i; break; }
            if (at2 < n || obj_grow_slots(place, n + 1)) {
                obj_set_slot(place, at2, made, CAP_READ | CAP_WRITE);
                obj_set_slot_name(place, at2,
                                  r->kind == HOT_PACK ? "packed"
                                                      : "unpacked");
                placed_it = true;
            }
        }
        obj_release(made);
        journal_says("system", !placed_it
                     ? "nowhere writable to lay it down"
                     : r->kind == HOT_PACK ? "packed into one thing"
                                           : "unfolded into a list");
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
        else if (r->index == 3) {
            made = obj_create(TYPE_PICTURE,
                              PICTURE_HEADER + PICTURE_W * PICTURE_H, 0);
            if (made) {
                u8 *pd = (u8 *)obj_data(made);
                pd[0] = (u8)(PICTURE_W & 0xFF);
                pd[1] = (u8)(PICTURE_W >> 8);
                pd[4] = (u8)(PICTURE_H & 0xFF);
                pd[5] = (u8)(PICTURE_H >> 8);
            }
            suggest = "picture";
            created = true;
        }
        else if (r->index == 4) {
            /* A text that means to be run. Nothing marks it as one --
             * any text can be run -- but starting from a line that
             * already works beats starting from a blank page. */
            made = obj_create(TYPE_TEXT, 1024, 0);
            if (made) {
                static const char first[] = "say hello from this text\n";
                u8 *sd = (u8 *)obj_data(made);
                for (u32 i = 0; i < sizeof(first); i++)
                    sd[i] = (u8)first[i];
            }
            suggest = "script";
            created = true;
        }
        else if (r->index == 5) {
            /* Room for a page from outside. The first line names it;
             * hand the page to fetch and the rest fills in. */
            made = obj_create(TYPE_TEXT, 16384, 0);
            if (made) {
                static const char ask[] = "example.com\n";
                u8 *pdta = (u8 *)obj_data(made);
                for (u32 i = 0; i < sizeof(ask); i++)
                    pdta[i] = (u8)ask[i];
            }
            suggest = "page";
            created = true;
        }
        else if (r->index == 6) {
            /* Work to be divided among machines. The first line says
             * how; the recipe below runs over there, each part on
             * its stretch of the range, and the answer is written
             * back in. This one sums the numbers from 1 to 10000. */
            made = obj_create(TYPE_TEXT, 1024, 0);
            if (made) {
                static const char seed[] =
                    "split 4 from 1 to 10000\n"
                    "wait\n"
                    "set a m\n"
                    "wait\n"
                    "set b m\n"
                    "set s 0\n"
                    "add s a\n"
                    "add a 1\n"
                    "if a > b\n"
                    "skip 1\n"
                    "back 4\n"
                    "answer s\n"
                    "stop\n";
                u8 *tdta = (u8 *)obj_data(made);
                for (u32 i = 0; i < sizeof(seed); i++)
                    tdta[i] = (u8)seed[i];
            }
            suggest = "task";
            created = true;
        }
        else if (r->index < PALETTE_FIXED + standard_count()) {
            /* A fresh instance, handed back with a hold of ours that
             * the slot below replaces with its own. Read and grant go
             * on the reference -- running programs are watched and
             * given to, never written into. */
            made = standard_launch(r->index - PALETTE_FIXED);
            if (!made) break;
            created = true;
            give = CAP_READ | CAP_GRANT;
            suggest = standard_name(r->index - PALETTE_FIXED);
        }
        else {
            carried carry[CARRY_MAX];
            u32 n = gather(carry);
            u32 c = r->index - PALETTE_FIXED - standard_count();
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

        /* The wheel reads whatever it is over -- and over the map it
         * zooms instead, around the pointer. */
        if (m.dz) {
            if (nav.mode == SHELL_GRAPH && graph_bot > graph_top &&
                nav.mouse_y >= graph_top && nav.mouse_y < graph_bot)
                graph_zoom_at(nav.mouse_x, nav.mouse_y, m.dz);
            else
                wheel_at(nav.mouse_x, nav.mouse_y, m.dz);
        }

        if (is && !was) {
            i32 h = hot_at(nav.mouse_x, nav.mouse_y);
            if (h >= 0) {
                act_on(&hots[h]);
            } else if (sendto_open) {
                sendto_open = false;     /* clicked past the chooser */
                nav.redraw = true;
            } else if (nav.mode == SHELL_GRAPH && graph_bot > graph_top &&
                       nav.mouse_y >= graph_top &&
                       nav.mouse_y < graph_bot) {
                /* Empty ground taken hold of: the map will follow. */
                graph_drag = true;
                graph_last_x = nav.mouse_x;
                graph_last_y = nav.mouse_y;
            }
        }
        if (!is && was) {
            nav.sel_drag = false;
            nav.paint_drag = false;
            graph_drag = false;
        }
    }

    if (!moved) return;

    /* A held button over the canvas keeps painting: the stroke is the
     * gesture, not one cell per click. */
    if (nav.paint_drag && (nav.buttons & 1)) paint_at_pointer();

    /* A held button on the map's empty ground drags the map. */
    if (graph_drag && (nav.buttons & 1)) {
        graph_pan_x += nav.mouse_x - graph_last_x;
        graph_pan_y += nav.mouse_y - graph_last_y;
        graph_last_x = nav.mouse_x;
        graph_last_y = nav.mouse_y;
        nav.redraw = true;
    }

    /* Dragging with the button down stretches the mark. The moving end
     * is also the caret, so letting go leaves one standing exactly at
     * the edge of what was marked. */
    if (nav.sel_drag && (nav.buttons & 1)) {
        i32 row = (nav.mouse_y - text_area.y) / GLYPH_H;
        i32 col = (nav.mouse_x - text_area.x + GLYPH_W / 2) / GLYPH_W;
        if (row < 0) row = 0;
        if (col < 0) col = 0;
        row += (i32)text_area.scroll;
        u64 at = text_index_at(focus(), row, col);
        if (at != nav.sel_b) {
            nav.sel_b = at;
            nav.caret = at;
            nav.redraw = true;
        }
    }

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
        u64 seq = journal_sequence() + term_sequence(term_screen());
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
