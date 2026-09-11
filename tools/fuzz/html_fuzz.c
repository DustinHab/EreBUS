/*
 * html_fuzz.c -- libFuzzer entry for the page renderer and the kernel's check of its answer.
 * - bytes as markup, laid into a display list the way the renderer program does it (programs/renderer); the
 *   block it makes must pass the kernel's render_check, or the check refuses honest work
 * - with a flag byte set, the first part of the input is a stylesheet the page links, and the page is rendered
 *   with its rules and folds; the second part is the markup
 * - with another flag byte set, the input patches a block a rendering made and the patched block goes through
 *   render_check: a program that lies must be refused, never followed out of bounds
 * - built and run by tools/fuzz/run.sh alongside the language tools
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/render.h>
#include "html.h"

void kprintf(const char *fmt, ...) { (void)fmt; }

static css_sheet  *sheet;
static render_out *out;            /* the block a rendering makes */
static render_out *patched;        /* a good block, patched by the input, for the check */
static u32         good_len;       /* the length of the last good block kept in patched's source */
static u8         *good;
static const u8   *sheet_bytes;
static u32         sheet_len;

/* Every other picture is "there", in a size the url decides. */
static const html_image *lend(void *ctx, const char *url)
{
    (void)ctx;
    static html_image im;
    u32 h = 0;
    for (u32 i = 0; url[i]; i++) h = h * 31 + (u8)url[i];
    if (h & 1) return NULL;
    im.w = 1 + (h >> 1) % 64; im.h = 1 + (h >> 7) % 48;
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
    if (!sheet) {
        sheet = malloc(sizeof(css_sheet));
        out = malloc(RENDER_OUT_MAX);
        patched = malloc(RENDER_OUT_MAX);
        good = malloc(RENDER_OUT_MAX);
    }

    /* A block that lies: the last good block, patched where the input
     * says, must be refused or taken without a step outside it. */
    if (size > 8 && (data[2] & 4) && good_len) {
        memcpy(patched, good, good_len);
        u32 len = good_len;
        const uint8_t *p = data + 6;
        size_t left = size - 6;
        for (;;) {
            if (left < 5) break;
            u32 at = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16);
            u32 n = (p[3] & 3) + 1;             /* one to four bytes at that offset */
            if (left < 4 + n) break;
            for (u32 i = 0; i < n; i++)
                if (at + i < len) ((u8 *)patched)[at + i] = p[4 + i];
            p += 4 + n; left -= 4 + n;
        }
        if (data[3] & 1) len = (u32)((data[4] | (data[5] << 8)) % (good_len + 1));
        render_check(patched, len);
        return 0;
    }

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

    /* The rendering, the way the program does it: into the block. */
    memset(out, 0, RENDER_OUT_HEAD);
    html_out list = { out->ops, RENDER_OPS_MAX, 0, false };
    u32 find_row = (u32)-1;
    html_sink sink = {
        .out = &list,
        .urls = out->urls, .url_count = &out->nurls,
        .link_spots = out->links, .link_spot_count = &out->nlinks,
        .forms = out->forms, .form_count = &out->nforms,
        .fields = out->fields, .field_count = &out->nfields,
        .field_spots = out->fspots, .field_spot_count = &out->nfspots,
        .field_init = out->init,
        .images = out->images, .image_count = &out->nimages, .image = lend,
        .title = out->title, .title_max = HTML_TITLE_MAX,
        .find = size > 2 && (data[2] & 1) ? "the" : NULL, .find_from = 0, .find_row = &find_row,
        .sheet = NULL, .fold = styled, .unfold = size > 3 ? data[3] : 0,
        .fold_spots = out->folds, .fold_spot_count = &out->nfolds,
        .hidden_count = &out->hidden_n, .fold_count = &out->fold_n,
        .sheets = out->sheets, .sheet_count = &out->nsheets, .sheet_text = lend_sheet,
    };
    html_view v;
    memset(&v, 0, sizeof v);
    v.src = page;
    v.len = plen;
    v.x = 0; v.y = 0;
    v.w = size ? (i32)(200 + (data[0] * 4)) : 640;     /* narrow to wide windows */
    v.h = 0x3FFF0000;
    v.scroll = 0;
    if (styled) {
        out->nrules = html_styles(&v, &sink, sheet);
        css_parse(sheet, sheet_bytes, sheet_len);          /* as if the page had it inline too */
        sink.sheet = sheet;
        out->dropped = sheet->dropped ? 1 : 0;
    }
    out->rows = html_render(&v, &sink);
    out->magic = RENDER_OUT_MAGIC;
    out->find_row = find_row;
    out->ops_words = list.len;
    out->total = RENDER_OUT_HEAD + list.len * 4;

    if (out->nurls > HTML_LINKS_MAX || out->nlinks > HTML_SPOTS_MAX || out->nforms > HTML_FORMS_MAX ||
        out->nfields > HTML_FIELDS_MAX || out->nfspots > HTML_SPOTS_MAX || out->nimages > HTML_IMAGES_MAX ||
        out->nfolds > HTML_SPOTS_MAX || out->nsheets > CSS_SHEETS_MAX) { fprintf(stderr, "a count past its table\n"); abort(); }
    for (u32 i = 0; out->title[i]; i++) if (i >= HTML_TITLE_MAX) { fprintf(stderr, "the title runs on\n"); abort(); }

    /* Honest work passes the kernel's check. */
    if (!render_check(out, out->total)) {
        fprintf(stderr, "render_check refused honest work: rows %u urls %u forms %u fields %u images %u sheets %u links %u fspots %u folds %u ops %u\n",
                out->rows, out->nurls, out->nforms, out->nfields, out->nimages, out->nsheets, out->nlinks, out->nfspots, out->nfolds, out->ops_words);
        for (u32 i = 0; i < out->nfields; i++) fprintf(stderr, "  field %u kind %u form %u\n", i, out->fields[i].kind, out->fields[i].form);
        for (u32 i = 0; i < out->nlinks; i++) if (out->links[i].ref >= out->nurls) fprintf(stderr, "  link spot %u ref %u\n", i, out->links[i].ref);
        for (u32 i = 0; i < out->nfspots; i++) if (out->fspots[i].ref >= out->nfields) fprintf(stderr, "  field spot %u ref %u\n", i, out->fspots[i].ref);
        u32 at = 0;
        while (at < out->ops_words) {
            u32 head = out->ops[at], kind = OP_KIND(head), words = OP_WORDS(head);
            const u32 *w = &out->ops[at + 1];
            fprintf(stderr, "  op %u kind %u words %u: %d %d %u %u %u %u\n", at, kind, words,
                    (i32)w[0], (i32)w[1], words > 2 ? w[2] : 0, words > 3 ? w[3] : 0, words > 4 ? w[4] : 0, words > 5 ? w[5] : 0);
            at += 1 + words;
            if (at > 60) break;
        }
        abort();
    }
    if (out->total <= 256 * 1024) { memcpy(good, out, out->total); good_len = out->total; }
    return 0;
}
