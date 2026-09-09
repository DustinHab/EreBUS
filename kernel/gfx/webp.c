/*
 * webp.c -- webp decoding into 0x00RRGGBB pixels: the RIFF container, lossless (VP8L) and lossy (VP8) still frames.
 * - VP8L: the bit stream (least significant bit first), canonical Huffman built from the stream, meta-Huffman groups,
 *   a colour cache, LZ77 back references, and the four transforms (predictor, colour, subtract-green, colour index)
 * - VP8: the boolean entropy decoder, per-macroblock intra prediction (16x16, 8x8 chroma, 4x4), the token tree with
 *   the default probabilities, dequantisation, the inverse DCT and Walsh-Hadamard transforms, the loop filter
 * - the alpha of an extended file (ALPH) is composed onto white, as png's is; animation is not read (the first frame)
 * - hostile input: every length, count and index is bounded; too little scratch, or a file that lies, answers false
 * - scratch is the caller's room: VP8L needs one argb image (4 * w * h) plus its transforms' small images; VP8 needs
 *   the three reconstructed planes and a little context. Too little of it is false.
 */
#include <eb/image.h>
#include <eb/string.h>

/* ================================================================== */
/* The container                                                       */
/* ================================================================== */

static bool vp8_decode(const u8 *body, u32 blen, u32 *out, u32 max_pixels,
                       u32 *ow, u32 *oh, u8 *scratch, u32 scratch_len);

static u32 le32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

/* Finds the picture chunk. Answers its tag ("VP8 ", "VP8L", or 0 when
 * none), where its body starts, and its length; reads past VP8X and
 * ALPH. alpha_off/alpha_len name an ALPH chunk when one stands. */
static u32 webp_chunk(const u8 *in, u32 len, u32 *body, u32 *blen,
                      u32 *alpha_off, u32 *alpha_len)
{
    *alpha_off = 0; *alpha_len = 0;
    if (len < 12 || in[0] != 'R' || in[1] != 'I' || in[2] != 'F' || in[3] != 'F') return 0;
    if (in[8] != 'W' || in[9] != 'E' || in[10] != 'B' || in[11] != 'P') return 0;
    u32 riff = le32(in + 4);
    u32 end = 8 + riff;
    if (end > len) end = len;
    u32 at = 12;
    while (at + 8 <= end) {
        u32 tag0 = ((u32)in[at] << 24) | ((u32)in[at+1] << 16) | ((u32)in[at+2] << 8) | in[at+3];
        u32 sz = le32(in + at + 4);
        u32 payload = at + 8;
        if (sz > end - payload) sz = end - payload;
        if (tag0 == 0x56503820u /* "VP8 " */ || tag0 == 0x5650384Cu /* "VP8L" */) {
            *body = payload; *blen = sz;
            return tag0;
        }
        if (tag0 == 0x414C5048u /* "ALPH" */) { *alpha_off = payload; *alpha_len = sz; }
        /* "VP8X", "ANIM", "ICCP", "EXIF", "XMP " and the rest are stepped over */
        at = payload + sz + (sz & 1);           /* chunks are padded to even */
    }
    return 0;
}

/* ================================================================== */
/* VP8L: the lossless bit stream                                       */
/* ================================================================== */

typedef struct {
    const u8 *buf;
    u32 len;
    u32 pos;            /* byte position */
    u32 bits;           /* the bit buffer */
    u32 nbits;          /* how many bits it holds */
    bool bad;           /* read past the end */
} lbits;

static void lb_init(lbits *b, const u8 *buf, u32 len)
{
    b->buf = buf; b->len = len; b->pos = 0; b->bits = 0; b->nbits = 0; b->bad = false;
}

/* n bits, least significant first, 0..24 at a time. */
static u32 lb_get(lbits *b, u32 n)
{
    while (b->nbits < n) {
        u32 byte = b->pos < b->len ? b->buf[b->pos] : 0;
        if (b->pos >= b->len) b->bad = true;
        b->pos++;
        b->bits |= byte << b->nbits;
        b->nbits += 8;
    }
    u32 v = b->bits & ((n < 32 ? (1u << n) : 0u) - 1u);
    b->bits >>= n;
    b->nbits -= n;
    return v;
}

/* ---- canonical Huffman, built from code lengths ---- */

#define HUFF_MAX_SYMS 2328          /* 256 + 24 green codes at most, and 40 for distance */

typedef struct {
    /* a table sorted so a symbol is found by walking the code; small
     * enough to search by the canonical method without a fast lookup */
    u16 counts[16];                 /* how many codes of each length */
    u16 symbols[HUFF_MAX_SYMS];     /* the symbols, in code order */
    u32 nsym;
    bool single;                    /* one symbol, no bits read */
    u16  single_sym;
} huff;

static bool huff_build(huff *h, const u8 *lengths, u32 n)
{
    for (u32 i = 0; i < 16; i++) h->counts[i] = 0;
    for (u32 i = 0; i < n; i++) if (lengths[i]) {
        if (lengths[i] >= 16) return false;
        h->counts[lengths[i]]++;
    }
    if (h->counts[0] == n) return false;                /* no codes at all */
    /* a well-formed set: the codes fill the tree exactly, or there is
     * a single code (allowed) */
    u32 left = 1;
    for (u32 l = 1; l < 16; l++) {
        left <<= 1;
        if (h->counts[l] > left) return false;
        left -= h->counts[l];
    }
    /* offsets into the symbol list per length */
    u16 offs[16];
    offs[1] = 0;
    for (u32 l = 1; l < 15; l++) offs[l + 1] = (u16)(offs[l] + h->counts[l]);
    h->nsym = 0;
    for (u32 i = 0; i < n; i++) if (lengths[i]) h->symbols[offs[lengths[i]]++] = (u16)i;
    for (u32 l = 1; l < 16; l++) h->nsym += h->counts[l];
    return h->nsym > 0;
}

