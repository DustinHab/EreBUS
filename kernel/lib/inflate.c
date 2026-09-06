/*
 * inflate.c -- deflate decompression: stored, fixed and dynamic blocks; the zlib and gzip wrappings.
 * - canonical codes decoded by walking the lengths, as puff does: small tables, no lookup arrays to overrun
 * - every read from the input and every write to the output is bounded; a bad stream answers -1
 */
#include <eb/inflate.h>

#define MAXBITS   15
#define MAXLCODES 286
#define MAXDCODES 30
#define MAXCODES  (MAXLCODES + MAXDCODES)
#define FIXLCODES 288

typedef struct {
    const u8 *in;  u32 ilen, ipos;
    u8       *out; u32 omax, opos;
    u32 bitbuf, bitcnt;
    bool bad;
} state;

typedef struct {
    u16 count[MAXBITS + 1];       /* codes of each length */
    u16 symbol[FIXLCODES];        /* symbols in code order */
} huff;

static u32 bits(state *s, u32 need)
{
    u32 v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->ipos >= s->ilen) { s->bad = true; return 0; }
        v |= (u32)s->in[s->ipos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return v & ((1u << need) - 1);
}

static i32 decode(state *s, const huff *h)
{
    i32 code = 0, first = 0, index = 0;
    for (u32 len = 1; len <= MAXBITS; len++) {
        code |= (i32)bits(s, 1);
        if (s->bad) return -1;
        i32 count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

/* Builds the code from the lengths; answers 0 for a complete code, a
 * positive number for an incomplete one (allowed for a single code),
 * negative for an over-subscribed one. */
static i32 construct(huff *h, const u16 *length, u32 n)
{
    for (u32 len = 0; len <= MAXBITS; len++) h->count[len] = 0;
    for (u32 sym = 0; sym < n; sym++) h->count[length[sym]]++;
    if (h->count[0] == n) return 0;

    i32 left = 1;
    for (u32 len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return left;
    }

    u16 offs[MAXBITS + 1];
    offs[1] = 0;
    for (u32 len = 1; len < MAXBITS; len++) offs[len + 1] = (u16)(offs[len] + h->count[len]);

    for (u32 sym = 0; sym < n; sym++)
        if (length[sym]) h->symbol[offs[length[sym]]++] = (u16)sym;
    return left;
}

static const u16 LBASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                               35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u16 LEXT[29]  = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                               3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 DBASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                               257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                               8193, 12289, 16385, 24577 };
static const u16 DEXT[30]  = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                               7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static bool codes(state *s, const huff *lencode, const huff *distcode)
{
    for (;;) {
        i32 sym = decode(s, lencode);
        if (sym < 0) return false;
        if (sym < 256) {
            if (s->opos >= s->omax) return false;
            s->out[s->opos++] = (u8)sym;
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            u32 len = LBASE[sym] + bits(s, LEXT[sym]);
            i32 ds = decode(s, distcode);
            if (ds < 0 || ds >= 30) return false;
            u32 dist = DBASE[ds] + bits(s, DEXT[ds]);
            if (s->bad) return false;
            if (dist > s->opos) return false;             /* before the start */
            if (s->opos + len > s->omax) return false;
            for (u32 i = 0; i < len; i++) {
                s->out[s->opos] = s->out[s->opos - dist];
                s->opos++;
            }
        }
    }
}

static bool stored(state *s)
{
    s->bitbuf = 0; s->bitcnt = 0;
    if (s->ipos + 4 > s->ilen) return false;
    u32 len = (u32)s->in[s->ipos] | ((u32)s->in[s->ipos + 1] << 8);
    u32 nlen = (u32)s->in[s->ipos + 2] | ((u32)s->in[s->ipos + 3] << 8);
    s->ipos += 4;
    if (len != (~nlen & 0xFFFF)) return false;
    if (s->ipos + len > s->ilen || s->opos + len > s->omax) return false;
    for (u32 i = 0; i < len; i++) s->out[s->opos++] = s->in[s->ipos++];
    return true;
}

static bool fixed(state *s)
{
    static huff lencode, distcode;
    static bool built;
    if (!built) {
        u16 lengths[FIXLCODES];
        u32 sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < FIXLCODES; sym++) lengths[sym] = 8;
        construct(&lencode, lengths, FIXLCODES);
        for (sym = 0; sym < MAXDCODES; sym++) lengths[sym] = 5;
        construct(&distcode, lengths, MAXDCODES);
        built = true;
    }
    return codes(s, &lencode, &distcode);
}

