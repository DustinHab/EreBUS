#ifndef EB_CSS_H
#define EB_CSS_H

#include <eb/types.h>

/* The part of CSS a text browser can honour.
 *
 * A stylesheet is read into a table of rules; each rule is a selector
 * (up to five compounds of tag, id, classes and one attribute, joined
 * by descendant or child) and the handful of declarations that change
 * what a page says or how its words are set: display (none, block or
 * inline), visibility, font-weight, color, text-align, list-style.
 * Everything else -- sizes, boxes, positions, fonts, animations -- is
 * skipped while parsing, so the table holds only what matters.
 *
 * Media queries on width, screen and print are evaluated; @supports
 * and @layer blocks are read; @import and the rest are skipped.
 * Selectors with pseudo-classes or -elements, sibling combinators or
 * unknown attribute operators never match.
 */

#define CSS_RULES_MAX   10800
#define CSS_PARTS_MAX   5           /* compounds in one selector */
#define CSS_CLASSES_MAX 3           /* per compound */
#define CSS_BUCKETS     1024
#define CSS_ELEM_CLASSES 8          /* classes remembered per element */
#define CSS_ELEM_ATTRS   8          /* attributes remembered per element */
#define CSS_SHEETS_MAX   8          /* external sheets fetched for one page */
#define CSS_SHEET_PAGES  512        /* 2 MiB for the rule table */

/* what a declaration block sets */
#define CSS_SET_DISPLAY    (1u << 0)
#define CSS_SET_VISIBILITY (1u << 1)
#define CSS_SET_WEIGHT     (1u << 2)
#define CSS_SET_COLOR      (1u << 3)
#define CSS_SET_ALIGN      (1u << 4)
#define CSS_SET_LIST       (1u << 5)
#define CSS_PROPS          6

#define CSS_DISPLAY_INLINE 0
#define CSS_DISPLAY_BLOCK  1
#define CSS_DISPLAY_NONE   2

#define CSS_ALIGN_LEFT   0
#define CSS_ALIGN_CENTER 1
#define CSS_ALIGN_RIGHT  2

typedef struct {
    u8  set;                        /* CSS_SET_* bits */
    u8  important;                  /* the same bits, marked !important */
    u8  display;                    /* CSS_DISPLAY_* */
    u8  hidden;                     /* visibility: hidden or collapse */
    u8  bold;                       /* font-weight bold or 600 and up */
    u8  align;                      /* CSS_ALIGN_* */
    u8  list_none;                  /* list-style(-type): none */
    u8  pad;
    u32 color;                      /* 0x00RRGGBB */
} css_decl;

typedef struct {
    u32 tag;                        /* hash of the tag name; 0 for any */
    u32 id;                         /* hash; 0 for none */
    u32 cls[CSS_CLASSES_MAX];       /* hashes; 0 unused */
    u32 attr, attr_val;             /* [attr] or [attr=val]; 0 for none / any value */
    u8  ncls;
    u8  child;                      /* the compound to the left must be the parent, not any ancestor */
    u8  pad[2];
} css_part;

typedef struct {
    css_part part[CSS_PARTS_MAX];   /* part[0] is the element itself, the rest its ancestors */
    u8  nparts;
    u8  pad;
    u16 spec;                       /* specificity: ids, classes and attributes, tags */
    u32 order;                      /* when it came; later wins among equals */
    u32 next;                       /* the next rule in the same bucket, or ~0 */
    css_decl d;
} css_rule;

typedef struct {
    u32 tag, id;
    u32 cls[CSS_ELEM_CLASSES];
    u32 attr[CSS_ELEM_ATTRS], attr_val[CSS_ELEM_ATTRS];
    u8  ncls, nattr;
} css_elem;

typedef struct {
    u32 count;                      /* rules in the table */
    u32 order;                      /* the next order number */
    u32 dropped;                    /* rules with no room left */
    u32 width;                      /* the window, for media queries, in pixels */
    u32 head[CSS_BUCKETS];          /* rules by the key of their first compound: id, class, tag, any */
    css_rule rule[CSS_RULES_MAX];
} css_sheet;

_Static_assert(sizeof(css_sheet) <= CSS_SHEET_PAGES * 4096, "the rule table must fit its pages");

u32  css_hash(const char *s);                      /* case-folded fnv-1a, never 0 */
u32  css_hash_n(const u8 *s, u32 n);

void css_reset(css_sheet *s, u32 width);
void css_parse(css_sheet *s, const u8 *text, u64 len);      /* appends the sheet's rules */
bool css_media(const u8 *q, u32 n, u32 width);              /* whether a media query holds */
void css_declarations(const u8 *text, u64 len, css_decl *d); /* a style attribute */

/* The declarations that apply to the element at chain[depth - 1],
 * whose ancestors are chain[0 .. depth - 2], the root first. */
void css_match(const css_sheet *s, const css_elem *chain, u32 depth, css_decl *out);

#endif /* EB_CSS_H */
