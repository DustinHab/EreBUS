#ifndef EB_RENDER_H
#define EB_RENDER_H

#include <eb/types.h>

/* The contract between the kernel and the page renderer, a program in
 * ring 3 (programs/renderer). Both sides include this and nothing else
 * of each other.
 *
 * The kernel lays an input block at RENDER_IN before the program runs:
 * the page's markup, the stylesheets it has fetched, the pictures it
 * has decoded (their sizes), the word to find, the measure and the
 * flags. It posts one message and takes the answer on a port of its
 * own: the address and length of an output block in the program's
 * memory. The kernel reads that block back through the program's own
 * page tables, checks every count, offset and string in it, and ends
 * the program. The block holds what the page was found to hold -- its
 * links, forms, fields, pictures and sheets, its title -- and a display
 * list: rectangles, runs of glyphs, pictures and fields in the flow's
 * own coordinates, which the kernel paints at any scroll without the
 * program running again. Nothing of the markup is read in ring 0. */

/* What the page may hold, as the renderer reports it. */
#define HTML_URL_MAX    400
#define HTML_NAME_MAX   64
#define HTML_VALUE_MAX  128
#define HTML_LINKS_MAX  64
#define HTML_SPOTS_MAX  128
#define HTML_FORMS_MAX  12
#define HTML_FIELDS_MAX 32
#define HTML_IMAGES_MAX 24
#define HTML_TITLE_MAX  96
#define RENDER_SHEETS_MAX 8         /* css.h's CSS_SHEETS_MAX, the same number */

/* What a form field is for. */
#define FIELD_TEXT   0
#define FIELD_PASS   1
#define FIELD_HIDDEN 2
#define FIELD_SUBMIT 3

#define METHOD_GET  0
#define METHOD_POST 1

/* A rectangle in the flow (x from the flow's left edge, y from its
 * top, in pixels) with what it stands for: a link's url index, a
 * field's index, or a fold's number. */
typedef struct {
    i32 x, y, w, h;
    u32 ref;
} html_spot;

typedef struct {
    char name[HTML_NAME_MAX];
    u32  form;                      /* which form it belongs to */
    u8   kind;                      /* FIELD_* */
    u8   pad[3];
} html_field;

typedef struct {
    char action[HTML_URL_MAX];
    u8   method;                    /* METHOD_* */
    u8   pad[3];
} html_form;

/* Where the input lies: past anything the image lays (the data and its
 * zeroed room begin at 0x1100000 and may reach 64 MiB). */
#define RENDER_IN       0x0000000006000000ULL
#define RENDER_IN_MAX   (6u << 20)                /* the page, the sheets, the tables */
#define RENDER_OUT_MAX  (12u << 20)               /* the block the program answers with */
#define RENDER_PAGE_MAX (2u << 20)
#define RENDER_SHEETS_BYTES (2u << 20)

#define RENDER_TAG_PAGE 0x45474150ULL             /* "PAGE": the length of the input block */
#define RENDER_TAG_DONE 0x454E4F44ULL             /* "DONE": the block's address and length in the program's memory */
#define RENDER_TAG_FAIL 0x4C494146ULL             /* "FAIL": the input was not a block it reads */

#define RENDER_PLAIN 1u                           /* no styles, no folds: the page as it came */
#define RENDER_LENS  2u                           /* the html lens: a text of the graph, no fetching */

#define RENDER_IN_MAGIC  0x49524245u              /* "EBRI" */
#define RENDER_OUT_MAGIC 0x4F524245u              /* "EBRO" */

typedef struct {
    u32 url_at;                     /* NUL-terminated, inside the block */
    u32 text_at, text_len;          /* the sheet as fetched; text_len 0 when it is not there */
} render_sheet_in;

typedef struct {
    u32 url_at;
    u32 w, h;                       /* 0 while the picture is not decoded */
} render_image_in;

/* The input block: this head, then the bytes the offsets name. Every
 * offset is from the block's start and lies inside total. */
typedef struct {
    u32 magic, total;
    u32 width, usew;                /* pixels: the window (for the media queries) and the measure the flow is laid in */
    u32 flags;                      /* RENDER_* */
    u32 find_from;                  /* the row to answer the found row from */
    u64 unfold;                     /* a set bit opens that fold */
    u32 col[5];                     /* text, dim, faint, accent, edge */
    u32 page_at, page_len;
    u32 find_at;                    /* NUL-terminated lower-case word, or 0 for none */
    u32 nsheets, nimages;
    render_sheet_in sheet[RENDER_SHEETS_MAX];
    render_image_in image[HTML_IMAGES_MAX];
} render_in;

/* The display list: one word of kind and length, then the words.
 *   RECT  x y w h color
 *   TEXT  x y color|scale<<24 cp...      one glyph per code point, GLYPH_W * scale apart
 *   IMAGE x y dw dh index                the picture at images[index], stretched to dw by dh
 *   FIELD x y w h index kind             the kernel draws the box and what is typed in it
 * Coordinates are the flow's: x from its left edge, y from its top,
 * in pixels; the kernel adds the window's origin and the scroll. */
#define OP_RECT  1u
#define OP_TEXT  2u
#define OP_IMAGE 3u
#define OP_FIELD 4u
#define OP_KIND(w)  ((w) & 0xFFu)
#define OP_WORDS(w) ((w) >> 8)
#define OP_HEAD(kind, words) ((kind) | ((words) << 8))
#define OP_TEXT_MAX 512                           /* code points in one run */

/* The output block: the counts and the tables, then ops_words words
 * of display list. The whole block is sizeof(render_out) + ops_words * 4
 * bytes, and that is what the answer names. */
typedef struct {
    u32 magic, total;
    u32 rows;                       /* the flow's height in glyph rows */
    u32 nrules, dropped;            /* the rule table: rules read, and whether it ran out of room */
    u32 hidden_n, fold_n;
    u32 find_row;                   /* the first row holding the word at or past find_from; ~0 for none */
    u32 nurls, nforms, nfields, nimages, nsheets;
    u32 nlinks, nfspots, nfolds;    /* the spots: links, fields, folds */
    u32 ops_words;
    char       title[HTML_TITLE_MAX];
    char       urls[HTML_LINKS_MAX][HTML_URL_MAX];
    html_form  forms[HTML_FORMS_MAX];
    html_field fields[HTML_FIELDS_MAX];
    char       init[HTML_FIELDS_MAX][HTML_VALUE_MAX];   /* what the markup put in each field */
    char       images[HTML_IMAGES_MAX][HTML_URL_MAX];
    char       sheets[RENDER_SHEETS_MAX][HTML_URL_MAX];
    html_spot  links[HTML_SPOTS_MAX];
    html_spot  fspots[HTML_SPOTS_MAX];
    html_spot  folds[HTML_SPOTS_MAX];
    u32        ops[];
} render_out;

#define RENDER_OUT_HEAD ((u32)sizeof(render_out))
#define RENDER_OPS_MAX  ((RENDER_OUT_MAX - RENDER_OUT_HEAD) / 4)

/* The kernel's check of a block a program answered with: every count
 * within its table, every string ended, every op whole and its indices
 * inside the tables. Kernel-side, but written against this header
 * alone so the fuzzer runs it too (kernel/gfx/render_check.c). */
bool render_check(render_out *o, u64 bytes);

#endif /* EB_RENDER_H */