static u32 huff_read(huff *h, lbits *b)
{
    i32 code = 0, first = 0, index = 0;
    for (u32 l = 1; l < 16; l++) {
        code |= (i32)lb_get(b, 1);
        u32 count = h->counts[l];
        if ((u32)(code - first) < count) return h->symbols[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
        if (b->bad) return 0;
    }
    b->bad = true;
    return 0;
}

/* the order the 19 code-length-code lengths arrive in */
static const u8 kLenOrder[19] = {
    17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/* Reads one Huffman code (its lengths) from the stream into h. alpha is
 * the count of symbols this code spans. */
static bool read_huffman(huff *h, lbits *b, u32 nsyms, u8 *lens)
{
    if (lb_get(b, 1)) {
        /* the simple code: one or two symbols */
        u32 num = lb_get(b, 1) + 1;
        u32 first_bits = lb_get(b, 1) ? 8 : 1;
        for (u32 i = 0; i < nsyms; i++) lens[i] = 0;
        u32 s0 = lb_get(b, first_bits);
        if (s0 >= nsyms) return false;
        u8 two[2] = { 0, 0 };
        two[0] = (u8)s0;
        u32 count = 1;
        if (num == 2) {
            u32 s1 = lb_get(b, 8);
            if (s1 >= nsyms) return false;
            two[1] = (u8)s1;
            count = 2;
        }
        (void)lens;
        if (count == 1) {
            /* one symbol: no bits are read for it, ever */
            h->single = true;
            h->single_sym = two[0];
            return true;
        }
        /* two symbols, each a one-bit code */
        for (u32 i = 0; i < 16; i++) h->counts[i] = 0;
        h->counts[1] = 2;
        h->symbols[0] = two[0];
        h->symbols[1] = two[1];
        h->nsym = 2;
        h->single = false;
        return true;
    }

    /* the normal code: the code-length code first */
    u8 clen[19];
    for (u32 i = 0; i < 19; i++) clen[i] = 0;
    u32 num_codes = lb_get(b, 4) + 4;
    if (num_codes > 19) return false;
    for (u32 i = 0; i < num_codes; i++) clen[kLenOrder[i]] = (u8)lb_get(b, 3);
    huff clh;
    clh.single = false;
    if (!huff_build(&clh, clen, 19)) return false;

    /* then the symbol lengths, with the repeat codes 16, 17, 18 */
    u32 max_symbol = nsyms;
    if (lb_get(b, 1)) {
        u32 len_bits = 2 + 2 * lb_get(b, 3);
        max_symbol = 2 + lb_get(b, len_bits);
        if (max_symbol > nsyms) max_symbol = nsyms;
    }
    for (u32 i = 0; i < nsyms; i++) lens[i] = 0;
    u32 i = 0, prev = 8;
    u32 symbols_left = max_symbol;
    while (i < nsyms) {
        if (!symbols_left) break;
        symbols_left--;
        u32 sym = huff_read(&clh, b);
        if (b->bad) return false;
        if (sym < 16) { lens[i++] = (u8)sym; if (sym) prev = sym; }
        else {
            u32 rep, val;
            if (sym == 16) { rep = 3 + lb_get(b, 2); val = prev; }
            else if (sym == 17) { rep = 3 + lb_get(b, 3); val = 0; }
            else { rep = 11 + lb_get(b, 7); val = 0; }
            if (i + rep > nsyms) rep = nsyms - i;
            for (u32 k = 0; k < rep; k++) lens[i++] = (u8)val;
        }
    }
    h->single = false;
    return huff_build(h, lens, nsyms);
}

static u32 huff_sym(huff *h, lbits *b)
{
    if (h->single) return h->single_sym;
    return huff_read(h, b);
}

/* ---- VP8L pixel decode ---- */

/* the five Huffman codes a group holds */
enum { HG_GREEN = 0, HG_RED = 1, HG_BLUE = 2, HG_ALPHA = 3, HG_DIST = 4 };
typedef struct { huff code[5]; } hgroup;

/* the number of extra symbols a green code carries beyond 256 literals:
 * 24 length codes; plus the colour cache when one is used */
#define NUM_LENGTH_CODES 24
#define NUM_DIST_CODES   40

/* the distance of a "plane code": maps the short codes to nearby pixels */
static const u8 kDistMap[120] = {
    0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a, 0x26, 0x2a,
    0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a, 0x25, 0x2b, 0x48, 0x04,
    0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b, 0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45,
    0x4b, 0x34, 0x3c, 0x03, 0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d,
    0x44, 0x4c, 0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
    0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b, 0x32, 0x3e,
    0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f, 0x64, 0x6c, 0x42, 0x4e,
    0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b, 0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e,
    0x00, 0x74, 0x7c, 0x41, 0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d,
    0x51, 0x5f, 0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

/* the length or distance a code names: base value and extra bits. */
static u32 prefix_value(lbits *b, u32 code)
{
    if (code < 4) return code + 1;
    u32 extra = (code - 2) >> 1;
    u32 offset = (2 + (code & 1)) << extra;
    return offset + lb_get(b, extra) + 1;
}

static u32 dist_to_pixel(u32 dist_code, u32 xsize)
{
    if (dist_code > 120) return dist_code - 120;         /* a plain distance */
    u32 m = kDistMap[dist_code - 1];
    i32 yoff = (i32)(m >> 4);
    i32 xoff = 8 - (i32)(m & 0xf);
    i32 d = yoff * (i32)xsize + xoff;
    return d < 1 ? 1 : (u32)d;
}

/* ---- one entropy-coded image into argb ---- */

typedef struct {
    /* the meta-huffman selector */
    const u32 *meta;            /* the entropy image (group index in the green channel), or NULL */
    u32 meta_bits;
    u32 meta_xsize;
    hgroup *groups;
    u32 ngroups;
    /* the colour cache */
    u32 *cache;
    u32 cache_bits;
} vp8l_dec;

static u32 group_of(const vp8l_dec *d, u32 x, u32 y)
{
    if (!d->meta) return 0;
    u32 mx = x >> d->meta_bits, my = y >> d->meta_bits;
    u32 idx = (d->meta[my * d->meta_xsize + mx] >> 8) & 0xffff;
    return idx < d->ngroups ? idx : 0;
}

/* Decodes an entropy-coded argb image of xsize*ysize into out. Uses the
 * groups, cache and (for the top image) meta selector in d. Returns
 * false on a malformed stream. */
static bool decode_image(lbits *b, vp8l_dec *d, u32 *out, u32 xsize, u32 ysize)
{
    u64 total = (u64)xsize * ysize;
    u32 cache_size = d->cache_bits ? (1u << d->cache_bits) : 0;
    u32 pos = 0;
    u32 x = 0, y = 0;
    while (pos < total) {
        hgroup *g = &d->groups[group_of(d, x, y)];
        u32 s = huff_sym(&g->code[HG_GREEN], b);
        if (b->bad) return false;
        if (s < 256) {
            u32 red = huff_sym(&g->code[HG_RED], b);
            u32 blue = huff_sym(&g->code[HG_BLUE], b);
            u32 alpha = huff_sym(&g->code[HG_ALPHA], b);
            if (b->bad) return false;
            u32 argb = (alpha << 24) | (red << 16) | (s << 8) | blue;
            out[pos++] = argb;
            if (cache_size) d->cache[(0x1e35a7bdu * argb) >> (32 - d->cache_bits)] = argb;
            if (++x == xsize) { x = 0; y++; }
        } else if (s < 256 + NUM_LENGTH_CODES) {
            u32 length = prefix_value(b, s - 256);
            u32 dcode = huff_sym(&g->code[HG_DIST], b);
            if (b->bad) return false;
            u32 dist = dist_to_pixel(prefix_value(b, dcode), xsize);
            if (dist > pos || pos + length > total) return false;
            for (u32 i = 0; i < length; i++) {
                u32 argb = out[pos - dist];
                out[pos++] = argb;
                if (cache_size) d->cache[(0x1e35a7bdu * argb) >> (32 - d->cache_bits)] = argb;
            }
            x = (u32)(pos % xsize); y = (u32)(pos / xsize);
        } else {
            u32 idx = s - 256 - NUM_LENGTH_CODES;
            if (!cache_size || idx >= cache_size) return false;
            u32 argb = d->cache[idx];
            out[pos++] = argb;
            /* the looked-up pixel is itself entered, as the encoder's
             * cache advanced past it on the next step */
            d->cache[(0x1e35a7bdu * argb) >> (32 - d->cache_bits)] = argb;
            if (++x == xsize) { x = 0; y++; }
        }
    }
    return true;
}

/* ---- a bump allocator over the caller's scratch ---- */

typedef struct { u8 *base; u32 len, used; } arena;

static void *arena_take(arena *a, u32 bytes)
{
    bytes = (bytes + 7) & ~7u;
    if (bytes > a->len - a->used) return 0;
    void *p = a->base + a->used;
    a->used += bytes;
    return p;
}

/* ---- reading the Huffman codes of an image stream ---- */

/* the number of symbols each of the five codes spans */
static void code_sizes(u32 cache_bits, u32 sizes[5])
{
    sizes[HG_GREEN] = 256 + NUM_LENGTH_CODES + (cache_bits ? (1u << cache_bits) : 0);
    sizes[HG_RED]   = 256;
    sizes[HG_BLUE]  = 256;
    sizes[HG_ALPHA] = 256;
    sizes[HG_DIST]  = NUM_DIST_CODES;
}

static bool read_groups(lbits *b, arena *a, u32 ngroups, u32 cache_bits, hgroup **out)
{
    hgroup *g = arena_take(a, ngroups * (u32)sizeof(hgroup));
    if (!g) return false;
    static u8 lens[2328];
    u32 sizes[5];
    code_sizes(cache_bits, sizes);
    for (u32 i = 0; i < ngroups; i++)
        for (u32 c = 0; c < 5; c++)
            if (!read_huffman(&g[i].code[c], b, sizes[c], lens)) return false;
    *out = g;
    return true;
}

/* Decodes an image stream: the colour cache, the meta-huffman (only at
 * the top), the groups, then the pixels. */
static bool decode_stream(lbits *b, arena *a, u32 *out, u32 xsize, u32 ysize, bool top)
{
    vp8l_dec d;
    d.meta = 0; d.meta_bits = 0; d.meta_xsize = 0; d.cache = 0; d.cache_bits = 0;

    if (lb_get(b, 1)) {
        d.cache_bits = lb_get(b, 4);
        if (d.cache_bits < 1 || d.cache_bits > 11) return false;
        d.cache = arena_take(a, (1u << d.cache_bits) * 4);
        if (!d.cache) return false;
        for (u32 i = 0; i < (1u << d.cache_bits); i++) d.cache[i] = 0;
    }

    u32 ngroups = 1;
    if (top && lb_get(b, 1)) {
        d.meta_bits = lb_get(b, 3) + 2;
        u32 mx = (xsize + (1u << d.meta_bits) - 1) >> d.meta_bits;
        u32 my = (ysize + (1u << d.meta_bits) - 1) >> d.meta_bits;
        u32 *ent = arena_take(a, mx * my * 4);
        if (!ent) return false;
        if (!decode_stream(b, a, ent, mx, my, false)) return false;
        d.meta = ent; d.meta_xsize = mx;
        u32 maxidx = 0;
        for (u32 i = 0; i < mx * my; i++) { u32 v = (ent[i] >> 8) & 0xffff; if (v > maxidx) maxidx = v; }
        ngroups = maxidx + 1;
        if (ngroups > 1024) return false;
    }

    if (!read_groups(b, a, ngroups, d.cache_bits, &d.groups)) return false;
    d.ngroups = ngroups;
    return decode_image(b, &d, out, xsize, ysize);
}

/* ---- the inverse transforms ---- */

static u8 clamp8(i32 v) { return v < 0 ? 0 : v > 255 ? 255 : (u8)v; }

static u8 avg2(u8 a, u8 b) { return (u8)((a + b) / 2); }

/* the fourteen predictors, each taking left (L), top (T), top-left (TL),
 * top-right (TR) argb and answering the predicted argb */
static u32 predict(u32 mode, u32 L, u32 T, u32 TL, u32 TR)
{
    u8 lb[4] = { (u8)L, (u8)(L>>8), (u8)(L>>16), (u8)(L>>24) };
    u8 tb[4] = { (u8)T, (u8)(T>>8), (u8)(T>>16), (u8)(T>>24) };
    u8 tlb[4] = { (u8)TL, (u8)(TL>>8), (u8)(TL>>16), (u8)(TL>>24) };
    u8 trb[4] = { (u8)TR, (u8)(TR>>8), (u8)(TR>>16), (u8)(TR>>24) };
    u8 o[4];
    switch (mode) {
    case 0: return 0xff000000u;                          /* black, opaque */
    case 1: return L;
    case 2: return T;
    case 3: return TR;
    case 4: return TL;
    case 5: for (u32 i=0;i<4;i++) o[i]=avg2(avg2(lb[i],trb[i]),tb[i]); break;
    case 6: for (u32 i=0;i<4;i++) o[i]=avg2(lb[i],tlb[i]); break;
    case 7: for (u32 i=0;i<4;i++) o[i]=avg2(lb[i],tb[i]); break;
    case 8: for (u32 i=0;i<4;i++) o[i]=avg2(tlb[i],tb[i]); break;
    case 9: for (u32 i=0;i<4;i++) o[i]=avg2(tb[i],trb[i]); break;
    case 10: for (u32 i=0;i<4;i++) o[i]=avg2(avg2(lb[i],tlb[i]),avg2(tb[i],trb[i])); break;
    case 11: {                                           /* select */
        i32 pl=0,pt=0;
        for (u32 i=0;i<4;i++){ i32 p=(i32)lb[i]+tb[i]-tlb[i]; pl+= p>lb[i]?p-lb[i]:lb[i]-p; pt+= p>tb[i]?p-tb[i]:tb[i]-p; }
        return pl < pt ? L : T; }
    case 12: for (u32 i=0;i<4;i++) o[i]=clamp8((i32)lb[i]+tb[i]-tlb[i]); break;
    case 13: for (u32 i=0;i<4;i++){ i32 g=avg2(lb[i],tb[i]); o[i]=clamp8(g + (g-(i32)tlb[i])/2); } break;
    default: return T;
    }
    return (u32)o[0] | ((u32)o[1]<<8) | ((u32)o[2]<<16) | ((u32)o[3]<<24);
}

static void inverse_predictor(u32 *img, u32 w, u32 h, const u32 *modes, u32 bits)
{
    u32 mw = (w + (1u << bits) - 1) >> bits;
    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            u32 *p = &img[y * w + x];
            u32 mode;
            if (x == 0 && y == 0) mode = 0xff;            /* opaque black special-cased below */
            else if (y == 0) mode = 1;                    /* left */
            else if (x == 0) mode = 2;                    /* top */
            else mode = (modes[(y >> bits) * mw + (x >> bits)] >> 8) & 0xff;
            u32 pred;
            if (x == 0 && y == 0) pred = 0xff000000u;
            else if (y == 0) pred = p[-1];
            else if (x == 0) pred = p[-(i32)w];
            else {
                /* top-right at the last column reads the current row's
                 * first pixel, contiguous in memory, as the encoder did */
                u32 L = p[-1], T = p[-(i32)w], TL = p[-(i32)w - 1], TR = p[-(i32)w + 1];
                pred = predict(mode, L, T, TL, TR);
            }
            u8 a = (u8)((*p >> 24) + (pred >> 24));
            u8 r = (u8)((*p >> 16) + (pred >> 16));
            u8 gg = (u8)((*p >> 8) + (pred >> 8));
            u8 bb = (u8)((*p) + pred);
            *p = ((u32)a << 24) | ((u32)r << 16) | ((u32)gg << 8) | bb;
        }
    }
}

static void inverse_color(u32 *img, u32 w, u32 h, const u32 *cd, u32 bits)
{
    u32 mw = (w + (1u << bits) - 1) >> bits;
    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            u32 *p = &img[y * w + x];
            u32 c = cd[(y >> bits) * mw + (x >> bits)];
            i8 g2r = (i8)(c & 0xff);
            i8 g2b = (i8)((c >> 8) & 0xff);
            i8 r2b = (i8)((c >> 16) & 0xff);
            i32 red = (i8)(*p >> 16);
            i32 green = (i8)(*p >> 8);
            i32 blue = (i8)(*p);
            red  += ((i32)g2r * green) >> 5;
            blue += ((i32)g2b * green) >> 5;
            blue += ((i32)r2b * (i8)(red & 0xff)) >> 5;
            u8 r = (u8)(red & 0xff), b8 = (u8)(blue & 0xff);
            *p = (*p & 0xff00ff00u) | ((u32)r << 16) | b8;
        }
    }
}

