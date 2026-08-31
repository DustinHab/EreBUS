/*
 * fb.c -- Bildpuffer und Bildschirmkonsole.
 *
 * Alles zeichnet direkt in den von der Firmware gemeldeten linearen
 * Puffer. Ein richtiger Compositor mit Doppelpufferung kommt spaeter;
 * fuer den Systemstart genuegt der direkte Weg, und er hat den Vorteil,
 * dass man einen Absturz noch auf dem Bildschirm sieht.
 */
#include <eb/fb.h>
#include <eb/fmt.h>
#include "font8x16.h"

static struct {
    u32 *base;
    u32  width, height;
    u32  stride;     /* Pixel pro Zeile, kann groesser als width sein */
    bool rgb;        /* true: RGBX statt des ueblichen BGRX */
    bool ready;
} fb;

void fb_init(const eb_boot_info *bi)
{
    fb.base   = (u32 *)(virt_addr)bi->fb_base;
    fb.width  = bi->fb_width;
    fb.height = bi->fb_height;
    fb.stride = bi->fb_stride;
    fb.rgb    = (bi->fb_format == EB_FB_RGBX8888);
    fb.ready  = (fb.base != NULL && fb.width > 0 && fb.height > 0);
}

u32 fb_width(void)  { return fb.width; }
u32 fb_height(void) { return fb.height; }

/* Kernfarben sind 0x00RRGGBB. Auf einem BGRX-Puffer landen die Bytes im
 * Speicher als B,G,R,X -- was auf einer Little-Endian-Maschine genau der
 * unveraenderten Zahl entspricht. Nur bei RGBX muss getauscht werden. */
static inline u32 to_native(color c)
{
    if (!fb.rgb) return (u32)c;
    return ((c & 0x0000FFu) << 16) | (c & 0x00FF00u) | ((c >> 16) & 0xFFu);
}

void fb_pixel(i32 x, i32 y, color c)
{
    if (!fb.ready) return;
    if (x < 0 || y < 0 || (u32)x >= fb.width || (u32)y >= fb.height) return;
    fb.base[(u32)y * fb.stride + (u32)x] = to_native(c);
}

void fb_rect(i32 x, i32 y, i32 w, i32 h, color c)
{
    if (!fb.ready || w <= 0 || h <= 0) return;

    /* Auf den sichtbaren Bereich zurechtschneiden, damit die innere
     * Schleife ohne Pruefung pro Pixel auskommt. */
    i32 x0 = x < 0 ? 0 : x;
    i32 y0 = y < 0 ? 0 : y;
    i32 x1 = x + w, y1 = y + h;
    if (x1 > (i32)fb.width)  x1 = (i32)fb.width;
    if (y1 > (i32)fb.height) y1 = (i32)fb.height;
    if (x0 >= x1 || y0 >= y1) return;

    u32 native = to_native(c);
    for (i32 yy = y0; yy < y1; yy++) {
        u32 *row = fb.base + (u32)yy * fb.stride;
        for (i32 xx = x0; xx < x1; xx++) row[xx] = native;
    }
}

void fb_frame(i32 x, i32 y, i32 w, i32 h, i32 t, color c)
{
    if (t <= 0) return;
    fb_rect(x, y, w, t, c);                 /* oben  */
    fb_rect(x, y + h - t, w, t, c);         /* unten */
    fb_rect(x, y + t, t, h - 2 * t, c);     /* links */
    fb_rect(x + w - t, y + t, t, h - 2 * t, c); /* rechts */
}

void fb_clear(color c)
{
    fb_rect(0, 0, (i32)fb.width, (i32)fb.height, c);
}

