/*
 * css_fuzz.c -- libFuzzer entry for the stylesheet reader: bytes as a sheet, a style attribute, a media query.
 * - the first byte picks the target; the rest is the input
 * - the sheet read is then matched against a chain of elements made from the input's own bytes
 * - built and run by tools/fuzz/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "css.h"

void kprintf(const char *fmt, ...) { (void)fmt; }

static css_sheet *sheet;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 1u << 20) return 0;
    if (!sheet) sheet = malloc(sizeof(css_sheet));
    uint8_t pick = data[0];
    const uint8_t *in = data + 1;
    uint32_t len = (uint32_t)(size - 1);

    switch (pick % 3) {
    case 0: {
        css_reset(sheet, 200 + (pick % 4) * 500);
        css_parse(sheet, in, len);
        if (sheet->count > CSS_RULES_MAX) abort();
        /* a chain of elements: tag, id and classes hashed from bytes of the input */
        css_elem chain[12];
        uint32_t depth = 1 + (len % 12);
        for (uint32_t d = 0; d < depth; d++) {
            css_elem *e = &chain[d];
            uint32_t at = (d * 7) % len;
            uint32_t tl = 1 + (in[at] % 3);
            if (tl > len - at) tl = len - at;
            e->tag = css_hash_n(in + at, tl);
            e->id = (in[at] & 1) ? css_hash_n(in + at, 1) : 0;
            e->ncls = in[at] % (CSS_ELEM_CLASSES + 1);
            for (uint32_t c = 0; c < e->ncls; c++) {
                uint32_t s = (at + c) % len, cl = 1 + c % 2;
                if (cl > len - s) cl = len - s;
                e->cls[c] = css_hash_n(in + s, cl);
            }
            e->nattr = in[at] % (CSS_ELEM_ATTRS + 1);
            for (uint32_t c = 0; c < e->nattr; c++) { e->attr[c] = css_hash_n(in + (at + c + 1) % len, 1); e->attr_val[c] = (c & 1) ? css_hash_n(in + (at + c) % len, 1) : 0; }
        }
        for (uint32_t d = 1; d <= depth; d++) {
            css_decl out;
            css_match(sheet, chain, d, &out);
            if (out.display > CSS_DISPLAY_NONE || out.align > CSS_ALIGN_RIGHT) abort();
        }
        break;
    }
    case 1: {
        css_decl d;
        css_declarations(in, len, &d);
        if (d.display > CSS_DISPLAY_NONE || d.align > CSS_ALIGN_RIGHT || (d.important & ~d.set)) abort();
        break;
    }
    default:
        css_media(in, len, 1920);
        break;
    }
    return 0;
}
