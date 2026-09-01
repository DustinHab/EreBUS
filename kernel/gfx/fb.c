/*
 * fb.c -- framebuffer and screen console.
 *
 * Everything draws straight into the linear buffer the firmware handed
 * us. A proper compositor with double buffering comes later; for start-
 * up the direct route is enough, and it has the advantage that a crash
 * is still visible on screen.
 */
#include <eb/fb.h>
#include <eb/fmt.h>
#include <eb/mm.h>
#include "font8x16.h"

static struct {
    u32 *base;       /* where drawing goes: the back buffer once there is one */
    u32 *front;      /* the real framebuffer */
    u32  width, height;
    u32  stride;     /* pixels per scanline, may exceed width */
    bool rgb;        /* true: RGBX instead of the usual BGRX */
    bool ready;
    bool buffered;
} fb;

/* The region changed since the last present, as one bounding box.
 *
 * One box rather than a list of rectangles: a list tracks damage more
 * precisely, but every drawing operation then has to merge into it, and
 * the merging costs more than the pixels saved unless the changes are
 * genuinely far apart. A single box is wrong only in the sense of
 * copying some untouched pixels, which is cheap and never incorrect. */
static struct { i32 x0, y0, x1, y1; bool any; } dirty;

void fb_damage(i32 x, i32 y, i32 w, i32 h)
{
    if (!fb.buffered || w <= 0 || h <= 0) return;

    if (!dirty.any) {
        dirty.x0 = x; dirty.y0 = y;
        dirty.x1 = x + w; dirty.y1 = y + h;
        dirty.any = true;
        return;
    }
    if (x < dirty.x0) dirty.x0 = x;
    if (y < dirty.y0) dirty.y0 = y;
    if (x + w > dirty.x1) dirty.x1 = x + w;
    if (y + h > dirty.y1) dirty.y1 = y + h;
}

void fb_present(void)
{
    if (!fb.buffered || !dirty.any) return;

    i32 x0 = dirty.x0 < 0 ? 0 : dirty.x0;
    i32 y0 = dirty.y0 < 0 ? 0 : dirty.y0;
    i32 x1 = dirty.x1 > (i32)fb.width  ? (i32)fb.width  : dirty.x1;
    i32 y1 = dirty.y1 > (i32)fb.height ? (i32)fb.height : dirty.y1;
    dirty.any = false;

    for (i32 y = y0; y < y1; y++) {
        const u32 *src = fb.base  + (u32)y * fb.stride + (u32)x0;
        u32 *dst       = fb.front + (u32)y * fb.stride + (u32)x0;
        for (i32 x = 0; x < x1 - x0; x++) dst[x] = src[x];
    }
}

void fb_init(const eb_boot_info *bi)
{
    /* The framebuffer address from the firmware is physical; reach it
     * through the direct map like any other physical memory. */
    fb.front  = (u32 *)phys_to_virt(bi->fb_base);
    fb.base   = fb.front;          /* draw straight to the screen for now */
    fb.width  = bi->fb_width;
    fb.height = bi->fb_height;
    fb.stride = bi->fb_stride;
    fb.rgb    = (bi->fb_format == EB_FB_RGBX8888);
    fb.ready  = (fb.base != NULL && fb.width > 0 && fb.height > 0);
    fb.buffered = false;
}

u64 fb_backbuffer_bytes(void)
{
    return (u64)fb.stride * fb.height * sizeof(u32);
}

/* Moves drawing off the screen and into memory.
 *
 * The framebuffer is not ordinary memory: it lives across a bus, is
 * uncached, and a read from it is far slower than a write. Anything
 * that reads back -- scrolling a console, moving a window, drawing
 * something translucent -- pays for that on every pixel. Drawing into
 * plain RAM and copying the changed part across once turns all of it
 * into cache-speed work plus one linear write.
 *
 * It also removes tearing: the screen never shows a half-finished
 * frame, because a frame only reaches it when it is finished. */
