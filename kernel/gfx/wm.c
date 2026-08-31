/*
 * wm.c -- the desktop and its windows.
 */
#include <eb/wm.h>
#include <eb/fb.h>
#include <eb/object.h>
#include <eb/kheap.h>
#include <eb/ps2.h>
#include <eb/thread.h>
#include <eb/ps2.h>

#define MAX_WINDOWS 8
#define TITLE_H  26
#define BORDER   2
#define PAD      8

#define C_DESK_TOP  RGB( 18,  22,  30)
#define C_DESK_BOT  RGB(  9,  11,  16)
#define C_WIN_BG    RGB( 24,  27,  35)
#define C_WIN_EDGE  RGB( 58,  66,  84)
#define C_TITLE     RGB( 38,  44,  58)
#define C_TITLE_ON  RGB( 52,  78, 122)
#define C_TEXT      RGB(214, 219, 230)
#define C_DIM       RGB(126, 136, 156)
#define C_ACCENT    RGB(122, 172, 255)
#define C_READONLY  RGB(206, 158,  92)
#define C_BAR       RGB( 14,  17,  23)

struct window {
    char       title[40];
    i32        x, y, w, h;
    domain    *dom;
    cap_handle cap;
    view_kind  view;
    bool       used;
};

static window windows[MAX_WINDOWS];
static u32    order[MAX_WINDOWS];    /* back to front */
static u32    window_count;

static i32  mouse_x, mouse_y;
static u8   mouse_buttons;
static i32  drag_index = -1, drag_dx, drag_dy;
static u64  frames;
static bool needs_redraw = true;

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static void copy_title(window *win, const char *s)
{
    u32 i = 0;
    while (s[i] && i < sizeof(win->title) - 1) { win->title[i] = s[i]; i++; }
    win->title[i] = 0;
}

/* Draws text and stops at the right edge instead of running past it. */
static void text_clipped(i32 x, i32 y, i32 limit, const char *s, color c)
{
    while (*s && x + GLYPH_W <= limit) {
        fb_glyph(x, y, (u8)*s++, c, 0, false);
        x += GLYPH_W;
    }
}