static void inverse_green(u32 *img, u32 w, u32 h)
{
    for (u64 i = 0; i < (u64)w * h; i++) {
        u32 p = img[i];
        u8 g = (u8)(p >> 8);
        u8 r = (u8)((p >> 16) + g);
        u8 b = (u8)(p + g);
        img[i] = (p & 0xff00ff00u) | ((u32)r << 16) | b;
    }
}

/* expands a colour-indexed image: the green channel is an index (packed
 * several to a pixel when the palette is small) into the palette. */
static void inverse_index(u32 *img, u32 full_w, u32 packed_w, u32 h,
                          const u32 *palette, u32 ncolors, u32 bits)
{
    u32 per = 1u << bits;                    /* indices packed per stored pixel */
    u32 width_bits = 8u >> bits;             /* bits each index takes */
    u32 mask = (1u << width_bits) - 1;
    for (i32 y = (i32)h - 1; y >= 0; y--) {
        for (i32 x = (i32)full_w - 1; x >= 0; x--) {
            u32 packed = img[(u32)y * packed_w + ((u32)x / per)];
            u32 idx = ((packed >> 8) >> (width_bits * ((u32)x & (per - 1)))) & mask;
            if (idx >= ncolors) idx = 0;
            img[(u32)y * full_w + (u32)x] = palette[idx];
        }
    }
}

/* ---- VP8L top level ---- */

#define TF_PRED  0
#define TF_COLOR 1
#define TF_GREEN 2
#define TF_INDEX 3

typedef struct {
    u8   type;
    u8   bits;
    u32 *data;              /* pred/color sub-image, or the palette */
    u32  ncolors;           /* index: palette size */
    u32  index_bits;        /* index: bits per stored pixel group */
    u32  full_w;            /* index: the width before packing */
} transform;

static bool vp8l_decode(const u8 *body, u32 blen, u32 *out, u32 max_pixels,
                        u32 *ow, u32 *oh, u8 *scratch, u32 scratch_len)
{
    lbits b;
    lb_init(&b, body, blen);
    if (lb_get(&b, 8) != 0x2f) return false;             /* the VP8L signature */
    u32 w = lb_get(&b, 14) + 1;
    u32 h = lb_get(&b, 14) + 1;
    (void)lb_get(&b, 1);                                 /* alpha_is_used */
    if (lb_get(&b, 3) != 0) return false;                /* version must be 0 */
    if (w == 0 || h == 0 || w > 16384 || h > 16384 || (u64)w * h > max_pixels) return false;

    arena a = { scratch, scratch_len, 0 };
    transform tf[4];
    u32 ntf = 0;
    u8 seen = 0;
    u32 cur_w = w;

    while (lb_get(&b, 1)) {
        if (ntf >= 4 || b.bad) return false;
        u32 type = lb_get(&b, 2);
        if (seen & (1u << type)) return false;           /* each kind once */
        seen |= (u8)(1u << type);
        transform *t = &tf[ntf++];
        t->type = (u8)type; t->bits = 0; t->data = 0; t->ncolors = 0; t->index_bits = 0; t->full_w = cur_w;
        if (type == TF_PRED || type == TF_COLOR) {
            t->bits = (u8)(lb_get(&b, 3) + 2);
            u32 sw = (cur_w + (1u << t->bits) - 1) >> t->bits;
            u32 sh = (h + (1u << t->bits) - 1) >> t->bits;
            t->data = arena_take(&a, sw * sh * 4);
            if (!t->data) return false;
            if (!decode_stream(&b, &a, t->data, sw, sh, false)) return false;
        } else if (type == TF_INDEX) {
            u32 nc = lb_get(&b, 8) + 1;
            t->ncolors = nc;
            t->data = arena_take(&a, nc * 4);
            if (!t->data) return false;
            if (!decode_stream(&b, &a, t->data, nc, 1, false)) return false;
            /* the palette is stored as deltas along the row */
            for (u32 i = 1; i < nc; i++) {
                u32 p = t->data[i], q = t->data[i - 1];
                t->data[i] = ((((p >> 24) + (q >> 24)) & 0xff) << 24) |
                             (((((p >> 16) + (q >> 16)) & 0xff)) << 16) |
                             (((((p >> 8) + (q >> 8)) & 0xff)) << 8) |
                             (((p + q) & 0xff));
            }
            u32 bits = nc > 16 ? 0 : nc > 4 ? 1 : nc > 2 ? 2 : 3;
            t->bits = (u8)bits;
            t->index_bits = bits;
            t->full_w = cur_w;
            cur_w = (cur_w + (1u << bits) - 1) >> bits;   /* the stored image is narrower */
        }
    }
    if (b.bad) return false;

    /* the main image, at the width colour-indexing left */
    if (!decode_stream(&b, &a, out, cur_w, h, true)) return false;

    /* the inverse transforms, last read first */
    u32 work_w = cur_w;
    for (i32 n = (i32)ntf - 1; n >= 0; n--) {
        transform *t = &tf[n];
        if (t->type == TF_GREEN) inverse_green(out, work_w, h);
        else if (t->type == TF_COLOR) inverse_color(out, work_w, h, t->data, t->bits);
        else if (t->type == TF_PRED) inverse_predictor(out, work_w, h, t->data, t->bits);
        else if (t->type == TF_INDEX) {
            inverse_index(out, t->full_w, work_w, h, t->data, t->ncolors, t->index_bits);
            work_w = t->full_w;
        }
    }

    /* argb -> 0x00RRGGBB, alpha composed onto white */
    for (u64 i = 0; i < (u64)w * h; i++) {
        u32 p = out[i];
        u32 al = p >> 24, r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, bl = p & 0xff;
        if (al != 255) {
            r = (r * al + 255 * (255 - al)) / 255;
            g = (g * al + 255 * (255 - al)) / 255;
            bl = (bl * al + 255 * (255 - al)) / 255;
        }
        out[i] = (r << 16) | (g << 8) | bl;
    }
    *ow = w; *oh = h;
    return true;
}

/* ================================================================== */
/* Size and kind                                                       */
/* ================================================================== */

bool webp_size(const u8 *in, u32 len, u32 *w, u32 *h)
{
    u32 body, blen, ao, al;
    u32 tag = webp_chunk(in, len, &body, &blen, &ao, &al);
    if (tag == 0x5650384Cu) {                            /* VP8L */
        if (blen < 5 || in[body] != 0x2f) return false;
        u32 bits = (u32)in[body+1] | ((u32)in[body+2] << 8) | ((u32)in[body+3] << 16) | ((u32)in[body+4] << 24);
        *w = (bits & 0x3fff) + 1;
        *h = ((bits >> 14) & 0x3fff) + 1;
        return *w <= 16384 && *h <= 16384;
    }
    if (tag == 0x56503820u) {                            /* VP8 */
        if (blen < 10) return false;
        const u8 *f = in + body;
        if (f[3] != 0x9d || f[4] != 0x01 || f[5] != 0x2a) return false;
        *w = ((u32)f[6] | ((u32)f[7] << 8)) & 0x3fff;
        *h = ((u32)f[8] | ((u32)f[9] << 8)) & 0x3fff;
        return *w && *h && *w <= 16384 && *h <= 16384;
    }
    return false;
}

bool webp_decode(const u8 *in, u32 len, u32 *out, u32 max_pixels, u32 *w, u32 *h,
                 u8 *scratch, u32 scratch_len)
{
    u32 body, blen, ao, al;
    u32 tag = webp_chunk(in, len, &body, &blen, &ao, &al);
    if (tag == 0x5650384Cu) return vp8l_decode(in + body, blen, out, max_pixels, w, h, scratch, scratch_len);
    if (tag == 0x56503820u) return vp8_decode(in + body, blen, out, max_pixels, w, h, scratch, scratch_len);
    return false;
}

/* ================================================================== */
/* VP8: the lossy still frame (RFC 6386, an intra key frame)           */
/* ================================================================== */

#include "webp_vp8_tables.h"

