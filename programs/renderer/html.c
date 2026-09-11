/*
 * html.c -- text browser lens: one pass over markup, with the part of the stylesheet that changes what a page says.
 * - headings, paragraphs, lists, quotes, pre, tables, links, forms (text fields, submit), pictures
 * - utf-8 decoded to code points; named and numeric entities
 * - an element stack carries the computed style: display none and visibility hidden drop the subtree, font-weight
 *   sets words strong, color colours them, text-align sets the line, list-style none drops the bullet, display
 *   block or inline decides the break; the style attribute wins over the sheet; hidden and align attributes read
 * - nav, header, footer and aside fold to one line with a count, opened by the person; header and footer only
 *   outside article, section and main
 * - a line is gathered and painted on the break, so it can be centred or set right
 * - scripts, styles, svg, templates, comments and unknown tags are dropped; noscript is read (there is no script)
 * - no JavaScript; width from the window; a picture is laid in when its lender has it decoded, a frame with the
 *   alternative text until then
 * - draws nothing itself: rectangles, glyph runs, pictures and fields go into a display list in the flow's own
 *   coordinates (eb/render.h); the kernel paints it. Runs in ring 3 (programs/renderer) and in the fuzzer.
 * - one page at a time: the state is static, too big for a thread's stack
 */
#include "html.h"
#include <eb/string.h>

#define WORD_MAX    96
#define IND_STEP    2               /* columns per level of indent */
#define FIELD_COLS  22              /* width of a text field, in glyphs */
#define IMAGE_MAX_H 480             /* pixels a picture may stand tall */
#define STACK_MAX   96              /* open elements remembered */
#define LINE_MAX    320             /* cells in one line */
#define LFIELDS_MAX 12              /* fields in one line */
#define ATTRS_MAX   12

typedef struct {
    u32 cp;                         /* 0 for blank */
    u32 color;
    i16 link;                       /* -1 for none */
    u8  found;
    u8  scale;                      /* 1, 2 or 3: the glyph's whole-factor size */
    u8  code;                       /* inside <pre> or <code>: gets a faint ground */
} cell;

typedef struct {
    i32 col;
    u32 w;
    u32 idx;
    u8  kind;
} lfield;

/* One open element: its tag and what the styles made of it. */
typedef struct {
    u32 tag;
    u8  display;                    /* CSS_DISPLAY_* */
    u8  bold, align, list_none, vis_hidden;
    u8  has_color;
    u8  fold;                       /* this element started a fold */
    u8  scale;                      /* 1, 2 or 3: the text size here */
    u32 color;
} frame;

typedef struct {
    const html_view *v;
    html_sink *sink;
    i32  cols, rows;
    i32  col;                       /* current column, from the window's left */
    u32  row;                       /* virtual row, from the top of the flow */
    i32  indent;                    /* left margin, in columns */

    u32  blanks;                    /* breaks owed but not yet paid */
    bool line_dirty;

    /* Mood. */
    u32  bold;
    i32  link;                      /* -1, or index into urls */
    u32  pre;                       /* inside preformatted text */
    u32  code;                      /* inside <code>, for its faint ground */
    u32  quote;                     /* blockquote nesting, for the left bar */

    /* Headings, as a level stack so a close knows what it closes. */
    u8   head_lvl[8];
    u32  head_depth;

    /* Lists: a stack of counters; zero means unordered. */
    i32  list_num[8];
    u32  list_depth;

    /* The form being filled, if any. */
    i32  cur_form;

    u32  word[WORD_MAX];            /* code points */
    u32  wlen;
    u8   wsc;                       /* the word's size, fixed when its first letter came */

    /* The line being gathered. */
    cell   line[LINE_MAX];
    i32    line_end;                /* one past the last cell written */
    lfield lf[LFIELDS_MAX];
    u32    nlf;
    u8     line_align;
    bool   align_set;
    u32    line_rows;               /* how tall the last painted line was, in glyph rows */

    /* The open elements. */
    css_elem chain[STACK_MAX];
    frame    meta[STACK_MAX];
    u32      depth;
    u32      overflow;              /* opens past STACK_MAX, so their closes pop nothing */
    i32      hide_at;               /* the element that hides its subtree, or -1 */
    i32      fold_at;               /* the element folded, or -1 */
    bool     fold_open;
    u8       fold_kind;
    u32      fold_n;                /* folds numbered so far */
    u32      fold_links, fold_words, fold_fields;
    bool     in_word;
    u32      hidden_count;
} flow;

static flow F;

/* ------------------------------------------------------------------ */
/* Names                                                               */
/* ------------------------------------------------------------------ */

static bool tag_is(const char *t, const char *want)
{
    u32 i = 0;
    while (want[i]) { if (t[i] != want[i]) return false; i++; }
    return t[i] == 0;
}

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static u32 slen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }

static bool is_heading(const char *name)
{
    return name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0;
}

static bool is_void(const char *n)
{
    return tag_is(n, "br") || tag_is(n, "hr") || tag_is(n, "img") || tag_is(n, "input") ||
           tag_is(n, "meta") || tag_is(n, "link") || tag_is(n, "area") || tag_is(n, "base") ||
           tag_is(n, "col") || tag_is(n, "embed") || tag_is(n, "source") || tag_is(n, "track") ||
           tag_is(n, "wbr") || tag_is(n, "param");
}

/* The break an element owes by its own nature: two rows, one, or none. */
static u32 ua_gap(const char *n)
{
    if (tag_is(n, "p") || tag_is(n, "section") || tag_is(n, "article") || tag_is(n, "header") ||
        tag_is(n, "footer") || tag_is(n, "main") || tag_is(n, "nav") || tag_is(n, "aside") ||
        tag_is(n, "figure") || tag_is(n, "dl") || tag_is(n, "address") || tag_is(n, "table") ||
        tag_is(n, "pre") || is_heading(n))
        return 2;
    if (tag_is(n, "div") || tag_is(n, "tr") || tag_is(n, "dt") || tag_is(n, "dd") ||
        tag_is(n, "ul") || tag_is(n, "ol") || tag_is(n, "li") || tag_is(n, "form") ||
        tag_is(n, "fieldset") || tag_is(n, "details") || tag_is(n, "summary") ||
        tag_is(n, "figcaption") || tag_is(n, "caption") || tag_is(n, "center") ||
        tag_is(n, "blockquote") || tag_is(n, "menu") || tag_is(n, "hr") || tag_is(n, "body"))
        return 1;
    return 0;
}

static bool is_block(const char *n)
{
    return ua_gap(n) > 0 || tag_is(n, "td") || tag_is(n, "th") || tag_is(n, "thead") ||
           tag_is(n, "tbody") || tag_is(n, "tfoot") || tag_is(n, "html") || tag_is(n, "head");
}

/* What folds: 1 navigation, 2 header, 3 footer, 4 aside; 0 for nothing. */
static u8 fold_kind_of(const char *n, const char *role)
{
    if (role) {
        if (tag_is(role, "navigation") || tag_is(role, "menu") || tag_is(role, "menubar")) return 1;
        if (tag_is(role, "banner")) return 2;
        if (tag_is(role, "contentinfo")) return 3;
        if (tag_is(role, "complementary")) return 4;
    }
    if (tag_is(n, "nav")) return 1;
    if (tag_is(n, "header")) return 2;
    if (tag_is(n, "footer")) return 3;
    if (tag_is(n, "aside")) return 4;
    return 0;
}

static const char *fold_name(u8 kind)
{
    switch (kind) {
    case 1: return "navigation";
    case 2: return "header";
    case 3: return "footer";
    default: return "aside";
    }
}

/* ------------------------------------------------------------------ */
/* Placement                                                           */
/* ------------------------------------------------------------------ */

static bool visible(const flow *f, u32 row)
{
    return row >= f->v->scroll && row < f->v->scroll + (u32)f->rows;
}

static i32 pixel_y(const flow *f, u32 row)
{
    return f->v->y + (i32)(row - f->v->scroll) * GLYPH_H;
}

static frame *top(flow *f) { return f->depth ? &f->meta[f->depth - 1] : NULL; }

static bool hidden(const flow *f) { return f->hide_at >= 0; }
static bool folded(const flow *f) { return f->fold_at >= 0 && !f->fold_open; }
static bool quiet(const flow *f)  { return hidden(f) || folded(f); }

static u8 cur_align(flow *f)
{
    frame *t = top(f);
    return t ? t->align : CSS_ALIGN_LEFT;
}

static void spot_add(html_spot *spots, u32 *count, i32 x, i32 y, i32 w, i32 h, u32 ref)
{
    if (!spots || !count || *count >= HTML_SPOTS_MAX) return;
    spots[(*count)++] = (html_spot){ x, y, w, h, ref };
}

/* ------------------------------------------------------------------ */
/* The display list                                                    */
/* ------------------------------------------------------------------ */

/* One op into the list: the head word, then n words. A list out of
 * room takes nothing more and says so. */
