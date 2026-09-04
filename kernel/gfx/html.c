/*
 * html.c -- text browser lens: one pass over markup.
 * - headings, paragraphs, lists, quotes, pre, tables, links, forms (text fields, submit)
 * - scripts, styles, comments and unknown tags are dropped
 * - no CSS, no JavaScript, no images; width from the window
 */
#include <eb/html.h>

#define WORD_MAX  96
#define IND_STEP  2                 /* columns per level of indent */
#define FIELD_COLS 22               /* width of a text field, in glyphs */

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
    u32  skip;                      /* inside script/style */
    u32  pre;                       /* inside preformatted text */

    /* Headings, as a level stack so a close knows what it closes. */
    u8   head_lvl[8];
    u32  head_depth;

    /* Lists: a stack of counters; zero means unordered. */
    i32  list_num[8];
    u32  list_depth;

    /* The form being filled, if any. */
    i32  cur_form;

    char word[WORD_MAX];
    u32  wlen;
} flow;

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

static void line_break(flow *f)
{
    f->col = f->indent;
    f->row++;
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

/* ------------------------------------------------------------------ */
/* Link spots                                                          */
/* ------------------------------------------------------------------ */

static void link_note(flow *f, i32 x, u32 row, u32 chars)
{
    html_sink *s = f->sink;
    if (!s || !s->link_spots || !s->link_spot_count || f->link < 0) return;
    if (!visible(f, row)) return;
    i32 y = pixel_y(f, row);

    if (*s->link_spot_count > 0) {
        html_spot *last = &s->link_spots[*s->link_spot_count - 1];
        if (last->ref == (u32)f->link && last->y == y &&
            last->x + last->w >= x - GLYPH_W) {
            last->w = (x + (i32)chars * GLYPH_W) - last->x;
            return;
        }
    }
    if (*s->link_spot_count >= HTML_SPOTS_MAX) return;
    s->link_spots[(*s->link_spot_count)++] = (html_spot){
        x, y, (i32)chars * GLYPH_W, GLYPH_H, (u32)f->link };
}

/* ------------------------------------------------------------------ */
/* Words                                                               */
/* ------------------------------------------------------------------ */

static void word_flush(flow *f)
{
    if (f->wlen == 0) return;
    settle_blanks(f);

    if (f->col + (i32)f->wlen > f->cols && f->col > f->indent) line_break(f);

    color c = f->v->col.dim;
    if (f->head_depth || f->bold) c = f->v->col.text;
    if (f->link >= 0) c = f->v->col.accent;

    i32 x = f->v->x + f->col * GLYPH_W;
    u32 shown = f->wlen;
    if ((i32)shown > f->cols - f->col) shown = (u32)(f->cols - f->col);

    if (visible(f, f->row)) {
        i32 y = pixel_y(f, f->row);
        for (u32 i = 0; i < shown; i++)
            fb_glyph(x + (i32)i * GLYPH_W, y, (u8)f->word[i], c, 0, false);
        if (f->link >= 0)
            fb_rect(x, y + GLYPH_H - 2, (i32)shown * GLYPH_W, 1, c);
    }
    link_note(f, x, f->row, shown);

    f->col += (i32)shown;
    f->line_dirty = true;
    f->wlen = 0;

    if (f->col < f->cols) f->col++;          /* the space after */
    else line_break(f);
}

static void word_add(flow *f, char c)
{
    if (f->wlen < WORD_MAX - 1) f->word[f->wlen++] = c;
    if (f->col + (i32)f->wlen >= f->cols) word_flush(f);
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
        fb_rect(f->v->x + f->indent * GLYPH_W, pixel_y(f, f->row) + GLYPH_H / 2,
                (f->cols - f->indent) * GLYPH_W, 1, c);
    f->row++;
    f->col = f->indent;
}

/* Two spaces between table cells, when the row already holds one. */
static void cell_gap(flow *f)
{
    word_flush(f);
    if (f->line_dirty && f->col + 2 < f->cols) f->col += 2;
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
    const char *label = value;
    if (kind == FIELD_SUBMIT) {
        u32 ll = 0; while (label[ll]) ll++;
        w = (ll ? ll : 2) + 2;               /* room for the brackets */
    }

    word_flush(f);
    settle_blanks(f);
    if (f->col + (i32)w > f->cols && f->col > f->indent) line_break(f);

    i32 x = f->v->x + f->col * GLYPH_W;
    i32 y = visible(f, f->row) ? pixel_y(f, f->row) : -1000;

    if (kind == FIELD_SUBMIT) {
        if (y > -1000) {
            fb_rect(x - 2, y - 2, (i32)w * GLYPH_W + 2, GLYPH_H + 4,
                    f->v->col.edge);
            i32 tx = x + GLYPH_W;
            fb_glyph(x, y, '[', f->v->col.accent, 0, false);
            for (u32 i = 0; label[i]; i++)
                fb_glyph(tx + (i32)i * GLYPH_W, y, (u8)label[i],
                         f->v->col.accent, 0, false);
            u32 ll = 0; while (label[ll]) ll++;
            fb_glyph(tx + (i32)ll * GLYPH_W, y, ']', f->v->col.accent,
                     0, false);
        }
    } else if (y > -1000) {
        fb_rect(x - 2, y - 2, (i32)w * GLYPH_W + 2, GLYPH_H + 3,
                f->v->col.edge);
        fb_rect(x - 1, y - 1, (i32)w * GLYPH_W, GLYPH_H + 1,
                f->v->col.faint);
        const char *shown = s->field_values ? s->field_values[idx] : value;
        for (u32 i = 0; shown[i] && i < w - 1; i++)
            fb_glyph(x + (i32)i * GLYPH_W, y,
                     kind == FIELD_PASS ? '*' : (u8)shown[i],
                     f->v->col.text, 0, false);
    }

    if (s->field_spots && s->field_spot_count &&
        *s->field_spot_count < HTML_SPOTS_MAX && visible(f, f->row))
        s->field_spots[(*s->field_spot_count)++] = (html_spot){
            x - 2, pixel_y(f, f->row) - 2, (i32)w * GLYPH_W + 2,
            GLYPH_H + 4, idx };

    f->col += (i32)w;
    f->line_dirty = true;
    if (f->col < f->cols) f->col++;
    else line_break(f);
}

/* ------------------------------------------------------------------ */
/* Entities                                                            */
/* ------------------------------------------------------------------ */

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static u32 entity(flow *f, const u8 *s, u64 left)
{
    char name[10];
    u32 n = 0;
    while (n + 1 < left && n < sizeof(name) - 1 && s[n + 1] != ';' &&
           s[n + 1] != '&' && s[n + 1] != '<' && s[n + 1] != ' ')
        { name[n] = (char)s[n + 1]; n++; }
    if (n + 1 >= left || s[n + 1] != ';') { word_add(f, '&'); return 1; }
    name[n] = 0;

    char out = 0;
    if (n >= 2 && name[0] == '#') {
        u32 v = 0;
        for (u32 i = 1; i < n; i++)
            if (name[i] >= '0' && name[i] <= '9')
                v = v * 10 + (u32)(name[i] - '0');
        out = (v >= 0x20 && v < 0x7F) ? (char)v : 0;
    }
    else if (n == 3 && name[0]=='a' && name[1]=='m' && name[2]=='p') out = '&';
    else if (n == 2 && name[0]=='l' && name[1]=='t') out = '<';
    else if (n == 2 && name[0]=='g' && name[1]=='t') out = '>';
    else if (n == 4 && name[0]=='q') out = '"';
    else if (n == 4 && name[0]=='n' && name[1]=='b') out = ' ';
    else if (n == 5 && name[0]=='a' && name[1]=='p') out = '\'';
    else if (n == 5 && name[0]=='m' && name[1]=='d') out = '-';  /* mdash */
    else if (n == 5 && name[0]=='n' && name[1]=='d') out = '-';  /* ndash */

    if (out == ' ') { if (f->pre) word_add(f, ' '); else word_flush(f); }
    else if (out) word_add(f, out);
    return n + 2;
}

/* ------------------------------------------------------------------ */
/* Tags                                                                */
/* ------------------------------------------------------------------ */

static bool tag_is(const char *t, const char *want)
{
    u32 i = 0;
    while (want[i]) { if (t[i] != want[i]) return false; i++; }
    return t[i] == 0;
}

/* A tag name that is a heading: h1..h6, or title standing in for one. */
static bool n_is_heading(const char *name)
{
    if (name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0)
        return true;
    return tag_is(name, "title");
}

#define ATTRS_MAX 10

typedef struct {
    char name[12];
    bool closing;
    char key[ATTRS_MAX][12];
    char val[ATTRS_MAX][HTML_URL_MAX];
    u32  nattr;
    u64  end;
} parsed_tag;

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

        char key[12];
        u32 kn = 0;
        while (p < left && s[p] != '=' && s[p] != '>' && s[p] != ' ' &&
               s[p] != '\t' && s[p] != '\n' && s[p] != '\r') {
            if (kn < sizeof(key) - 1) key[kn++] = to_lower((char)s[p]);
            p++;
        }
        key[kn] = 0;

        char val[HTML_URL_MAX];
        u32 vn = 0;
        if (p < left && s[p] == '=') {
            p++;
            char q = 0;
            if (p < left && (s[p] == '"' || s[p] == '\'')) q = (char)s[p++];
            while (p < left && s[p] != '>' &&
                   (q ? (char)s[p] != q : (s[p] != ' ' && s[p] != '\t' &&
                                           s[p] != '\n' && s[p] != '\r'))) {
                if (vn < sizeof(val) - 1) val[vn++] = (char)s[p];
                p++;
            }
            if (q && p < left && (char)s[p] == q) p++;
        }
        val[vn] = 0;

        if (kn && t->nattr < ATTRS_MAX) {
            u32 i = 0;
            while (key[i] && i < 11) { t->key[t->nattr][i] = key[i]; i++; }
            t->key[t->nattr][i] = 0;
            u32 j = 0;
            while (val[j] && j < HTML_URL_MAX - 1)
                { t->val[t->nattr][j] = val[j]; j++; }
            t->val[t->nattr][j] = 0;
            t->nattr++;
        }
    }
    if (p < left) p++;                        /* the '>' */
    t->end = p;
}