static bool dynamic(state *s)
{
    static const u16 order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    u16 lengths[MAXCODES];
    huff lencode, distcode;

    u32 nlen = bits(s, 5) + 257;
    u32 ndist = bits(s, 5) + 1;
    u32 ncode = bits(s, 4) + 4;
    if (s->bad || nlen > MAXLCODES || ndist > MAXDCODES) return false;

    u32 index = 0;
    for (; index < ncode; index++) lengths[order[index]] = (u16)bits(s, 3);
    for (; index < 19; index++) lengths[order[index]] = 0;
    if (s->bad) return false;
    if (construct(&lencode, lengths, 19) != 0) return false;

    index = 0;
    while (index < nlen + ndist) {
        i32 sym = decode(s, &lencode);
        if (sym < 0) return false;
        if (sym < 16) {
            lengths[index++] = (u16)sym;
        } else {
            u16 len = 0;
            u32 rep;
            if (sym == 16) {
                if (index == 0) return false;
                len = lengths[index - 1];
                rep = 3 + bits(s, 2);
            } else if (sym == 17) {
                rep = 3 + bits(s, 3);
            } else {
                rep = 11 + bits(s, 7);
            }
            if (s->bad || index + rep > nlen + ndist) return false;
            while (rep--) lengths[index++] = len;
        }
    }
    if (lengths[256] == 0) return false;               /* no end code */

    i32 err = construct(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return false;
    err = construct(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1)) return false;

    return codes(s, &lencode, &distcode);
}

i64 inflate_raw(const u8 *in, u32 ilen, u8 *out, u32 omax, u32 *consumed)
{
    state s = { in, ilen, 0, out, omax, 0, 0, 0, false };
    u32 last;
    do {
        last = bits(&s, 1);
        u32 type = bits(&s, 2);
        if (s.bad) return -1;
        bool ok = type == 0 ? stored(&s) : type == 1 ? fixed(&s) : type == 2 ? dynamic(&s) : false;
        if (!ok || s.bad) return -1;
    } while (!last);
    if (consumed) *consumed = s.ipos;
    return (i64)s.opos;
}

i64 inflate_zlib(const u8 *in, u32 ilen, u8 *out, u32 omax)
{
    if (ilen < 6) return -1;
    if ((in[0] & 0x0F) != 8) return -1;                 /* deflate */
    if (((in[0] << 8) | in[1]) % 31 != 0) return -1;
    if (in[1] & 0x20) return -1;                        /* a preset dictionary: not here */
    return inflate_raw(in + 2, ilen - 2, out, omax, NULL);
}

i64 inflate_gzip(const u8 *in, u32 ilen, u8 *out, u32 omax)
{
    if (ilen < 18 || in[0] != 0x1F || in[1] != 0x8B || in[2] != 8) return -1;
    u8 flags = in[3];
    u32 at = 10;
    if (flags & 4) {                                    /* extra field */
        if (at + 2 > ilen) return -1;
        u32 xlen = (u32)in[at] | ((u32)in[at + 1] << 8);
        at += 2 + xlen;
    }
    if (flags & 8) { while (at < ilen && in[at]) at++; at++; }   /* name */
    if (flags & 16) { while (at < ilen && in[at]) at++; at++; }  /* comment */
    if (flags & 2) at += 2;                                       /* header crc */
    if (at >= ilen) return -1;
    return inflate_raw(in + at, ilen - at, out, omax, NULL);
}
