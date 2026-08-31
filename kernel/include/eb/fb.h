#ifndef EB_FB_H
#define EB_FB_H

#include <eb/types.h>
#include <common/bootinfo.h>

/* Colours are always carried as 0x00RRGGBB inside the kernel and only
 * converted on the way into the framebuffer. That way no caller has to
 * care whether the firmware handed us BGR or RGB. */
typedef u32 color;

#define RGB(r, g, b) ((color)(((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b)))

void fb_init(const eb_boot_info *bi);
u32  fb_width(void);
u32  fb_height(void);

/* Double buffering. Until a back buffer is handed over, drawing goes
 * straight to the screen. */
u64  fb_backbuffer_bytes(void);
void fb_enable_backbuffer(void *memory);
bool fb_is_buffered(void);

/* Marks a region as changed, and copies everything marked since the
 * last call to the screen. */
void fb_damage(i32 x, i32 y, i32 w, i32 h);
void fb_present(void);

void fb_clear(color c);
void fb_pixel(i32 x, i32 y, color c);
void fb_rect(i32 x, i32 y, i32 w, i32 h, color c);
void fb_frame(i32 x, i32 y, i32 w, i32 h, i32 thickness, color c);

/* Vertical gradient -- cheap, and useful once there are windows. */
void fb_gradient(i32 x, i32 y, i32 w, i32 h, color top, color bottom);

/* The font is 8x16. */
#define GLYPH_W 8
#define GLYPH_H 16

void fb_glyph(i32 x, i32 y, u8 ch, color fg, color bg, bool opaque);
void fb_text(i32 x, i32 y, const char *s, color fg, color bg, bool opaque);

/* Like fb_text, but each pixel drawn as a scale x scale block. A bitmap
 * font can only be enlarged by whole factors -- anything else would
 * need real antialiasing. */
void fb_text_scaled(i32 x, i32 y, const char *s, color fg, i32 scale);

/* Width of a string in pixels, UTF-8 aware. */
i32 fb_text_width(const char *s, i32 scale);

/* Screen console: registers itself as an output sink for kprintf. */
void fbcon_init(color fg, color bg, i32 scale);  /* scale 0 = choose one */
void fbcon_putc(char c);
void fbcon_set_origin(i32 x, i32 y, i32 w, i32 h);
i32  fbcon_cols(void);
i32  fbcon_rows(void);

#endif /* EB_FB_H */
