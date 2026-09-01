/*
 * html.c -- markup in, prose out.
 *
 * A single pass with a small mind: text accumulates into words, words
 * wrap at the window's edge, and tags adjust the mood -- block tags
 * break the line, headings brighten, anchors colour and underline and
 * leave a note of where they pointed and where they were drawn.
 * Scripts and styles are skipped whole. Everything unrecognised is
 * ignored rather than shown, because markup a reader cannot use is
 * noise wearing angle brackets.
 */
#include <eb/html.h>

/* ------------------------------------------------------------------ */
/* The flowing cursor                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    const html_view *v;
    i32  cols, rows;                /* the window, in glyphs */
    i32  col;
    u32  row;                       /* virtual, from the top of the flow */

    u32  blanks;                    /* line breaks owed but not yet paid */
    bool line_dirty;                /* anything on the current line yet */

    /* Mood. */
    u32  heading;                   /* nested h1..h6 */
    u32  bold;
    i32  link;                      /* -1, or index into urls */
    u32  skip;                      /* inside script or style */

    /* The word being gathered. */
    char word[64];
    u32  wlen;

    /* Where link words landed. */
    char (*urls)[HTML_URL_MAX];
    u32  *url_count;
    html_spot *spots;
    u32  *spot_count;
} flow;

static bool visible(const flow *f, u32 row)
{
    return row >= f->v->scroll && row < f->v->scroll + (u32)f->rows;
}

static i32 pixel_y(const flow *f, u32 row)
{
    return f->v->y + (i32)(row - f->v->scroll) * GLYPH_H;
}

static void spot_note(flow *f, i32 x, u32 row, u32 chars)
{
    if (!f->spots || !f->spot_count || f->link < 0) return;
    if (!visible(f, row)) return;

    i32 y = pixel_y(f, row);

    /* Stretch the previous note when this one continues it. */
    if (*f->spot_count > 0) {
        html_spot *last = &f->spots[*f->spot_count - 1];
        if (last->link == (u32)f->link && last->y == y &&
            last->x + last->w >= x - GLYPH_W) {
            last->w = (x + (i32)chars * GLYPH_W) - last->x;
            return;
        }
    }
    if (*f->spot_count >= HTML_SPOTS_MAX) return;
    f->spots[(*f->spot_count)++] = (html_spot){
        .x = x, .y = y, .w = (i32)chars * GLYPH_W, .h = GLYPH_H,
        .link = (u32)f->link,
    };
}

static void line_break(flow *f)
{
    f->col = 0;
    f->row++;
    f->line_dirty = false;
}

/* Pays the owed break before the next word. Owing rather than
 * emitting is what collapses six closing blocks into one gap, and
 * what keeps a page from starting with its own margin. */
static void settle_blanks(flow *f)
{
    if (f->blanks == 0) return;
    if (f->line_dirty) line_break(f);
    if (f->blanks > 1 && f->row > 0) f->row++;   /* one empty line */
    f->blanks = 0;
}

static void word_flush(flow *f)
{
    if (f->wlen == 0) return;
    settle_blanks(f);

    if (f->col + (i32)f->wlen > f->cols && f->col > 0) line_break(f);

    color c = f->v->col.dim;
    if (f->heading || f->bold) c = f->v->col.text;
    if (f->link >= 0) c = f->v->col.accent;

    i32 x = f->v->x + f->col * GLYPH_W;
    bool on = visible(f, f->row);
    u32 shown = f->wlen;
    if ((i32)shown > f->cols - f->col) shown = (u32)(f->cols - f->col);

    if (on) {
        i32 y = pixel_y(f, f->row);
        for (u32 i = 0; i < shown; i++)
            fb_glyph(x + (i32)i * GLYPH_W, y, (u8)f->word[i], c, 0, false);
        if (f->link >= 0)
            fb_rect(x, y + GLYPH_H - 2, (i32)shown * GLYPH_W, 1, c);
    }
    spot_note(f, x, f->row, shown);

    f->col += (i32)shown;
    f->line_dirty = true;
    f->wlen = 0;

    /* The space after a word, unless the edge already broke the line. */
    if (f->col < f->cols) f->col++;
    else line_break(f);
}

