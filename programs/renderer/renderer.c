/*
 * renderer.c -- the page renderer, a program in ring 3: given a page, its sheets and its pictures' sizes, it
 * answers with what the page holds and a display list of the whole flow.
 * - the kernel lays the input block (eb/render.h) at RENDER_IN before the program starts and posts "PAGE"
 * - the output block is laid in this program's own memory; "DONE" names its address and length, and the kernel
 *   reads it back, checks it and paints it at any scroll; "FAIL" says the input is not a block this reads
 * - after the answer the program waits in its letter box; the kernel ends it once the block is taken
 * - built like the decoder (programs/decoder): by clang and lld on the host into the image format of MANUAL 18.4,
 *   and by the machine's own compiler in the self-build; the kernel carries the image as bytes
 */
#include <eb/render.h>
#include "html.h"
#include "erebus.h"

_Static_assert(CSS_SHEETS_MAX == RENDER_SHEETS_MAX, "the sheet tables must agree");

/* The output block, the rule table and the list: this program's own
 * zeroed memory, mapped with its data. */
static u32       out_words[RENDER_OUT_MAX / 4];
static css_sheet sheet;

static const render_in *in;

/* The tables in the input, looked up by url. */
static bool same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static const html_image *lend_image(void *ctx, const char *url)
{
    (void)ctx;
    static html_image im;
    for (u32 i = 0; i < in->nimages && i < HTML_IMAGES_MAX; i++) {
        const char *u = (const char *)in + in->image[i].url_at;
        if (!same(u, url)) continue;
        if (!in->image[i].w || !in->image[i].h) return NULL;
        im.w = in->image[i].w; im.h = in->image[i].h;
        return &im;
    }
    return NULL;
}

static const u8 *lend_sheet(void *ctx, const char *url, u32 *len)
{
    (void)ctx;
    for (u32 i = 0; i < in->nsheets && i < RENDER_SHEETS_MAX; i++) {
        const char *u = (const char *)in + in->sheet[i].url_at;
        if (!same(u, url)) continue;
        if (!in->sheet[i].text_len) return NULL;
        *len = in->sheet[i].text_len;
        return (const u8 *)in + in->sheet[i].text_at;
    }
    return NULL;
}

/* Whether the block is what it claims: every offset and length inside
 * it, every string ended inside it. */
static bool string_inside(u32 at, u32 total)
{
    if (at >= total) return false;
    const char *s = (const char *)in;
    for (u32 i = at; i < total; i++) if (!s[i]) return true;
    return false;
}

static bool input_ok(long len)
{
    if (len < (long)sizeof(render_in) || len > (long)RENDER_IN_MAX) return false;
    if (in->magic != RENDER_IN_MAGIC || in->total != (u32)len) return false;
    u32 t = in->total;
    if (in->page_at >= t || in->page_len > t - in->page_at) return false;
    if (in->find_at && !string_inside(in->find_at, t)) return false;
    if (in->nsheets > RENDER_SHEETS_MAX || in->nimages > HTML_IMAGES_MAX) return false;
    for (u32 i = 0; i < in->nsheets; i++) {
        if (!string_inside(in->sheet[i].url_at, t)) return false;
        if (in->sheet[i].text_len && (in->sheet[i].text_at >= t || in->sheet[i].text_len > t - in->sheet[i].text_at)) return false;
    }
    for (u32 i = 0; i < in->nimages; i++)
        if (!string_inside(in->image[i].url_at, t)) return false;
    if (!in->width || in->width > 65536 || !in->usew || in->usew > in->width) return false;
    return true;
}

long main(long console, long inbox)
{
    unsigned char m[EB_MSG_BYTES];
    long len = -1;
    while (len < 0) {
        if (eb_receive(inbox, m, 0) != EB_OK) continue;
        if (eb_msg_tag(m) != (long)RENDER_TAG_PAGE) continue;
        len = eb_msg_word(m, 0);
    }

    in = (const render_in *)RENDER_IN;
    if (!input_ok(len)) {
        eb_send(console, (long)RENDER_TAG_FAIL, 0, 0, 0);
        for (;;) eb_receive(inbox, m, 0);
    }

#if defined(EREBUS_TEST_FAULT) && EREBUS_TEST_FAULT == 5
    /* The test build (tools/renderer-fault.sh): a page whose markup
     * begins "<!--fault-->" ends this program on a page fault, one that
     * begins "<!--hang-->" never answers. Neither can reach the kernel. */
    {
        const char *p = (const char *)in + in->page_at;
        if (in->page_len > 12 && p[0] == '<' && p[1] == '!' && p[4] == 'f' && p[5] == 'a' && p[6] == 'u') *(volatile u32 *)0 = 1;
        if (in->page_len > 11 && p[0] == '<' && p[1] == '!' && p[4] == 'h' && p[5] == 'a' && p[6] == 'n') for (;;) {}
    }
#endif

    render_out *o = (render_out *)out_words;
    html_out list = { o->ops, RENDER_OPS_MAX, 0, false };
    u32 find_row = (u32)-1;

    html_sink sink = {
        .out = &list,
        .urls = o->urls, .url_count = &o->nurls,
        .link_spots = o->links, .link_spot_count = &o->nlinks,
        .forms = o->forms, .form_count = &o->nforms,
        .fields = o->fields, .field_count = &o->nfields,
        .field_spots = o->fspots, .field_spot_count = &o->nfspots,
        .field_init = o->init,
        .images = o->images, .image_count = &o->nimages,
        .image = lend_image, .image_ctx = NULL,
        .title = o->title, .title_max = HTML_TITLE_MAX,
        .find = in->find_at ? (const char *)in + in->find_at : NULL,
        .find_from = in->find_from, .find_row = &find_row,
        .sheet = NULL, .fold = !(in->flags & RENDER_PLAIN), .unfold = in->unfold,
        .fold_spots = o->folds, .fold_spot_count = &o->nfolds,
        .hidden_count = &o->hidden_n, .fold_count = &o->fold_n,
        .sheets = o->sheets, .sheet_count = &o->nsheets,
        .sheet_text = lend_sheet, .sheet_ctx = NULL,
    };
    if (in->flags & RENDER_LENS) { sink.images = NULL; sink.image_count = NULL; sink.sheets = NULL; sink.sheet_count = NULL; }

    /* The flow is laid from (0, 0) with every row visible: the kernel
     * shows the rows the scroll picks out. The media queries see the
     * window's true width; the flow is laid in the measure. */
    html_view v = {
        .src = (const u8 *)in + in->page_at, .len = in->page_len,
        .x = 0, .y = 0, .w = (i32)in->width, .h = 0x3FFF0000,
        .scroll = 0,
        .col = { in->col[0], in->col[1], in->col[2], in->col[3], in->col[4] },
    };
    o->nrules = 0;
    o->dropped = 0;
    if (!(in->flags & RENDER_PLAIN)) {
        o->nrules = html_styles(&v, &sink, &sheet);
        o->dropped = sheet.dropped ? 1 : 0;
        sink.sheet = &sheet;
    }
    v.w = (i32)in->usew;
    o->rows = html_render(&v, &sink);

    o->magic = RENDER_OUT_MAGIC;
    o->find_row = find_row;
    o->ops_words = list.len;
    o->total = RENDER_OUT_HEAD + list.len * 4;

    eb_send(console, (long)RENDER_TAG_DONE, (long)(unsigned long)o, (long)o->total, 0);
    for (;;) eb_receive(inbox, m, 0);
}
