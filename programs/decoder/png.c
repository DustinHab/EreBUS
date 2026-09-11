/*
 * png.c -- png decoding: chunks, the zlib stream inflated, the five filters undone, every colour type to rgb.
 * - bit depths 1, 2, 4, 8 and 16 (16 keeps the high byte); grey, rgb, palette, with and without alpha
 * - alpha is composed onto white: the page's ground is light, and the screen keeps no alpha
 * - interlaced files are refused (rare on the web, and their rows arrive in seven passes)
 */
#include "image.h"
#include <eb/inflate.h>

static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

u32 image_kind(const u8 *in, u32 len)
{
    if (len >= 8 && in[0] == 0x89 && in[1] == 'P' && in[2] == 'N' && in[3] == 'G') return IMAGE_PNG;
    if (len >= 3 && in[0] == 0xFF && in[1] == 0xD8 && in[2] == 0xFF) return IMAGE_JPEG;
    if (len >= 12 && in[0] == 'R' && in[1] == 'I' && in[2] == 'F' && in[3] == 'F' &&
        in[8] == 'W' && in[9] == 'E' && in[10] == 'B' && in[11] == 'P') return IMAGE_WEBP;
    return IMAGE_NONE;
}

static bool png_header(const u8 *in, u32 len, u32 *w, u32 *h, u8 *depth, u8 *ctype, u8 *interlace)
{
    if (image_kind(in, len) != IMAGE_PNG || len < 33) return false;
    if (be32(in + 8) != 13 || in[12] != 'I' || in[13] != 'H' || in[14] != 'D' || in[15] != 'R') return false;
    u32 pw = be32(in + 16), ph = be32(in + 20);
    if (pw == 0 || ph == 0 || pw > 16384 || ph > 16384) return false;
    *w = pw;
    *h = ph;
    *depth = in[24];
    *ctype = in[25];
    *interlace = in[28];
    return true;
}

static u32 channels_of(u8 ctype)
{
    switch (ctype) {
    case 0: return 1;           /* grey */
    case 2: return 3;           /* rgb */
    case 3: return 1;           /* palette */
    case 4: return 2;           /* grey + alpha */
    case 6: return 4;           /* rgb + alpha */
    default: return 0;
    }
}