void fb_enable_backbuffer(void *memory)
{
    if (!memory || !fb.ready) return;

    u32 *back = (u32 *)memory;
    const u32 *front = fb.front;

    /* Start from what is already on screen, so enabling this does not
     * blank whatever has been logged so far. */
    for (u32 i = 0; i < fb.stride * fb.height; i++) back[i] = front[i];

    fb.base = back;
    fb.buffered = true;
    dirty.any = false;
}

bool fb_is_buffered(void) { return fb.buffered; }

u32 fb_width(void)  { return fb.width; }
u32 fb_height(void) { return fb.height; }

/* Kernel colours are 0x00RRGGBB. On a BGRX buffer the bytes land in
 * memory as B,G,R,X -- which on a little-endian machine is exactly the
 * unchanged number. Only RGBX needs a swap. */
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
    fb_damage(x, y, 1, 1);
}

void fb_rect(i32 x, i32 y, i32 w, i32 h, color c)
{
    if (!fb.ready || w <= 0 || h <= 0) return;

    /* Clip to the visible area so the inner loop needs no per-pixel
     * check. */
    i32 x0 = x < 0 ? 0 : x;
    i32 y0 = y < 0 ? 0 : y;
    i32 x1 = x + w, y1 = y + h;
    if (x1 > (i32)fb.width)  x1 = (i32)fb.width;
    if (y1 > (i32)fb.height) y1 = (i32)fb.height;
    if (x0 >= x1 || y0 >= y1) return;

    fb_damage(x0, y0, x1 - x0, y1 - y0);

    u32 native = to_native(c);
    for (i32 yy = y0; yy < y1; yy++) {
        u32 *row = fb.base + (u32)yy * fb.stride;
        for (i32 xx = x0; xx < x1; xx++) row[xx] = native;
    }
}

void fb_frame(i32 x, i32 y, i32 w, i32 h, i32 t, color c)
{
    if (t <= 0) return;
    fb_rect(x, y, w, t, c);                     /* top    */
    fb_rect(x, y + h - t, w, t, c);             /* bottom */
    fb_rect(x, y + t, t, h - 2 * t, c);         /* left   */
    fb_rect(x + w - t, y + t, t, h - 2 * t, c); /* right  */
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
    i32 span = (h > 1) ? h - 1 : 1;

    for (i32 i = 0; i < h; i++) {
        /* Integer interpolation: no floating point in the kernel. */
        i32 r = r0 + (r1 - r0) * i / span;
        i32 g = g0 + (g1 - g0) * i / span;
        i32 b = b0 + (b1 - b0) * i / span;
        fb_rect(x, y + i, w, 1, RGB(r, g, b));
    }
}

/* The sixteen inks. Fixed, and deliberately not any theme's: a
 * picture keeps its colours whatever the shell is wearing. Ink 0 is
 * the paper itself. */
const color fb_inks[16] = {
    RGB(236, 233, 226), RGB( 30,  30,  33), RGB(122, 122, 126),
    RGB(188,  62,  52), RGB(214, 122,  52), RGB(222, 186,  72),
    RGB( 82, 160,  92), RGB( 62, 160, 150), RGB( 72, 112, 200),
    RGB(132,  92, 190), RGB(180,  82, 150), RGB(132,  92,  62),
    RGB(224, 162, 172), RGB(182, 182, 182), RGB( 52, 100,  62),
    RGB( 42,  62, 112),
};

/* ------------------------------------------------------------------ */
/* Font                                                                */
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

/* Draws one character, optionally enlarged. A bitmap font can only be
 * scaled by whole factors -- every pixel becomes a square.
 *
 * Two paths on purpose. The common case is a glyph that lies wholly
 * inside the screen, and there the destination pointer can be worked
 * out once per row and the pixels stored straight into the buffer. The
 * clipped case goes through fb_pixel and pays for a bounds check per
 * pixel, which only happens at the edges.
 *
 * This is worth the duplication: the console draws thousands of glyphs
 * during start-up, and the framebuffer is uncached memory across a bus,
 * so each avoided call and each avoided branch is real time. */