void fb_gradient(i32 x, i32 y, i32 w, i32 h, color top, color bottom)
{
    if (h <= 0) return;
    i32 r0 = (top >> 16) & 0xFF, g0 = (top >> 8) & 0xFF, b0 = top & 0xFF;
    i32 r1 = (bottom >> 16) & 0xFF, g1 = (bottom >> 8) & 0xFF, b1 = bottom & 0xFF;

    for (i32 i = 0; i < h; i++) {
        /* Ganzzahlige Interpolation: kein Fliesskomma im Kernel. */
        i32 r = r0 + (r1 - r0) * i / (h - 1 ? h - 1 : 1);
        i32 g = g0 + (g1 - g0) * i / (h - 1 ? h - 1 : 1);
        i32 b = b0 + (b1 - b0) * i / (h - 1 ? h - 1 : 1);
        fb_rect(x, y + i, w, 1, RGB(r, g, b));
    }
}

/* ------------------------------------------------------------------ */
/* Zeichensatz                                                         */
/* ------------------------------------------------------------------ */

static const u8 *glyph_for(u32 cp)
{
    for (u32 i = 0; i < FONT_RANGE_COUNT; i++) {
        if (cp >= font_ranges[i].first && cp <= font_ranges[i].last)
            return font_bitmap[font_ranges[i].offset +
                               (cp - font_ranges[i].first)];
    }
    return font_missing;
}

/* Zeichnet ein Zeichen, wahlweise vergroessert. Ein Bitmap-Zeichensatz
 * laesst sich nur ganzzahlig skalieren -- jedes Pixel wird zum Quadrat. */
static void draw_glyph_cp(i32 x, i32 y, u32 cp, color fg, color bg,
                          bool opaque, i32 scale)
{
    const u8 *g = glyph_for(cp);
    if (scale < 1) scale = 1;

    for (i32 row = 0; row < GLYPH_H; row++) {
        u8 bits = g[row];
        for (i32 col = 0; col < GLYPH_W; col++) {
            bool on = (bits & (0x80u >> col)) != 0;
            if (!on && !opaque) continue;
            color c = on ? fg : bg;
            if (scale == 1) fb_pixel(x + col, y + row, c);
            else fb_rect(x + col * scale, y + row * scale, scale, scale, c);
        }
    }
}

void fb_glyph(i32 x, i32 y, u8 ch, color fg, color bg, bool opaque)
{
    draw_glyph_cp(x, y, ch, fg, bg, opaque, 1);
}

/* Erwartet UTF-8 und decodiert im Vorbeigehen -- damit stehen Umlaute
 * und Rahmenzeichen genauso auf dem Schirm wie im Quelltext. */
void fb_text(i32 x, i32 y, const char *s, color fg, color bg, bool opaque)
{
    while (*s) {
        u8 c = (u8)*s++;
        u32 cp = c;
        u32 more = 0;

        if      ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; more = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; more = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; more = 3; }

        while (more-- && ((u8)*s & 0xC0) == 0x80)
            cp = (cp << 6) | ((u8)*s++ & 0x3Fu);

        draw_glyph_cp(x, y, cp, fg, bg, opaque, 1);
        x += GLYPH_W;
    }
}

/* Naechsten Codepunkt aus einer UTF-8-Kette holen und den Zeiger
 * weiterruecken. Fehlerhafte Folgen liefern das Rohbyte -- der Kernel
 * soll bei kaputtem Text weiterlaufen, nicht stehenbleiben. */
static u32 utf8_next(const char **p)
{
    u8 c = (u8)*(*p)++;
    u32 cp = c, more = 0;

    if      ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; more = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; more = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; more = 3; }

    while (more-- && ((u8)**p & 0xC0) == 0x80)
        cp = (cp << 6) | ((u8)*(*p)++ & 0x3Fu);

    return cp;
}

i32 fb_text_width(const char *s, i32 scale)
{
    i32 n = 0;
    while (*s) { utf8_next(&s); n++; }
    return n * GLYPH_W * (scale < 1 ? 1 : scale);
}

void fb_text_scaled(i32 x, i32 y, const char *s, color fg, i32 scale)
{
    if (scale < 1) scale = 1;
    if (scale == 1) { fb_text(x, y, s, fg, 0, false); return; }

    while (*s) {
        u32 cp = utf8_next(&s);
        draw_glyph_cp(x, y, cp, fg, 0, false, scale);
        x += GLYPH_W * scale;
    }
}