static void emit(flow *f, u32 kind, const u32 *w, u32 n)
{
    html_out *o = f->sink ? f->sink->out : NULL;
    if (!o || !o->ops) return;
    if (o->len + 1 + n > o->cap) { o->full = true; return; }
    o->ops[o->len++] = OP_HEAD(kind, n);
    for (u32 i = 0; i < n; i++) o->ops[o->len++] = w[i];
}

static void emit_rect(flow *f, i32 x, i32 y, i32 w, i32 h, color c)
{
    if (w <= 0 || h <= 0) return;
    u32 v[5] = { (u32)x, (u32)y, (u32)w, (u32)h, c };
    emit(f, OP_RECT, v, 5);
}

/* A run of glyphs, all of one colour and size, GLYPH_W * scale apart. */
static void emit_text(flow *f, i32 x, i32 y, color c, u32 scale, const u32 *cps, u32 n)
{
    static u32 v[3 + OP_TEXT_MAX];
    while (n) {
        u32 take = n > OP_TEXT_MAX ? OP_TEXT_MAX : n;
        v[0] = (u32)x; v[1] = (u32)y; v[2] = (c & 0xFFFFFFu) | (scale << 24);
        for (u32 i = 0; i < take; i++) v[3 + i] = cps[i];
        emit(f, OP_TEXT, v, 3 + take);
        x += (i32)(take * scale * GLYPH_W);
        cps += take;
        n -= take;
    }
}

/* Plain ascii, one glyph per letter. */
static void emit_ascii(flow *f, i32 x, i32 y, color c, const char *s, u32 n)
{
    static u32 cps[OP_TEXT_MAX];
    while (n) {
        u32 take = n > OP_TEXT_MAX ? OP_TEXT_MAX : n;
        for (u32 i = 0; i < take; i++) cps[i] = (u8)s[i];
        emit_text(f, x, y, c, 1, cps, take);
        x += (i32)(take * GLYPH_W);
        s += take;
        n -= take;
    }
}

static void emit_image(flow *f, i32 x, i32 y, i32 dw, i32 dh, u32 index)
{
    u32 v[5] = { (u32)x, (u32)y, (u32)dw, (u32)dh, index };
    emit(f, OP_IMAGE, v, 5);
}

static void emit_field(flow *f, i32 x, i32 y, i32 w, i32 h, u32 idx, u32 kind)
{
    u32 v[6] = { (u32)x, (u32)y, (u32)w, (u32)h, idx, kind };
    emit(f, OP_FIELD, v, 6);
}

/* Paints the gathered line where it falls, shifted for its alignment,
 * scaled to its tallest glyph, and notes the links and fields in it.
 * Leaves the line's height in rows in f->line_rows. */
static void line_paint(flow *f)
{
    html_sink *s = f->sink;
    u32 tall = 1;
    for (i32 c = 0; c < f->line_end; c++)
        if (f->line[c].cp && f->line[c].scale > tall) tall = f->line[c].scale;
    f->line_rows = tall;

    if (f->line_dirty) {
        i32 shift = 0;
        if (f->line_align == CSS_ALIGN_CENTER) shift = (f->cols - f->line_end) / 2;
        else if (f->line_align == CSS_ALIGN_RIGHT) shift = f->cols - f->line_end;
        if (shift < 0) shift = 0;
        /* the whole line, however tall, must fall inside the window */
        bool vis = f->row >= f->v->scroll && f->row + tall <= f->v->scroll + (u32)f->rows;
        i32 top = vis ? pixel_y(f, f->row) : 0;

        if (vis) {
            /* a faint ground behind runs of preformatted or <code> cells */
            for (i32 c = 0; c < f->line_end; ) {
                if (!(f->line[c].cp && f->line[c].code)) { c++; continue; }
                i32 start = c;
                while (c < f->line_end && f->line[c].cp && f->line[c].code) c++;
                emit_rect(f, f->v->x + (start + shift) * GLYPH_W - 1, top,
                          (c - start) * GLYPH_W + 2, (i32)tall * GLYPH_H, f->v->col.faint);
            }
            /* the left accent bar(s) of a blockquote, one per nesting level */
            for (u32 q = 1; q <= f->quote; q++) {
                i32 gcol = f->indent - (i32)q * IND_STEP;
                if (gcol < 0) gcol = 0;
                emit_rect(f, f->v->x + gcol * GLYPH_W + 2, top, 2, (i32)tall * GLYPH_H, f->v->col.accent);
            }
            /* the glyphs, as runs of one colour and size; a found word
             * has the accent behind it */
            static u32 glyphs[LINE_MAX];
            for (i32 c = 0; c < f->line_end; ) {
                cell *k = &f->line[c];
                if (!k->cp) { c++; continue; }
                u32 sc = k->scale ? k->scale : 1;
                color col = k->color;
                u8 found = k->found;
                i32 start = c;
                u32 n = 0;
                /* a cell of scale sc holds sc columns; the next glyph is sc cells on */
                while (c < f->line_end && f->line[c].cp &&
                       (f->line[c].scale ? f->line[c].scale : 1) == sc &&
                       f->line[c].color == col && f->line[c].found == found && n < LINE_MAX) {
                    glyphs[n++] = f->line[c].cp;
                    c += (i32)sc;
                }
                i32 x = f->v->x + (start + shift) * GLYPH_W;
                i32 y = top + (i32)(tall - sc) * GLYPH_H;   /* sit on the line's floor */
                if (found) emit_rect(f, x, y, (i32)(n * sc) * GLYPH_W, (i32)sc * GLYPH_H, f->v->col.accent);
                emit_text(f, x, y, col, sc, glyphs, n);
            }
            if (s && s->link_spots && s->link_spot_count) {
                i32 run = -1, start = 0, end = 0;
                for (i32 c = 0; c <= f->line_end; c++) {
                    const cell *k = c < f->line_end ? &f->line[c] : NULL;
                    if (k && !k->cp) continue;
                    i32 l = k ? k->link : -1;
                    if (run >= 0 && l == run && c - end <= 1) { end = c + 1; continue; }
                    if (run >= 0)
                        spot_add(s->link_spots, s->link_spot_count, f->v->x + (start + shift) * GLYPH_W, top,
                                 (end - start) * GLYPH_W, (i32)tall * GLYPH_H, (u32)run);
                    if (l >= 0) { run = l; start = c; end = c + 1; } else run = -1;
                }
            }
        }

        /* the fields: the kernel draws the box and what stands in it,
         * so typing into one needs no new render */
        for (u32 i = 0; i < f->nlf; i++) {
            const lfield *lf = &f->lf[i];
            i32 x = f->v->x + (lf->col + shift) * GLYPH_W;
            i32 w = (i32)lf->w * GLYPH_W;
            i32 y = top + (i32)(tall - 1) * GLYPH_H;
            if (!vis) continue;
            emit_field(f, x, y, w, GLYPH_H, lf->idx, lf->kind);
            if (s) spot_add(s->field_spots, s->field_spot_count, x - 2, y - 2, w + 2, GLYPH_H + 4, lf->idx);
        }
    }
    for (i32 c = 0; c < f->line_end; c++) { f->line[c].cp = 0; f->line[c].code = 0; }
    f->line_end = 0;
    f->nlf = 0;
    f->align_set = false;
}

static void line_break(flow *f)
{
    line_paint(f);
    f->col = f->indent;
    f->row += f->line_rows;
    f->line_dirty = false;
}

/* Pays the owed break before the next word. Owing rather than emitting
 * collapses a run of closing blocks into one gap and keeps a page from
 * opening with its own margin. */
static void settle_blanks(flow *f)
{
    if (f->blanks == 0) return;
    if (f->line_dirty) line_break(f);
    if (f->blanks > 1 && f->row > 0) f->row++;
    f->blanks = 0;
    f->col = f->indent;
}

static void line_take_align(flow *f)
{
    if (f->align_set) return;
    f->line_align = cur_align(f);
    f->align_set = true;
}

/* ------------------------------------------------------------------ */
/* Words                                                               */
/* ------------------------------------------------------------------ */

