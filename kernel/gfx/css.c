/*
 * css.c -- the stylesheet reader: rules with display, visibility, font-weight, color, text-align, list-style.
 * - selectors: tag, *, .class, #id, [attr], [attr=value], descendant and child combinators, up to five compounds
 * - a selector with a pseudo-class, a pseudo-element, a sibling combinator or another attribute operator never matches
 * - @media on width, height, screen, print and the usual preferences; @supports and @layer blocks read; the rest skipped
 * - rules keyed by the element's own compound (id, else a class, else the tag, else any) into buckets
 * - every read bounded by the length given; the table refuses rules past CSS_RULES_MAX and counts them
 */
#include <eb/css.h>

#define NONE ((u32)-1)

/* ------------------------------------------------------------------ */
/* Names                                                               */
/* ------------------------------------------------------------------ */

static u8 low(u8 c) { return (c >= 'A' && c <= 'Z') ? (u8)(c + 32) : c; }

u32 css_hash_n(const u8 *s, u32 n)
{
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) { h ^= s[i]; h *= 16777619u; }
    return h ? h : 1;
}

u32 css_hash(const char *s)
{
    u32 n = 0;
    while (s[n]) n++;
    return css_hash_n((const u8 *)s, n);
}

static u32 hash_folded(const u8 *s, u32 n)
{
    u32 h = 2166136261u;
    for (u32 i = 0; i < n; i++) { h ^= low(s[i]); h *= 16777619u; }
    return h ? h : 1;
}

static bool ws(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }
static bool ident_char(u8 c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c >= 0x80;
}
static bool ident_start(u8 c) { return ident_char(c) && !(c >= '0' && c <= '9'); }

static bool word_is(const u8 *s, u32 n, const char *w)
{
    u32 i = 0;
    while (i < n && w[i] && low(s[i]) == (u8)w[i]) i++;
    return i == n && w[i] == 0;
}

/* An identifier at s: its length, backslash escapes taken as the
 * character they hide. */
static u32 ident_len(const u8 *s, u32 n)
{
    u32 i = 0;
    while (i < n) {
        if (s[i] == '\\') { i += (i + 1 < n) ? 2 : 1; continue; }
        if (!ident_char(s[i])) break;
        i++;
    }
    return i;
}

/* ------------------------------------------------------------------ */
/* Colours and numbers                                                 */
/* ------------------------------------------------------------------ */

static const struct { const char *name; u32 rgb; } NAMED[] = {
    { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x008000 },
    { "blue", 0x0000FF }, { "gray", 0x808080 }, { "grey", 0x808080 }, { "silver", 0xC0C0C0 },
    { "navy", 0x000080 }, { "maroon", 0x800000 }, { "purple", 0x800080 }, { "orange", 0xFFA500 },
    { "yellow", 0xFFFF00 }, { "teal", 0x008080 }, { "aqua", 0x00FFFF }, { "lime", 0x00FF00 },
    { "olive", 0x808000 }, { "fuchsia", 0xFF00FF }, { "darkgray", 0xA9A9A9 }, { "darkgrey", 0xA9A9A9 },
    { "lightgray", 0xD3D3D3 }, { "lightgrey", 0xD3D3D3 }, { "dimgray", 0x696969 }, { "dimgrey", 0x696969 },
    { "brown", 0xA52A2A }, { "pink", 0xFFC0CB }, { "gold", 0xFFD700 }, { "indigo", 0x4B0082 },
    { "violet", 0xEE82EE }, { "crimson", 0xDC143C }, { "tomato", 0xFF6347 }, { "coral", 0xFF7F50 },
    { "darkred", 0x8B0000 }, { "darkblue", 0x00008B }, { "darkgreen", 0x006400 }, { "steelblue", 0x4682B4 },
    { "royalblue", 0x4169E1 }, { "dodgerblue", 0x1E90FF }, { "firebrick", 0xB22222 }, { "orangered", 0xFF4500 },
    { "slategray", 0x708090 }, { "slategrey", 0x708090 }, { "darkslategray", 0x2F4F4F }, { "whitesmoke", 0xF5F5F5 },
    { "midnightblue", 0x191970 }, { "forestgreen", 0x228B22 }, { "seagreen", 0x2E8B57 }, { "chocolate", 0xD2691E },
};

