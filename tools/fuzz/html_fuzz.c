/*
 * html_fuzz.c -- libFuzzer entry for the page renderer: bytes as markup, drawn into nothing.
 * - the framebuffer calls are stubs that only check the coordinates stay sane
 * - with a flag byte set, the first part of the input is a stylesheet the page links, and the page is
 *   rendered with its rules and folds; the second part is the markup
 * - built and run by tools/fuzz/run.sh alongside the language tools
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/html.h>

void kprintf(const char *fmt, ...) { (void)fmt; }
void fb_rect(i32 x, i32 y, i32 w, i32 h, color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
void fb_glyph(i32 x, i32 y, u8 ch, color fg, color bg, bool opaque)
{ (void)x; (void)y; (void)ch; (void)fg; (void)bg; (void)opaque; }
void fb_glyph_cp(i32 x, i32 y, u32 cp, color fg, color bg, bool opaque)
{ (void)x; (void)y; (void)fg; (void)bg; (void)opaque; if (cp > 0x10FFFF) abort(); }
void fb_image(i32 x, i32 y, i32 dw, i32 dh, const u32 *px, u32 iw, u32 ih, i32 c0, i32 c1)
{ (void)x; (void)y; (void)px; (void)c0; (void)c1; if (dw <= 0 || dh <= 0 || !iw || !ih) abort(); }

static char       urls[HTML_LINKS_MAX][HTML_URL_MAX];
static html_spot  link_spots[HTML_SPOTS_MAX];
static html_form  forms[HTML_FORMS_MAX];
static html_field fields[HTML_FIELDS_MAX];
static html_spot  field_spots[HTML_SPOTS_MAX];
static html_spot  fold_spots[HTML_SPOTS_MAX];
static char       values[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static char       init[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static char       images[HTML_IMAGES_MAX][HTML_URL_MAX];
static char       sheets[CSS_SHEETS_MAX][HTML_URL_MAX];
static char       title[HTML_TITLE_MAX];
static u32        pix[64 * 48];
static css_sheet *sheet;
static const u8  *sheet_bytes;
static u32        sheet_len;

/* Every other picture is "there", in a size the url decides. */
static const html_image *lend(void *ctx, const char *url)
{
    (void)ctx;
    static html_image im;
    u32 h = 0;
    for (u32 i = 0; url[i]; i++) h = h * 31 + (u8)url[i];
    if (h & 1) return NULL;
    im.px = pix; im.w = 1 + (h >> 1) % 64; im.h = 1 + (h >> 7) % 48;
    return &im;
}

/* Every linked sheet is the input's first part. */
static const u8 *lend_sheet(void *ctx, const char *url, u32 *len)
{
    (void)ctx; (void)url;
    *len = sheet_len;
    return sheet_bytes;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 65536) return 0;
    if (!sheet) sheet = malloc(sizeof(css_sheet));

    bool styled = size > 6 && (data[2] & 2);
    const uint8_t *page = data;
    size_t plen = size;
    sheet_bytes = NULL; sheet_len = 0;
    if (styled) {
        /* the sheet runs to the first '\n', or to the middle */
        size_t cut = size / 2;
        for (size_t i = 3; i < size; i++) if (data[i] == '\n') { cut = i; break; }
        if (cut < 3) cut = 3;
        sheet_bytes = data + 3; sheet_len = (u32)(cut - 3);
        page = data + cut; plen = size - cut;
    }

    html_view v;
    memset(&v, 0, sizeof v);
    v.src = page;
    v.len = plen;
    v.x = 10; v.y = 10;
    v.w = size ? (i32)(200 + (data[0] * 4)) : 640;     /* narrow to wide windows */
    v.h = 400;
    v.scroll = size > 1 ? data[1] : 0;

    u32 nurl = 0, nls = 0, nform = 0, nfield = 0, nfs = 0, nimg = 0, frow = (u32)-1;
    u32 nfold = 0, nfolds = 0, nhidden = 0, nsheet = 0;
    html_sink sink = {
        .urls = urls, .url_count = &nurl,
        .link_spots = link_spots, .link_spot_count = &nls,
        .forms = forms, .form_count = &nform,
        .fields = fields, .field_count = &nfield,
        .field_spots = field_spots, .field_spot_count = &nfs,
        .field_values = (const char (*)[HTML_VALUE_MAX])values,
        .field_init = init,
        .images = images, .image_count = &nimg, .image = lend,
        .title = title, .title_max = sizeof title,
        .find = size > 2 && (data[2] & 1) ? "the" : NULL, .find_from = 0, .find_row = &frow,
        .sheet = NULL, .fold = styled, .unfold = size > 3 ? data[3] : 0,
        .fold_spots = fold_spots, .fold_spot_count = &nfold,
        .hidden_count = &nhidden, .fold_count = &nfolds,
        .sheets = sheets, .sheet_count = &nsheet, .sheet_text = lend_sheet,
    };
    if (styled) {
        html_styles(&v, &sink, sheet);
        css_parse(sheet, sheet_bytes, sheet_len);          /* as if the page had it inline too */
        sink.sheet = sheet;
    }
    html_render(&v, &sink);
    html_render(&v, NULL);                              /* without a sink, as the preview draws */
    if (nurl > HTML_LINKS_MAX || nls > HTML_SPOTS_MAX || nform > HTML_FORMS_MAX ||
        nfield > HTML_FIELDS_MAX || nfs > HTML_SPOTS_MAX || nimg > HTML_IMAGES_MAX ||
        nfold > HTML_SPOTS_MAX || nsheet > CSS_SHEETS_MAX) abort();
    for (u32 i = 0; title[i]; i++) if (i >= HTML_TITLE_MAX) abort();
    return 0;
}