#define BPS      32                             /* the per-block cache stride */
#define Y_OFF    (BPS * 1 + 8)
#define U_OFF    (Y_OFF + BPS * 16 + BPS)
#define V_OFF    (U_OFF + 16)
#define YUV_SIZE (BPS * 17 + BPS * 9)

/* ---- the boolean entropy decoder ---- */

/* The value keeps its meaningful bits at the top of a 64-bit word and is
 * refilled eagerly, so a decision at the very end of the partition still
 * sees the bits it needs (a plain 16-bit window loses them at boundaries). */
typedef struct {
    const u8 *p, *end;
    u64 value;
    int count;                                   /* valid bits above the low byte */
    u32 range;                                   /* 128..255 */
} bdec;

static void bd_fill(bdec *d)
{
    int shift = 64 - 8 - (d->count + 8);
    while (shift >= 0) {
        if (d->p < d->end) {
            d->count += 8;
            d->value |= (u64)*d->p++ << shift;
            shift -= 8;
        } else {
            d->count += 0x40000000;              /* out of data: read zeros forever */
            break;
        }
    }
}

static void bd_init(bdec *d, const u8 *buf, u32 len)
{
    d->p = buf; d->end = buf + len;
    d->value = 0; d->count = -8; d->range = 255;
    bd_fill(d);
}

static int bd_bit(bdec *d, int prob)
{
    u32 split = 1 + (((d->range - 1) * (u32)prob) >> 8);
    u64 big = (u64)split << 56;
    int bit;
    if (d->count < 0) bd_fill(d);
    if (d->value >= big) { d->range -= split; d->value -= big; bit = 1; }
    else { d->range = split; bit = 0; }
    int shift = 0; u32 r = d->range;
    while (r < 128) { r <<= 1; shift++; }
    d->range <<= shift;
    d->value <<= shift;
    d->count -= shift;
    return bit;
}

static u32 bd_lit(bdec *d, int n) { u32 v = 0; while (n-- > 0) v = (v << 1) | (u32)bd_bit(d, 128); return v; }
static i32 bd_svalue(bdec *d, int n) { i32 v = (i32)bd_lit(d, n); return bd_bit(d, 128) ? -v : v; }
static int bd_flag(bdec *d) { return bd_bit(d, 128); }
static i32 bd_sig_(bdec *d, int v) { return bd_bit(d, 128) ? -v : v; }   /* a coefficient's sign */

/* ---- clipping ---- */

static u8 vclip(int v) { return v < 0 ? 0 : v > 255 ? 255 : (u8)v; }
static int sclip1(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }
static int sclip2(int v) { return v < -16 ? -16 : v > 15 ? 15 : v; }
static int iabs(int v) { return v < 0 ? -v : v; }

/* ---- the inverse transforms ---- */

/* The multiply is done in 64 bits: a malformed stream can drive the
 * second-pass operands past what a 32-bit product holds (signed overflow
 * is undefined), while a valid stream stays well inside 32 bits, so the
 * result is unchanged for real images and merely well-defined for junk. */
#define AC3_MUL1(a) ((int)((((i64)(a) * 20091) >> 16) + (a)))
#define AC3_MUL2(a) ((int)(((i64)(a) * 35468) >> 16))

static void tr_one(const i16 *in, u8 *dst)
{
    int C[16], *tmp = C;
    for (int i = 0; i < 4; i++) {
        int a = in[0] + in[8], b = in[0] - in[8];
        int c = AC3_MUL2(in[4]) - AC3_MUL1(in[12]);
        int d = AC3_MUL1(in[4]) + AC3_MUL2(in[12]);
        tmp[0] = a + d; tmp[1] = b + c; tmp[2] = b - c; tmp[3] = a - d;
        tmp += 4; in++;
    }
    tmp = C;
    for (int i = 0; i < 4; i++) {
        int dc = tmp[0] + 4;
        int a = dc + tmp[8], b = dc - tmp[8];
        int c = AC3_MUL2(tmp[4]) - AC3_MUL1(tmp[12]);
        int d = AC3_MUL1(tmp[4]) + AC3_MUL2(tmp[12]);
        dst[0] = vclip(dst[0] + ((a + d) >> 3));
        dst[1] = vclip(dst[1] + ((b + c) >> 3));
        dst[2] = vclip(dst[2] + ((b - c) >> 3));
        dst[3] = vclip(dst[3] + ((a - d) >> 3));
        tmp++; dst += BPS;
    }
}

static void tr_dc(const i16 *in, u8 *dst)
{
    int dc = in[0] + 4;
    for (int j = 0; j < 4; j++) { for (int i = 0; i < 4; i++) dst[i] = vclip(dst[i] + (dc >> 3)); dst += BPS; }
}

static void tr_ac3(const i16 *in, u8 *dst)
{
    int a = in[0] + 4;
    int c4 = AC3_MUL2(in[4]), d4 = AC3_MUL1(in[4]);
    int c1 = AC3_MUL2(in[1]), d1 = AC3_MUL1(in[1]);
    int dcs[4] = { a + d4, a + c4, a - c4, a - d4 };
    for (int y = 0; y < 4; y++) {
        int DC = dcs[y];
        dst[0] = vclip(dst[0] + ((DC + d1) >> 3));
        dst[1] = vclip(dst[1] + ((DC + c1) >> 3));
        dst[2] = vclip(dst[2] + ((DC - c1) >> 3));
        dst[3] = vclip(dst[3] + ((DC - d1) >> 3));
        dst += BPS;
    }
}

static void tr_wht(const i16 *in, i16 *out)
{
    int tmp[16];
    for (int i = 0; i < 4; i++) {
        int a0 = in[0 + i] + in[12 + i], a1 = in[4 + i] + in[8 + i];
        int a2 = in[4 + i] - in[8 + i], a3 = in[0 + i] - in[12 + i];
        tmp[0 + i] = a0 + a1; tmp[8 + i] = a0 - a1;
        tmp[4 + i] = a3 + a2; tmp[12 + i] = a3 - a2;
    }
    for (int i = 0; i < 4; i++) {
        int dc = tmp[0 + i * 4] + 3;
        int a0 = dc + tmp[3 + i * 4], a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
        int a2 = tmp[1 + i * 4] - tmp[2 + i * 4], a3 = dc - tmp[3 + i * 4];
        out[0] = (i16)((a0 + a1) >> 3); out[16] = (i16)((a3 + a2) >> 3);
        out[32] = (i16)((a0 - a1) >> 3); out[48] = (i16)((a3 - a2) >> 3);
        out += 64;
    }
}

static void do_transform(u32 bits, const i16 *src, u8 *dst)
{
    switch (bits >> 30) {
    case 3: tr_one(src, dst); break;
    case 2: tr_ac3(src, dst); break;
    case 1: tr_dc(src, dst); break;
    default: break;
    }
}

static void do_uv_transform(u32 bits, const i16 *src, u8 *dst)
{
    if (bits & 0xff) {
        if (bits & 0xaa) {                       /* any AC: the full transform */
            tr_one(src + 0 * 16, dst); tr_one(src + 1 * 16, dst + 4);
            tr_one(src + 2 * 16, dst + 4 * BPS); tr_one(src + 3 * 16, dst + 4 * BPS + 4);
        } else {                                 /* DC only, per block */
            if (src[0 * 16]) tr_dc(src + 0 * 16, dst);
            if (src[1 * 16]) tr_dc(src + 1 * 16, dst + 4);
            if (src[2 * 16]) tr_dc(src + 2 * 16, dst + 4 * BPS);
            if (src[3 * 16]) tr_dc(src + 3 * 16, dst + 4 * BPS + 4);
        }
    }
}

/* ---- intra prediction ---- */

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) ((u8)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static void true_motion(u8 *dst, int size)
{
    const u8 *top = dst - BPS;                   /* the row above stays fixed */
    int tl = top[-1];
    for (int y = 0; y < size; y++) {
        int base = dst[-1] - tl;
        for (int x = 0; x < size; x++) dst[x] = vclip(top[x] + base);
        dst += BPS;
    }
}

/* 16x16 */
static void put16(int v, u8 *dst) { for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) dst[i + j * BPS] = (u8)v; }
static void VE16(u8 *dst) { for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) dst[i + j * BPS] = dst[i - BPS]; }
static void HE16(u8 *dst) { for (int j = 0; j < 16; j++) { for (int i = 0; i < 16; i++) dst[i + j * BPS] = dst[j * BPS - 1]; } }
static void DC16(u8 *dst) { int dc = 16; for (int j = 0; j < 16; j++) dc += dst[-1 + j * BPS] + dst[j - BPS]; put16(dc >> 5, dst); }
static void DC16NoTop(u8 *dst) { int dc = 8; for (int j = 0; j < 16; j++) dc += dst[-1 + j * BPS]; put16(dc >> 4, dst); }
static void DC16NoLeft(u8 *dst) { int dc = 8; for (int i = 0; i < 16; i++) dc += dst[i - BPS]; put16(dc >> 4, dst); }
static void DC16None(u8 *dst) { put16(0x80, dst); }
static void TM16(u8 *dst) { true_motion(dst, 16); }

/* 8x8 chroma */
static void put8(int v, u8 *dst) { for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) dst[i + j * BPS] = (u8)v; }
static void VE8(u8 *dst) { for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) dst[i + j * BPS] = dst[i - BPS]; }
static void HE8(u8 *dst) { for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) dst[i + j * BPS] = dst[j * BPS - 1]; }
static void DC8(u8 *dst) { int dc = 8; for (int i = 0; i < 8; i++) dc += dst[i - BPS] + dst[-1 + i * BPS]; put8(dc >> 4, dst); }
static void DC8NoTop(u8 *dst) { int dc = 4; for (int i = 0; i < 8; i++) dc += dst[-1 + i * BPS]; put8(dc >> 3, dst); }
static void DC8NoLeft(u8 *dst) { int dc = 4; for (int i = 0; i < 8; i++) dc += dst[i - BPS]; put8(dc >> 3, dst); }
static void DC8None(u8 *dst) { put8(0x80, dst); }
static void TM8(u8 *dst) { true_motion(dst, 8); }