static u32 lower_cp(u32 c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* Whether the word holds the phrase being looked for, letters compared
 * without case. */
static bool word_has(const flow *f, const char *find)
{
    u32 n = 0;
    while (find[n]) n++;
    if (n == 0 || n > f->wlen) return false;
    for (u32 at = 0; at + n <= f->wlen; at++) {
        u32 k = 0;
        while (k < n && lower_cp(f->word[at + k]) == lower_cp((u8)find[k])) k++;
        if (k == n) return true;
    }
    return false;
}

/* A colour the sheet asked for, unless it would vanish on the page. */
static bool usable_color(u32 c)
{
    u32 r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    return (r * 299 + g * 587 + b * 114) / 1000 <= 190;
}

static color word_color(flow *f)
{
    frame *t = top(f);
    if (f->link >= 0) return f->v->col.accent;
    if (f->head_depth || f->bold || (t && t->bold)) return f->v->col.text;
    if (t && t->has_color && usable_color(t->color)) return (color)t->color;
    return f->v->col.dim;
}

static u32 word_scale(flow *f)
{
    frame *t = top(f);
    u32 sc = t ? t->scale : 1;
    return sc >= 1 && sc <= 3 ? sc : 1;
}

static void word_flush(flow *f)
{
    if (f->wlen == 0) return;
    settle_blanks(f);

    u32 sc = f->wsc >= 1 && f->wsc <= 3 ? f->wsc : 1;
    /* how many columns a glyph of this size takes; the word is that wide. */
    if (f->col + (i32)(f->wlen * sc) > f->cols && f->col > f->indent) line_break(f);

    color c = word_color(f);
    u32 shown = f->wlen;
    i32 room = (f->cols - f->col) / (i32)sc;
    if (room < 1) room = 1;
    if ((i32)shown > room) shown = (u32)room;
    while (shown && f->col + (i32)(shown * sc) > LINE_MAX) shown--;
    if (shown == 0) { f->wlen = 0; return; }

    /* A word that is being looked for is drawn marked, and its row
     * is answered when it is the first at or past the row asked from. */
    bool found = f->sink && f->sink->find && word_has(f, f->sink->find);
    if (found && f->sink->find_row && f->row >= f->sink->find_from &&
        *f->sink->find_row == (u32)-1)
        *f->sink->find_row = f->row;

    line_take_align(f);
    for (u32 i = 0; i < shown; i++) {
        i32 at = f->col + (i32)(i * sc);
        if (at >= LINE_MAX) break;
        cell *k = &f->line[at];
        k->cp = f->word[i];
        k->color = found ? f->v->col.text : c;
        k->link = (i16)(f->link >= 0 && f->link < 32000 ? f->link : -1);
        k->found = found;
        k->scale = (u8)sc;
        k->code = (f->pre || f->code) ? 1 : 0;
    }
    f->col += (i32)(shown * sc);
    if (f->col > f->line_end) f->line_end = f->col;
    f->line_dirty = true;
    f->wlen = 0;

    if (f->col + (i32)sc <= f->cols) f->col += (i32)sc;   /* the space after */
    else line_break(f);
}

static void word_add(flow *f, u32 c)
{
    if (f->wlen == 0) f->wsc = (u8)word_scale(f);      /* the size the word keeps to its end */
    if (f->wlen < WORD_MAX - 1) f->word[f->wlen++] = c;
    if (f->col + (i32)(f->wlen * f->wsc) >= f->cols) word_flush(f);
}

/* One code point from the utf-8 at s, and how many bytes it took. A
 * malformed sequence yields the byte itself, so nothing is skipped. */
static u32 utf8_take(const u8 *s, u64 left, u32 *cp)
{
    u8 c = s[0];
    u32 more = 0, v = c;
    if      ((c & 0xE0) == 0xC0) { v = c & 0x1Fu; more = 1; }
    else if ((c & 0xF0) == 0xE0) { v = c & 0x0Fu; more = 2; }
    else if ((c & 0xF8) == 0xF0) { v = c & 0x07u; more = 3; }
    if (more == 0 || more >= left) { *cp = c; return 1; }
    for (u32 i = 1; i <= more; i++) {
        if ((s[i] & 0xC0) != 0x80) { *cp = c; return 1; }
        v = (v << 6) | (s[i] & 0x3Fu);
    }
    if (v > 0x10FFFF) { *cp = c; return 1; }        /* past the last code point: not utf-8 */
    *cp = v;
    return more + 1;
}

static void want_break(flow *f, u32 gap)
{
    word_flush(f);
    if (gap > f->blanks) f->blanks = gap;
}

/* A full-width rule on its own row: <hr>, and under a heading. */
static void rule(flow *f, color c)
{
    word_flush(f);
    settle_blanks(f);
    if (f->line_dirty) line_break(f);
    if (visible(f, f->row))
        emit_rect(f, f->v->x + f->indent * GLYPH_W, pixel_y(f, f->row) + GLYPH_H / 2,
                  (f->cols - f->indent) * GLYPH_W, 1, c);
    f->row++;
    f->col = f->indent;
}

/* One step in, never past the middle of the window. */
static void indent_more(flow *f)
{
    if (f->indent + IND_STEP <= f->cols / 2) f->indent += IND_STEP;
    f->col = f->indent;
}

/* Table cells advance to the next column stop, so the cells of one row
 * line up under the cells of the rows above and below. A cell wider than
 * a stop pushes the next cell to the following stop. The first cell of a
 * row (nothing dirty yet) starts at the margin. */
#define CELL_STOP  14                   /* columns per table column */
static void cell_gap(flow *f)
{
    word_flush(f);
    if (!f->line_dirty) return;
    i32 rel  = f->col - f->indent;
    i32 stop = ((rel / CELL_STOP) + 1) * CELL_STOP;
    i32 want = f->indent + stop;
    if (want + 1 < f->cols) f->col = want;
    else if (f->col + 2 < f->cols) f->col += 2;   /* out of room: a plain gap */
}

/* A line of plain ascii on a row of its own, in one colour, and the
 * rectangle it took. */
static void plain_line(flow *f, const char *text, color c, i32 *x, i32 *y, i32 *w)
{
    word_flush(f);
    settle_blanks(f);
    if (f->line_dirty) line_break(f);
    u32 n = slen(text);
    if ((i32)n > f->cols - f->indent) n = (u32)(f->cols - f->indent);
    *x = f->v->x + f->indent * GLYPH_W;
    *w = (i32)n * GLYPH_W;
    *y = -1;
    if (visible(f, f->row)) {
        *y = pixel_y(f, f->row);
        emit_ascii(f, *x, *y, c, text, n);
    }
    f->row++;
    f->col = f->indent;
    f->line_dirty = false;
}

/* ------------------------------------------------------------------ */
/* Form fields                                                         */
/* ------------------------------------------------------------------ */

static void field_emit(flow *f, u8 kind, const char *name,
                       const char *value)
{
    html_sink *s = f->sink;
    if (!s || !s->fields || !s->field_count) {
        if (kind != FIELD_HIDDEN) { word_add(f, '['); word_add(f, ']'); }
        return;
    }
    if (*s->field_count >= HTML_FIELDS_MAX) return;

    u32 idx = (*s->field_count)++;
    html_field *fld = &s->fields[idx];
    u32 n = 0;
    while (name[n] && n < HTML_NAME_MAX - 1) { fld->name[n] = name[n]; n++; }
    fld->name[n] = 0;
    fld->kind = kind;
    fld->form = (u32)(f->cur_form >= 0 ? f->cur_form : 0);

    if (s->field_init) {
        u32 vi = 0;
        while (value[vi] && vi < HTML_VALUE_MAX - 1)
            { s->field_init[idx][vi] = value[vi]; vi++; }
        s->field_init[idx][vi] = 0;
    }

    if (kind == FIELD_HIDDEN) return;

    /* Its width in the flow. A submit is as wide as its label; a text
     * field a fixed box. */
    u32 w = FIELD_COLS;
    if (kind == FIELD_SUBMIT) {
        u32 ll = slen(value);
        w = (ll ? ll : 2) + 2;               /* room for the brackets */
    }
    if ((i32)w > f->cols - f->indent) w = (u32)(f->cols - f->indent);

    word_flush(f);
    settle_blanks(f);
    if (f->col + (i32)w > f->cols && f->col > f->indent) line_break(f);
    if (f->nlf >= LFIELDS_MAX) line_break(f);
    if (f->nlf >= LFIELDS_MAX) return;

    line_take_align(f);
    f->lf[f->nlf++] = (lfield){ f->col, w, idx, kind };
    f->col += (i32)w;
    if (f->col > f->line_end) f->line_end = f->col;
    f->line_dirty = true;
    if (f->col < f->cols) f->col++;
    else line_break(f);
}

/* ------------------------------------------------------------------ */
/* Entities                                                            */
/* ------------------------------------------------------------------ */

/* The named entities a page is likely to use; the rest are numbers. */
static const struct { const char *name; u32 cp; } ENTITIES[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
    { "nbsp", 0xA0 }, { "mdash", 0x2014 }, { "ndash", 0x2013 }, { "hellip", 0x2026 },
    { "laquo", 0xAB }, { "raquo", 0xBB }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 },
    { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "bull", 0x2022 }, { "middot", 0xB7 },
    { "copy", 0xA9 }, { "reg", 0xAE }, { "trade", 0x2122 }, { "deg", 0xB0 },
    { "euro", 0x20AC }, { "pound", 0xA3 }, { "yen", 0xA5 }, { "cent", 0xA2 },
    { "sect", 0xA7 }, { "para", 0xB6 }, { "times", 0xD7 }, { "divide", 0xF7 },
    { "plusmn", 0xB1 }, { "frac12", 0xBD }, { "frac14", 0xBC }, { "sup2", 0xB2 },
    { "larr", 0x2190 }, { "rarr", 0x2192 }, { "uarr", 0x2191 }, { "darr", 0x2193 },
    { "auml", 0xE4 }, { "ouml", 0xF6 }, { "uuml", 0xFC }, { "Auml", 0xC4 },
    { "Ouml", 0xD6 }, { "Uuml", 0xDC }, { "szlig", 0xDF }, { "eacute", 0xE9 },
    { "egrave", 0xE8 }, { "ecirc", 0xEA }, { "agrave", 0xE0 }, { "aacute", 0xE1 },
    { "acirc", 0xE2 }, { "ccedil", 0xE7 }, { "ntilde", 0xF1 }, { "oacute", 0xF3 },
    { "uacute", 0xFA }, { "iacute", 0xED }, { "Eacute", 0xC9 }, { "aring", 0xE5 },
    { "oslash", 0xF8 }, { "aelig", 0xE6 }, { "iexcl", 0xA1 }, { "iquest", 0xBF },
};