static u8 paeth(u8 a, u8 b, u8 c)
{
    i32 p = (i32)a + b - c;
    i32 pa = p > a ? p - a : a - p;
    i32 pb = p > b ? p - b : b - p;
    i32 pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

bool png_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                u8 *scratch, u32 scratch_len)
{
    u8 depth, ctype, interlace;
    if (!png_header(in, len, w, h, &depth, &ctype, &interlace)) return false;
    if (interlace) return false;
    u32 ch = channels_of(ctype);
    if (!ch) return false;
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) return false;
    if (depth < 8 && ctype != 0 && ctype != 3) return false;
    if (depth == 16 && ctype == 3) return false;
    if ((u64)*w * *h > max_pixels) return false;

    /* The row: bits per pixel, bytes per row, the filter byte in front. */
    u32 bpp = ch * depth;                            /* bits per pixel */
    u32 stride = (*w * bpp + 7) / 8;
    u32 bstep = bpp >= 8 ? bpp / 8 : 1;              /* bytes back for the filters */
    u64 need = (u64)(stride + 1) * *h;
    if (need > scratch_len) return false;

    /* The chunks: the palette, and the data chunks inflated as one
     * stream. The concatenation is done into the tail of the scratch
     * when the data is split over several chunks; whole in one chunk it
     * is inflated in place. */
    const u8 *plte = NULL; u32 plte_n = 0;
    const u8 *trns = NULL; u32 trns_n = 0;
    u32 at = 8;
    u32 idat_total = 0, idat_chunks = 0;
    const u8 *idat_first = NULL; u32 idat_first_len = 0;
    while (at + 8 <= len) {
        u32 clen = be32(in + at);
        const u8 *ctag = in + at + 4;
        if (clen > len - at - 8) return false;
        const u8 *cdata = in + at + 8;
        if (ctag[0] == 'P' && ctag[1] == 'L' && ctag[2] == 'T' && ctag[3] == 'E') { plte = cdata; plte_n = clen / 3; }
        else if (ctag[0] == 't' && ctag[1] == 'R' && ctag[2] == 'N' && ctag[3] == 'S') { trns = cdata; trns_n = clen; }
        else if (ctag[0] == 'I' && ctag[1] == 'D' && ctag[2] == 'A' && ctag[3] == 'T') {
            if (!idat_chunks) { idat_first = cdata; idat_first_len = clen; }
            idat_total += clen; idat_chunks++;
        }
        else if (ctag[0] == 'I' && ctag[1] == 'E' && ctag[2] == 'N' && ctag[3] == 'D') break;
        at += 12 + clen;                              /* length, tag, data, crc */
    }
    if (!idat_chunks) return false;
    if (ctype == 3 && !plte) return false;

    const u8 *zdata = idat_first;
    u32 zlen = idat_first_len;
    if (idat_chunks > 1) {
        /* Gather the pieces behind the room the rows need. */
        if (need + idat_total > scratch_len) return false;
        u8 *g = scratch + need;
        u32 gat = 0;
        at = 8;
        while (at + 8 <= len) {
            u32 clen = be32(in + at);
            const u8 *ctag = in + at + 4;
            if (clen > len - at - 8) return false;
            if (ctag[0] == 'I' && ctag[1] == 'D' && ctag[2] == 'A' && ctag[3] == 'T') {
                for (u32 i = 0; i < clen; i++) g[gat + i] = in[at + 8 + i];
                gat += clen;
            } else if (ctag[0] == 'I' && ctag[1] == 'E' && ctag[2] == 'N' && ctag[3] == 'D') break;
            at += 12 + clen;
        }
        zdata = g; zlen = gat;
    }

    i64 got = inflate_zlib(zdata, zlen, scratch, (u32)need);
    if (got < (i64)need) return false;

    /* Unfilter, row by row, in place. */
    for (u32 y = 0; y < *h; y++) {
        u8 *row = scratch + (u64)y * (stride + 1);
        u8 filter = row[0];
        u8 *cur = row + 1;
        const u8 *prev = y ? row - (stride + 1) + 1 : NULL;
        for (u32 i = 0; i < stride; i++) {
            u8 a = i >= bstep ? cur[i - bstep] : 0;
            u8 b = prev ? prev[i] : 0;
            u8 c = (prev && i >= bstep) ? prev[i - bstep] : 0;
            switch (filter) {
            case 0: break;
            case 1: cur[i] = (u8)(cur[i] + a); break;
            case 2: cur[i] = (u8)(cur[i] + b); break;
            case 3: cur[i] = (u8)(cur[i] + ((u32)a + b) / 2); break;
            case 4: cur[i] = (u8)(cur[i] + paeth(a, b, c)); break;
            default: return false;
            }
        }
    }

    /* To pixels. */
    for (u32 y = 0; y < *h; y++) {
        const u8 *cur = scratch + (u64)y * (stride + 1) + 1;
        u32 *o = out + (u64)y * *w;
        for (u32 x = 0; x < *w; x++) {
            u32 r = 0, g = 0, b = 0, a = 255;
            if (depth < 8) {
                u32 bit = x * depth;
                u32 v = (cur[bit / 8] >> (8 - depth - (bit % 8))) & ((1u << depth) - 1);
                if (ctype == 3) {
                    if (v >= plte_n) return false;
                    r = plte[v * 3]; g = plte[v * 3 + 1]; b = plte[v * 3 + 2];
                    if (trns && v < trns_n) a = trns[v];
                } else {
                    u32 s = v * 255 / ((1u << depth) - 1);
                    r = g = b = s;
                }
            } else {
                u32 bytes = depth / 8;
                const u8 *p = cur + (u64)x * ch * bytes;
                if (ctype == 0)      { r = g = b = p[0]; }
                else if (ctype == 2) { r = p[0]; g = p[bytes]; b = p[2 * bytes]; }
                else if (ctype == 3) {
                    u32 v = p[0];
                    if (v >= plte_n) return false;
                    r = plte[v * 3]; g = plte[v * 3 + 1]; b = plte[v * 3 + 2];
                    if (trns && v < trns_n) a = trns[v];
                }
                else if (ctype == 4) { r = g = b = p[0]; a = p[bytes]; }
                else                 { r = p[0]; g = p[bytes]; b = p[2 * bytes]; a = p[3 * bytes]; }
            }
            if (a != 255) {                          /* onto white */
                r = (r * a + 255 * (255 - a)) / 255;
                g = (g * a + 255 * (255 - a)) / 255;
                b = (b * a + 255 * (255 - a)) / 255;
            }
            o[x] = (r << 16) | (g << 8) | b;
        }
    }
    return true;
}

/* Only the size: png from the header, jpeg from its frame marker. */
bool image_size(const u8 *in, u32 len, u32 *w, u32 *h)
{
    u32 kind = image_kind(in, len);
    if (kind == IMAGE_PNG) {
        u8 d, c, i;
        return png_header(in, len, w, h, &d, &c, &i);
    }
    if (kind == IMAGE_JPEG) {
        u32 at = 2;
        while (at + 9 < len) {
            if (in[at] != 0xFF) return false;
            u8 m = in[at + 1];
            if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01 || m == 0xFF) { at += 2; continue; }
            u32 seglen = ((u32)in[at + 2] << 8) | in[at + 3];
            if (seglen < 2 || at + 2 + seglen > len) return false;
            if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
                if (seglen < 7) return false;
                u32 jh = ((u32)in[at + 5] << 8) | in[at + 6];
                u32 jw = ((u32)in[at + 7] << 8) | in[at + 8];
                if (jw == 0 || jh == 0 || jw > 16384 || jh > 16384) return false;
                *w = jw; *h = jh;
                return true;
            }
            if (m == 0xDA) return false;
            at += 2 + seglen;
        }
    }
    if (kind == IMAGE_WEBP) return webp_size(in, len, w, h);
    return false;
}
