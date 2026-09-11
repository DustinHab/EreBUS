#ifndef EB_RENDERER_H
#define EB_RENDERER_H

#include <eb/types.h>
#include <eb/object.h>
#include <eb/cap.h>
#include <eb/render.h>

/* The page renderer as the kernel drives it: a program in ring 3
 * (programs/renderer) started on a page, answering with a block the
 * kernel checks and then paints from at any scroll. The contract with
 * the program is eb/render.h; this is the kernel's side of it. The
 * browser and the html lens both come through here. */

/* An input block being put together, in a buffer the caller owns of
 * RENDER_IN_MAX bytes. */
typedef struct {
    render_in *in;
    u32        cap, used;
    bool       full;
} render_input;

void render_input_begin(render_input *b, void *buf, u32 cap, u32 width, u32 usew, u32 flags,
                        u64 unfold, const u32 col[5]);
bool render_input_page(render_input *b, const u8 *page, u32 len);
bool render_input_sheet(render_input *b, const char *url, const u8 *text, u32 len);
bool render_input_image(render_input *b, const char *url, u32 w, u32 h);
bool render_input_find(render_input *b, const char *word, u32 from);
u32  render_input_end(render_input *b);           /* the block's length; 0 when it did not fit */

/* One rendering: the program started on the block, its answer awaited. */
typedef struct {
    object    *program;
    object    *reply;
    cap_handle reply_cap;
    domain    *dom;
    u64        started_ns;
} render_job;

bool render_start(render_job *j, domain *d, const void *block, u32 len);

/* What became of it. On RENDER_DONE the block has been read into out
 * (cap bytes the caller owns, RENDER_OUT_MAX at most) and checked. The
 * other answers end the job; the caller shows the page without a
 * rendering. A block larger than cap is RENDER_FAULTED. */
#define RENDER_WAIT    0
#define RENDER_DONE    1
#define RENDER_REFUSED 2      /* the program said the input is not a block it reads */
#define RENDER_FAULTED 3      /* the program ended, or answered wrongly, without a block */
#define RENDER_LATE    4      /* no answer within the limit; ended */
int  render_poll(render_job *j, render_out *out, u32 cap);
void render_end(render_job *j);
bool render_busy(const render_job *j);

#define RENDER_LIMIT_NS (30ULL * 1000000000ULL)

/* Painting a checked block: the ops whose rows fall inside the window
 * (x, y, w, h) at the scroll, in screen coordinates. Fields are drawn
 * with the values the caller holds (what was typed), or the markup's
 * own; pictures with the pixels the caller lends by index. */
typedef struct {
    const u32 *px;                  /* NULL while the picture is not decoded */
    u32 w, h;
} render_picture;

typedef struct {
    i32  x, y, w, h;                /* the window */
    u32  scroll;                    /* the first row shown */
    u32  col[5];                    /* text, dim, faint, accent, edge */
    const char (*values)[HTML_VALUE_MAX];  /* what stands in each field, or NULL for the markup's */
    const render_picture *pictures; /* by the block's image index; nimages of them, or NULL */
} render_window;

void render_paint(const render_out *o, const render_window *w);

/* The spots of one kind that fall whole inside the window, in screen
 * coordinates, into dst (HTML_SPOTS_MAX of them); answers how many. */
#define RENDER_SPOTS_LINKS  0
#define RENDER_SPOTS_FIELDS 1
#define RENDER_SPOTS_FOLDS  2
u32 render_spots(const render_out *o, const render_window *w, u32 kind, html_spot *dst);

#endif /* EB_RENDERER_H */