/* 4x4 */
static void DC4(u8 *dst) { u32 dc = 4; for (int i = 0; i < 4; i++) dc += dst[i - BPS] + dst[-1 + i * BPS]; dc >>= 3; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) dst[i + j * BPS] = (u8)dc; }
static void TM4(u8 *dst) { true_motion(dst, 4); }
static void VE4(u8 *dst)
{
    const u8 *top = dst - BPS;
    u8 v[4] = { AVG3(top[-1], top[0], top[1]), AVG3(top[0], top[1], top[2]),
                AVG3(top[1], top[2], top[3]), AVG3(top[2], top[3], top[4]) };
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) dst[i + j * BPS] = v[i];
}
static void HE4(u8 *dst)
{
    int A = dst[-1 - BPS], B = dst[-1], C = dst[-1 + BPS], D = dst[-1 + 2 * BPS], E = dst[-1 + 3 * BPS];
    for (int i = 0; i < 4; i++) DST(i, 0) = AVG3(A, B, C);
    for (int i = 0; i < 4; i++) DST(i, 1) = AVG3(B, C, D);
    for (int i = 0; i < 4; i++) DST(i, 2) = AVG3(C, D, E);
    for (int i = 0; i < 4; i++) DST(i, 3) = AVG3(D, E, E);
}
static void RD4(u8 *dst)
{
    int I = dst[-1 + 0 * BPS], J = dst[-1 + 1 * BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS];
    int X = dst[-1 - BPS], A = dst[0 - BPS], B = dst[1 - BPS], C = dst[2 - BPS], D = dst[3 - BPS];
    DST(0, 3) = AVG3(J, K, L);
    DST(1, 3) = DST(0, 2) = AVG3(I, J, K);
    DST(2, 3) = DST(1, 2) = DST(0, 1) = AVG3(X, I, J);
    DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
    DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
    DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
    DST(3, 0) = AVG3(D, C, B);
}
static void LD4(u8 *dst)
{
    int A = dst[0 - BPS], B = dst[1 - BPS], C = dst[2 - BPS], D = dst[3 - BPS];
    int E = dst[4 - BPS], F = dst[5 - BPS], G = dst[6 - BPS], H = dst[7 - BPS];
    DST(0, 0) = AVG3(A, B, C);
    DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
    DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
    DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
    DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
    DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
    DST(3, 3) = AVG3(G, H, H);
}
static void VR4(u8 *dst)
{
    int I = dst[-1 + 0 * BPS], J = dst[-1 + 1 * BPS], K = dst[-1 + 2 * BPS];
    int X = dst[-1 - BPS], A = dst[0 - BPS], B = dst[1 - BPS], C = dst[2 - BPS], D = dst[3 - BPS];
    DST(0, 0) = DST(1, 2) = AVG2(X, A);
    DST(1, 0) = DST(2, 2) = AVG2(A, B);
    DST(2, 0) = DST(3, 2) = AVG2(B, C);
    DST(3, 0) = AVG2(C, D);
    DST(0, 3) = AVG3(K, J, I);
    DST(0, 2) = AVG3(J, I, X);
    DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
    DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
    DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
    DST(3, 1) = AVG3(B, C, D);
}
static void VL4(u8 *dst)
{
    int A = dst[0 - BPS], B = dst[1 - BPS], C = dst[2 - BPS], D = dst[3 - BPS];
    int E = dst[4 - BPS], F = dst[5 - BPS], G = dst[6 - BPS], H = dst[7 - BPS];
    DST(0, 0) = AVG2(A, B);
    DST(1, 0) = DST(0, 2) = AVG2(B, C);
    DST(2, 0) = DST(1, 2) = AVG2(C, D);
    DST(3, 0) = DST(2, 2) = AVG2(D, E);
    DST(0, 1) = AVG3(A, B, C);
    DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
    DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
    DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
    DST(3, 2) = AVG3(E, F, G);
    DST(3, 3) = AVG3(F, G, H);
}
static void HU4(u8 *dst)
{
    int I = dst[-1 + 0 * BPS], J = dst[-1 + 1 * BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS];
    DST(0, 0) = AVG2(I, J);
    DST(2, 0) = DST(0, 1) = AVG2(J, K);
    DST(2, 1) = DST(0, 2) = AVG2(K, L);
    DST(1, 0) = AVG3(I, J, K);
    DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
    DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
    DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = (u8)L;
}
static void HD4(u8 *dst)
{
    int I = dst[-1 + 0 * BPS], J = dst[-1 + 1 * BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS];
    int X = dst[-1 - BPS], A = dst[0 - BPS], B = dst[1 - BPS], C = dst[2 - BPS];
    DST(0, 0) = DST(2, 1) = AVG2(I, X);
    DST(0, 1) = DST(2, 2) = AVG2(J, I);
    DST(0, 2) = DST(2, 3) = AVG2(K, J);
    DST(0, 3) = AVG2(L, K);
    DST(3, 0) = AVG3(A, B, C);
    DST(2, 0) = AVG3(X, A, B);
    DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
    DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
    DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
    DST(1, 3) = AVG3(L, K, J);
}

typedef void (*predfn)(u8 *);
static const predfn pred16[7] = { DC16, TM16, VE16, HE16, DC16NoTop, DC16NoLeft, DC16None };
static const predfn pred8[7]  = { DC8, TM8, VE8, HE8, DC8NoTop, DC8NoLeft, DC8None };
static const predfn pred4[10] = { DC4, TM4, VE4, HE4, RD4, VR4, LD4, VL4, HD4, HU4 };

static int check_mode(int mb_x, int mb_y, int mode)   /* the DC edge variants */
{
    if (mode == 0) {                                  /* B_DC_PRED / DC_PRED */
        if (mb_x == 0) return (mb_y == 0) ? 6 : 5;    /* none / no-left */
        return (mb_y == 0) ? 4 : 0;                   /* no-top / full */
    }
    return mode;
}

/* ---- the loop filter ---- */

static void filt2(u8 *p, int step)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    int a = 3 * (q0 - p0) + sclip1(p1 - q1);
    int a1 = sclip2((a + 4) >> 3), a2 = sclip2((a + 3) >> 3);
    p[-step] = vclip(p0 + a2);
    p[0] = vclip(q0 - a1);
}
static void filt4(u8 *p, int step)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    int a = 3 * (q0 - p0);
    int a1 = sclip2((a + 4) >> 3), a2 = sclip2((a + 3) >> 3), a3 = (a1 + 1) >> 1;
    p[-2 * step] = vclip(p1 + a3);
    p[-step] = vclip(p0 + a2);
    p[0] = vclip(q0 - a1);
    p[step] = vclip(q1 - a3);
}
static void filt6(u8 *p, int step)
{
    int p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
    int q0 = p[0], q1 = p[step], q2 = p[2 * step];
    int a = sclip1(3 * (q0 - p0) + sclip1(p1 - q1));
    int a1 = (27 * a + 63) >> 7, a2 = (18 * a + 63) >> 7, a3 = (9 * a + 63) >> 7;
    p[-3 * step] = vclip(p2 + a3);
    p[-2 * step] = vclip(p1 + a2);
    p[-step] = vclip(p0 + a1);
    p[0] = vclip(q0 - a1);
    p[step] = vclip(q1 - a2);
    p[2 * step] = vclip(q2 - a3);
}
static int hev(const u8 *p, int step, int thresh)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    return iabs(p1 - p0) > thresh || iabs(q1 - q0) > thresh;
}
static int needs1(const u8 *p, int step, int t)
{
    int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    return (4 * iabs(p0 - q0) + iabs(p1 - q1)) <= t;
}
static int needs2(const u8 *p, int step, int t, int it)
{
    int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step], q0 = p[0];
    int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
    if ((4 * iabs(p0 - q0) + iabs(p1 - q1)) > t) return 0;
    return iabs(p3 - p2) <= it && iabs(p2 - p1) <= it && iabs(p1 - p0) <= it &&
           iabs(q3 - q2) <= it && iabs(q2 - q1) <= it && iabs(q1 - q0) <= it;
}
static void simple_v(u8 *p, int stride, int thresh) { int t2 = 2 * thresh + 1; for (int i = 0; i < 16; i++) if (needs1(p + i, stride, t2)) filt2(p + i, stride); }
static void simple_h(u8 *p, int stride, int thresh) { int t2 = 2 * thresh + 1; for (int i = 0; i < 16; i++) if (needs1(p + i * stride, 1, t2)) filt2(p + i * stride, 1); }
static void floop26(u8 *p, int hs, int vs, int size, int th, int it, int ht)
{
    int t2 = 2 * th + 1;
    while (size-- > 0) { if (needs2(p, hs, t2, it)) { if (hev(p, hs, ht)) filt2(p, hs); else filt6(p, hs); } p += vs; }
}
static void floop24(u8 *p, int hs, int vs, int size, int th, int it, int ht)
{
    int t2 = 2 * th + 1;
    while (size-- > 0) { if (needs2(p, hs, t2, it)) { if (hev(p, hs, ht)) filt2(p, hs); else filt4(p, hs); } p += vs; }
}

/* ---- the decoder state ---- */

typedef struct { u8 nz, nz_dc; } mbnz;
typedef struct { u8 f_limit, f_ilevel, hev_thresh, f_inner; } finfo;
typedef struct { u8 segment, skip, is_i4x4, uvmode; u8 imodes[16]; u32 nz_y, nz_uv; } mbmode;

typedef struct {
    bdec br;                                    /* partition 0: modes and the header */
    bdec parts[8];                              /* the token partitions */
    u32  nparts_mask;

    int mb_w, mb_h;

    /* header state */
    int use_skip_proba, skip_p;
    int filter_type;                            /* 0 none, 1 simple, 2 complex */
    struct { int simple, level, sharpness, use_lf_delta, ref0, mode0; } fh;
    struct { int use_segment, update_map, absolute_delta;
             int quantizer[4], filter_strength[4]; u8 segments[3]; } seg;
    u8   coeff_prob[4][8][3][11];
    u16  dq[4][6];                              /* y1_dc,y1_ac, y2_dc,y2_ac, uv_dc,uv_ac */
    finfo fstr[4][2];

    /* frame buffers (in the arena) */
    u8  *ybuf, *ubuf, *vbuf; int ys, uvs;
    u8  *yuv_b;
    struct { u8 y[16], u[8], v[8]; } *top;      /* per-column top samples */
    mbnz *nz;                                   /* mb_w + 1 */
    u8  *intra_t; u8 intra_l[4];
    mbmode *mode;                               /* mb_w */
    finfo *fmap;                                /* mb_w * mb_h */
    i16  coeffs[384];
} vp8;