static u32 entity(flow *f, const u8 *s, u64 left)
{
    char name[12];
    u32 n = 0;
    while (n + 1 < left && n < sizeof(name) - 1 && s[n + 1] != ';' &&
           s[n + 1] != '&' && s[n + 1] != '<' && s[n + 1] != ' ')
        { name[n] = (char)s[n + 1]; n++; }
    if (n + 1 >= left || s[n + 1] != ';') { word_add(f, '&'); return 1; }
    name[n] = 0;

    u32 out = 0;
    if (n >= 2 && name[0] == '#') {
        u32 v = 0;
        if (name[1] == 'x' || name[1] == 'X') {
            for (u32 i = 2; i < n; i++) {
                char c = to_lower(name[i]);
                if (c >= '0' && c <= '9') v = v * 16 + (u32)(c - '0');
                else if (c >= 'a' && c <= 'f') v = v * 16 + (u32)(c - 'a' + 10);
                else { v = 0; break; }
                if (v > 0x10FFFF) { v = 0; break; }
            }
        } else {
            for (u32 i = 1; i < n; i++) {
                if (name[i] >= '0' && name[i] <= '9') v = v * 10 + (u32)(name[i] - '0');
                else { v = 0; break; }
                if (v > 0x10FFFF) { v = 0; break; }
            }
        }
        out = (v >= 0x20 && v != 0x7F) ? v : 0;
    } else {
        for (u32 i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
            if (tag_is(name, ENTITIES[i].name)) { out = ENTITIES[i].cp; break; }
    }

    if (out == 0xA0 || out == ' ') { if (f->pre) word_add(f, ' '); else word_flush(f); }
    else if (out) word_add(f, out);
    return n + 2;
}

/* ------------------------------------------------------------------ */
/* Tags                                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[12];
    bool closing;
    char key[ATTRS_MAX][16];
    char val[ATTRS_MAX][HTML_URL_MAX];
    u32  nattr;
    u64  end;
} parsed_tag;

static parsed_tag T;

static const char *attr(const parsed_tag *t, const char *key)
{
    for (u32 i = 0; i < t->nattr; i++)
        if (tag_is(t->key[i], key)) return t->val[i];
    return 0;
}

static void parse_tag(const u8 *s, u64 left, parsed_tag *t)
{
    t->nattr = 0;
    u64 p = 1;
    t->closing = (p < left && s[p] == '/');
    if (t->closing) p++;

    u32 n = 0;
    while (p < left && ((s[p] >= 'a' && s[p] <= 'z') ||
                        (s[p] >= 'A' && s[p] <= 'Z') ||
                        (s[p] >= '0' && s[p] <= '9'))) {
        if (n < sizeof(t->name) - 1) t->name[n++] = to_lower((char)s[p]);
        p++;
    }
    t->name[n] = 0;

    while (p < left && s[p] != '>') {
        while (p < left && (s[p] == ' ' || s[p] == '\t' ||
                            s[p] == '\n' || s[p] == '\r' || s[p] == '/'))
            p++;
        if (p >= left || s[p] == '>') break;

        char key[16];
        u32 kn = 0;
        while (p < left && s[p] != '=' && s[p] != '>' && s[p] != ' ' &&
               s[p] != '\t' && s[p] != '\n' && s[p] != '\r') {
            if (kn < sizeof(key) - 1) key[kn++] = to_lower((char)s[p]);
            p++;
        }
        key[kn] = 0;

        /* the value goes straight into its slot; a slot past the last
         * is a scratch one, written and forgotten */
        u32 slot = t->nattr < ATTRS_MAX ? t->nattr : ATTRS_MAX - 1;
        char *val = t->val[slot];
        u32 vn = 0;
        if (p < left && s[p] == '=') {
            p++;
            char q = 0;
            if (p < left && (s[p] == '"' || s[p] == '\'')) q = (char)s[p++];
            while (p < left && s[p] != '>' &&
                   (q ? (char)s[p] != q : (s[p] != ' ' && s[p] != '\t' &&
                                           s[p] != '\n' && s[p] != '\r'))) {
                /* the entities a link carries: &amp; above all */
                if (s[p] == '&') {
                    static const struct { const char *name; char c; } E[] = {
                        { "&amp;", '&' }, { "&quot;", '"' }, { "&apos;", '\'' },
                        { "&lt;", '<' }, { "&gt;", '>' }, { "&#39;", '\'' }, { "&#38;", '&' } };
                    u32 hit = 0;
                    for (u32 e = 0; e < sizeof(E) / sizeof(E[0]) && !hit; e++) {
                        u32 k = 0;
                        while (E[e].name[k] && p + k < left && s[p + k] == (u8)E[e].name[k]) k++;
                        if (E[e].name[k] == 0) { if (vn < HTML_URL_MAX - 1) val[vn++] = E[e].c; p += k; hit = 1; }
                    }
                    if (hit) continue;
                }
                if (vn < HTML_URL_MAX - 1) val[vn++] = (char)s[p];
                p++;
            }
            if (q && p < left && (char)s[p] == q) p++;
        }
        val[vn] = 0;

        if (kn && t->nattr < ATTRS_MAX) {
            u32 i = 0;
            while (key[i]) { t->key[t->nattr][i] = key[i]; i++; }
            t->key[t->nattr][i] = 0;
            t->nattr++;
        }
    }
    if (p < left) p++;                        /* the '>' */
    t->end = p;
}

/* Swallow to a named closing tag, returning the source offset past it;
 * where the closing tag begins goes to *at when asked. */
static u64 swallow_to(const u8 *s, u64 left, u64 from, const char *close, u64 *at)
{
    u64 p = from;
    while (p < left) {
        if (s[p] == '<' && p + 1 < left && s[p+1] == '/') {
            u64 q = p + 2;
            u32 i = 0;
            while (close[i] && q < left &&
                   to_lower((char)s[q]) == close[i]) { i++; q++; }
            if (close[i] == 0 && (q >= left || s[q] == '>' || s[q] == ' ' || s[q] == '\n' || s[q] == '\t' || s[q] == '\r')) {
                if (at) *at = p;
                while (q < left && s[q] != '>') q++;
                return q < left ? q + 1 : left;
            }
        }
        p++;
    }
    if (at) *at = left;
    return left;
}

/* ------------------------------------------------------------------ */
/* The element stack and the styles                                    */
/* ------------------------------------------------------------------ */

static void fold_end(flow *f);

static void pop_top(flow *f)
{
    if (!f->depth) return;
    u32 i = --f->depth;
    if ((i32)i == f->hide_at) f->hide_at = -1;
    if ((i32)i == f->fold_at) fold_end(f);
}

/* Pops through the nearest element with one of the tags wanted, unless
 * one of the stop tags stands above it. */
static void pop_within(flow *f, const u32 *want, u32 nwant, const u32 *stop, u32 nstop)
{
    i32 i = (i32)f->depth - 1;
    while (i >= 0) {
        u32 t = f->chain[i].tag;
        for (u32 k = 0; k < nstop; k++) if (t == stop[k]) return;
        for (u32 k = 0; k < nwant; k++)
            if (t == want[k]) { while ((i32)f->depth > i) pop_top(f); return; }
        i--;
    }
}

static u32 H_HTML, H_P, H_LI, H_UL, H_OL, H_MENU, H_DT, H_DD, H_DL, H_TD, H_TH, H_TR, H_TABLE,
           H_THEAD, H_TBODY, H_TFOOT, H_ARTICLE, H_SECTION, H_MAIN;

