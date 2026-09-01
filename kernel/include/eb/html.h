#ifndef EB_HTML_H
#define EB_HTML_H

#include <eb/types.h>
#include <eb/fb.h>

/* Reading a page the way it was meant, more or less.
 *
 * This renders the markup a fetch brings home: headings stand out,
 * paragraphs break, lists get their dashes, links get their colour
 * and their underline, and everything else -- the scripts, the
 * styles, the tags themselves -- is furniture the reader was never
 * meant to see. No boxes, no columns, no pictures: one honest flow
 * of words, wrapped at the window's edge.
 *
 * The renderer draws nothing outside [scroll, scroll+rows) and tells
 * the caller how tall the whole flow was, which is all a scrollbar
 * needs to know. Links come back twice: the addresses themselves,
 * and the rectangles where their words were drawn.
 */

#define HTML_URL_MAX   192
#define HTML_LINKS_MAX 48
#define HTML_SPOTS_MAX 96

typedef struct {
    i32 x, y, w, h;
    u32 link;                       /* index into the url table */
} html_spot;

typedef struct {
    color text;                     /* headings, strong words */
    color dim;                      /* the body of the prose */
    color faint;                    /* asides: alt texts, bullets */
    color accent;                   /* links */
} html_colors;

typedef struct {
    const u8 *src;
    u64       len;
    i32       x, y, w, h;           /* the window, in pixels */
    u32       scroll;               /* first visible row of the flow */
    html_colors col;
} html_view;

/* Renders (or, with a zero-height window, merely measures). Returns
 * the total rows of the flow. urls/spots may be NULL when links are
 * not wanted. */
u32 html_render(const html_view *v,
                char urls[][HTML_URL_MAX], u32 *url_count,
                html_spot *spots, u32 *spot_count);

#endif /* EB_HTML_H */