/* Swallow to a named closing tag, returning the source offset past it. */
static u64 swallow_to(const u8 *s, u64 left, u64 from, const char *close)
{
    u64 p = from;
    while (p < left) {
        if (s[p] == '<' && p + 1 < left && s[p+1] == '/') {
            u64 q = p + 2;
            u32 i = 0;
            while (close[i] && q < left &&
                   to_lower((char)s[q]) == close[i]) { i++; q++; }
            if (close[i] == 0) {
                while (q < left && s[q] != '>') q++;
                return q < left ? q + 1 : left;
            }
        }
        p++;
    }
    return left;
}

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

    parsed_tag t;
    parse_tag(s, left, &t);
    const char *name = t.name;
    bool close = t.closing;

    if (tag_is(name, "script") || tag_is(name, "style") ||
        tag_is(name, "head") || tag_is(name, "svg") ||
        tag_is(name, "noscript")) {
        if (close) return t.end;
        return swallow_to(s, left, t.end, name);
    }

    if (tag_is(name, "br")) { want_break(f, 1); return t.end; }
    if (tag_is(name, "hr")) { rule(f, f->v->col.faint); return t.end; }

    if (tag_is(name, "p") || tag_is(name, "section") ||
        tag_is(name, "article") || tag_is(name, "header") ||
        tag_is(name, "footer") || tag_is(name, "main") ||
        tag_is(name, "nav") || tag_is(name, "figure") ||
        tag_is(name, "dl") || tag_is(name, "address")) {
        want_break(f, 2);
        return t.end;
    }
    if (tag_is(name, "div") || tag_is(name, "tr") || tag_is(name, "dt") ||
        tag_is(name, "dd") || tag_is(name, "label")) {
        want_break(f, 1);
        return t.end;
    }

    if (tag_is(name, "table")) {
        want_break(f, 2);
        return t.end;
    }
    if (tag_is(name, "td") || tag_is(name, "th")) {
        if (!close) cell_gap(f);
        if (tag_is(name, "th")) { if (close && f->bold) f->bold--; else f->bold++; }
        return t.end;
    }

    if (tag_is(name, "pre")) {
        want_break(f, 2);
        if (close) { if (f->pre) f->pre--; }
        else f->pre++;
        return t.end;
    }

    if (tag_is(name, "blockquote")) {
        want_break(f, 1);
        if (close) { if (f->indent >= IND_STEP) f->indent -= IND_STEP; }
        else f->indent += IND_STEP;
        f->col = f->indent;
        return t.end;
    }

    if (tag_is(name, "ul") || tag_is(name, "ol")) {
        want_break(f, 1);
        if (close) {
            if (f->list_depth) f->list_depth--;
            if (f->indent >= IND_STEP) f->indent -= IND_STEP;
        } else {
            if (f->list_depth < 8) {
                f->list_num[f->list_depth] = tag_is(name, "ol") ? 1 : 0;
                f->list_depth++;
            }
            f->indent += IND_STEP;
        }
        f->col = f->indent;
        return t.end;
    }

    if (tag_is(name, "li")) {
        want_break(f, 1);
        if (!close) {
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
        }
        return t.end;
    }

    if (n_is_heading(name)) {
        u32 lvl = (name[0] == 't') ? 1 : (u32)(name[1] - '0');
        want_break(f, 2);
        if (close) {
            if (f->head_depth) f->head_depth--;
            if (lvl <= 2) rule(f, f->v->col.faint);
        } else if (f->head_depth < 8) {
            f->head_lvl[f->head_depth++] = (u8)lvl;
        }
        return t.end;
    }

    if (tag_is(name, "b") || tag_is(name, "strong") ||
        tag_is(name, "em") || tag_is(name, "i") || tag_is(name, "code")) {
        word_flush(f);
        if (close) { if (f->bold) f->bold--; }
        else f->bold++;
        return t.end;
    }

    if (tag_is(name, "a")) {
        word_flush(f);
        if (close) { f->link = -1; return t.end; }
        html_sink *sk = f->sink;
        const char *href = attr(&t, "href");
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
        return t.end;
    }

    if (tag_is(name, "img")) {
        const char *alt = attr(&t, "alt");
        if (alt && alt[0]) {
            word_flush(f);
            word_add(f, '[');
            for (u32 i = 0; alt[i]; i++) {
                if (alt[i] == ' ') word_flush(f);
                else word_add(f, alt[i]);
            }
            word_add(f, ']');
            word_flush(f);
        }
        return t.end;
    }

    /* --- forms --- */
    if (tag_is(name, "form")) {
        want_break(f, 1);
        if (close) { f->cur_form = -1; return t.end; }
        html_sink *sk = f->sink;
        if (sk && sk->forms && sk->form_count &&
            *sk->form_count < HTML_FORMS_MAX) {
            u32 i = (*sk->form_count)++;
            const char *act = attr(&t, "action");
            u32 c = 0;
            if (act) while (act[c] && c < HTML_URL_MAX - 1)
                { sk->forms[i].action[c] = act[c]; c++; }
            sk->forms[i].action[c] = 0;
            const char *m = attr(&t, "method");
            sk->forms[i].method = (m && (m[0] == 'p' || m[0] == 'P'))
                                ? METHOD_POST : METHOD_GET;
            f->cur_form = (i32)i;
        }
        return t.end;
    }

    if (tag_is(name, "input")) {
        if (close) return t.end;
        const char *type = attr(&t, "type");
        const char *nm = attr(&t, "name");
        const char *vl = attr(&t, "value");
        u8 kind = FIELD_TEXT;
        if (type) {
            if (type[0] == 'h' || type[0] == 'H') kind = FIELD_HIDDEN;
            else if (type[0] == 'p' || type[0] == 'P') kind = FIELD_PASS;
            else if ((type[0]=='s'||type[0]=='S') &&
                     (type[1]=='u'||type[1]=='U')) kind = FIELD_SUBMIT;
            else if (type[0]=='b'||type[0]=='B'||type[0]=='i'||type[0]=='I')
                kind = FIELD_SUBMIT;    /* button, image */
        }
        if (kind == FIELD_SUBMIT)
            field_emit(f, kind, nm ? nm : "", vl ? vl : "go");
        else
            field_emit(f, kind, nm ? nm : "", vl ? vl : "");
        return t.end;
    }

    if (tag_is(name, "textarea")) {
        if (close) return t.end;
        const char *nm = attr(&t, "name");
        field_emit(f, FIELD_TEXT, nm ? nm : "", "");
        return swallow_to(s, left, t.end, "textarea");
    }

    if (tag_is(name, "button")) {
        if (close) return t.end;
        /* The label is the button's own words. */
        char label[HTML_VALUE_MAX];
        u32 ln = 0;
        u64 p = t.end;
        while (p < left && s[p] != '<') {
            char ch = (char)s[p++];
            if (ch == '\n' || ch == '\t' || ch == '\r') ch = ' ';
            if (ln < sizeof(label) - 1) label[ln++] = ch;
        }
        while (ln && label[ln - 1] == ' ') ln--;
        label[ln] = 0;
        const char *nm = attr(&t, "name");
        field_emit(f, FIELD_SUBMIT, nm ? nm : "", ln ? label : "go");
        return swallow_to(s, left, t.end, "button");
    }

    if (tag_is(name, "select")) {
        /* Not fillable here; show its first option's text and move on. */
        return swallow_to(s, left, t.end, "select");
    }

    return t.end;
}

