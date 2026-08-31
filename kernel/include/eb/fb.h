#ifndef EB_FB_H
#define EB_FB_H

#include <eb/types.h>
#include <common/bootinfo.h>

/* Farben werden im Kernel immer als 0x00RRGGBB geführt und erst beim
 * Schreiben in das Format des Bildpuffers gedreht. So muss sich kein
 * Aufrufer darum kümmern, ob die Firmware BGR oder RGB liefert. */
typedef u32 color;

#define RGB(r, g, b) ((color)(((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b)))

void fb_init(const eb_boot_info *bi);
u32  fb_width(void);
u32  fb_height(void);

void fb_clear(color c);
void fb_pixel(i32 x, i32 y, color c);
void fb_rect(i32 x, i32 y, i32 w, i32 h, color c);
void fb_frame(i32 x, i32 y, i32 w, i32 h, i32 thickness, color c);

/* Senkrechter Verlauf -- billig, aber macht sofort klar, dass wir den
 * Bildpuffer wirklich in der Hand haben. */
void fb_gradient(i32 x, i32 y, i32 w, i32 h, color top, color bottom);

/* Zeichensatz ist 8x16. */
#define GLYPH_W 8
#define GLYPH_H 16

void fb_glyph(i32 x, i32 y, u8 ch, color fg, color bg, bool opaque);
void fb_text(i32 x, i32 y, const char *s, color fg, color bg, bool opaque);

/* Wie fb_text, aber jedes Pixel als scale x scale grosser Block. Ein
 * Bitmap-Zeichensatz laesst sich nur ganzzahlig vergroessern -- alles
 * andere braeuchte echte Kantenglaettung. */
void fb_text_scaled(i32 x, i32 y, const char *s, color fg, i32 scale);

/* Breite einer Zeichenkette in Pixeln (UTF-8-bewusst). */
i32 fb_text_width(const char *s, i32 scale);

/* Bildschirmkonsole: meldet sich als Ausgabesenke bei kprintf an. */
/* scale = 0: passende Vergroesserung anhand der Bildbreite waehlen. */
void fbcon_init(color fg, color bg, i32 scale);
void fbcon_putc(char c);
void fbcon_set_origin(i32 x, i32 y, i32 w, i32 h);
i32  fbcon_cols(void);
i32  fbcon_rows(void);

#endif /* EB_FB_H */
