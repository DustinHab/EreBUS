/*
 * html_fuzz.c -- libFuzzer entry for the page renderer: bytes as markup, drawn into nothing.
 * - the framebuffer calls are stubs that only check the coordinates stay sane
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
static char       values[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static char       init[HTML_FIELDS_MAX][HTML_VALUE_MAX];
static char       images[HTML_IMAGES_MAX][HTML_URL_MAX];
static char       title[HTML_TITLE_MAX];
static u32        pix[64 * 48];

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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 65536) return 0;
    html_view v;
    memset(&v, 0, sizeof v);
    v.src = data;
    v.len = size;
    v.x = 10; v.y = 10;
    v.w = size ? (i32)(200 + (data[0] * 4)) : 640;     /* narrow to wide windows */
    v.h = 400;
    v.scroll = size > 1 ? data[1] : 0;

    u32 nurl = 0, nls = 0, nform = 0, nfield = 0, nfs = 0, nimg = 0, frow = (u32)-1;
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
    };
    html_render(&v, &sink);
    html_render(&v, NULL);                              /* without a sink, as the preview draws */
    if (nurl > HTML_LINKS_MAX || nls > HTML_SPOTS_MAX || nform > HTML_FORMS_MAX ||
        nfield > HTML_FIELDS_MAX || nfs > HTML_SPOTS_MAX || nimg > HTML_IMAGES_MAX) abort();
    for (u32 i = 0; title[i]; i++) if (i >= HTML_TITLE_MAX) abort();
    return 0;
}