static i32 hexval(u8 c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = low(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* A number with an optional fraction and unit: its value times 100,
 * false when there is none. Advances at. */
static bool number(const u8 *s, u32 n, u32 *at, i64 *out, bool *percent)
{
    u32 i = *at;
    bool neg = false, any = false;
    i64 v = 0;
    if (i < n && (s[i] == '-' || s[i] == '+')) { neg = s[i] == '-'; i++; }
    while (i < n && s[i] >= '0' && s[i] <= '9') { if (v < (1LL << 40)) v = v * 10 + (s[i] - '0'); i++; any = true; }
    v *= 100;
    if (i < n && s[i] == '.') {
        i++;
        i64 scale = 10;
        while (i < n && s[i] >= '0' && s[i] <= '9') { if (scale <= 100) v += (s[i] - '0') * scale; scale /= 10; i++; any = true; }
    }
    if (!any) return false;
    *percent = i < n && s[i] == '%';
    if (*percent) i++;
    *at = i;
    *out = neg ? -v : v;
    return true;
}

/* rgb(...) / rgba(...): the channels as integers or percentages,
 * separated by commas or spaces; an alpha of zero is no colour. */
static bool rgb_function(const u8 *s, u32 n, u32 *rgb)
{
    u32 at = 0;
    while (at < n && s[at] != '(') at++;
    if (at >= n) return false;
    at++;
    u32 ch[3];
    for (u32 c = 0; c < 3; c++) {
        while (at < n && (ws(s[at]) || s[at] == ',')) at++;
        i64 v; bool pc;
        if (!number(s, n, &at, &v, &pc)) return false;
        if (pc) v = v * 255 / 100;
        v /= 100;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        ch[c] = (u32)v;
    }
    while (at < n && (ws(s[at]) || s[at] == ',' || s[at] == '/')) at++;
    if (at < n && s[at] != ')') {
        i64 a; bool pc;
        if (number(s, n, &at, &a, &pc) && a == 0) return false;
    }
    *rgb = (ch[0] << 16) | (ch[1] << 8) | ch[2];
    return true;
}

static bool parse_color(const u8 *s, u32 n, u32 *rgb)
{
    while (n && ws(s[0])) { s++; n--; }
    while (n && ws(s[n - 1])) n--;
    if (n == 0) return false;
    if (s[0] == '#') {
        u32 digits = 0;
        while (1 + digits < n && hexval(s[1 + digits]) >= 0) digits++;
        if (1 + digits != n) return false;
        u32 v = 0;
        if (digits == 3 || digits == 4) {
            for (u32 i = 0; i < 3; i++) { u32 d = (u32)hexval(s[1 + i]); v = (v << 8) | (d << 4) | d; }
            if (digits == 4 && hexval(s[4]) == 0) return false;
        } else if (digits == 6 || digits == 8) {
            for (u32 i = 0; i < 6; i++) v = (v << 4) | (u32)hexval(s[1 + i]);
            if (digits == 8 && hexval(s[7]) == 0 && hexval(s[8]) == 0) return false;
        } else return false;
        *rgb = v;
        return true;
    }
    u32 wl = ident_len(s, n);
    if (wl >= 3 && low(s[0]) == 'r' && low(s[1]) == 'g' && low(s[2]) == 'b' && (wl == 3 || (wl == 4 && low(s[3]) == 'a')))
        return rgb_function(s, n, rgb);
    if (wl != n) return false;
    for (u32 i = 0; i < sizeof(NAMED) / sizeof(NAMED[0]); i++)
        if (word_is(s, n, NAMED[i].name)) { *rgb = NAMED[i].rgb; return true; }
    return false;
}

/* ------------------------------------------------------------------ */
/* Declarations                                                        */
/* ------------------------------------------------------------------ */

/* The end of a quoted string starting at s[at] (the quote itself),
 * just past the closing quote. */
static u64 string_end(const u8 *s, u64 n, u64 at)
{
    u8 q = s[at++];
    while (at < n && s[at] != q) {
        if (s[at] == '\\') { at += 2; continue; }     /* the escaped char, even a quote or newline */
        if (s[at] == '\n') break;
        at++;
    }
    return at < n ? at + 1 : n;
}

/* Just past the block whose '{' stands at s[at]; strings and nested
 * blocks skipped. */
static u64 block_end(const u8 *s, u64 n, u64 at)
{
    u32 depth = 0;
    while (at < n) {
        u8 c = s[at];
        if (c == '"' || c == '\'') { at = string_end(s, n, at); continue; }
        if (c == '/' && at + 1 < n && s[at + 1] == '*') {
            at += 2;
            while (at + 1 < n && !(s[at] == '*' && s[at + 1] == '/')) at++;
            at = at + 2 <= n ? at + 2 : n;
            continue;
        }
        if (c == '{') depth++;
        if (c == '}') { if (depth <= 1) return at + 1; depth--; }
        at++;
    }
    return n;
}

/* The first word of a value: where it starts and how long. */
static u32 first_word(const u8 *v, u32 n, u32 *start)
{
    u32 at = 0;
    while (at < n && ws(v[at])) at++;
    *start = at;
    u32 l = 0;
    while (at + l < n && !ws(v[at + l]) && v[at + l] != ',' && v[at + l] != ';') l++;
    return l;
}

static bool value_has_word(const u8 *v, u32 n, const char *w)
{
    u32 at = 0;
    while (at < n) {
        while (at < n && (ws(v[at]) || v[at] == ',')) at++;
        u32 l = 0;
        while (at + l < n && !ws(v[at + l]) && v[at + l] != ',') l++;
        if (l && word_is(v + at, l, w)) return true;
        at += l ? l : 1;
    }
    return false;
}

/* A value that leans on something this reader does not carry. */
static bool value_unknown(const u8 *v, u32 n)
{
    for (u32 i = 0; i + 3 < n; i++)
        if (low(v[i]) == 'v' && low(v[i + 1]) == 'a' && low(v[i + 2]) == 'r' && v[i + 3] == '(') return true;
    return value_has_word(v, n, "inherit") || value_has_word(v, n, "initial") ||
           value_has_word(v, n, "unset") || value_has_word(v, n, "revert") || value_has_word(v, n, "revert-layer");
}

static void set_prop(css_decl *d, u8 bit, bool imp)
{
    d->set |= bit;
    if (imp) d->important |= bit; else d->important &= (u8)~bit;
}

static void declaration(css_decl *d, const u8 *name, u32 nl, const u8 *v, u32 vl, bool imp)
{
    if (value_unknown(v, vl)) return;
    u32 ws0; u32 wl = first_word(v, vl, &ws0);
    const u8 *w = v + ws0;
    if (wl == 0) return;

    if (word_is(name, nl, "display")) {
        u8 disp;
        if (word_is(w, wl, "none")) disp = CSS_DISPLAY_NONE;
        else if (word_is(w, wl, "inline") || word_is(w, wl, "inline-block") || word_is(w, wl, "inline-flex") ||
                 word_is(w, wl, "inline-grid") || word_is(w, wl, "inline-table") || word_is(w, wl, "contents"))
            disp = CSS_DISPLAY_INLINE;
        else disp = CSS_DISPLAY_BLOCK;
        d->display = disp;
        set_prop(d, CSS_SET_DISPLAY, imp);
    } else if (word_is(name, nl, "visibility")) {
        if (word_is(w, wl, "hidden") || word_is(w, wl, "collapse")) d->hidden = 1;
        else if (word_is(w, wl, "visible")) d->hidden = 0;
        else return;
        set_prop(d, CSS_SET_VISIBILITY, imp);
    } else if (word_is(name, nl, "font-weight")) {
        if (word_is(w, wl, "bold") || word_is(w, wl, "bolder")) d->bold = 1;
        else if (word_is(w, wl, "normal") || word_is(w, wl, "lighter")) d->bold = 0;
        else if (w[0] >= '0' && w[0] <= '9') {
            u32 at = 0; i64 num; bool pc;
            if (!number(w, wl, &at, &num, &pc)) return;
            d->bold = num >= 60000;
        } else return;
        set_prop(d, CSS_SET_WEIGHT, imp);
    } else if (word_is(name, nl, "font")) {
        /* the shorthand: only a bold in it is taken */
        u32 at = 0;
        while (at < vl) {
            while (at < vl && (ws(v[at]) || v[at] == ',')) at++;
            u32 l = 0;
            while (at + l < vl && !ws(v[at + l]) && v[at + l] != ',' && v[at + l] != '/') l++;
            if (!l) { at++; continue; }
            if (word_is(v + at, l, "bold") || word_is(v + at, l, "bolder")) { d->bold = 1; set_prop(d, CSS_SET_WEIGHT, imp); return; }
            if (v[at] >= '1' && v[at] <= '9') {
                u32 k = 0; i64 num; bool pc;
                if (number(v + at, l, &k, &num, &pc) && k == l && num >= 60000 && num <= 90000) {
                    d->bold = 1; set_prop(d, CSS_SET_WEIGHT, imp); return;
                }
                return;                                   /* the size: past the weight */
            }
            at += l;
        }
    } else if (word_is(name, nl, "color")) {
        u32 rgb;
        if (!parse_color(v, vl, &rgb)) return;
        d->color = rgb;
        set_prop(d, CSS_SET_COLOR, imp);
    } else if (word_is(name, nl, "text-align")) {
        if (word_is(w, wl, "center")) d->align = CSS_ALIGN_CENTER;
        else if (word_is(w, wl, "right") || word_is(w, wl, "end")) d->align = CSS_ALIGN_RIGHT;
        else if (word_is(w, wl, "left") || word_is(w, wl, "start") || word_is(w, wl, "justify")) d->align = CSS_ALIGN_LEFT;
        else return;
        set_prop(d, CSS_SET_ALIGN, imp);
    } else if (word_is(name, nl, "list-style") || word_is(name, nl, "list-style-type")) {
        if (value_has_word(v, vl, "none")) d->list_none = 1;
        else if (value_has_word(v, vl, "disc") || value_has_word(v, vl, "circle") || value_has_word(v, vl, "square") ||
                 value_has_word(v, vl, "decimal") || value_has_word(v, vl, "lower-alpha") || value_has_word(v, vl, "upper-alpha") ||
                 value_has_word(v, vl, "lower-roman") || value_has_word(v, vl, "upper-roman") || value_has_word(v, vl, "lower-latin") ||
                 value_has_word(v, vl, "upper-latin"))
            d->list_none = 0;
        else return;
        set_prop(d, CSS_SET_LIST, imp);
    }
}

void css_declarations(const u8 *t, u64 n, css_decl *d)
{
    d->set = d->important = 0;
    d->display = d->hidden = d->bold = d->align = d->list_none = d->pad = 0;
    d->color = 0;
    u64 at = 0;
    while (at < n) {
        u8 c = t[at];
        if (ws(c) || c == ';' || c == '}') { at++; continue; }
        if (c == '/' && at + 1 < n && t[at + 1] == '*') {
            at += 2;
            while (at + 1 < n && !(t[at] == '*' && t[at + 1] == '/')) at++;
            at = at + 2 <= n ? at + 2 : n;
            continue;
        }
        if (c == '{') { at = block_end(t, n, at); continue; }      /* a nested rule */
        if (c == '"' || c == '\'') { at = string_end(t, n, at); continue; }

        u64 ns = at;
        while (at < n && t[at] != ':' && t[at] != ';' && t[at] != '{' && t[at] != '}') at++;
        if (at >= n || t[at] != ':') continue;
        u64 ne = at;
        while (ne > ns && ws(t[ne - 1])) ne--;
        at++;
        u64 vs = at;
        while (at < n && t[at] != ';' && t[at] != '}' && t[at] != '{') {
            if (t[at] == '"' || t[at] == '\'') { at = string_end(t, n, at); continue; }
            if (t[at] == '(') { while (at < n && t[at] != ')') { if (t[at] == '"' || t[at] == '\'') at = string_end(t, n, at) - 1; at++; } }
            if (at < n) at++;
        }
        u64 ve = at;
        while (ve > vs && ws(t[ve - 1])) ve--;

        /* !important at the end */
        bool imp = false;
        {
            u64 k = ve;
            while (k > vs && ws(t[k - 1])) k--;
            if (k >= vs + 9 && word_is(t + k - 9, 9, "important")) {
                u64 j = k - 9;
                while (j > vs && ws(t[j - 1])) j--;
                if (j > vs && t[j - 1] == '!') { imp = true; ve = j - 1; while (ve > vs && ws(t[ve - 1])) ve--; }
            }
        }
        if (ne > ns && ve > vs && ne - ns < 64 && ve - vs < 4096)
            declaration(d, t + ns, (u32)(ne - ns), t + vs, (u32)(ve - vs), imp);
    }
}

/* ------------------------------------------------------------------ */
/* Media queries                                                       */
/* ------------------------------------------------------------------ */

/* A length in pixels times 100: px as given, em and rem at 16 px. */
static bool length(const u8 *s, u32 n, u32 *at, i64 *px)
{
    i64 v; bool pc;
    if (!number(s, n, at, &v, &pc)) return false;
    u32 ul = ident_len(s + *at, n - *at);
    if (word_is(s + *at, ul, "em") || word_is(s + *at, ul, "rem")) v *= 16;
    *at += ul;
    *px = v;
    return true;
}

static bool compare(i64 have, const u8 *op, u32 ol, i64 want)
{
    if (ol == 1 && op[0] == '<') return have < want;
    if (ol == 1 && op[0] == '>') return have > want;
    if (ol == 2 && op[0] == '<' && op[1] == '=') return have <= want;
    if (ol == 2 && op[0] == '>' && op[1] == '=') return have >= want;
    if (ol == 1 && op[0] == '=') return have == want;
    return false;
}

/* One feature between parentheses. */
static bool feature(const u8 *f, u32 n, u32 width, u32 height)
{
    u32 at = 0;
    while (at < n && ws(f[at])) at++;
    while (n > at && ws(f[n - 1])) n--;
    if (at >= n) return false;

    /* the range form: 400px <= width <= 800px */
    if (f[at] >= '0' && f[at] <= '9') {
        i64 lo; bool ok = true;
        if (!length(f, n, &at, &lo)) return false;
        while (at < n && ws(f[at])) at++;
        u32 os = at; while (at < n && (f[at] == '<' || f[at] == '>' || f[at] == '=')) at++;
        u32 ol = at - os;
        while (at < n && ws(f[at])) at++;
        u32 nl = ident_len(f + at, n - at);
        i64 have = word_is(f + at, nl, "width") ? (i64)width * 100 : word_is(f + at, nl, "height") ? (i64)height * 100 : -1;
        if (have < 0) return false;
        at += nl;
        ok = compare(lo, f + os, ol, have);
        while (at < n && ws(f[at])) at++;
        if (at < n) {
            os = at; while (at < n && (f[at] == '<' || f[at] == '>' || f[at] == '=')) at++;
            ol = at - os;
            while (at < n && ws(f[at])) at++;
            i64 hi;
            if (!length(f, n, &at, &hi)) return false;
            ok = ok && compare(have, f + os, ol, hi);
        }
        return ok;
    }

    u32 nl = ident_len(f + at, n - at);
    const u8 *name = f + at;
    at += nl;
    while (at < n && ws(f[at])) at++;
    u32 os = at;
    while (at < n && (f[at] == ':' || f[at] == '<' || f[at] == '>' || f[at] == '=')) at++;
    u32 ol = at - os;
    while (at < n && ws(f[at])) at++;
    const u8 *val = f + at;
    u32 vl = n > at ? n - at : 0;

    bool is_w = word_is(name, nl, "width") || word_is(name, nl, "device-width");
    bool is_h = word_is(name, nl, "height") || word_is(name, nl, "device-height");
    if (is_w || is_h) {
        i64 have = (i64)(is_w ? width : height) * 100, want; u32 k = 0;
        if (!length(val, vl, &k, &want)) return false;
        if (ol == 1 && f[os] == ':') return have == want;
        return compare(have, f + os, ol, want);
    }
    if (word_is(name, nl, "min-width") || word_is(name, nl, "min-device-width")) {
        i64 want; u32 k = 0;
        return length(val, vl, &k, &want) && (i64)width * 100 >= want;
    }
    if (word_is(name, nl, "max-width") || word_is(name, nl, "max-device-width")) {
        i64 want; u32 k = 0;
        return length(val, vl, &k, &want) && (i64)width * 100 <= want;
    }
    if (word_is(name, nl, "min-height")) {
        i64 want; u32 k = 0;
        return length(val, vl, &k, &want) && (i64)height * 100 >= want;
    }
    if (word_is(name, nl, "max-height")) {
        i64 want; u32 k = 0;
        return length(val, vl, &k, &want) && (i64)height * 100 <= want;
    }
    if (word_is(name, nl, "orientation"))
        return word_is(val, ident_len(val, vl), width >= height ? "landscape" : "portrait");
    if (word_is(name, nl, "hover") || word_is(name, nl, "any-hover"))
        return word_is(val, ident_len(val, vl), "hover");
    if (word_is(name, nl, "pointer") || word_is(name, nl, "any-pointer"))
        return word_is(val, ident_len(val, vl), "fine");
    if (word_is(name, nl, "prefers-color-scheme"))
        return word_is(val, ident_len(val, vl), "light");
    if (word_is(name, nl, "prefers-reduced-motion") || word_is(name, nl, "prefers-reduced-transparency") ||
        word_is(name, nl, "prefers-contrast") || word_is(name, nl, "forced-colors") || word_is(name, nl, "inverted-colors"))
        return word_is(val, ident_len(val, vl), "no-preference") || word_is(val, ident_len(val, vl), "none");
    if (word_is(name, nl, "display-mode")) return word_is(val, ident_len(val, vl), "browser");
    if (word_is(name, nl, "scripting")) return word_is(val, ident_len(val, vl), "none");
    if (word_is(name, nl, "color") || word_is(name, nl, "min-color") || word_is(name, nl, "color-gamut")) return true;
    if (word_is(name, nl, "monochrome")) return false;
    if (word_is(name, nl, "grid")) return vl == 0 || val[0] == '0';
    if (word_is(name, nl, "resolution") || word_is(name, nl, "min-resolution") || word_is(name, nl, "max-resolution") ||
        word_is(name, nl, "-webkit-min-device-pixel-ratio") || word_is(name, nl, "-webkit-max-device-pixel-ratio") ||
        word_is(name, nl, "-webkit-device-pixel-ratio")) {
        i64 v; bool pc; u32 k = 0;
        if (!number(val, vl, &k, &v, &pc)) return false;
        u32 ul = ident_len(val + k, vl - k);
        i64 have = 100;                                   /* one pixel per pixel */
        if (word_is(val + k, ul, "dpi")) have = 9600;
        else if (word_is(val + k, ul, "dpcm")) have = 3780;
        if (name[0] == 'm' && name[1] == 'i') return have >= v;
        if (name[0] == 'm' && name[1] == 'a') return have <= v;
        if (name[0] == '-') return word_is(name, nl, "-webkit-min-device-pixel-ratio") ? have >= v :
                                   word_is(name, nl, "-webkit-max-device-pixel-ratio") ? have <= v : have == v;
        return have == v;
    }
    return false;
}

bool css_media(const u8 *q, u32 n, u32 width)
{
    u32 height = width * 5 / 8;
    u32 at = 0;
    bool any = false, result = false;
    while (at < n) {
        bool neg = false, ok = true, seen = false;
        for (;;) {
            while (at < n && ws(q[at])) at++;
            if (at >= n || q[at] == ',') break;
            if (q[at] == '(') {
                u32 depth = 0, s = at;
                while (at < n) {
                    if (q[at] == '(') depth++;
                    if (q[at] == ')') { if (--depth == 0) break; }
                    at++;
                }
                u32 e = at < n ? at : n;
                if (at < n) at++;
                /* a nested group is one of and/or/not of features: read as its features anded */
                const u8 *inner = q + s + 1; u32 il = e > s + 1 ? e - s - 1 : 0;
                bool v;
                bool has_paren = false;
                for (u32 i = 0; i < il; i++) if (inner[i] == '(') has_paren = true;
                if (has_paren) v = css_media(inner, il, width);
                else v = feature(inner, il, width, height);
                ok = ok && v;
                seen = true;
                continue;
            }
            u32 wl = ident_len(q + at, n - at);
            if (!wl) { at++; ok = false; seen = true; continue; }
            const u8 *w = q + at;
            at += wl;
            if (word_is(w, wl, "not")) neg = !neg;
            else if (word_is(w, wl, "only") || word_is(w, wl, "and") || word_is(w, wl, "or")) { }
            else if (word_is(w, wl, "screen") || word_is(w, wl, "all")) seen = true;
            else { ok = false; seen = true; }
        }
        bool v = seen ? (neg ? !ok : ok) : true;
        result = result || v;
        any = true;
        if (at < n && q[at] == ',') at++;
    }
    return any ? result : true;
}

/* ------------------------------------------------------------------ */
/* Selectors                                                           */
/* ------------------------------------------------------------------ */

static u32 key_of(const css_part *p)
{
    if (p->id) return p->id;
    if (p->ncls) return p->cls[0];
    if (p->tag) return p->tag;
    return 0;
}

static u32 bucket(u32 key) { return (key * 2654435761u) >> 22; }

static void add_rule(css_sheet *s, css_rule *r)
{
    if (s->count >= CSS_RULES_MAX) { s->dropped++; return; }
    r->order = s->order++;
    u32 b = bucket(key_of(&r->part[0]));
    r->next = s->head[b];
    s->head[b] = s->count;
    s->rule[s->count++] = *r;
}

/* One selector, without commas. False when it is one this reader
 * cannot match, which is the same as never matching. */
static bool parse_selector(const u8 *s, u32 n, css_rule *r)
{
    css_part tmp[CSS_PARTS_MAX];
    u8 comb[CSS_PARTS_MAX];
    u32 np = 0, at = 0, a = 0, b = 0, c = 0;

    while (at < n && ws(s[at])) at++;
    while (at < n) {
        css_part p;
        p.tag = p.id = p.attr = p.attr_val = 0;
        p.cls[0] = p.cls[1] = p.cls[2] = 0;
        p.ncls = 0; p.child = 0; p.pad[0] = p.pad[1] = 0;
        bool got = false;
        for (;;) {
            if (at >= n) break;
            u8 ch = s[at];
            if (ident_start(ch) && !got) {
                u32 l = ident_len(s + at, n - at);
                p.tag = hash_folded(s + at, l);
                at += l; c++; got = true;
            } else if (ch == '*' && !got) {
                at++; got = true;
            } else if (ch == '.') {
                at++;
                u32 l = ident_len(s + at, n - at);
                if (!l || p.ncls >= CSS_CLASSES_MAX) return false;
                p.cls[p.ncls++] = css_hash_n(s + at, l);
                at += l; b++; got = true;
            } else if (ch == '#') {
                at++;
                u32 l = ident_len(s + at, n - at);
                if (!l) return false;
                p.id = css_hash_n(s + at, l);
                at += l; a++; got = true;
            } else if (ch == '[') {
                at++;
                while (at < n && ws(s[at])) at++;
                u32 l = ident_len(s + at, n - at);
                if (!l) return false;
                p.attr = hash_folded(s + at, l);
                at += l;
                while (at < n && ws(s[at])) at++;
                if (at >= n) return false;
                if (s[at] == '=') {
                    at++;
                    while (at < n && ws(s[at])) at++;
                    u32 vs, vl;
                    if (at < n && (s[at] == '"' || s[at] == '\'')) {
                        u8 q = s[at++];
                        vs = at;
                        while (at < n && s[at] != q) at++;
                        vl = at - vs;
                        if (at < n) at++;
                    } else {
                        vs = at; vl = ident_len(s + at, n - at); at += vl;
                    }
                    p.attr_val = css_hash_n(s + vs, vl);
                    while (at < n && ws(s[at])) at++;
                    if (at < n && (s[at] == 'i' || s[at] == 'I' || s[at] == 's' || s[at] == 'S')) { at++; while (at < n && ws(s[at])) at++; }
                }
                if (at >= n || s[at] != ']') return false;
                at++; b++; got = true;
            } else break;
        }
        if (!got || np >= CSS_PARTS_MAX) return false;
        tmp[np++] = p;

        bool space = false;
        while (at < n && ws(s[at])) { at++; space = true; }
        if (at >= n) break;
        if (s[at] == '>') { at++; comb[np - 1] = 1; while (at < n && ws(s[at])) at++; }
        else if (space && (ident_start(s[at]) || s[at] == '.' || s[at] == '#' || s[at] == '[' || s[at] == '*')) comb[np - 1] = 0;
        else return false;                                /* + ~ : :: | or something else */
    }
    if (np == 0) return false;

    for (u32 i = 0; i < np; i++) {
        r->part[i] = tmp[np - 1 - i];
        r->part[i].child = (i + 1 < np) ? comb[np - 2 - i] : 0;
    }
    r->nparts = (u8)np;
    if (a > 15) a = 15;
    if (b > 15) b = 15;
    if (c > 15) c = 15;
    r->spec = (u16)((a << 8) | (b << 4) | c);
    return true;
}

/* The selector list before a block: one rule per selector that reads. */
static void add_selectors(css_sheet *s, const u8 *sel, u64 n, const css_decl *d)
{
    u64 at = 0;
    while (at < n) {
        u64 start = at;
        u32 depth = 0;
        while (at < n && !(sel[at] == ',' && depth == 0)) {
            if (sel[at] == '(' || sel[at] == '[') depth++;
            else if ((sel[at] == ')' || sel[at] == ']') && depth) depth--;
            else if (sel[at] == '"' || sel[at] == '\'') { at = string_end(sel, n, at); continue; }
            at++;
        }
        u64 end = at;
        if (at < n) at++;
        while (start < end && ws(sel[start])) start++;
        while (end > start && ws(sel[end - 1])) end--;
        if (end <= start || end - start > 1024) continue;
        css_rule r;
        r.nparts = 0; r.pad = 0; r.spec = 0; r.order = 0; r.next = NONE;
        r.d = *d;
        if (parse_selector(sel + start, (u32)(end - start), &r)) add_rule(s, &r);
    }
}

/* ------------------------------------------------------------------ */
/* The sheet                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    const u8 *s;
    u64 n, at;
    css_sheet *sheet;
    u32 depth;
} reader;

static void skip_space(reader *r)
{
    for (;;) {
        while (r->at < r->n && ws(r->s[r->at])) r->at++;
        if (r->at + 1 < r->n && r->s[r->at] == '/' && r->s[r->at + 1] == '*') {
            r->at += 2;
            while (r->at + 1 < r->n && !(r->s[r->at] == '*' && r->s[r->at + 1] == '/')) r->at++;
            r->at = r->at + 2 <= r->n ? r->at + 2 : r->n;
            continue;
        }
        break;
    }
}

static void parse_rules(reader *r, bool nested);

static void at_rule(reader *r)
{
    r->at++;                                              /* the '@' */
    u32 nl = ident_len(r->s + r->at, (u32)(r->n - r->at > 64 ? 64 : r->n - r->at));
    const u8 *name = r->s + r->at;
    r->at += nl;

    /* the prelude, up to '{' or ';' */
    u64 ps = r->at;
    while (r->at < r->n && r->s[r->at] != '{' && r->s[r->at] != ';') {
        if (r->s[r->at] == '"' || r->s[r->at] == '\'') { r->at = string_end(r->s, r->n, r->at); continue; }
        r->at++;
    }
    u64 pe = r->at;
    if (r->at >= r->n) return;
    if (r->s[r->at] == ';') { r->at++; return; }          /* @import, @charset, @namespace, @layer a, b; */

    bool descend = false;
    if (word_is(name, nl, "media")) descend = css_media(r->s + ps, (u32)(pe - ps), r->sheet->width);
    else if (word_is(name, nl, "supports") || word_is(name, nl, "layer")) descend = true;

    if (descend && r->depth < 8) {
        r->at++;                                          /* the '{' */
        r->depth++;
        parse_rules(r, true);
        r->depth--;
    } else {
        r->at = block_end(r->s, r->n, r->at);
    }
}

static void parse_rules(reader *r, bool nested)
{
    for (;;) {
        skip_space(r);
        if (r->at >= r->n) return;
        u8 c = r->s[r->at];
        if (c == '}') { r->at++; if (nested) return; continue; }
        if (c == '@') { at_rule(r); continue; }

        u64 ss = r->at;
        while (r->at < r->n && r->s[r->at] != '{' && r->s[r->at] != '}' && r->s[r->at] != ';') {
            if (r->s[r->at] == '"' || r->s[r->at] == '\'') { r->at = string_end(r->s, r->n, r->at); continue; }
            r->at++;
        }
        if (r->at >= r->n) return;
        if (r->s[r->at] != '{') { r->at++; continue; }
        u64 se = r->at;
        u64 ds = r->at + 1;
        u64 de = block_end(r->s, r->n, r->at);
        r->at = de;
        u64 dl = de > ds + 1 ? de - ds - 1 : 0;
        css_decl d;
        css_declarations(r->s + ds, dl, &d);
        if (d.set) add_selectors(r->sheet, r->s + ss, se - ss, &d);
    }
}

void css_reset(css_sheet *s, u32 width)
{
    s->count = 0;
    s->order = 0;
    s->dropped = 0;
    s->width = width;
    for (u32 i = 0; i < CSS_BUCKETS; i++) s->head[i] = NONE;
}

void css_parse(css_sheet *s, const u8 *text, u64 len)
{
    reader r;
    r.s = text; r.n = len; r.at = 0; r.sheet = s; r.depth = 0;
    if (len >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF) r.at = 3;
    parse_rules(&r, false);
}

/* ------------------------------------------------------------------ */
/* Matching                                                            */
/* ------------------------------------------------------------------ */

static bool part_fits(const css_part *p, const css_elem *e)
{
    if (p->tag && p->tag != e->tag) return false;
    if (p->id && p->id != e->id) return false;
    for (u32 i = 0; i < p->ncls; i++) {
        bool found = false;
        for (u32 k = 0; k < e->ncls && !found; k++) found = e->cls[k] == p->cls[i];
        if (!found) return false;
    }
    if (p->attr) {
        bool found = false;
        for (u32 k = 0; k < e->nattr && !found; k++)
            found = e->attr[k] == p->attr && (!p->attr_val || e->attr_val[k] == p->attr_val);
        if (!found) return false;
    }
    return true;
}

static bool rule_matches(const css_rule *r, const css_elem *chain, u32 depth)
{
    if (!part_fits(&r->part[0], &chain[depth - 1])) return false;
    i32 level = (i32)depth - 1;
    for (u32 i = 1; i < r->nparts; i++) {
        i32 k = level - 1;
        if (r->part[i - 1].child) {
            if (k < 0 || !part_fits(&r->part[i], &chain[k])) return false;
        } else {
            while (k >= 0 && !part_fits(&r->part[i], &chain[k])) k--;
            if (k < 0) return false;
        }
        level = k;
    }
    return true;
}

static void take(css_decl *out, const css_decl *d, u8 bit)
{
    out->set |= bit;
    switch (bit) {
    case CSS_SET_DISPLAY:    out->display = d->display; break;
    case CSS_SET_VISIBILITY: out->hidden = d->hidden; break;
    case CSS_SET_WEIGHT:     out->bold = d->bold; break;
    case CSS_SET_COLOR:      out->color = d->color; break;
    case CSS_SET_ALIGN:      out->align = d->align; break;
    case CSS_SET_LIST:       out->list_none = d->list_none; break;
    default: break;
    }
}

void css_match(const css_sheet *s, const css_elem *chain, u32 depth, css_decl *out)
{
    out->set = out->important = 0;
    out->display = out->hidden = out->bold = out->align = out->list_none = out->pad = 0;
    out->color = 0;
    if (!depth) return;
    const css_elem *e = &chain[depth - 1];

    u16 best_spec[CSS_PROPS];
    u32 best_order[CSS_PROPS];
    u8  best_imp[CSS_PROPS];
    for (u32 p = 0; p < CSS_PROPS; p++) { best_spec[p] = 0; best_order[p] = 0; best_imp[p] = 0; }

    u32 keys[3 + CSS_ELEM_CLASSES];
    u32 nk = 0;
    if (e->id) keys[nk++] = e->id;
    for (u32 i = 0; i < e->ncls && i < CSS_ELEM_CLASSES; i++) keys[nk++] = e->cls[i];
    keys[nk++] = e->tag;
    keys[nk++] = 0;

    for (u32 ki = 0; ki < nk; ki++) {
        u32 key = keys[ki];
        for (u32 ri = s->head[bucket(key)]; ri != NONE && ri < s->count; ri = s->rule[ri].next) {
            const css_rule *r = &s->rule[ri];
            if (key_of(&r->part[0]) != key) continue;
            if (!rule_matches(r, chain, depth)) continue;
            for (u32 p = 0; p < CSS_PROPS; p++) {
                u8 bit = (u8)(1u << p);
                if (!(r->d.set & bit)) continue;
                u8 imp = (r->d.important & bit) ? 1 : 0;
                bool wins = !(out->set & bit) ||
                            imp > best_imp[p] ||
                            (imp == best_imp[p] && (r->spec > best_spec[p] ||
                                                    (r->spec == best_spec[p] && r->order >= best_order[p])));
                if (!wins) continue;
                take(out, &r->d, bit);
                best_imp[p] = imp; best_spec[p] = r->spec; best_order[p] = r->order;
            }
        }
    }
}