static void names_init(void)
{
    if (H_HTML) return;
    H_HTML = css_hash("html"); H_P = css_hash("p"); H_LI = css_hash("li"); H_UL = css_hash("ul");
    H_OL = css_hash("ol"); H_MENU = css_hash("menu"); H_DT = css_hash("dt"); H_DD = css_hash("dd");
    H_DL = css_hash("dl"); H_TD = css_hash("td"); H_TH = css_hash("th"); H_TR = css_hash("tr");
    H_TABLE = css_hash("table"); H_THEAD = css_hash("thead"); H_TBODY = css_hash("tbody");
    H_TFOOT = css_hash("tfoot"); H_ARTICLE = css_hash("article"); H_SECTION = css_hash("section");
    H_MAIN = css_hash("main");
}

/* The closes the markup left implicit before this element opens. */
static void implicit_closes(flow *f, const char *name)
{
    if (tag_is(name, "body")) {
        while (f->depth && f->chain[f->depth - 1].tag != H_HTML) pop_top(f);
        return;
    }
    if (is_block(name) && f->depth && f->chain[f->depth - 1].tag == H_P) pop_top(f);
    if (tag_is(name, "li")) {
        u32 want[1] = { H_LI }, stop[3] = { H_UL, H_OL, H_MENU };
        pop_within(f, want, 1, stop, 3);
    } else if (tag_is(name, "dt") || tag_is(name, "dd")) {
        u32 want[2] = { H_DT, H_DD }, stop[1] = { H_DL };
        pop_within(f, want, 2, stop, 1);
    } else if (tag_is(name, "td") || tag_is(name, "th")) {
        u32 want[2] = { H_TD, H_TH }, stop[2] = { H_TR, H_TABLE };
        pop_within(f, want, 2, stop, 2);
    } else if (tag_is(name, "tr")) {
        u32 want[1] = { H_TR }, stop[4] = { H_TABLE, H_THEAD, H_TBODY, H_TFOOT };
        pop_within(f, want, 1, stop, 4);
    } else if (tag_is(name, "thead") || tag_is(name, "tbody") || tag_is(name, "tfoot")) {
        u32 want[3] = { H_THEAD, H_TBODY, H_TFOOT }, stop[1] = { H_TABLE };
        pop_within(f, want, 3, stop, 1);
    }
}

/* Whether an ancestor is an article, section or main: a header or
 * footer inside one belongs to it and is not folded. */
static bool inside_sectioning(const flow *f)
{
    for (u32 i = 0; i < f->depth; i++) {
        u32 t = f->chain[i].tag;
        if (t == H_ARTICLE || t == H_SECTION || t == H_MAIN) return true;
    }
    return false;
}

static void fold_start(flow *f, u8 kind);

/* Opens an element: the implicit closes, the stack, the computed
 * style. A void element is worked out in place and not kept. Returns
 * the frame, or NULL when the stack is full. */
static frame *elem_open(flow *f, const parsed_tag *t)
{
    const char *name = t->name;
    implicit_closes(f, name);
    if (f->depth >= STACK_MAX) { if (!is_void(name)) f->overflow++; return NULL; }

    css_elem *e = &f->chain[f->depth];
    frame *fr = &f->meta[f->depth];
    frame *parent = top(f);

    e->tag = css_hash(name);
    e->id = 0; e->ncls = 0; e->nattr = 0;
    const char *id = attr(t, "id");
    if (id && id[0]) e->id = css_hash(id);
    const char *cls = attr(t, "class");
    if (cls) {
        u32 i = 0;
        while (cls[i] && e->ncls < CSS_ELEM_CLASSES) {
            while (cls[i] == ' ' || cls[i] == '\t' || cls[i] == '\n' || cls[i] == '\r') i++;
            u32 s = i;
            while (cls[i] && cls[i] != ' ' && cls[i] != '\t' && cls[i] != '\n' && cls[i] != '\r') i++;
            if (i > s) e->cls[e->ncls++] = css_hash_n((const u8 *)cls + s, i - s);
        }
    }
    for (u32 i = 0; i < t->nattr && e->nattr < CSS_ELEM_ATTRS; i++) {
        e->attr[e->nattr] = css_hash(t->key[i]);
        e->attr_val[e->nattr] = css_hash(t->val[i]);
        e->nattr++;
    }

    /* inherited from the parent */
    fr->tag = e->tag;
    fr->bold = parent ? parent->bold : 0;
    fr->align = parent ? parent->align : CSS_ALIGN_LEFT;
    fr->list_none = parent ? parent->list_none : 0;
    fr->vis_hidden = parent ? parent->vis_hidden : 0;
    fr->has_color = parent ? parent->has_color : 0;
    fr->color = parent ? parent->color : 0;
    fr->scale = parent ? parent->scale : 1;
    fr->fold = 0;

    /* the sheet, then the style attribute */
    css_decl d;
    d.set = 0; d.important = 0; d.display = 0; d.hidden = 0; d.bold = 0; d.align = 0; d.list_none = 0; d.color = 0;
    if (f->sink && f->sink->sheet) css_match(f->sink->sheet, f->chain, f->depth + 1, &d);
    const char *style = attr(t, "style");
    if (style && style[0]) {
        css_decl sd;
        css_declarations((const u8 *)style, slen(style), &sd);
        for (u32 p = 0; p < CSS_PROPS; p++) {
            u8 bit = (u8)(1u << p);
            if (!(sd.set & bit)) continue;
            if ((d.important & bit) && !(sd.important & bit)) continue;
            d.set |= bit;
            switch (bit) {
            case CSS_SET_DISPLAY:    d.display = sd.display; break;
            case CSS_SET_VISIBILITY: d.hidden = sd.hidden; break;
            case CSS_SET_WEIGHT:     d.bold = sd.bold; break;
            case CSS_SET_COLOR:      d.color = sd.color; break;
            case CSS_SET_ALIGN:      d.align = sd.align; break;
            case CSS_SET_LIST:       d.list_none = sd.list_none; break;
            default: break;
            }
        }
    }

    fr->display = (d.set & CSS_SET_DISPLAY) ? d.display : (is_block(name) ? CSS_DISPLAY_BLOCK : CSS_DISPLAY_INLINE);
    bool by_nature = tag_is(name, "template") || tag_is(name, "head");
    if (attr(t, "hidden") || by_nature) fr->display = CSS_DISPLAY_NONE;
    if (fr->display == CSS_DISPLAY_NONE && (tag_is(name, "html") || tag_is(name, "body"))) fr->display = CSS_DISPLAY_BLOCK;
    if (d.set & CSS_SET_VISIBILITY) fr->vis_hidden = d.hidden;
    if (d.set & CSS_SET_WEIGHT) fr->bold = d.bold;
    if (d.set & CSS_SET_COLOR) { fr->has_color = 1; fr->color = d.color; }
    if (d.set & CSS_SET_ALIGN) fr->align = d.align;
    else {
        const char *al = attr(t, "align");
        if (tag_is(name, "center")) fr->align = CSS_ALIGN_CENTER;
        else if (al && (al[0] == 'c' || al[0] == 'C')) fr->align = CSS_ALIGN_CENTER;
        else if (al && (al[0] == 'r' || al[0] == 'R')) fr->align = CSS_ALIGN_RIGHT;
        else if (al && (al[0] == 'l' || al[0] == 'L')) fr->align = CSS_ALIGN_LEFT;
    }
    if (d.set & CSS_SET_LIST) fr->list_none = d.list_none;
    if (d.set & CSS_SET_FONTSIZE) fr->scale = d.fontscale;
    else if (is_heading(name)) fr->scale = name[1] == '1' ? 3 : name[1] == '2' ? 2 : fr->scale;
    if (fr->scale < 1) fr->scale = 1;
    if (fr->scale > 3) fr->scale = 3;

    if (is_void(name)) return fr;

    u32 at = f->depth++;
    bool was_quiet = quiet(f);
    if ((fr->display == CSS_DISPLAY_NONE || fr->vis_hidden) && f->hide_at < 0) {
        f->hide_at = (i32)at;
        if (!was_quiet && !by_nature) f->hidden_count++;
    }
    if (f->sink && f->sink->fold && !quiet(f) && f->fold_at < 0) {
        u8 kind = fold_kind_of(name, attr(t, "role"));
        if (kind == 2 || kind == 3) { if (inside_sectioning(f)) kind = 0; }
        if (kind) { fr->fold = 1; f->fold_at = (i32)at; fold_start(f, kind); }
    }
    return fr;
}

/* Closes an element: pops through it. Returns whether it was inside
 * something quiet (hidden, or folded shut) -- then its closing owes
 * the page nothing. found says whether it was open at all; display
 * answers what the styles made of it. */
static bool elem_close(flow *f, const char *name, bool *found, u8 *display)
{
    *found = false;
    *display = is_block(name) ? CSS_DISPLAY_BLOCK : CSS_DISPLAY_INLINE;
    if (f->overflow) { f->overflow--; return true; }
    u32 h = css_hash(name);
    i32 i = (i32)f->depth - 1;
    while (i >= 0 && f->chain[i].tag != h) i--;
    if (i < 0) return false;
    *found = true;
    *display = f->meta[i].display;
    bool was_quiet = quiet(f);
    while ((i32)f->depth > i) pop_top(f);
    return was_quiet;
}