/* ------------------------------------------------------------------ */
/* Bildschirmkonsole                                                   */
/* ------------------------------------------------------------------ */

static struct {
    i32   x, y, w, h;     /* Textbereich in Pixeln */
    i32   col, row;       /* Schreibmarke in Zeichen */
    i32   cols, rows;
    i32   scale;          /* Vergroesserungsfaktor des Zeichensatzes */
    color fg, bg;
    /* Zwischenspeicher fuer die UTF-8-Zerlegung */
    u32   pending;
    u32   pending_left;
    bool  ready;
} con;

static void recalc(void)
{
    if (con.scale < 1) con.scale = 1;
    con.cols = con.w / (GLYPH_W * con.scale);
    con.rows = con.h / (GLYPH_H * con.scale);
    con.col = con.row = 0;
}

void fbcon_set_origin(i32 x, i32 y, i32 w, i32 h)
{
    con.x = x; con.y = y; con.w = w; con.h = h;
    recalc();
}

i32 fbcon_cols(void) { return con.cols; }
i32 fbcon_rows(void) { return con.rows; }

/* scale = 0 heisst: selbst entscheiden. Auf einem 4K-Panel waere ein
 * 8x16-Zeichensatz sonst nicht mehr lesbar. */
void fbcon_init(color fg, color bg, i32 scale)
{
    con.fg = fg;
    con.bg = bg;
    if (con.w == 0)
        fbcon_set_origin(0, 0, (i32)fb.width, (i32)fb.height);

    if (scale <= 0) {
        if      (fb.width >= 2400) scale = 3;
        else if (fb.width >= 1500) scale = 2;
        else                       scale = 1;
    }
    con.scale = scale;
    recalc();
    con.ready = fb.ready;
}

/* Alles um eine Zeile hochschieben. Das ist der teuerste Vorgang im
 * ganzen Startablauf -- spaeter uebernimmt das der Compositor. */
static void scroll(void)
{
    i32 line = GLYPH_H * con.scale;
    for (i32 yy = 0; yy < con.h - line; yy++) {
        u32 *dst = fb.base + (u32)(con.y + yy) * fb.stride + (u32)con.x;
        u32 *src = dst + (u32)line * fb.stride;
        for (i32 xx = 0; xx < con.w; xx++) dst[xx] = src[xx];
    }
    fb_rect(con.x, con.y + con.h - line, con.w, line, con.bg);
    con.row--;
}

static void put_cp(u32 cp)
{
    if (con.col >= con.cols) { con.col = 0; con.row++; }
    while (con.row >= con.rows) scroll();

    draw_glyph_cp(con.x + con.col * GLYPH_W * con.scale,
                  con.y + con.row * GLYPH_H * con.scale,
                  cp, con.fg, con.bg, true, con.scale);
    con.col++;
}

void fbcon_putc(char c)
{
    if (!con.ready) return;
    u8 b = (u8)c;

    /* Fortsetzungsbyte einer laufenden UTF-8-Folge? */
    if (con.pending_left && (b & 0xC0) == 0x80) {
        con.pending = (con.pending << 6) | (b & 0x3Fu);
        if (--con.pending_left == 0) put_cp(con.pending);
        return;
    }
    con.pending_left = 0;

    if (b == '\n') {
        con.col = 0;
        con.row++;
        while (con.row >= con.rows) scroll();
        return;
    }
    if (b == '\r') { con.col = 0; return; }
    if (b == '\t') { do { put_cp(' '); } while (con.col % 8); return; }

    if      ((b & 0xE0) == 0xC0) { con.pending = b & 0x1Fu; con.pending_left = 1; }
    else if ((b & 0xF0) == 0xE0) { con.pending = b & 0x0Fu; con.pending_left = 2; }
    else if ((b & 0xF8) == 0xF0) { con.pending = b & 0x07u; con.pending_left = 3; }
    else                          put_cp(b);
}
