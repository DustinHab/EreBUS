/*
 * img_fuzz.c -- libFuzzer entry for the picture decoders and inflate: bytes as a png, a jpeg, a gzip stream.
 * - the first byte picks the target; the rest is the input
 * - the decoders answer into fixed room and must never write past it or read past the input
 * - built and run by tools/fuzz/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/image.h>
#include <eb/inflate.h>

void kprintf(const char *fmt, ...) { (void)fmt; }

#define PIXELS (1600u * 1200u)
static uint32_t *pixels;
static uint8_t *scratch;
static uint8_t *out;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 4u << 20) return 0;
    if (!pixels) {
        pixels = malloc(PIXELS * 4);
        scratch = malloc(8u << 20);
        out = malloc(4u << 20);
    }
    uint8_t pick = data[0];
    const uint8_t *in = data + 1;
    uint32_t len = (uint32_t)(size - 1);
    uint32_t w = 0, h = 0;
    bool ok = false;

    switch (pick % 4) {
    case 0: ok = png_decode(in, len, pixels, PIXELS, &w, &h, scratch, 8u << 20); break;
    case 1: ok = jpeg_decode(in, len, pixels, PIXELS, &w, &h, scratch, 8u << 20); break;
    case 2: inflate_gzip(in, len, out, 4u << 20); break;
    default: inflate_zlib(in, len, out, 4u << 20); ok = image_size(in, len, &w, &h); break;
    }
    /* A size that was read is bounded; a picture that was decoded fits the room given. */
    if (ok && (w == 0 || h == 0 || w > 16384 || h > 16384)) abort();
    if (ok && pick % 4 < 2 && (uint64_t)w * h > PIXELS) abort();
    return 0;
}