/* ---- the quantiser ---- */

static int qclip(int v, int m) { return v < 0 ? 0 : v > m ? m : v; }

static void vp8_quant(vp8 *v)
{
    bdec *b = &v->br;
    int base = (int)bd_lit(b, 7);
    int dqy1_dc = bd_flag(b) ? bd_svalue(b, 4) : 0;
    int dqy2_dc = bd_flag(b) ? bd_svalue(b, 4) : 0;
    int dqy2_ac = bd_flag(b) ? bd_svalue(b, 4) : 0;
    int dquv_dc = bd_flag(b) ? bd_svalue(b, 4) : 0;
    int dquv_ac = bd_flag(b) ? bd_svalue(b, 4) : 0;
    for (int i = 0; i < 4; i++) {
        int q;
        if (v->seg.use_segment) {
            q = v->seg.quantizer[i];
            if (!v->seg.absolute_delta) q += base;
        } else {
            if (i > 0) { for (int k = 0; k < 6; k++) v->dq[i][k] = v->dq[0][k]; continue; }
            q = base;
        }
        u16 *m = v->dq[i];
        m[0] = kDcTable[qclip(q + dqy1_dc, 127)];
        m[1] = kAcTable[qclip(q + 0, 127)];
        m[2] = (u16)(kDcTable[qclip(q + dqy2_dc, 127)] * 2);
        m[3] = (u16)((kAcTable[qclip(q + dqy2_ac, 127)] * 101581) >> 16);
        if (m[3] < 8) m[3] = 8;
        m[4] = kDcTable[qclip(q + dquv_dc, 117)];
        m[5] = kAcTable[qclip(q + dquv_ac, 127)];
    }
}

/* ---- coefficient reading ---- */

static int get_large(bdec *b, const u8 *p)
{
    int v;
    if (!bd_bit(b, p[3])) {
        if (!bd_bit(b, p[4])) v = 2;
        else v = 3 + bd_bit(b, p[5]);
    } else if (!bd_bit(b, p[6])) {
        if (!bd_bit(b, p[7])) v = 5 + bd_bit(b, 159);
        else { v = 7 + 2 * bd_bit(b, 165); v += bd_bit(b, 145); }
    } else {
        static const u8 c3[] = {173, 148, 140, 0}, c4[] = {176, 155, 140, 135, 0};
        static const u8 c5[] = {180, 157, 141, 134, 130, 0};
        static const u8 c6[] = {254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0};
        static const u8 *const cats[4] = { c3, c4, c5, c6 };
        int b1 = bd_bit(b, p[8]);
        int b0 = bd_bit(b, p[9 + b1]);
        int cat = 2 * b1 + b0;
        const u8 *t = cats[cat];
        v = 0;
        for (; *t; ++t) v += v + bd_bit(b, *t);
        v += 3 + (8 << cat);
    }
    return v;
}

/* Reads one 4x4 block's coefficients. probs is coeff_prob[type]; ctx is the
 * running context; dq the two quantisers; n the first coefficient. */
static int get_coeffs(bdec *b, const u8 probs[8][3][11], int ctx, const u16 *dq, int n, i16 *out)
{
    const u8 *p = probs[kBands[n]][ctx];
    for (; n < 16; ++n) {
        if (!bd_bit(b, p[0])) return n;
        while (!bd_bit(b, p[1])) { p = probs[kBands[++n]][0]; if (n == 16) return 16; }
        {
            const u8 (*pc)[11] = probs[kBands[n + 1]];
            int val;
            if (!bd_bit(b, p[2])) { val = 1; p = pc[1]; }
            else { val = get_large(b, p); p = pc[2]; }
            out[kZigzag[n]] = (i16)(bd_sig_(b, val) * dq[n > 0]);
        }
    }
    return 16;
}

static u32 nz_bits(u32 nz_coeffs, int nz, int dc_nz)
{
    nz_coeffs <<= 2;
    nz_coeffs |= (nz > 3) ? 3 : (nz > 1) ? 2 : (u32)dc_nz;
    return nz_coeffs;
}

/* Parses the residuals of the macroblock at mb_x into v->coeffs, returning
 * whether the block was empty. */
static int parse_residuals(vp8 *v, int mb_x, bdec *tb)
{
    mbmode *blk = &v->mode[mb_x];
    const u16 *q = v->dq[blk->segment];
    i16 *dst = v->coeffs;
    mbnz *mb = &v->nz[mb_x + 1];
    mbnz *left = &v->nz[0];                       /* the shared left slot */
    u32 nzy = 0, nzuv = 0;
    int first;
    const u8 (*ac)[3][11];

    for (int i = 0; i < 384; i++) dst[i] = 0;

    if (!blk->is_i4x4) {
        i16 dc[16]; for (int i = 0; i < 16; i++) dc[i] = 0;
        int ctx = mb->nz_dc + left->nz_dc;
        int nz = get_coeffs(tb, v->coeff_prob[1], ctx, &q[2], 0, dc);
        mb->nz_dc = left->nz_dc = (nz > 0);
        if (nz > 1) tr_wht(dc, dst);
        else { int dc0 = (dc[0] + 3) >> 3; for (int i = 0; i < 256; i += 16) dst[i] = (i16)dc0; }
        first = 1; ac = v->coeff_prob[0];
    } else { first = 0; ac = v->coeff_prob[3]; }

    u8 tnz = mb->nz & 0x0f, lnz = left->nz & 0x0f;
    for (int y = 0; y < 4; y++) {
        int l = lnz & 1;
        u32 nzc = 0;
        for (int x = 0; x < 4; x++) {
            int ctx = l + (tnz & 1);
            int nz = get_coeffs(tb, ac, ctx, &q[0], first, dst);
            l = (nz > first);
            tnz = (u8)((tnz >> 1) | (l << 7));
            nzc = nz_bits(nzc, nz, dst[0] != 0);
            dst += 16;
        }
        tnz >>= 4;
        lnz = (u8)((lnz >> 1) | (l << 7));
        nzy = (nzy << 8) | nzc;
    }
    u32 out_t = tnz, out_l = lnz >> 4;

    for (int ch = 0; ch < 4; ch += 2) {
        u32 nzc = 0;
        tnz = (u8)(mb->nz >> (4 + ch));
        lnz = (u8)(left->nz >> (4 + ch));
        for (int y = 0; y < 2; y++) {
            int l = lnz & 1;
            for (int x = 0; x < 2; x++) {
                int ctx = l + (tnz & 1);
                int nz = get_coeffs(tb, v->coeff_prob[2], ctx, &q[4], 0, dst);
                l = (nz > 0);
                tnz = (u8)((tnz >> 1) | (l << 3));
                nzc = nz_bits(nzc, nz, dst[0] != 0);
                dst += 16;
            }
            tnz >>= 2;
            lnz = (u8)((lnz >> 1) | (l << 5));
        }
        nzuv |= nzc << (4 * ch);
        out_t |= (u32)(tnz << 4) << ch;
        out_l |= (u32)(lnz & 0xf0) << ch;
    }
    mb->nz = (u8)out_t;
    left->nz = (u8)out_l;
    blk->nz_y = nzy; blk->nz_uv = nzuv;
    return !(nzy | nzuv);
}

/* ---- mode parsing (one macroblock) ---- */

/* the 16x16 and chroma modes now use the B_ enum ids (DC 0, TM 1, V 2, H 3),
 * which already match the predictor-array order (DC 0, TM 1, VE 2, HE 3). */
static const u8 kMode16[4] = { 0, 1, 2, 3 };

static void parse_mode(vp8 *v, int mb_x)
{
    bdec *b = &v->br;
    u8 *top = v->intra_t + 4 * mb_x;
    u8 *left = v->intra_l;
    mbmode *blk = &v->mode[mb_x];

    if (v->seg.update_map)
        blk->segment = !bd_bit(b, v->seg.segments[0])
                     ? (u8)bd_bit(b, v->seg.segments[1])
                     : (u8)(bd_bit(b, v->seg.segments[2]) + 2);
    else blk->segment = 0;

    blk->skip = v->use_skip_proba ? (u8)bd_bit(b, v->skip_p) : 0;
    blk->is_i4x4 = !bd_bit(b, 145);
    if (!blk->is_i4x4) {
        /* 16x16 luma mode ids follow the B_ enum used to index kBModesProba as
         * neighbour context: DC=0, TM=1, V(=B_VE)=2, H(=B_HE)=3. */
        u8 ym = (u8)(bd_bit(b, 156) ? (bd_bit(b, 128) ? 1 : 3)
                                    : (bd_bit(b, 163) ? 2 : 0));
        blk->imodes[0] = ym;
        for (int i = 0; i < 4; i++) { top[i] = ym; left[i] = ym; }
    } else {
        u8 *modes = blk->imodes;
        for (int y = 0; y < 4; y++) {
            int ym = left[y];
            for (int x = 0; x < 4; x++) {
                const u8 *pr = kBModesProba[top[x]][ym];
                ym = !bd_bit(b, pr[0]) ? 0
                   : !bd_bit(b, pr[1]) ? 1
                   : !bd_bit(b, pr[2]) ? 2
                   : !bd_bit(b, pr[3]) ? (!bd_bit(b, pr[4]) ? 3 : (!bd_bit(b, pr[5]) ? 4 : 5))
                   : (!bd_bit(b, pr[6]) ? 6 : (!bd_bit(b, pr[7]) ? 7 : (!bd_bit(b, pr[8]) ? 8 : 9)));
                top[x] = (u8)ym;
                modes[x] = (u8)ym;
            }
            modes += 4;
            left[y] = (u8)ym;
        }
    }
    blk->uvmode = (u8)(!bd_bit(b, 142) ? 0 : !bd_bit(b, 114) ? 2 : bd_bit(b, 183) ? 1 : 3);
}

/* ---- reconstruction of one macroblock into the frame ---- */