static void word_add(flow *f, char c)
{
    if (f->wlen < sizeof(f->word) - 1) f->word[f->wlen++] = c;
    if ((i32)f->wlen >= f->cols) word_flush(f);   /* longer than a line */
}

static void want_break(flow *f, u32 gap)
{
    word_flush(f);
    if (gap > f->blanks) f->blanks = gap;
}

/* ------------------------------------------------------------------ */
/* Entities and tags                                                   */
/* ------------------------------------------------------------------ */

static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* &amp; and friends. Returns how many source bytes were eaten. */
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
    else if (n == 4 && name[0]=='#' ) out = 0;
    else if (n == 5 && name[0]=='a' && name[1]=='p') out = '\'';

    if (out == ' ') word_flush(f);
    else if (out) word_add(f, out);
    return n + 2;
}

static bool tag_is(const char *t, const char *want)
{
    u32 i = 0;
    while (want[i]) { if (t[i] != want[i]) return false; i++; }
    return t[i] == 0;
}

/* One tag, from '<' up to its '>'. Returns bytes eaten. */
static u32 tag(flow *f, const u8 *s, u64 left)
{
    u64 end = 1;
    bool closing = (end < left && s[end] == '/');
    if (closing) end++;

    char name[12];
    u32 n = 0;
    while (end < left && ((s[end] >= 'a' && s[end] <= 'z') ||
                          (s[end] >= 'A' && s[end] <= 'Z') ||
                          (s[end] >= '0' && s[end] <= '9'))) {
        if (n < sizeof(name) - 1) name[n++] = to_lower((char)s[end]);
        end++;
    }
    name[n] = 0;

    /* The rest of the tag, minding quotes; href and alt are fished
     * out on the way past. */
    char grab[HTML_URL_MAX];
    u32  glen = 0;
    bool want_href = !closing && tag_is(name, "a");
    bool want_alt  = !closing && tag_is(name, "img");
    bool got_grab = false;

    while (end < left && s[end] != '>') {
        if ((want_href || want_alt) && !got_grab) {
            const char *key = want_href ? "href=" : "alt=";
            u32 kl = want_href ? 5 : 4;
            bool hit = true;
            for (u32 i = 0; i < kl && hit; i++)
                if (end + i >= left ||
                    to_lower((char)s[end + i]) != key[i]) hit = false;
            if (hit) {
                u64 p = end + kl;
                char q = 0;
                if (p < left && (s[p] == '"' || s[p] == '\'')) q = (char)s[p++];
                while (p < left && s[p] != '>' &&
                       (q ? (char)s[p] != q : s[p] != ' ') &&
                       glen < sizeof(grab) - 1)
                    grab[glen++] = (char)s[p++];
                got_grab = true;
            }
        }
        end++;
    }
    grab[glen] = 0;
    if (end < left) end++;                   /* the '>' itself */

    /* Comments: <!-- ... --> arrive here as a nameless tag. */
    if (n == 0 && left > 3 && s[1] == '!' && s[2] == '-' && s[3] == '-') {
        u64 p = 4;
        while (p + 2 < left &&
               !(s[p] == '-' && s[p+1] == '-' && s[p+2] == '>')) p++;
        return (u32)(p + 3 < left ? p + 3 : left);
    }

    if (tag_is(name, "script") || tag_is(name, "style")) {
        f->skip = closing ? 0 : 1;
        if (!closing) {
            /* Swallow up to the matching close. */
            u64 p = end;
            while (p + 8 < left) {
                if (s[p] == '<' && s[p+1] == '/' &&
                    to_lower((char)s[p+2]) == name[0]) {
                    while (p < left && s[p] != '>') p++;
                    p++;
                    f->skip = 0;
                    return (u32)p;
                }
                p++;
            }
            return (u32)left;                /* never closed; done */
        }
        return (u32)end;
    }

    if (tag_is(name, "br")) { want_break(f, 1); return (u32)end; }
    if (tag_is(name, "p") || tag_is(name, "tr") ||
        tag_is(name, "blockquote") || tag_is(name, "section") ||
        tag_is(name, "article") || tag_is(name, "header") ||
        tag_is(name, "footer") || tag_is(name, "table") ||
        tag_is(name, "form") || tag_is(name, "ul") || tag_is(name, "ol") ||
        tag_is(name, "pre") || tag_is(name, "nav")) {
        want_break(f, 2);
        return (u32)end;
    }
    if (tag_is(name, "div") || tag_is(name, "td") || tag_is(name, "th")) {
        want_break(f, 1);
        return (u32)end;
    }

    if (n == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
        want_break(f, 2);
        if (closing) { if (f->heading) f->heading--; }
        else f->heading++;
        return (u32)end;
    }
    if (tag_is(name, "title")) {
        want_break(f, 2);
        if (closing) { if (f->heading) f->heading--; }
        else f->heading++;
        return (u32)end;
    }

    if (tag_is(name, "b") || tag_is(name, "strong") ||
        tag_is(name, "em") || tag_is(name, "i")) {
        word_flush(f);
        if (closing) { if (f->bold) f->bold--; }
        else f->bold++;
        return (u32)end;
    }

    if (tag_is(name, "li")) {
        want_break(f, 1);
        if (!closing) {
            settle_blanks(f);
            word_add(f, '-');
            word_flush(f);
        }
        return (u32)end;
    }

    if (tag_is(name, "a")) {
        word_flush(f);
        if (closing) {
            f->link = -1;
        } else if (f->urls && f->url_count && glen > 0 &&
                   *f->url_count < HTML_LINKS_MAX) {
            u32 i = (*f->url_count)++;
            u32 c = 0;
            while (c < glen && c < HTML_URL_MAX - 1 &&
                   grab[c] != '#') {         /* fragments mean nothing here */
                f->urls[i][c] = grab[c];
                c++;
            }
            f->urls[i][c] = 0;
            f->link = (c > 0) ? (i32)i : -1;
            if (c == 0) (*f->url_count)--;
        }
        return (u32)end;
    }

    if (tag_is(name, "img") && glen > 0) {
        word_flush(f);
        word_add(f, '[');
        for (u32 i = 0; i < glen; i++) {
            if (grab[i] == ' ') word_flush(f);
            else word_add(f, grab[i]);
        }
        word_add(f, ']');
        word_flush(f);
        return (u32)end;
    }

    return (u32)end;
}

/* ------------------------------------------------------------------ */

u32 html_render(const html_view *v,
                char urls[][HTML_URL_MAX], u32 *url_count,
                html_spot *spots, u32 *spot_count)
{
    flow f = { 0 };
    f.v = v;
    f.cols = v->w / GLYPH_W;
    f.rows = v->h / GLYPH_H;
    f.link = -1;
    f.urls = urls;
    f.url_count = url_count;
    f.spots = spots;
    f.spot_count = spot_count;
    if (url_count) *url_count = 0;
    if (spot_count) *spot_count = 0;
    if (f.cols < 8) return 0;

    const u8 *s = v->src;
    u64 i = 0;

    while (i < v->len && s[i]) {
        u8 c = s[i];

        if (c == '<') { i += tag(&f, s + i, v->len - i); continue; }
        if (f.skip) { i++; continue; }
        if (c == '&') { i += entity(&f, s + i, v->len - i); continue; }

        if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
            word_flush(&f);
        else if (c >= 0x20 && c < 0x7F)
            word_add(&f, (char)c);
        i++;
    }
    word_flush(&f);

    return f.row + (f.line_dirty ? 1 : 0);
}
