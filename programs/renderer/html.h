#ifndef RENDERER_HTML_H
#define RENDERER_HTML_H

#include <eb/types.h>
#include <eb/render.h>
#include "css.h"

/* Reading a page the way it was meant, more or less.
 *
 * This renders the markup a fetch brings home: headings stand out and
 * are ruled off, paragraphs break, lists indent and number, quotes
 * step in, tables lay their cells side by side, preformatted text
 * keeps its spaces, links take their colour, and forms grow fields a
 * person can type into and a button that sends them. Everything else
 * -- the scripts, the tags themselves -- is furniture the reader was
 * never meant to see.
 *
 * Of the stylesheets, the part that changes what a page says is
 * honoured: what they hide stays hidden, what they set bold, centred
 * or coloured is drawn so, what they make a block or a line is broken
 * so. Nothing is laid out in boxes; there is one font. Navigation,
 * headers, footers and asides fold to a line each, opened on request.
 * No scripting: the page decides what it says, not what runs.
 *
 * The renderer runs in ring 3 (programs/renderer) and draws nothing
 * itself: it lays the rectangles, glyph runs, pictures and fields of
 * the whole flow into a display list (eb/render.h) in the flow's own
 * coordinates, which the kernel paints at any scroll. It returns how
 * tall the whole flow was. The sink collects the links, fields and
 * folds it found and the rectangles where they landed. */

/* The font's cell, as the kernel draws it. */
#define GLYPH_W 8
#define GLYPH_H 16

typedef u32 color;

/* A decoded picture's size, as the browser lends it to the renderer. */
typedef struct {
    u32 w, h;
} html_image;

typedef struct {
    color text;                     /* headings, strong words */
    color dim;                      /* the body of the prose */
    color faint;                    /* asides, bullets, rules */
    color accent;                   /* links, buttons */
    color edge;                     /* field boxes and table rules */
} html_colors;

typedef struct {
    const u8 *src;
    u64       len;
    i32       x, y, w, h;           /* the window, in pixels */
    u32       scroll;               /* first visible row of the flow */
    html_colors col;
} html_view;

/* Where the drawing goes: words of display list, up to cap. A list
 * that ran out of room is marked full and the rest of the page draws
 * nothing, so a page too large to hold is cut rather than wrong. */
typedef struct {
    u32 *ops;
    u32  cap, len;
    bool full;
} html_out;

/* Where the renderer's findings go. Any pointer may be NULL. The
 * initial values of the fields go out (what the markup asked for);
 * what a person typed is the kernel's, drawn by it into the field
 * boxes the list names. */
typedef struct {
    html_out   *out;

    char      (*urls)[HTML_URL_MAX];
    u32        *url_count;
    html_spot  *link_spots;
    u32        *link_spot_count;

    html_form  *forms;
    u32        *form_count;
    html_field *fields;
    u32        *field_count;
    html_spot  *field_spots;
    u32        *field_spot_count;
    char       (*field_init)[HTML_VALUE_MAX];     /* out: the defaults */

    /* Pictures: the urls the page names go out; for each the renderer
     * asks the lender for the decoded picture's size and lays it in
     * place, or a frame with the alternative text while there is none. */
    char      (*images)[HTML_URL_MAX];
    u32        *image_count;
    const html_image *(*image)(void *ctx, const char *url);
    void       *image_ctx;

    /* The page's title, when it has one. */
    char       *title;
    u32         title_max;

    /* A word to find: every word holding it is drawn marked, and the
     * first such row at or past find_from is answered in find_row
     * (left as it came when there is none). */
    const char *find;
    u32         find_from;
    u32        *find_row;

    /* Styles: the page's rules (NULL for none), and whether the
     * navigation, headers, footers and asides fold. Each fold is
     * numbered as it comes; a set bit in unfold opens it. The line
     * that stands for a fold is a spot, its ref the fold's number.
     * How many parts were hidden and how many folds there were go out. */
    const css_sheet *sheet;
    bool        fold;
    u64         unfold;
    html_spot  *fold_spots;
    u32        *fold_spot_count;
    u32        *hidden_count;
    u32        *fold_count;

    /* The sheets the page links (html_styles): the urls go out; for
     * each the lender answers the fetched text, or NULL. */
    char      (*sheets)[HTML_URL_MAX];
    u32        *sheet_count;
    const u8 *(*sheet_text)(void *ctx, const char *url, u32 *len);
    void       *sheet_ctx;
} html_sink;

u32 html_render(const html_view *v, html_sink *sink);

/* Reads the page's <style> blocks and the stylesheets it links into
 * the rule table, in the page's order; returns how many rules. */
u32 html_styles(const html_view *v, html_sink *sink, css_sheet *sheet);

#endif /* RENDERER_HTML_H */