static u64 text_length(const u8 *data, u64 size)
{
    u64 n = 0;
    while (n < size && data[n]) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Viewers                                                             */
/* ------------------------------------------------------------------ */

/* Each of these renders the same object differently. None of them owns
 * it, none of them has a private copy, and adding another would not
 * require the object to know about it. */

static void view_text(object *o, i32 x, i32 y, i32 w, i32 h)
{
    const u8 *data = (const u8 *)obj_data(o);
    if (!data) return;

    u64 len = text_length(data, obj_size(o));
    i32 cols = w / GLYPH_W;
    i32 cx = 0, cy = 0;

    for (u64 i = 0; i < len && cy * GLYPH_H < h; i++) {
        u8 ch = data[i];
        if (ch == '\n' || cx >= cols) { cx = 0; cy++; if (ch == '\n') continue; }
        if (cy * GLYPH_H >= h) break;
        fb_glyph(x + cx * GLYPH_W, y + cy * GLYPH_H, ch, C_TEXT, 0, false);
        cx++;
    }

    /* A caret, so it is obvious where typing would land. */
    if (cy * GLYPH_H < h)
        fb_rect(x + cx * GLYPH_W, y + cy * GLYPH_H, 2, GLYPH_H, C_ACCENT);
}

static void view_hex(object *o, i32 x, i32 y, i32 w, i32 h)
{
    const u8 *data = (const u8 *)obj_data(o);
    if (!data) return;

    u64 size = obj_size(o);
    i32 per_row = (w / GLYPH_W - 9) / 3;
    if (per_row > 16) per_row = 16;
    if (per_row < 4) per_row = 4;

    for (i32 row = 0; row * GLYPH_H < h; row++) {
        u64 base = (u64)row * per_row;
        if (base >= size) break;

        i32 ty = y + row * GLYPH_H;
        char line[96];
        u32 p = 0;
        const char *hex = "0123456789abcdef";

        line[p++] = hex[(base >> 12) & 0xF];
        line[p++] = hex[(base >> 8) & 0xF];
        line[p++] = hex[(base >> 4) & 0xF];
        line[p++] = hex[base & 0xF];
        line[p++] = ' ';
        line[p++] = ' ';

        for (i32 i = 0; i < per_row && base + (u64)i < size; i++) {
            u8 b = data[base + i];
            line[p++] = hex[b >> 4];
            line[p++] = hex[b & 0xF];
            line[p++] = ' ';
        }
        line[p] = 0;
        text_clipped(x, ty, x + w, line, C_DIM);
    }
}

static void view_inspect(object *o, i32 x, i32 y, i32 w, i32 h)
{
    char line[80];
    i32 ty = y;
    const char *hex = "0123456789abcdef";

    /* No printf here: this runs inside the draw path, and the log is a
     * different thing from the screen. Short and by hand. */
    u32 p = 0;
    const char *tn = type_name(obj_type(o));
    line[p++] = 't'; line[p++] = 'y'; line[p++] = 'p'; line[p++] = 'e';
    line[p++] = ' '; line[p++] = ' ';
    for (u32 i = 0; tn[i] && p < 70; i++) line[p++] = tn[i];
    line[p] = 0;
    text_clipped(x, ty, x + w, line, C_TEXT);
    ty += GLYPH_H;

    p = 0;
    line[p++] = 'i'; line[p++] = 'd'; line[p++] = ' '; line[p++] = ' ';
    line[p++] = ' '; line[p++] = ' ';
    u64 id = obj_id(o);
    for (i32 s = 12; s >= 0; s -= 4) line[p++] = hex[(id >> s) & 0xF];
    line[p] = 0;
    text_clipped(x, ty, x + w, line, C_DIM);
    ty += GLYPH_H;

    p = 0;
    for (const char *s = "refs  "; *s;) line[p++] = *s++;
    u64 refs = obj_refs(o);
    line[p++] = (char)('0' + (refs / 10) % 10);
    line[p++] = (char)('0' + refs % 10);
    line[p] = 0;
    text_clipped(x, ty, x + w, line, C_DIM);
    ty += GLYPH_H;

    p = 0;
    for (const char *s = "bytes "; *s;) line[p++] = *s++;
    u64 sz = obj_size(o);
    for (i32 s = 12; s >= 0; s -= 4) line[p++] = hex[(sz >> s) & 0xF];
    line[p] = 0;
    text_clipped(x, ty, x + w, line, C_DIM);
    ty += GLYPH_H + GLYPH_H / 2;

    if (ty < y + h)
        text_clipped(x, ty, x + w,
                     "this window holds a capability,", C_DIM);
    ty += GLYPH_H;
    if (ty < y + h)
        text_clipped(x, ty, x + w, "not a path to a file", C_DIM);
}

/* ------------------------------------------------------------------ */
/* Windows                                                             */
/* ------------------------------------------------------------------ */

void wm_init(void)
{
    window_count = 0;
    mouse_x = (i32)fb_width() / 2;
    mouse_y = (i32)fb_height() / 2;
    needs_redraw = true;
}

window *wm_open(const char *title, i32 x, i32 y, i32 w, i32 h,
                domain *d, cap_handle cap, view_kind view)
{
    if (window_count >= MAX_WINDOWS) return NULL;

    window *win = &windows[window_count];
    copy_title(win, title);
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->dom = d; win->cap = cap; win->view = view;
    win->used = true;

    order[window_count] = window_count;
    window_count++;
    needs_redraw = true;
    return win;
}

static void draw_window(window *win, bool focused)
{
    fb_rect(win->x - BORDER, win->y - BORDER,
            win->w + 2 * BORDER, win->h + 2 * BORDER,
            focused ? C_ACCENT : C_WIN_EDGE);
    fb_rect(win->x, win->y, win->w, TITLE_H,
            focused ? C_TITLE_ON : C_TITLE);
    fb_rect(win->x, win->y + TITLE_H, win->w, win->h - TITLE_H, C_WIN_BG);

    text_clipped(win->x + PAD, win->y + (TITLE_H - GLYPH_H) / 2,
                 win->x + win->w - PAD, win->title, C_TEXT);

    /* Whether this window may write is a property of its capability,
     * so say so where it can be seen. */
    u32 rights = cap_rights(win->dom, win->cap);
    const char *mark = (rights & CAP_WRITE) ? "rw" : "ro";
    text_clipped(win->x + win->w - PAD - 2 * GLYPH_W,
                 win->y + (TITLE_H - GLYPH_H) / 2,
                 win->x + win->w, mark,
                 (rights & CAP_WRITE) ? C_DIM : C_READONLY);

    i32 cx = win->x + PAD;
    i32 cy = win->y + TITLE_H + PAD;
    i32 cw = win->w - 2 * PAD;
    i32 ch = win->h - TITLE_H - 2 * PAD;

    /* The window resolves its capability every time it draws. If it was
     * revoked in the meantime, the view goes blank -- authority is not
     * cached here any more than anywhere else. */
    object *o = cap_lookup(win->dom, win->cap, CAP_READ);
    if (!o) {
        text_clipped(cx, cy, cx + cw, "capability revoked", C_READONLY);
        return;
    }

    switch (win->view) {
    case VIEW_TEXT:    view_text(o, cx, cy, cw, ch); break;
    case VIEW_HEX:     view_hex(o, cx, cy, cw, ch); break;
    case VIEW_INSPECT: view_inspect(o, cx, cy, cw, ch); break;
    }
}

static void draw_cursor(void)
{
    for (i32 i = 0; i < 17; i++) {
        i32 len = 1 + i / 2;
        fb_rect(mouse_x, mouse_y + i, len + 1, 1, RGB(0, 0, 0));
        fb_rect(mouse_x + 1, mouse_y + i, len - 1, 1, RGB(255, 255, 255));
    }
}

static void draw_all(void)
{
    i32 sw = (i32)fb_width(), sh = (i32)fb_height();

    fb_gradient(0, 0, sw, sh - 28, C_DESK_TOP, C_DESK_BOT);

    fb_rect(0, sh - 28, sw, 28, C_BAR);
    i32 by = sh - 28 + (28 - GLYPH_H) / 2;
    text_clipped(PAD * 2, by, sw / 2,
                 "Erebus  --  three windows, one object, "
                 "no file anywhere", C_DIM);

    /* Event counters, on screen rather than in the log. When input does
     * not seem to arrive, the first question is whether it arrives at
     * all, and the answer belongs where one is already looking. */
    char counts[48];
    u32 p = 0;
    for (const char *s = "keys "; *s;) counts[p++] = *s++;
    u64 k = ps2_key_count();
    counts[p++] = (char)('0' + (k / 100) % 10);
    counts[p++] = (char)('0' + (k / 10) % 10);
    counts[p++] = (char)('0' + k % 10);
    for (const char *s = "   mouse "; *s;) counts[p++] = *s++;
    u64 mo = ps2_mouse_count();
    counts[p++] = (char)('0' + (mo / 100) % 10);
    counts[p++] = (char)('0' + (mo / 10) % 10);
    counts[p++] = (char)('0' + mo % 10);
    counts[p] = 0;
    text_clipped(sw - 30 * GLYPH_W, by, sw, counts, C_DIM);

    for (u32 i = 0; i < window_count; i++)
        draw_window(&windows[order[i]], i + 1 == window_count);

    draw_cursor();
    frames++;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

static void raise_window(u32 index)
{
    u32 at = 0;
    for (u32 i = 0; i < window_count; i++) if (order[i] == index) at = i;
    for (u32 i = at; i + 1 < window_count; i++) order[i] = order[i + 1];
    order[window_count - 1] = index;
}

static i32 window_at(i32 x, i32 y)
{
    for (i32 i = (i32)window_count - 1; i >= 0; i--) {
        window *w = &windows[order[i]];
        if (x >= w->x - BORDER && x < w->x + w->w + BORDER &&
            y >= w->y - BORDER && y < w->y + w->h + BORDER)
            return (i32)order[i];
    }
    return -1;
}

static void handle_mouse(void)
{
    mouse_event m;
    while (ps2_poll_mouse(&m)) {
        mouse_x += m.dx;
        mouse_y += m.dy;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > (i32)fb_width() - 2)  mouse_x = (i32)fb_width() - 2;
        if (mouse_y > (i32)fb_height() - 2) mouse_y = (i32)fb_height() - 2;

        bool was_down = (mouse_buttons & 1) != 0;
        bool is_down  = (m.buttons & 1) != 0;
        mouse_buttons = m.buttons;

        if (is_down && !was_down) {
            i32 hit = window_at(mouse_x, mouse_y);
            if (hit >= 0) {
                raise_window((u32)hit);
                window *w = &windows[hit];
                if (mouse_y < w->y + TITLE_H) {
                    drag_index = hit;
                    drag_dx = mouse_x - w->x;
                    drag_dy = mouse_y - w->y;
                }
            }
        }
        if (!is_down) drag_index = -1;

        if (drag_index >= 0) {
            windows[drag_index].x = mouse_x - drag_dx;
            windows[drag_index].y = mouse_y - drag_dy;
        }
        needs_redraw = true;
    }
}

static void handle_keys(void)
{
    key_event k;
    while (ps2_poll_key(&k)) {
        if (!k.down || k.codepoint == 0) continue;
        if (window_count == 0) continue;

        window *win = &windows[order[window_count - 1]];

        /* Typing is a write, so it needs a capability that permits one.
         * A read-only window simply gets nothing back and nothing
         * happens -- no check to forget, no flag to get wrong. */
        object *o = cap_lookup(win->dom, win->cap, CAP_WRITE);
        if (!o) { needs_redraw = true; continue; }

        u8 *data = (u8 *)obj_data(o);
        if (!data) continue;
        u64 size = obj_size(o);
        u64 len = text_length(data, size);

        if (k.codepoint == '\b') {
            if (len > 0) data[len - 1] = 0;
        } else if (len + 1 < size) {
            data[len] = (u8)k.codepoint;
            data[len + 1] = 0;
        }
        needs_redraw = true;
    }
}

u64 wm_frames(void) { return frames; }

void wm_run(void *arg)
{
    (void)arg;

    for (;;) {
        handle_mouse();
        handle_keys();

        if (needs_redraw) {
            needs_redraw = false;
            draw_all();
            fb_present();
        }
        sched_yield();
    }
}