/* ------------------------------------------------------------------ */
/* Folds                                                               */
/* ------------------------------------------------------------------ */

static void fold_line(flow *f, const char *text, u32 ordinal)
{
    i32 x, y, w;
    plain_line(f, text, f->v->col.accent, &x, &y, &w);
    if (y >= 0 && f->sink)
        spot_add(f->sink->fold_spots, f->sink->fold_spot_count, x, y, w, GLYPH_H, ordinal);
    f->blanks = 1;
}

static void fold_start(flow *f, u8 kind)
{
    u32 ordinal = f->fold_n++;
    f->fold_kind = kind;
    f->fold_open = ordinal < 64 && ((f->sink->unfold >> ordinal) & 1);
    f->fold_links = f->fold_words = f->fold_fields = 0;
    f->in_word = false;
    if (f->sink->fold_count) *f->sink->fold_count = f->fold_n;
    if (f->fold_open) {
        char text[40];
        u32 n = 0;
        text[n++] = '-'; text[n++] = ' ';
        for (const char *s = fold_name(kind); *s; s++) text[n++] = *s;
        text[n] = 0;
        fold_line(f, text, ordinal);
    }
}

static u32 put_dec(char *d, u32 at, u32 v)
{
    char tmp[12]; u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) d[at++] = tmp[--n];
    return at;
}

static u32 put_str(char *d, u32 at, const char *s)
{
    while (*s) d[at++] = *s++;
    return at;
}

/* The fold's element closed: shut, it is one line with what it held. */
static void fold_end(flow *f)
{
    u32 ordinal = f->fold_n - 1;
    bool open = f->fold_open;
    f->fold_at = -1;
    f->fold_open = false;
    if (open) return;
    if (f->fold_links + f->fold_words + f->fold_fields == 0) return;
    char text[96];
    u32 n = 0;
    n = put_str(text, n, "+ ");
    n = put_str(text, n, fold_name(f->fold_kind));
    n = put_str(text, n, " (");
    bool first = true;
    if (f->fold_links) {
        n = put_dec(text, n, f->fold_links); n = put_str(text, n, f->fold_links == 1 ? " link" : " links"); first = false;
    }
    if (f->fold_words) {
        if (!first) n = put_str(text, n, ", ");
        n = put_dec(text, n, f->fold_words); n = put_str(text, n, f->fold_words == 1 ? " word" : " words"); first = false;
    }
    if (f->fold_fields) {
        if (!first) n = put_str(text, n, ", ");
        n = put_dec(text, n, f->fold_fields); n = put_str(text, n, f->fold_fields == 1 ? " field" : " fields");
    }
    n = put_str(text, n, ")");
    text[n] = 0;
    fold_line(f, text, ordinal);
}

/* ------------------------------------------------------------------ */
/* Pictures                                                            */
/* ------------------------------------------------------------------ */

/* A picture: its url goes to the lender's list; drawn when the lender
 * has it, a frame with the alternative text until then. It stands on
 * a line of its own, as wide as it is up to the window, and inside a
 * link the whole of it is the link. */
static void picture(flow *f, const parsed_tag *t)
{
    const char *src = attr(t, "src");
    const char *alt = attr(t, "alt");
    const char *wa = attr(t, "width"), *ha = attr(t, "height");
    html_sink *sk = f->sink;
    const html_image *img = NULL;
    u32 index = HTML_IMAGES_MAX;    /* the picture's place in the list of urls */

    /* a tracking pixel is nothing to look at */
    if ((wa && (wa[0] == '0' || wa[0] == '1') && wa[1] == 0) ||
        (ha && (ha[0] == '0' || ha[0] == '1') && ha[1] == 0)) return;

    if (sk && sk->images && sk->image_count && src && src[0] &&
        src[0] != 'd' /* data: urls are not fetched */) {
        u32 i = 0;
        for (; i < *sk->image_count; i++) if (tag_is(sk->images[i], src)) break;
        if (i == *sk->image_count && i < HTML_IMAGES_MAX) {
            u32 c = 0;
            while (src[c] && c < HTML_URL_MAX - 1) { sk->images[i][c] = src[c]; c++; }
            sk->images[i][c] = 0;
            (*sk->image_count)++;
        }
        if (i < HTML_IMAGES_MAX) {
            index = i;
            if (sk->image) img = sk->image(sk->image_ctx, src);
        }
    }
    if (img && (!img->w || !img->h || index >= HTML_IMAGES_MAX)) img = NULL;

    if (!img && !(alt && alt[0])) {
        if (!src || !sk || !sk->images) return;       /* nothing to say for it */
    }

    want_break(f, 1);
    settle_blanks(f);
    if (f->line_dirty) line_break(f);

    i32 avail = (f->cols - f->indent) * GLYPH_W;
    i32 dw, dh;
    if (img && img->w && img->h) {
        dw = (i32)img->w; dh = (i32)img->h;
        if (dw > avail) { dh = (i32)((i64)dh * avail / dw); dw = avail; }
        if (dh > IMAGE_MAX_H) { dw = (i32)((i64)dw * IMAGE_MAX_H / dh); dh = IMAGE_MAX_H; }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    } else {
        u32 al = 0;
        if (alt) while (alt[al]) al++;
        dw = (i32)((al ? al : 7) + 2) * GLYPH_W;
        if (dw > avail) dw = avail;
        dh = GLYPH_H + 4;
    }
    u32 rows = (u32)((dh + GLYPH_H - 1) / GLYPH_H);
    i32 x = f->v->x + f->indent * GLYPH_W;
    u8 al = cur_align(f);
    if (al == CSS_ALIGN_CENTER) x += (avail - dw) / 2;
    else if (al == CSS_ALIGN_RIGHT) x += avail - dw;

    /* Drawn where the rows fall on the screen, clipped to the window. */
    i32 win_top = f->v->y, win_bottom = f->v->y + f->rows * GLYPH_H;
    i32 y = f->v->y + ((i32)f->row - (i32)f->v->scroll) * GLYPH_H;
    if (y + dh > win_top && y < win_bottom) {
        if (img) {
            emit_image(f, x, y, dw, dh, index);
        } else {
            i32 cy = y > win_top ? y : win_top;
            i32 ch = (y + dh < win_bottom ? y + dh : win_bottom) - cy;
            if (ch > 0) emit_rect(f, x, cy, dw, ch, f->v->col.faint);
            if (y >= win_top && y + GLYPH_H + 4 <= win_bottom) {
                const char *say = (alt && alt[0]) ? alt : "picture";
                u32 n = 0;
                while (say[n] && x + GLYPH_W + (i32)(n + 1) * GLYPH_W <= x + dw) n++;
                emit_ascii(f, x + GLYPH_W, y + 2, f->v->col.dim, say, n);
            }
        }
        if (f->link >= 0 && sk) {
            i32 sy = y > win_top ? y : win_top;
            i32 sh = (y + dh < win_bottom ? y + dh : win_bottom) - sy;
            if (sh > 0) spot_add(sk->link_spots, sk->link_spot_count, x, sy, dw, sh, (u32)f->link);
        }
    }

    f->row += rows;
    f->col = f->indent;
    f->line_dirty = false;
    f->blanks = 0;
}

/* ------------------------------------------------------------------ */
/* One tag                                                             */
/* ------------------------------------------------------------------ */

