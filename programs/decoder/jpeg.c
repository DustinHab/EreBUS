/*
 * jpeg.c -- baseline jpeg decoding: huffman, dequantisation, an integer idct, the chroma planes upsampled.
 * - sequential dct, 8 bits, one or three components, sampling factors 1 and 2, restart intervals
 * - progressive and arithmetic files are refused; the caller shows the alternative text
 * - the planes are laid in the scratch the caller lends, then joined to rgb
 */
#include "image.h"

typedef struct {
    u8  bits[17];              /* codes of each length, 1..16 */
    u8  vals[256];
    u16 mincode[17], maxcode[18];
    u8  valptr[17];
    bool present;
} htable;

typedef struct {
    u8  id, h, v, tq;
    u8  td, ta;                /* dc and ac tables, from the scan */
    u8 *plane;                 /* samples, in the scratch */
    u32 pw, ph;                /* plane size, a whole number of blocks */
    i32 dcpred;
} component;

typedef struct {
    const u8 *in; u32 len, at;
    u32 bitbuf, bitcnt;
    bool bad;
    u16 qt[4][64];
    htable dc[4], ac[4];
    component comp[3];
    u32 ncomp;
    u32 w, h;
    u32 hmax, vmax;
    u32 mcux, mcuy;
    u32 restart;
    u8 *scratch; u32 scratch_len, scratch_at;
} jpeg;

static const u8 ZIGZAG[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };

static u8 clamp8(i32 v) { return v < 0 ? 0 : v > 255 ? 255 : (u8)v; }

/* ------------------------------------------------------------------ */
/* Bits from the entropy-coded segment: 0xFF00 is a stuffed 0xFF,     */
/* a marker ends the segment.                                          */
/* ------------------------------------------------------------------ */

static u32 getbit(jpeg *j)
{
    if (j->bitcnt == 0) {
        if (j->at >= j->len) { j->bad = true; return 0; }
        u8 b = j->in[j->at];
        if (b == 0xFF) {
            u8 n = j->at + 1 < j->len ? j->in[j->at + 1] : 0;
            if (n == 0) j->at += 2;
            else { j->bad = true; return 0; }          /* a marker: no more data here */
        } else {
            j->at++;
        }
        j->bitbuf = b;
        j->bitcnt = 8;
    }
    j->bitcnt--;
    return (j->bitbuf >> j->bitcnt) & 1;
}

static u32 getbits(jpeg *j, u32 n)
{
    u32 v = 0;
    for (u32 i = 0; i < n; i++) v = (v << 1) | getbit(j);
    return v;
}

static i32 extend(u32 v, u32 n)
{
    if (n == 0) return 0;
    return (i32)v < (1 << (n - 1)) ? (i32)v - (1 << n) + 1 : (i32)v;
}

static i32 huff_decode(jpeg *j, const htable *t)
{
    i32 code = 0;
    for (u32 l = 1; l <= 16; l++) {
        code = (code << 1) | (i32)getbit(j);
        if (j->bad) return -1;
        if (t->maxcode[l] != 0xFFFF && code <= t->maxcode[l] && code >= t->mincode[l]) {
            u32 idx = t->valptr[l] + (u32)(code - t->mincode[l]);
            if (idx >= 256) return -1;
            return t->vals[idx];
        }
    }
    return -1;
}

static bool build_table(htable *t)
{
    u32 code = 0, k = 0;
    for (u32 l = 1; l <= 16; l++) {
        u32 n = t->bits[l];
        if (n) {
            t->valptr[l] = (u8)k;
            t->mincode[l] = (u16)code;
            code += n;
            k += n;
            t->maxcode[l] = (u16)(code - 1);
        } else {
            t->maxcode[l] = 0xFFFF;
        }
        code <<= 1;
        if (k > 256) return false;
    }
    t->present = true;
    return true;
}

/* ------------------------------------------------------------------ */
/* The inverse transform: separable, fixed point, good enough for a    */
/* screen -- the same arithmetic in both directions.                   */
/* ------------------------------------------------------------------ */

static const i32 COS[8][8] = {   /* cos((2x+1)u*pi/16) * c(u) * 4096 */
    { 2896, 4017, 3784, 3406, 2896, 2276, 1567,  799 },
    { 2896, 3406, 1567, -799, -2896, -4017, -3784, -2276 },
    { 2896, 2276, -1567, -4017, -2896, 799, 3784, 3406 },
    { 2896, 799, -3784, -2276, 2896, 3406, -1567, -4017 },
    { 2896, -799, -3784, 2276, 2896, -3406, -1567, 4017 },
    { 2896, -2276, -1567, 4017, -2896, -799, 3784, -3406 },
    { 2896, -3406, 1567, 799, -2896, 4017, -3784, 2276 },
    { 2896, -4017, 3784, -3406, 2896, -2276, 1567, -799 } };