static const u16 kScan[16] = {
    0 + 0 * BPS,  4 + 0 * BPS,  8 + 0 * BPS,  12 + 0 * BPS,
    0 + 4 * BPS,  4 + 4 * BPS,  8 + 4 * BPS,  12 + 4 * BPS,
    0 + 8 * BPS,  4 + 8 * BPS,  8 + 8 * BPS,  12 + 8 * BPS,
    0 + 12 * BPS, 4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS
};

static void recon_mb(vp8 *v, int mb_x, int mb_y)
{
    u8 *yd = v->yuv_b + Y_OFF;
    u8 *ud = v->yuv_b + U_OFF;
    u8 *vd = v->yuv_b + V_OFF;
    mbmode *blk = &v->mode[mb_x];
    const i16 *coeffs = v->coeffs;

    /* the left border: 129, or the previous block's right edge rotated in */
    if (mb_x == 0) {
        for (int j = 0; j < 16; j++) yd[j * BPS - 1] = 129;
        for (int j = 0; j < 8; j++) { ud[j * BPS - 1] = 129; vd[j * BPS - 1] = 129; }
        if (mb_y > 0) yd[-1 - BPS] = ud[-1 - BPS] = vd[-1 - BPS] = 129;
    } else {
        for (int j = -1; j < 16; j++) for (int k = 0; k < 4; k++) yd[j * BPS - 4 + k] = yd[j * BPS + 12 + k];
        for (int j = -1; j < 8; j++) for (int k = 0; k < 4; k++) {
            ud[j * BPS - 4 + k] = ud[j * BPS + 4 + k];
            vd[j * BPS - 4 + k] = vd[j * BPS + 4 + k];
        }
    }

    /* the top border */
    if (mb_y == 0 && mb_x == 0)
        for (int i = -1; i < 16 + 4; i++) { yd[i - BPS] = 127; if (i < 8) { ud[i - BPS] = 127; vd[i - BPS] = 127; } }
    if (mb_y == 0 && mb_x > 0)
        for (int i = 0; i < 16 + 4; i++) { yd[i - BPS] = 127; if (i < 8) { ud[i - BPS] = 127; vd[i - BPS] = 127; } }
    if (mb_y > 0) {
        for (int i = 0; i < 16; i++) yd[i - BPS] = v->top[mb_x].y[i];
        for (int i = 0; i < 8; i++) { ud[i - BPS] = v->top[mb_x].u[i]; vd[i - BPS] = v->top[mb_x].v[i]; }
    }

    u32 bits = blk->nz_y;
    if (blk->is_i4x4) {
        u8 *tr = yd - BPS + 16;
        if (mb_y > 0) {
            if (mb_x >= v->mb_w - 1) for (int k = 0; k < 4; k++) tr[k] = v->top[mb_x].y[15];
            else for (int k = 0; k < 4; k++) tr[k] = v->top[mb_x + 1].y[k];
        }
        /* the top-right is repeated on the row above each block row (3, 7, 11) */
        for (int k = 0; k < 4; k++) tr[4 * BPS + k] = tr[8 * BPS + k] = tr[12 * BPS + k] = tr[k];
        for (int n = 0; n < 16; n++, bits <<= 2) {
            u8 *dst = yd + kScan[n];
            pred4[blk->imodes[n]](dst);
            do_transform(bits, coeffs + n * 16, dst);
        }
    } else {
        pred16[check_mode(mb_x, mb_y, kMode16[blk->imodes[0]])](yd);
        if (bits) for (int n = 0; n < 16; n++, bits <<= 2) do_transform(bits, coeffs + n * 16, yd + kScan[n]);
    }
    {
        u32 buv = blk->nz_uv;
        int pf = check_mode(mb_x, mb_y, kMode16[blk->uvmode]);
        pred8[pf](ud); pred8[pf](vd);
        do_uv_transform(buv >> 0, coeffs + 16 * 16, ud);
        do_uv_transform(buv >> 8, coeffs + 20 * 16, vd);
    }

    /* stash the bottom row as the next row's top samples */
    for (int i = 0; i < 16; i++) v->top[mb_x].y[i] = yd[15 * BPS + i];
    for (int i = 0; i < 8; i++) { v->top[mb_x].u[i] = ud[7 * BPS + i]; v->top[mb_x].v[i] = vd[7 * BPS + i]; }

    /* copy the cache into the frame */
    u8 *yo = v->ybuf + mb_y * 16 * v->ys + mb_x * 16;
    u8 *uo = v->ubuf + mb_y * 8 * v->uvs + mb_x * 8;
    u8 *vo = v->vbuf + mb_y * 8 * v->uvs + mb_x * 8;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) yo[j * v->ys + i] = yd[j * BPS + i];
    for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) {
        uo[j * v->uvs + i] = ud[j * BPS + i];
        vo[j * v->uvs + i] = vd[j * BPS + i];
    }
}

/* ---- the loop filter over the whole frame ---- */

static void filter_mb(vp8 *v, int mb_x, int mb_y)
{
    const finfo *f = &v->fmap[mb_y * v->mb_w + mb_x];
    int limit = f->f_limit;
    if (limit == 0) return;
    u8 *yd = v->ybuf + mb_y * 16 * v->ys + mb_x * 16;
    int yb = v->ys;
    if (v->filter_type == 1) {
        if (mb_x > 0) simple_h(yd, yb, limit + 4);
        if (f->f_inner) for (int k = 4; k < 16; k += 4) simple_h(yd + k, yb, limit);
        if (mb_y > 0) simple_v(yd, yb, limit + 4);
        if (f->f_inner) for (int k = 4; k < 16; k += 4) simple_v(yd + k * yb, yb, limit);
    } else {
        int uvb = v->uvs;
        u8 *ud = v->ubuf + mb_y * 8 * uvb + mb_x * 8;
        u8 *vd = v->vbuf + mb_y * 8 * uvb + mb_x * 8;
        int ht = f->hev_thresh, il = f->f_ilevel;
        if (mb_x > 0) {
            floop26(yd, 1, yb, 16, limit + 4, il, ht);
            floop26(ud, 1, uvb, 8, limit + 4, il, ht);
            floop26(vd, 1, uvb, 8, limit + 4, il, ht);
        }
        if (f->f_inner) {
            for (int k = 4; k < 16; k += 4) floop24(yd + k, 1, yb, 16, limit, il, ht);
            floop24(ud + 4, 1, uvb, 8, limit, il, ht);
            floop24(vd + 4, 1, uvb, 8, limit, il, ht);
        }
        if (mb_y > 0) {
            floop26(yd, yb, 1, 16, limit + 4, il, ht);
            floop26(ud, uvb, 1, 8, limit + 4, il, ht);
            floop26(vd, uvb, 1, 8, limit + 4, il, ht);
        }
        if (f->f_inner) {
            for (int k = 4; k < 16; k += 4) floop24(yd + k * yb, yb, 1, 16, limit, il, ht);
            floop24(ud + 4 * uvb, uvb, 1, 8, limit, il, ht);
            floop24(vd + 4 * uvb, uvb, 1, 8, limit, il, ht);
        }
    }
}

/* ---- the filter strengths per segment and block size ---- */

static void precompute_filters(vp8 *v)
{
    if (v->filter_type == 0) return;
    for (int s = 0; s < 4; s++) {
        int base = v->seg.use_segment ? (v->seg.filter_strength[s] + (v->seg.absolute_delta ? 0 : v->fh.level))
                                      : v->fh.level;
        for (int i4 = 0; i4 <= 1; i4++) {
            finfo *info = &v->fstr[s][i4];
            int level = base;
            if (v->fh.use_lf_delta) { level += v->fh.ref0; if (i4) level += v->fh.mode0; }
            level = level < 0 ? 0 : level > 63 ? 63 : level;
            info->f_limit = 0; info->f_ilevel = 0; info->hev_thresh = 0;
            if (level > 0) {
                int il = level;
                if (v->fh.sharpness > 0) {
                    il >>= (v->fh.sharpness > 4) ? 2 : 1;
                    if (il > 9 - v->fh.sharpness) il = 9 - v->fh.sharpness;
                }
                if (il < 1) il = 1;
                info->f_ilevel = (u8)il;
                info->f_limit = (u8)(2 * level + il);
                info->hev_thresh = (u8)(level >= 40 ? 2 : level >= 15 ? 1 : 0);
            }
            info->f_inner = (u8)i4;
        }
    }
}

/* ---- YUV -> 0x00RRGGBB ---- */

static int mulhi(int v, int c) { return (v * c) >> 8; }
static u8 yuvclip(int v) { return ((v & ~((256 << 6) - 1)) == 0) ? (u8)(v >> 6) : (v < 0 ? 0 : 255); }

static u32 yuv_rgb(int Y, int U, int V)
{
    int yy = mulhi(Y, 19077);
    u8 r = yuvclip(yy + mulhi(V, 26149) - 14234);
    u8 g = yuvclip(yy - mulhi(U, 6419) - mulhi(V, 13320) + 8708);
    u8 b = yuvclip(yy + mulhi(U, 33050) - 17685);
    return ((u32)r << 16) | ((u32)g << 8) | b;
}

/* Fancy (bilinear) chroma upsampling, a pair of output rows at a time from
 * the chroma row above (tu/tv) and the current one (cu/cv), exactly as the
 * reference does; a nearest sample would leave colour edges blocky. */