/* Handles one tag. Returns the source offset just past it. */
static u64 tag(flow *f, const u8 *s, u64 left)
{
    /* Comments first: they arrive nameless. */
    if (left > 3 && s[1] == '!' && s[2] == '-' && s[3] == '-') {
        u64 p = 4;
        while (p + 2 < left &&
               !(s[p] == '-' && s[p+1] == '-' && s[p+2] == '>')) p++;
        return p + 3 < left ? p + 3 : left;
    }

    parsed_tag *t = &T;
    parse_tag(s, left, t);
    const char *name = t->name;
    bool close = t->closing;
    if (!name[0]) return t->end;

    if (tag_is(name, "script") || tag_is(name, "style") ||
        tag_is(name, "svg") || tag_is(name, "template")) {
        if (close) return t->end;
        return swallow_to(s, left, t->end, name, NULL);
    }

    /* The title is the page's name, not its prose: kept for the
     * lender, never drawn. */
    if (tag_is(name, "title")) {
        if (close) return t->end;
        u64 p = t->end;
        u32 n = 0;
        html_sink *sk = f->sink;
        while (p < left && s[p] != '<') {
            u8 c = s[p++];
            if (c == '\n' || c == '\t' || c == '\r') c = ' ';
            if (sk && sk->title && n + 1 < sk->title_max && (n || c != ' '))
                sk->title[n++] = (char)c;
        }
        if (sk && sk->title && sk->title_max) {
            while (n && sk->title[n - 1] == ' ') n--;
            sk->title[n] = 0;
        }
        return swallow_to(s, left, t->end, "title", NULL);
    }

    /* --- closing --- */
    if (close) {
        bool found; u8 display;
        bool was_quiet = elem_close(f, name, &found, &display);
        if (was_quiet) return t->end;
        u32 gap = ua_gap(name);
        if (display == CSS_DISPLAY_INLINE) gap = 0;
        else if (gap == 0 && display == CSS_DISPLAY_BLOCK && !is_void(name)) gap = 1;

        if (tag_is(name, "td") || tag_is(name, "th")) {
            if (tag_is(name, "th") && f->bold) f->bold--;
            return t->end;
        }
        if (tag_is(name, "pre")) { if (gap) want_break(f, gap); if (f->pre) f->pre--; return t->end; }
        if (tag_is(name, "blockquote")) {
            if (gap) want_break(f, gap);
            if (f->indent >= IND_STEP) f->indent -= IND_STEP;
            f->col = f->indent;
            if (f->quote) f->quote--;
            return t->end;
        }
        if (tag_is(name, "ul") || tag_is(name, "ol")) {
            if (gap) want_break(f, gap);
            if (f->list_depth) f->list_depth--;
            if (f->indent >= IND_STEP) f->indent -= IND_STEP;
            f->col = f->indent;
            return t->end;
        }
        if (is_heading(name)) {
            u32 lvl = (u32)(name[1] - '0');
            if (gap) want_break(f, gap);
            if (f->head_depth) f->head_depth--;
            if (lvl <= 2 && gap) rule(f, f->v->col.faint);
            return t->end;
        }
        if (tag_is(name, "b") || tag_is(name, "strong") ||
            tag_is(name, "em") || tag_is(name, "i") || tag_is(name, "code")) {
            word_flush(f);
            if (f->bold) f->bold--;
            if (tag_is(name, "code") && f->code) f->code--;
            if (gap) want_break(f, gap);
            return t->end;
        }
        if (tag_is(name, "a")) { word_flush(f); f->link = -1; if (gap) want_break(f, gap); return t->end; }
        if (tag_is(name, "form")) { if (gap) want_break(f, gap); f->cur_form = -1; return t->end; }
        if (gap) want_break(f, gap);
        return t->end;
    }

    /* --- opening --- */
    frame *fr = elem_open(f, t);
    bool self_hidden = fr && (fr->display == CSS_DISPLAY_NONE || fr->vis_hidden);
    if (quiet(f) || self_hidden) {
        bool counting = folded(f) && !hidden(f) && !self_hidden;
        if (tag_is(name, "a")) {
            const char *href = attr(t, "href");
            if (counting && href && href[0] && href[0] != '#') f->fold_links++;
            return t->end;
        }
        if (tag_is(name, "form")) {
            html_sink *sk = f->sink;
            if (!counting && sk && sk->forms && sk->form_count && *sk->form_count < HTML_FORMS_MAX) {
                u32 i = (*sk->form_count)++;
                const char *act = attr(t, "action");
                u32 c = 0;
                if (act) while (act[c] && c < HTML_URL_MAX - 1) { sk->forms[i].action[c] = act[c]; c++; }
                sk->forms[i].action[c] = 0;
                const char *m = attr(t, "method");
                sk->forms[i].method = (m && (m[0] == 'p' || m[0] == 'P')) ? METHOD_POST : METHOD_GET;
                f->cur_form = (i32)i;
            }
            return t->end;
        }
        if (tag_is(name, "input")) {
            const char *type = attr(t, "type");
            bool submit = type && (type[0] == 's' || type[0] == 'S' || type[0] == 'b' || type[0] == 'B' || type[0] == 'i' || type[0] == 'I');
            if (counting) { if (!(type && (type[0] == 'h' || type[0] == 'H'))) f->fold_fields++; }
            else if (!submit) {
                /* a hidden field still goes with its form */
                const char *nm = attr(t, "name"), *vl = attr(t, "value");
                if (nm && nm[0]) field_emit(f, FIELD_HIDDEN, nm, vl ? vl : "");
            }
            return t->end;
        }
        if (tag_is(name, "textarea") || tag_is(name, "button") || tag_is(name, "select")) {
            if (counting && !tag_is(name, "select")) f->fold_fields++;
            return swallow_to(s, left, t->end, name, NULL);
        }
        return t->end;
    }

    u32 gap = ua_gap(name);
    if (fr && fr->display == CSS_DISPLAY_INLINE) gap = 0;
    else if (fr && gap == 0 && fr->display == CSS_DISPLAY_BLOCK && !is_void(name)) gap = 1;

    if (tag_is(name, "br")) { want_break(f, 1); return t->end; }
    if (tag_is(name, "hr")) { rule(f, f->v->col.faint); return t->end; }

    if (tag_is(name, "td") || tag_is(name, "th")) {
        cell_gap(f);
        if (tag_is(name, "th")) f->bold++;
        return t->end;
    }

    if (tag_is(name, "pre")) { if (gap) want_break(f, gap); f->pre++; return t->end; }

    if (tag_is(name, "blockquote")) {
        if (gap) want_break(f, gap);
        indent_more(f);
        f->quote++;
        return t->end;
    }

    if (tag_is(name, "ul") || tag_is(name, "ol")) {
        if (gap) want_break(f, gap);
        if (f->list_depth < 8) {
            f->list_num[f->list_depth] = tag_is(name, "ol") ? 1 : 0;
            f->list_depth++;
        }
        indent_more(f);
        return t->end;
    }

    if (tag_is(name, "li")) {
        if (gap) want_break(f, gap);
        bool bullet = gap > 0 && !(fr && fr->list_none);
        if (bullet) {
            settle_blanks(f);
            u32 depth = f->list_depth ? f->list_depth - 1 : 0;
            if (f->list_depth && f->list_num[depth] > 0) {
                char num[8];
                i32 v = f->list_num[depth]++;
                u32 k = 0;
                char tmp[8]; u32 tn = 0;
                if (v == 0) tmp[tn++] = '0';
                while (v) { tmp[tn++] = (char)('0' + v % 10); v /= 10; }
                while (tn) num[k++] = tmp[--tn];
                num[k++] = '.';
                num[k] = 0;
                for (u32 i = 0; num[i]; i++) word_add(f, num[i]);
            } else {
                word_add(f, '-');
            }
            word_flush(f);
        } else if (f->list_depth && f->list_num[f->list_depth - 1] > 0) {
            f->list_num[f->list_depth - 1]++;
        }
        return t->end;
    }

    if (is_heading(name)) {
        u32 lvl = (u32)(name[1] - '0');
        if (gap) want_break(f, gap);
        if (f->head_depth < 8) f->head_lvl[f->head_depth++] = (u8)lvl;
        return t->end;
    }

    if (tag_is(name, "b") || tag_is(name, "strong") ||
        tag_is(name, "em") || tag_is(name, "i") || tag_is(name, "code")) {
        word_flush(f);
        if (gap) want_break(f, gap);
        f->bold++;
        if (tag_is(name, "code")) f->code++;
        return t->end;
    }

    if (tag_is(name, "a")) {
        word_flush(f);
        if (gap) want_break(f, gap);
        html_sink *sk = f->sink;
        const char *href = attr(t, "href");
        if (sk && sk->urls && sk->url_count && href &&
            *sk->url_count < HTML_LINKS_MAX) {
            u32 i = (*sk->url_count)++;
            u32 c = 0;
            while (href[c] && href[c] != '#' && c < HTML_URL_MAX - 1)
                { sk->urls[i][c] = href[c]; c++; }
            sk->urls[i][c] = 0;
            f->link = c > 0 ? (i32)i : -1;
            if (c == 0) (*sk->url_count)--;
        }
        return t->end;
    }

    if (tag_is(name, "img")) {
        picture(f, t);
        return t->end;
    }

    /* --- forms --- */
    if (tag_is(name, "form")) {
        if (gap) want_break(f, gap);
        html_sink *sk = f->sink;
        if (sk && sk->forms && sk->form_count &&
            *sk->form_count < HTML_FORMS_MAX) {
            u32 i = (*sk->form_count)++;
            const char *act = attr(t, "action");
            u32 c = 0;
            if (act) while (act[c] && c < HTML_URL_MAX - 1)
                { sk->forms[i].action[c] = act[c]; c++; }
            sk->forms[i].action[c] = 0;
            const char *m = attr(t, "method");
            sk->forms[i].method = (m && (m[0] == 'p' || m[0] == 'P'))
                                ? METHOD_POST : METHOD_GET;
            f->cur_form = (i32)i;
        }
        return t->end;
    }

    if (tag_is(name, "input")) {
        const char *type = attr(t, "type");
        const char *nm = attr(t, "name");
        const char *vl = attr(t, "value");
        u8 kind = FIELD_TEXT;
        if (type) {
            if (type[0] == 'h' || type[0] == 'H') kind = FIELD_HIDDEN;
            else if (type[0] == 'p' || type[0] == 'P') kind = FIELD_PASS;
            else if ((type[0]=='s'||type[0]=='S') &&
                     (type[1]=='u'||type[1]=='U')) kind = FIELD_SUBMIT;
            else if (type[0]=='b'||type[0]=='B'||type[0]=='i'||type[0]=='I')
                kind = FIELD_SUBMIT;    /* button, image */
        }
        if (gap) want_break(f, gap);
        if (kind == FIELD_SUBMIT)
            field_emit(f, kind, nm ? nm : "", vl ? vl : "go");
        else
            field_emit(f, kind, nm ? nm : "", vl ? vl : "");
        return t->end;
    }

    if (tag_is(name, "textarea")) {
        const char *nm = attr(t, "name");
        if (gap) want_break(f, gap);
        field_emit(f, FIELD_TEXT, nm ? nm : "", "");
        return swallow_to(s, left, t->end, "textarea", NULL);
    }

    if (tag_is(name, "button")) {
        /* The label is the button's own words. */
        char label[HTML_VALUE_MAX];
        u32 ln = 0;
        u64 p = t->end;
        while (p < left && s[p] != '<') {
            char ch = (char)s[p++];
            if (ch == '\n' || ch == '\t' || ch == '\r') ch = ' ';
            if (ln < sizeof(label) - 1) label[ln++] = ch;
        }
        while (ln && label[ln - 1] == ' ') ln--;
        label[ln] = 0;
        const char *nm = attr(t, "name");
        if (gap) want_break(f, gap);
        field_emit(f, FIELD_SUBMIT, nm ? nm : "", ln ? label : "go");
        return swallow_to(s, left, t->end, "button", NULL);
    }

    if (tag_is(name, "select")) {
        /* Not fillable here; show its first option's text and move on. */
        return swallow_to(s, left, t->end, "select", NULL);
    }

    if (gap) want_break(f, gap);
    return t->end;
}