static void idct8x8(const i32 *in, u8 *out, u32 stride)
{
    i32 tmp[64];
    for (u32 y = 0; y < 8; y++) {           /* rows: over u */
        for (u32 x = 0; x < 8; x++) {
            i64 s = 0;
            for (u32 u = 0; u < 8; u++) s += (i64)COS[x][u] * in[y * 8 + u];
            tmp[y * 8 + x] = (i32)(s >> 12);
        }
    }
    for (u32 x = 0; x < 8; x++) {           /* columns: over v */
        for (u32 y = 0; y < 8; y++) {
            i64 s = 0;
            for (u32 v = 0; v < 8; v++) s += (i64)COS[y][v] * tmp[v * 8 + x];
            out[y * stride + x] = clamp8((i32)((s >> 12) / 4) + 128);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Blocks and the scan                                                 */
/* ------------------------------------------------------------------ */

static bool decode_block(jpeg *j, component *c, u8 *dst, u32 stride)
{
    i32 coef[64];
    for (u32 i = 0; i < 64; i++) coef[i] = 0;

    i32 t = huff_decode(j, &j->dc[c->td]);
    if (t < 0 || t > 11) return false;
    i32 diff = extend(getbits(j, (u32)t), (u32)t);
    if (j->bad) return false;
    c->dcpred += diff;
    coef[0] = c->dcpred * j->qt[c->tq][0];

    for (u32 k = 1; k < 64; ) {
        i32 rs = huff_decode(j, &j->ac[c->ta]);
        if (rs < 0) return false;
        u32 r = (u32)rs >> 4, s = (u32)rs & 15;
        if (s == 0) {
            if (r == 15) { k += 16; continue; }
            break;                                     /* end of block */
        }
        k += r;
        if (k > 63) return false;
        i32 v = extend(getbits(j, s), s);
        if (j->bad) return false;
        coef[ZIGZAG[k]] = v * j->qt[c->tq][k];
        k++;
    }
    idct8x8(coef, dst, stride);
    return true;
}

static bool restart_here(jpeg *j)
{
    j->bitcnt = 0;
    if (j->at + 1 >= j->len) return false;
    if (j->in[j->at] != 0xFF) return false;
    u8 m = j->in[j->at + 1];
    if (m < 0xD0 || m > 0xD7) return false;
    j->at += 2;
    for (u32 i = 0; i < j->ncomp; i++) j->comp[i].dcpred = 0;
    return true;
}

static bool decode_scan(jpeg *j)
{
    u32 mcus = j->mcux * j->mcuy, done = 0;
    for (u32 my = 0; my < j->mcuy; my++) {
        for (u32 mx = 0; mx < j->mcux; mx++) {
            for (u32 ci = 0; ci < j->ncomp; ci++) {
                component *c = &j->comp[ci];
                for (u32 by = 0; by < c->v; by++) {
                    for (u32 bx = 0; bx < c->h; bx++) {
                        u32 px = (mx * c->h + bx) * 8, py = (my * c->v + by) * 8;
                        if (px + 8 > c->pw || py + 8 > c->ph) return false;
                        if (!decode_block(j, c, c->plane + (u64)py * c->pw + px, c->pw)) return false;
                    }
                }
            }
            done++;
            if (j->restart && done < mcus && done % j->restart == 0 && !restart_here(j)) return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------ */

bool jpeg_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                 u8 *scratch, u32 scratch_len)
{
    if (image_kind(in, len) != IMAGE_JPEG) return false;
    static jpeg J;
    jpeg *j = &J;
    for (u32 i = 0; i < sizeof(jpeg); i++) ((u8 *)j)[i] = 0;
    j->in = in; j->len = len; j->at = 2;
    j->scratch = scratch; j->scratch_len = scratch_len;
    bool frame = false;

    while (j->at + 4 <= len) {
        if (in[j->at] != 0xFF) return false;
        u8 m = in[j->at + 1];
        if (m == 0xFF) { j->at++; continue; }
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7)) { j->at += 2; continue; }
        if (m == 0xD9) break;
        u32 seglen = ((u32)in[j->at + 2] << 8) | in[j->at + 3];
        if (seglen < 2 || j->at + 2 + seglen > len) return false;
        const u8 *p = in + j->at + 4;
        u32 plen = seglen - 2;

        if (m == 0xDB) {                                   /* quantisation tables */
            u32 q = 0;
            while (q < plen) {
                u8 pq = p[q] >> 4, tq = p[q] & 15;
                if (tq > 3) return false;
                u32 need = 1 + (pq ? 128 : 64);
                if (q + need > plen) return false;
                for (u32 i = 0; i < 64; i++)
                    j->qt[tq][i] = pq ? (u16)(((u16)p[q + 1 + 2 * i] << 8) | p[q + 2 + 2 * i]) : p[q + 1 + i];
                q += need;
            }
        } else if (m == 0xC4) {                            /* huffman tables */
            u32 q = 0;
            while (q + 17 <= plen) {
                u8 tc = p[q] >> 4, th = p[q] & 15;
                if (th > 3 || tc > 1) return false;
                htable *t = tc ? &j->ac[th] : &j->dc[th];
                u32 total = 0;
                t->bits[0] = 0;
                for (u32 i = 1; i <= 16; i++) { t->bits[i] = p[q + i]; total += p[q + i]; }
                if (total > 256 || q + 17 + total > plen) return false;
                for (u32 i = 0; i < total; i++) t->vals[i] = p[q + 17 + i];
                if (!build_table(t)) return false;
                q += 17 + total;
            }
        } else if (m == 0xC0 || m == 0xC1) {               /* baseline / extended sequential */
            if (frame || plen < 6) return false;
            if (p[0] != 8) return false;
            j->h = ((u32)p[1] << 8) | p[2];
            j->w = ((u32)p[3] << 8) | p[4];
            j->ncomp = p[5];
            if (!j->w || !j->h || j->w > 16384 || j->h > 16384) return false;
            if (j->ncomp != 1 && j->ncomp != 3) return false;
            if (plen < 6 + 3 * j->ncomp) return false;
            if ((u64)j->w * j->h > max_pixels) return false;
            j->hmax = j->vmax = 1;
            for (u32 i = 0; i < j->ncomp; i++) {
                component *c = &j->comp[i];
                c->id = p[6 + 3 * i];
                c->h = p[7 + 3 * i] >> 4;
                c->v = p[7 + 3 * i] & 15;
                c->tq = p[8 + 3 * i];
                if (c->h < 1 || c->h > 2 || c->v < 1 || c->v > 2 || c->tq > 3) return false;
                if (c->h > j->hmax) j->hmax = c->h;
                if (c->v > j->vmax) j->vmax = c->v;
            }
            j->mcux = (j->w + 8 * j->hmax - 1) / (8 * j->hmax);
            j->mcuy = (j->h + 8 * j->vmax - 1) / (8 * j->vmax);
            for (u32 i = 0; i < j->ncomp; i++) {
                component *c = &j->comp[i];
                c->pw = j->mcux * c->h * 8;
                c->ph = j->mcuy * c->v * 8;
                u64 bytes = (u64)c->pw * c->ph;
                if (j->scratch_at + bytes > j->scratch_len) return false;
                c->plane = j->scratch + j->scratch_at;
                j->scratch_at += (u32)bytes;
            }
            frame = true;
        } else if (m == 0xC2 || m == 0xC3 || (m >= 0xC5 && m <= 0xCF && m != 0xC8 && m != 0xCC)) {
            return false;                                  /* progressive, lossless, arithmetic */
        } else if (m == 0xDD) {                            /* restart interval */
            if (plen < 2) return false;
            j->restart = ((u32)p[0] << 8) | p[1];
        } else if (m == 0xDA) {                            /* the scan */
            if (!frame || plen < 1) return false;
            u32 ns = p[0];
            if (ns != j->ncomp || plen < 1 + 2 * ns + 3) return false;
            for (u32 i = 0; i < ns; i++) {
                u8 id = p[1 + 2 * i], tt = p[2 + 2 * i];
                component *c = NULL;
                for (u32 k = 0; k < j->ncomp; k++) if (j->comp[k].id == id) c = &j->comp[k];
                if (!c) return false;
                c->td = tt >> 4; c->ta = tt & 15;
                if (c->td > 3 || c->ta > 3) return false;
                if (!j->dc[c->td].present || !j->ac[c->ta].present) return false;
            }
            j->at += 2 + seglen;
            j->bitcnt = 0;
            if (!decode_scan(j)) return false;
            break;                                         /* one scan is the whole picture */
        }
        j->at += 2 + seglen;
    }
    if (!frame) return false;

    /* The planes to pixels: chroma planes are sampled at their own
     * factors, the nearest sample is taken. */
    *w = j->w; *h = j->h;
    for (u32 y = 0; y < j->h; y++) {
        u32 *o = out + (u64)y * j->w;
        for (u32 x = 0; x < j->w; x++) {
            i32 Y, Cb = 128, Cr = 128;
            const component *c0 = &j->comp[0];
            Y = c0->plane[(u64)(y * c0->v / j->vmax) * c0->pw + (x * c0->h / j->hmax)];
            if (j->ncomp == 3) {
                const component *c1 = &j->comp[1], *c2 = &j->comp[2];
                Cb = c1->plane[(u64)(y * c1->v / j->vmax) * c1->pw + (x * c1->h / j->hmax)];
                Cr = c2->plane[(u64)(y * c2->v / j->vmax) * c2->pw + (x * c2->h / j->hmax)];
            }
            i32 r = Y + ((91881 * (Cr - 128)) >> 16);
            i32 g = Y - ((22554 * (Cb - 128) + 46802 * (Cr - 128)) >> 16);
            i32 b = Y + ((116130 * (Cb - 128)) >> 16);
            o[x] = ((u32)clamp8(r) << 16) | ((u32)clamp8(g) << 8) | clamp8(b);
        }
    }
    return true;
}