/* ------------------------------------------------------------------ */

u32 html_render(const html_view *v, html_sink *sink)
{
    flow f = { 0 };
    f.v = v;
    f.sink = sink;
    f.cols = v->w / GLYPH_W;
    f.rows = v->h / GLYPH_H;
    f.link = -1;
    f.cur_form = -1;

    if (sink) {
        if (sink->url_count) *sink->url_count = 0;
        if (sink->link_spot_count) *sink->link_spot_count = 0;
        if (sink->form_count) *sink->form_count = 0;
        if (sink->field_count) *sink->field_count = 0;
        if (sink->field_spot_count) *sink->field_spot_count = 0;
    }
    if (f.cols < 8) return 0;

    const u8 *s = v->src;
    u64 i = 0;

    while (i < v->len && s[i]) {
        u8 c = s[i];

        if (c == '<') { i += tag(&f, s + i, v->len - i); continue; }
        if (f.skip) { i++; continue; }
        if (c == '&') { i += entity(&f, s + i, v->len - i); continue; }

        if (f.pre) {
            if (c == '\n') { word_flush(&f); want_break(&f, 1); }
            else if (c == '\t') { word_add(&f, ' '); word_add(&f, ' '); }
            else if (c == ' ') word_add(&f, ' ');
            else if (c >= 0x20 && c < 0x7F) word_add(&f, (char)c);
        } else {
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
                word_flush(&f);
            else if (c >= 0x20 && c < 0x7F)
                word_add(&f, (char)c);
        }
        i++;
    }
    word_flush(&f);

    return f.row + (f.line_dirty ? 1 : 0);
}