/* ------------------------------------------------------------------ */

u32 html_render(const html_view *v, html_sink *sink)
{
    flow *f = &F;
    names_init();

    /* everything but the line's cells, which are kept clean */
    f->v = v;
    f->sink = sink;
    f->cols = v->w / GLYPH_W;
    if (f->cols > LINE_MAX) f->cols = LINE_MAX;
    f->rows = v->h / GLYPH_H;
    f->col = 0; f->row = 0; f->indent = 0;
    f->blanks = 0; f->line_dirty = false;
    f->bold = 0; f->link = -1; f->pre = 0;
    f->head_depth = 0; f->list_depth = 0;
    f->cur_form = -1;
    f->wlen = 0;
    f->wsc = 1;
    for (i32 c = 0; c < f->line_end; c++) { f->line[c].cp = 0; f->line[c].code = 0; }
    f->line_end = 0; f->nlf = 0; f->line_align = CSS_ALIGN_LEFT; f->align_set = false; f->line_rows = 1;
    f->depth = 0; f->overflow = 0;
    f->hide_at = -1; f->fold_at = -1; f->fold_open = false; f->fold_kind = 0; f->fold_n = 0;
    f->fold_links = f->fold_words = f->fold_fields = 0; f->in_word = false;
    f->hidden_count = 0;

    if (sink) {
        if (sink->url_count) *sink->url_count = 0;
        if (sink->link_spot_count) *sink->link_spot_count = 0;
        if (sink->form_count) *sink->form_count = 0;
        if (sink->field_count) *sink->field_count = 0;
        if (sink->field_spot_count) *sink->field_spot_count = 0;
        if (sink->image_count) *sink->image_count = 0;
        if (sink->title && sink->title_max) sink->title[0] = 0;
        if (sink->find_row) *sink->find_row = (u32)-1;
        if (sink->fold_spot_count) *sink->fold_spot_count = 0;
        if (sink->fold_count) *sink->fold_count = 0;
        if (sink->hidden_count) *sink->hidden_count = 0;
    }
    if (f->cols < 8) return 0;

    const u8 *s = v->src;
    u64 i = 0;

    while (i < v->len && s[i]) {
        u8 c = s[i];

        if (c == '<') { i += tag(f, s + i, v->len - i); continue; }
        if (hidden(f)) { i++; continue; }
        if (folded(f)) {
            bool space = c == ' ' || c == '\n' || c == '\t' || c == '\r';
            if (space) f->in_word = false;
            else if (!f->in_word) { f->in_word = true; f->fold_words++; }
            i++;
            continue;
        }
        if (c == '&') { i += entity(f, s + i, v->len - i); continue; }

        if (c >= 0x80) {
            u32 cp;
            i += utf8_take(s + i, v->len - i, &cp);
            if (cp == 0xA0) { if (f->pre) word_add(f, ' '); else word_flush(f); }
            else if (cp >= 0xA0) word_add(f, cp);
            continue;
        }

        if (f->pre) {
            if (c == '\n') { word_flush(f); want_break(f, 1); }
            else if (c == '\t') { word_add(f, ' '); word_add(f, ' '); }
            else if (c == ' ') word_add(f, ' ');
            else if (c >= 0x20 && c < 0x7F) word_add(f, c);
        } else {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
                word_flush(f);
            else if (c >= 0x20 && c < 0x7F)
                word_add(f, c);
        }
        i++;
    }
    word_flush(f);
    while (f->depth) pop_top(f);                 /* a fold left open by the page closes here */
    word_flush(f);
    bool dirty = f->line_dirty;
    if (dirty) line_paint(f);
    if (sink && sink->hidden_count) *sink->hidden_count = f->hidden_count;

    return f->row + (dirty ? f->line_rows : 0);
}

/* ------------------------------------------------------------------ */
/* The styles                                                          */
/* ------------------------------------------------------------------ */

/* Whether a rel attribute names a stylesheet, and not an alternate one. */
static bool rel_stylesheet(const char *rel)
{
    bool sheet = false, alternate = false;
    u32 i = 0;
    while (rel[i]) {
        while (rel[i] == ' ' || rel[i] == '\t') i++;
        u32 s = i;
        while (rel[i] && rel[i] != ' ' && rel[i] != '\t') i++;
        u32 n = i - s;
        char w[16];
        u32 k = 0;
        for (u32 j = s; j < i && k < sizeof(w) - 1; j++) w[k++] = to_lower(rel[j]);
        w[k] = 0;
        if (n == 10 && tag_is(w, "stylesheet")) sheet = true;
        if (n == 9 && tag_is(w, "alternate")) alternate = true;
    }
    return sheet && !alternate;
}

u32 html_styles(const html_view *v, html_sink *sink, css_sheet *sheet)
{
    css_reset(sheet, (u32)v->w);
    if (sink && sink->sheet_count) *sink->sheet_count = 0;
    const u8 *s = v->src;
    u64 n = v->len;
    u64 i = 0;
    parsed_tag *t = &T;

    while (i < n && s[i]) {
        if (s[i] != '<') { i++; continue; }
        if (n - i > 3 && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-') {
            u64 p = i + 4;
            while (p + 2 < n && !(s[p] == '-' && s[p + 1] == '-' && s[p + 2] == '>')) p++;
            i = p + 3 < n ? p + 3 : n;
            continue;
        }
        parse_tag(s + i, n - i, t);
        u64 end = i + t->end;
        if (!t->closing && tag_is(t->name, "script")) {
            end = i + swallow_to(s + i, n - i, t->end, "script", NULL);
        } else if (!t->closing && tag_is(t->name, "style")) {
            u64 close_at;
            end = i + swallow_to(s + i, n - i, t->end, "style", &close_at);
            close_at += i;
            const char *media = attr(t, "media");
            if (!media || !media[0] || css_media((const u8 *)media, slen(media), sheet->width))
                css_parse(sheet, s + i + t->end, close_at > i + t->end ? close_at - i - t->end : 0);
        } else if (!t->closing && tag_is(t->name, "link")) {
            const char *rel = attr(t, "rel");
            const char *href = attr(t, "href");
            const char *media = attr(t, "media");
            if (rel && href && href[0] && rel_stylesheet(rel) &&
                (!media || !media[0] || css_media((const u8 *)media, slen(media), sheet->width))) {
                if (sink && sink->sheets && sink->sheet_count) {
                    u32 k = 0;
                    for (; k < *sink->sheet_count; k++) if (tag_is(sink->sheets[k], href)) break;
                    if (k == *sink->sheet_count && k < CSS_SHEETS_MAX) {
                        u32 c = 0;
                        while (href[c] && c < HTML_URL_MAX - 1) { sink->sheets[k][c] = href[c]; c++; }
                        sink->sheets[k][c] = 0;
                        (*sink->sheet_count)++;
                    }
                }
                if (sink && sink->sheet_text) {
                    u32 len = 0;
                    const u8 *text = sink->sheet_text(sink->sheet_ctx, href, &len);
                    if (text && len) css_parse(sheet, text, len);
                }
            }
        }
        i = end > i ? end : i + 1;
    }
    return sheet->count;
}