static void upsample_pair(const u8 *ty, const u8 *by,
                          const u8 *tu, const u8 *tv, const u8 *cu, const u8 *cv,
                          u32 *td, u32 *bd, int len)
{
    int last = (len - 1) >> 1;
    int tlu = tu[0], tlv = tv[0], lu = cu[0], lv = cv[0];
    td[0] = yuv_rgb(ty[0], (3 * tlu + lu + 2) >> 2, (3 * tlv + lv + 2) >> 2);
    if (by) bd[0] = yuv_rgb(by[0], (3 * lu + tlu + 2) >> 2, (3 * lv + tlv + 2) >> 2);
    for (int x = 1; x <= last; x++) {
        int tuu = tu[x], tvv = tv[x], cuu = cu[x], cvv = cv[x];
        int au = tlu + tuu + lu + cuu + 8, av = tlv + tvv + lv + cvv + 8;
        int d12u = (au + 2 * (tuu + lu)) >> 3, d12v = (av + 2 * (tvv + lv)) >> 3;
        int d03u = (au + 2 * (tlu + cuu)) >> 3, d03v = (av + 2 * (tlv + cvv)) >> 3;
        td[2 * x - 1] = yuv_rgb(ty[2 * x - 1], (d12u + tlu) >> 1, (d12v + tlv) >> 1);
        td[2 * x] = yuv_rgb(ty[2 * x], (d03u + tuu) >> 1, (d03v + tvv) >> 1);
        if (by) {
            bd[2 * x - 1] = yuv_rgb(by[2 * x - 1], (d03u + lu) >> 1, (d03v + lv) >> 1);
            bd[2 * x] = yuv_rgb(by[2 * x], (d12u + cuu) >> 1, (d12v + cvv) >> 1);
        }
        tlu = tuu; tlv = tvv; lu = cuu; lv = cvv;
    }
    if (!(len & 1)) {
        td[len - 1] = yuv_rgb(ty[len - 1], (3 * tlu + lu + 2) >> 2, (3 * tlv + lv + 2) >> 2);
        if (by) bd[len - 1] = yuv_rgb(by[len - 1], (3 * lu + tlu + 2) >> 2, (3 * lv + tlv + 2) >> 2);
    }
}

static bool vp8_decode(const u8 *body, u32 blen, u32 *out, u32 max_pixels,
                       u32 *ow, u32 *oh, u8 *scratch, u32 scratch_len)
{
    if (blen < 10) return false;
    const u8 *f = body;
    u32 tag = (u32)f[0] | ((u32)f[1] << 8) | ((u32)f[2] << 16);
    if (tag & 1) return false;                            /* not a key frame */
    u32 part0_len = tag >> 5;
    if (f[3] != 0x9d || f[4] != 0x01 || f[5] != 0x2a) return false;
    u32 w = ((u32)f[6] | ((u32)f[7] << 8)) & 0x3fff;
    u32 h = ((u32)f[8] | ((u32)f[9] << 8)) & 0x3fff;
    if (!w || !h || w > 16384 || h > 16384 || (u64)w * h > max_pixels) return false;
    if (10 + part0_len > blen) return false;

    static vp8 V;                                         /* too large for the stack */
    vp8 *v = &V;
    memset(v, 0, sizeof(*v));
    v->mb_w = (int)((w + 15) >> 4);
    v->mb_h = (int)((h + 15) >> 4);

    bd_init(&v->br, f + 10, part0_len);
    bdec *b = &v->br;
    (void)bd_flag(b); (void)bd_flag(b);                   /* colour space, clamp type */

    /* segment header */
    v->seg.use_segment = bd_flag(b);
    if (v->seg.use_segment) {
        v->seg.update_map = bd_flag(b);
        if (bd_flag(b)) {                                 /* update data */
            v->seg.absolute_delta = bd_flag(b);
            for (int s = 0; s < 4; s++) v->seg.quantizer[s] = bd_flag(b) ? bd_svalue(b, 7) : 0;
            for (int s = 0; s < 4; s++) v->seg.filter_strength[s] = bd_flag(b) ? bd_svalue(b, 6) : 0;
        }
        if (v->seg.update_map)
            for (int s = 0; s < 3; s++) v->seg.segments[s] = bd_flag(b) ? (u8)bd_lit(b, 8) : 255;
    }
    if (!v->seg.update_map) for (int s = 0; s < 3; s++) v->seg.segments[s] = 255;

    /* filter header */
    v->fh.simple = bd_flag(b);
    v->fh.level = (int)bd_lit(b, 6);
    v->fh.sharpness = (int)bd_lit(b, 3);
    v->fh.use_lf_delta = bd_flag(b);
    if (v->fh.use_lf_delta && bd_flag(b)) {
        for (int i = 0; i < 4; i++) if (bd_flag(b)) { int d = bd_svalue(b, 6); if (i == 0) v->fh.ref0 = d; }
        for (int i = 0; i < 4; i++) if (bd_flag(b)) { int d = bd_svalue(b, 6); if (i == 0) v->fh.mode0 = d; }
    }
    v->filter_type = (v->fh.level == 0) ? 0 : v->fh.simple ? 1 : 2;

    /* token partitions */
    u32 nparts = 1u << bd_lit(b, 2);
    v->nparts_mask = nparts - 1;
    const u8 *pbuf = f + 10 + part0_len;
    u32 psize_total = blen - 10 - part0_len;
    const u8 *sizes = pbuf;
    if (psize_total < 3 * (nparts - 1)) return false;
    const u8 *pstart = pbuf + 3 * (nparts - 1);
    u32 left = psize_total - 3 * (nparts - 1);
    for (u32 pi = 0; pi < nparts - 1; pi++) {
        u32 psz = (u32)sizes[0] | ((u32)sizes[1] << 8) | ((u32)sizes[2] << 16);
        if (psz > left) psz = left;
        bd_init(&v->parts[pi], pstart, psz);
        pstart += psz; left -= psz; sizes += 3;
    }
    bd_init(&v->parts[nparts - 1], pstart, left);

    vp8_quant(v);

    (void)bd_flag(b);                                     /* refresh/update proba flag, ignored */

    /* coefficient probabilities */
    for (int t = 0; t < 4; t++)
        for (int bnd = 0; bnd < 8; bnd++)
            for (int c = 0; c < 3; c++)
                for (int p = 0; p < 11; p++)
                    v->coeff_prob[t][bnd][c][p] = bd_bit(b, kCoeffsUpdate[t][bnd][c][p])
                                                ? (u8)bd_lit(b, 8) : kCoeffsProba0[t][bnd][c][p];

    v->use_skip_proba = bd_flag(b);
    if (v->use_skip_proba) v->skip_p = (int)bd_lit(b, 8);

    precompute_filters(v);

    /* the frame buffers, from the caller's scratch */
    arena a = { scratch, scratch_len, 0 };
    v->ys = v->mb_w * 16; v->uvs = v->mb_w * 8;
    v->ybuf = arena_take(&a, v->ys * v->mb_h * 16);
    v->ubuf = arena_take(&a, v->uvs * v->mb_h * 8);
    v->vbuf = arena_take(&a, v->uvs * v->mb_h * 8);
    v->yuv_b = arena_take(&a, YUV_SIZE + 128);
    v->top = arena_take(&a, (u32)v->mb_w * (u32)sizeof(*v->top));
    v->nz = arena_take(&a, (u32)(v->mb_w + 1) * (u32)sizeof(mbnz));
    v->intra_t = arena_take(&a, (u32)v->mb_w * 4);
    v->mode = arena_take(&a, (u32)v->mb_w * (u32)sizeof(mbmode));
    v->fmap = v->filter_type ? arena_take(&a, (u32)v->mb_w * (u32)v->mb_h * (u32)sizeof(finfo)) : (finfo *)v->yuv_b;
    if (!v->ybuf || !v->ubuf || !v->vbuf || !v->top || !v->nz || !v->intra_t || !v->mode || !v->fmap) return false;
    v->yuv_b += 64;                                       /* room for the negative border indices */
    for (int i = 0; i < v->mb_w + 1; i++) { v->nz[i].nz = 0; v->nz[i].nz_dc = 0; }
    for (int i = 0; i < v->mb_w * 4; i++) v->intra_t[i] = 0;

    /* decode every macroblock row */
    for (int my = 0; my < v->mb_h; my++) {
        bdec *tb = &v->parts[(u32)my & v->nparts_mask];
        v->nz[0].nz = v->nz[0].nz_dc = 0;                 /* the left context resets each row */
        for (int i = 0; i < 4; i++) v->intra_l[i] = 0;
        for (int mx = 0; mx < v->mb_w; mx++) parse_mode(v, mx);
        for (int mx = 0; mx < v->mb_w; mx++) {
            mbmode *blk = &v->mode[mx];
            int skip;
            if (blk->skip) {
                v->nz[0].nz = v->nz[mx + 1].nz = 0;
                if (!blk->is_i4x4) v->nz[0].nz_dc = v->nz[mx + 1].nz_dc = 0;
                blk->nz_y = 0; blk->nz_uv = 0; skip = 1;
            } else {
                skip = parse_residuals(v, mx, tb);
            }
            if (v->filter_type) {
                finfo fi = v->fstr[blk->segment][blk->is_i4x4];
                fi.f_inner = (u8)(fi.f_inner | !skip);
                v->fmap[my * v->mb_w + mx] = fi;
            }
            recon_mb(v, mx, my);
        }
    }

    /* the loop filter, in raster order over the reconstructed frame */
#ifndef WEBP_NOFILTER
    if (v->filter_type)
        for (int my = 0; my < v->mb_h; my++)
            for (int mx = 0; mx < v->mb_w; mx++) filter_mb(v, mx, my);
#endif

    /* YUV 4:2:0 -> rgb with fancy (bilinear) chroma upsampling */
    {
        int uvh = ((int)h + 1) >> 1;
        int ys = v->ys, uvs = v->uvs;
        /* first output row: chroma row 0 mirrored above itself */
        upsample_pair(v->ybuf, 0, v->ubuf, v->vbuf, v->ubuf, v->vbuf,
                      out, 0, (int)w);
        for (int cy = 1; cy < uvh; cy++) {
            int r0 = 2 * cy - 1, r1 = 2 * cy;
            const u8 *tu = v->ubuf + (cy - 1) * uvs, *tv = v->vbuf + (cy - 1) * uvs;
            const u8 *cu = v->ubuf + cy * uvs, *cv = v->vbuf + cy * uvs;
            const u8 *by = (r1 < (int)h) ? v->ybuf + r1 * ys : 0;
            u32 *bd = by ? out + r1 * w : 0;
            upsample_pair(v->ybuf + r0 * ys, by, tu, tv, cu, cv,
                          out + r0 * w, bd, (int)w);
        }
        /* even height: last row uses the bottom chroma row mirrored below */
        if ((h & 1) == 0) {
            int r = (int)h - 1;
            const u8 *cu = v->ubuf + (uvh - 1) * uvs, *cv = v->vbuf + (uvh - 1) * uvs;
            upsample_pair(v->ybuf + r * ys, 0, cu, cv, cu, cv,
                          out + r * w, 0, (int)w);
        }
    }
    *ow = w; *oh = h;
    return true;
}
