/*
 * shell.c -- moving through the object graph.
 */
#include <eb/shell.h>
#include <eb/fb.h>
#include <eb/ps2.h>
#include <eb/thread.h>
#include <eb/msg.h>

#define TRAIL_MAX 12
#define LENS_MAX   3
#define GRAPH_MAX 48

#define PAD    10
#define ROW    (GLYPH_H + 6)

#define C_BACK      RGB( 12,  14,  19)
#define C_PANEL     RGB( 22,  26,  34)
#define C_PANEL_HI  RGB( 30,  36,  48)
#define C_EDGE      RGB( 48,  56,  72)
#define C_TEXT      RGB(214, 219, 230)
#define C_DIM       RGB(120, 130, 150)
#define C_FAINT     RGB( 74,  82,  98)
#define C_ACCENT    RGB(122, 172, 255)
#define C_WRITE     RGB(126, 200, 140)
#define C_READONLY  RGB(206, 158,  92)
#define C_BAR       RGB(  8,  10,  14)

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

    u64  changes;
    bool redraw;
    i32  mouse_x, mouse_y;
    u8   buttons;
} nav;

static object *focus(void)      { return nav.node[nav.depth - 1]; }
static u32     focus_rights(void){ return nav.rights[nav.depth - 1]; }

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

static void lens_text(object *o, i32 x, i32 y, i32 w, i32 h, bool caret)
{
    const u8 *d = (const u8 *)obj_data(o);
    if (!d) { text_at(x, y, x + w, "(nothing to show)", C_FAINT); return; }

    u64 len = text_len(d, obj_size(o));
    i32 cols = w / GLYPH_W;
    i32 cx = 0, cy = 0;

    for (u64 i = 0; i < len && cy * GLYPH_H < h; i++) {
        u8 ch = d[i];
        if (ch == '\n' || cx >= cols) {
            cx = 0; cy++;
            if (ch == '\n') continue;
        }
        if (cy * GLYPH_H >= h) break;
        fb_glyph(x + cx * GLYPH_W, y + cy * GLYPH_H, ch, C_TEXT, 0, false);
        cx++;
    }
    if (caret && cy * GLYPH_H < h)
        fb_rect(x + cx * GLYPH_W, y + cy * GLYPH_H, 2, GLYPH_H, C_ACCENT);
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

    u32 at = put(line, 0, "type    ");
    at = put(line, at, type_name(obj_type(o)));
    line[at] = 0;
    text_at(x, ty, x + w, line, C_TEXT);
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
    ty += ROW + ROW / 2;

    if (obj_slots(o) == 0) {
        text_at(x, ty, x + w, "points at nothing", C_FAINT);
        return;
    }

    text_at(x, ty, x + w, "points at", C_DIM);
    ty += ROW;
    for (u64 i = 0; i < obj_slots(o) && ty < y + h; i++) {
        object *t = obj_get_slot(o, i);
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
        text_at(x, ty, x + w, line, t ? C_DIM : C_FAINT);
        ty += ROW;
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
    i32 left_w = 300;
    i32 right_w = 340;
    i32 mid_x = PAD + left_w + PAD;
    i32 mid_w = sw - mid_x - right_w - 2 * PAD;

    object *f = focus();

    /* --- the path we took --------------------------------------- */
    fb_rect(PAD, top, left_w, bottom - top, C_PANEL);
    text_at(PAD + PAD, top + PAD, PAD + left_w, "how you got here", C_FAINT);

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
        if (here) fb_rect(PAD, ty - 3, left_w, ROW, C_PANEL_HI);
        text_at(PAD + PAD, ty, PAD + left_w - PAD, line,
                here ? C_TEXT : C_DIM);
        ty += ROW;
    }

    /* --- the object itself, through its lenses -------------------- */
    fb_rect(mid_x, top, mid_w, bottom - top, C_PANEL);

    i32 tab_x = mid_x + PAD;
    for (u32 i = 0; i < LENS_COUNT; i++) {
        bool on = false;
        for (u32 j = 0; j < nav.lens_count; j++) if (nav.lens[j] == i) on = true;

        const char *n = lens_name((lens_kind)i);
        i32 w = (i32)(8 * GLYPH_W);
        if (on) fb_rect(tab_x - 4, top + PAD - 4, w, ROW, C_PANEL_HI);
        text_at(tab_x, top + PAD, mid_x + mid_w, n, on ? C_ACCENT : C_FAINT);
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
    text_at(rx + PAD, top + PAD, sw - PAD, "where it leads", C_FAINT);

    ty = top + PAD + ROW + 4;
    u64 slots = obj_slots(f);
    if (slots == 0) {
        text_at(rx + PAD, ty, sw - PAD, "nowhere -- this is a leaf", C_FAINT);
    }
    for (u64 i = 0; i < slots && ty < bottom - ROW; i++) {
        object *t = obj_get_slot(f, i);
        char what[40], line[80], r[4];
        label_of(f, i, t, what, sizeof(what));
        rights_text(focus_rights() & obj_slot_rights(f, i), r);

        bool picked = (i == nav.selected);
        if (picked) fb_rect(rx, ty - 3, right_w, ROW, C_PANEL_HI);

        u32 at = put(line, 0, picked ? "> " : "  ");
        at = put(line, at, r);
        at = put(line, at, "  ");
        at = put(line, at, what);
        line[at] = 0;

        text_at(rx + PAD, ty, sw - PAD, line,
                !t ? C_FAINT : (picked ? C_TEXT : C_DIM));
        ty += ROW;
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

        fb_rect(x - 2, y - 2, node_w + 4, node_h + 4,
                is_focus ? C_ACCENT : (on_path ? C_EDGE : C_PANEL));
        fb_rect(x, y, node_w, node_h, is_focus ? C_PANEL_HI : C_PANEL);

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
            claimed ? "the object calls itself that -- you have not named it"
                    : "you named this reference",
            C_FAINT);

    char line[96];
    u32 at = put(line, 0, type_name(obj_type(focus())));
    at = put(line, at, "  ");
    at = put(line, at, r);
    at = put(line, at, (focus_rights() & CAP_WRITE) ? "  you may change this"
                                                    : "  read only");
    line[at] = 0;
    text_at(sw / 2 + PAD, 14, sw - PAD, line,
            (focus_rights() & CAP_WRITE) ? C_WRITE : C_READONLY);

    switch (nav.mode) {
    case SHELL_FOCUS: draw_focus_shell(sw, sh, top, bottom); break;
    case SHELL_GRAPH: draw_graph_shell(sw, sh, top, bottom); break;
    case SHELL_TILES: draw_tiles_shell(sw, sh, top, bottom); break;
    default: break;
    }

    /* Footer: the mode, and what the keys do. No menu bar -- there is
     * nothing to put in one that is not already visible. */
    fb_rect(0, sh - 28, sw, 28, C_BAR);
    at = put(line, 0, mode_name(nav.mode));
    at = put(line, at, "   tab: another way of looking   "
                       "arrows: move through the graph   "
                       "1 2 3: lenses   typing changes the object");
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
    set_lenses_for(t);
    nav.redraw = true;
}

static void go_back(void)
{
    if (nav.depth <= 1) return;
    nav.depth--;
    nav.selected = nav.via[nav.depth];
    set_lenses_for(focus());
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

    if (codepoint == '\b') {
        if (len > 0) d[len - 1] = 0;
    } else if (len + 1 < size) {
        d[len] = (u8)codepoint;
        d[len + 1] = 0;
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

        switch (k.codepoint) {
        case KEY_TAB:
            nav.mode = (shell_mode)((nav.mode + 1) % SHELL_MODE_COUNT);
            nav.redraw = true;
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
        case '1': toggle_lens(LENS_TEXT);      continue;
        case '2': toggle_lens(LENS_BYTES);     continue;
        case '3': toggle_lens(LENS_STRUCTURE); continue;
        default: break;
        }

        if (k.codepoint < 0x110000u) type_into_focus(k.codepoint);
    }
}

static void handle_mouse(void)
{
    mouse_event m;
    while (ps2_poll_mouse(&m)) {
        nav.mouse_x += m.dx;
        nav.mouse_y += m.dy;
        if (nav.mouse_x < 0) nav.mouse_x = 0;
        if (nav.mouse_y < 0) nav.mouse_y = 0;
        if (nav.mouse_x > (i32)fb_width() - 2)  nav.mouse_x = (i32)fb_width() - 2;
        if (nav.mouse_y > (i32)fb_height() - 2) nav.mouse_y = (i32)fb_height() - 2;

        bool was = (nav.buttons & 1) != 0;
        bool is = (m.buttons & 1) != 0;
        nav.buttons = m.buttons;

        /* One gesture, one meaning: pointing at something and pressing
         * goes there. Nothing here opens a menu. */
        if (is && !was && nav.mode == SHELL_GRAPH) {
            for (u32 i = 0; i < gcount; i++) {
                if (nav.mouse_x < gnodes[i].x || nav.mouse_x > gnodes[i].x + 250)
                    continue;
                if (nav.mouse_y < gnodes[i].y || nav.mouse_y > gnodes[i].y + 46)
                    continue;

                /* Walk to it from where we are, one reference at a
                 * time, so the path stays a real chain of references
                 * rather than a jump that skipped the permissions. */
                for (u64 s = 0; s < obj_slots(focus()); s++) {
                    if (obj_get_slot(focus(), s) == gnodes[i].o) {
                        follow(s);
                        break;
                    }
                }
                break;
            }
        }
        nav.redraw = true;
    }
}

/* ------------------------------------------------------------------ */

void shell_init(domain *d, object *root, u32 rights)
{
    nav.dom = d;
    nav.node[0] = root;
    nav.rights[0] = rights;
    nav.via[0] = 0;
    nav.depth = 1;
    nav.selected = 0;
    nav.mode = SHELL_FOCUS;
    nav.mouse_x = (i32)fb_width() / 2;
    nav.mouse_y = (i32)fb_height() / 2;
    set_lenses_for(root);
    nav.redraw = true;
}

void shell_run(void *arg)
{
    (void)arg;
    for (;;) {
        handle_mouse();
        handle_keys();

        if (nav.redraw) {
            nav.redraw = false;
            draw_all();
            fb_present();
        }
        sched_yield();
    }
}
