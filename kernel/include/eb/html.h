#ifndef EB_HTML_H
#define EB_HTML_H

#include <eb/types.h>
#include <eb/fb.h>
#include <eb/css.h>

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
 * The renderer draws nothing outside [scroll, scroll+rows) and returns
 * how tall the whole flow was. The sink, if given, collects the links
 * and form fields it found and the rectangles where they landed.
 */

#define HTML_URL_MAX    400
#define HTML_NAME_MAX   64
#define HTML_VALUE_MAX  128
#define HTML_LINKS_MAX  64
#define HTML_SPOTS_MAX  128
#define HTML_FORMS_MAX  12
#define HTML_FIELDS_MAX 32
#define HTML_IMAGES_MAX 24
#define HTML_TITLE_MAX  96

/* A decoded picture, as the browser lends it to the renderer. */
typedef struct {
    const u32 *px;                  /* 0x00RRGGBB, w * h of them */
    u32 w, h;
} html_image;

/* What a form field is for. */
#define FIELD_TEXT   0
#define FIELD_PASS   1
#define FIELD_HIDDEN 2
#define FIELD_SUBMIT 3

#define METHOD_GET  0
#define METHOD_POST 1

typedef struct {
    i32 x, y, w, h;
    u32 ref;                        /* link: url index. field: field index. fold: its number. */
} html_spot;

typedef struct {
    char name[HTML_NAME_MAX];
    u32  form;                      /* which form it belongs to */
    u8   kind;
} html_field;

typedef struct {
    char action[HTML_URL_MAX];
    u8   method;
} html_form;

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

/* Where the renderer's findings go. Any pointer may be NULL. The
 * field values come in (what to draw in each box) and the initial
 * values go out (what the markup asked for), which is how a form
 * keeps what a person typed across redraws while still starting from
 * its own defaults. */
typedef struct {
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

    const char (*field_values)[HTML_VALUE_MAX];   /* in: draw these */
    char       (*field_init)[HTML_VALUE_MAX];     /* out: the defaults */

    /* Pictures: the urls the page names go out; for each the renderer
     * asks the lender for the decoded picture and draws it in place,
     * or a frame with the alternative text while there is none. */
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

#endif /* EB_HTML_H */