static void draw_glyph_cp(i32 x, i32 y, u32 cp, color fg, color bg,
                          bool opaque, i32 scale)
{
    if (!fb.ready) return;
    if (scale < 1) scale = 1;

    const u8 *g = glyph_for(cp);
    i32 w = GLYPH_W * scale, h = GLYPH_H * scale;

    if (x + w <= 0 || y + h <= 0 ||
        x >= (i32)fb.width || y >= (i32)fb.height)
        return;

    bool inside = (x >= 0 && y >= 0 &&
                   x + w <= (i32)fb.width && y + h <= (i32)fb.height);

    if (!inside) {
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
        return;
    }

    u32 fgn = to_native(fg), bgn = to_native(bg);
    fb_damage(x, y, w, h);


    for (i32 row = 0; row < GLYPH_H; row++) {
        u8 bits = g[row];
        if (!bits && !opaque) continue;          /* blank row, nothing to do */

        for (i32 sy = 0; sy < scale; sy++) {
            u32 *p = fb.base + (u32)(y + row * scale + sy) * fb.stride
                             + (u32)x;
            for (i32 col = 0; col < GLYPH_W; col++) {
                bool on = (bits & (0x80u >> col)) != 0;
                if (!on && !opaque) continue;
                u32 c = on ? fgn : bgn;
                u32 *q = p + col * scale;
                for (i32 sx = 0; sx < scale; sx++) q[sx] = c;
            }
        }
    }
}

void fb_glyph(i32 x, i32 y, u8 ch, color fg, color bg, bool opaque)
{
    draw_glyph_cp(x, y, ch, fg, bg, opaque, 1);
}

/* Pull the next code point out of a UTF-8 string and advance the
 * pointer. A malformed sequence yields the raw byte -- the kernel
 * should keep going on broken text, not stop. */
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

void fb_text(i32 x, i32 y, const char *s, color fg, color bg, bool opaque)
{
    while (*s) {
        u32 cp = utf8_next(&s);
        draw_glyph_cp(x, y, cp, fg, bg, opaque, 1);
        x += GLYPH_W;
    }
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
    while (*s) {
        u32 cp = utf8_next(&s);
        draw_glyph_cp(x, y, cp, fg, 0, false, scale);
        x += GLYPH_W * scale;
    }
}

/* ------------------------------------------------------------------ */
/* Screen console                                                      */
/* ------------------------------------------------------------------ */

static struct {
    i32   x, y, w, h;     /* text area in pixels */
    i32   col, row;       /* cursor, in characters */
    i32   cols, rows;
    i32   scale;
    color fg, bg;
    /* partial UTF-8 sequence carried between calls */
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

/* scale = 0 means: work it out. On a 4K panel an 8x16 font would
 * otherwise be unreadable. */
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

/* Shift everything up by one line. The most expensive thing that
 * happens during start-up -- the compositor will take this over. */
static void scroll(void)
{
    i32 line = GLYPH_H * con.scale;
    for (i32 yy = 0; yy < con.h - line; yy++) {
        u32 *dst = fb.base + (u32)(con.y + yy) * fb.stride + (u32)con.x;
        u32 *src = dst + (u32)line * fb.stride;
        for (i32 xx = 0; xx < con.w; xx++) dst[xx] = src[xx];
    }
    fb_rect(con.x, con.y + con.h - line, con.w, line, con.bg);
    fb_damage(con.x, con.y, con.w, con.h);
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

    /* Continuation byte of a UTF-8 sequence already in progress? */
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
        /* Push the finished line to the screen. Presenting per line
         * rather than per character keeps the log appearing live while
         * still doing one copy instead of eight per glyph. */
        fb_present();
        return;
    }
    if (b == '\r') { con.col = 0; return; }
    if (b == '\t') { do { put_cp(' '); } while (con.col % 8); return; }

    if      ((b & 0xE0) == 0xC0) { con.pending = b & 0x1Fu; con.pending_left = 1; }
    else if ((b & 0xF0) == 0xE0) { con.pending = b & 0x0Fu; con.pending_left = 2; }
    else if ((b & 0xF8) == 0xF0) { con.pending = b & 0x07u; con.pending_left = 3; }
    else                          put_cp(b);
}
