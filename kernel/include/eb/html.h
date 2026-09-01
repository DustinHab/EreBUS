#ifndef EB_HTML_H
#define EB_HTML_H

#include <eb/types.h>
#include <eb/fb.h>

/* Reading a page the way it was meant, more or less.
 *
 * This renders the markup a fetch brings home: headings stand out and
 * are ruled off, paragraphs break, lists indent and number, quotes
 * step in, tables lay their cells side by side, preformatted text
 * keeps its spaces, links take their colour, and forms grow fields a
 * person can type into and a button that sends them. Everything else
 * -- the scripts, the styles, the tags themselves -- is furniture the
 * reader was never meant to see.
 *
 * No CSS and no scripting: the page decides what it says, not how the
 * screen is painted or what runs when it loads. A text browser, then,
 * and honest about it -- but a text browser that fills in forms.
 *
 * The renderer draws nothing outside [scroll, scroll+rows) and returns
 * how tall the whole flow was. The sink, if given, collects the links
 * and form fields it found and the rectangles where they landed.
 */

#define HTML_URL_MAX    192
#define HTML_NAME_MAX   64
#define HTML_VALUE_MAX  128
#define HTML_LINKS_MAX  64
#define HTML_SPOTS_MAX  128
#define HTML_FORMS_MAX  12
#define HTML_FIELDS_MAX 32

/* What a form field is for. */
#define FIELD_TEXT   0
#define FIELD_PASS   1
#define FIELD_HIDDEN 2
#define FIELD_SUBMIT 3

#define METHOD_GET  0
#define METHOD_POST 1

typedef struct {
    i32 x, y, w, h;
    u32 ref;                        /* link: url index. field: field index. */
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
} html_sink;

u32 html_render(const html_view *v, html_sink *sink);

#endif /* EB_HTML_H */
