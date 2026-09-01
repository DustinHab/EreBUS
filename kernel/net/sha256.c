/*
 * sha256.c -- the one hash, with its keyed and derived forms.
 *
 * FIPS 180-4 SHA-256, HMAC over it, and the two HKDF steps TLS 1.3
 * builds its whole key schedule from. Nothing clever: the compression
 * function as published, a copyable context so a transcript can be
 * snapshotted mid-stream, and lengths counted in bytes until the
 * final bit-length is written.
 */
#include <eb/crypto.h>

static const u32 K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static u32 rotr(u32 v, u32 n) { return (v >> n) | (v << (32 - n)); }

static void compress(u32 h[8], const u8 *p)
{
    u32 w[64];
    for (u32 i = 0; i < 16; i++)
        w[i] = ((u32)p[i*4] << 24) | ((u32)p[i*4+1] << 16) |
               ((u32)p[i*4+2] << 8) | p[i*4+3];
    for (u32 i = 16; i < 64; i++) {
        u32 s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        u32 s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    u32 a = h[0], b = h[1], c = h[2], d = h[3];
    u32 e = h[4], f = h[5], g = h[6], hh = h[7];

    for (u32 i = 0; i < 64; i++) {
        u32 s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t1 = hh + s1 + ch + K[i] + w[i];
        u32 s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        u32 mj = (a & b) ^ (a & c) ^ (b & c);
        u32 t2 = s0 + mj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void sha256_init(sha256_ctx *c)
{
    c->h[0] = 0x6a09e667; c->h[1] = 0xbb67ae85;
    c->h[2] = 0x3c6ef372; c->h[3] = 0xa54ff53a;
    c->h[4] = 0x510e527f; c->h[5] = 0x9b05688c;
    c->h[6] = 0x1f83d9ab; c->h[7] = 0x5be0cd19;
    c->total = 0;
    c->fill = 0;
}

void sha256_update(sha256_ctx *c, const void *data, u64 len)
{
    const u8 *p = (const u8 *)data;
    c->total += len;

    while (len) {
        u32 take = 64 - c->fill;
        if (take > len) take = (u32)len;
        for (u32 i = 0; i < take; i++) c->buf[c->fill + i] = p[i];
        c->fill += take;
        p += take;
        len -= take;
        if (c->fill == 64) { compress(c->h, c->buf); c->fill = 0; }
    }
}

void sha256_final(sha256_ctx *c, u8 out[32])
{
    u64 bits = c->total * 8;
    u8 pad = 0x80;
    sha256_update(c, &pad, 1);
    pad = 0;
    while (c->fill != 56) sha256_update(c, &pad, 1);

    u8 l[8];
    for (u32 i = 0; i < 8; i++) l[i] = (u8)(bits >> (56 - i * 8));
    sha256_update(c, l, 8);

    for (u32 i = 0; i < 8; i++) {
        out[i*4]     = (u8)(c->h[i] >> 24);
        out[i*4 + 1] = (u8)(c->h[i] >> 16);
        out[i*4 + 2] = (u8)(c->h[i] >> 8);
        out[i*4 + 3] = (u8)c->h[i];
    }
}

void sha256(const void *data, u64 len, u8 out[32])
{
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, out);
}

/* ------------------------------------------------------------------ */

void hmac_sha256(const u8 *key, u32 klen,
                 const void *msg, u64 mlen, u8 out[32])
{
    u8 k[64] = { 0 };
    if (klen > 64) sha256(key, klen, k);
    else for (u32 i = 0; i < klen; i++) k[i] = key[i];

    u8 pad[64];
    sha256_ctx c;

    for (u32 i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    sha256_init(&c);
    sha256_update(&c, pad, 64);
    sha256_update(&c, msg, mlen);
    u8 inner[32];
    sha256_final(&c, inner);

    for (u32 i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    sha256_init(&c);
    sha256_update(&c, pad, 64);
    sha256_update(&c, inner, 32);
    sha256_final(&c, out);
}

void hkdf_extract(const u8 *salt, u32 slen,
                  const u8 *ikm, u32 ilen, u8 out[32])
{
    static const u8 zero[32] = { 0 };
    if (!salt) { salt = zero; slen = 32; }
    hmac_sha256(salt, slen, ikm, ilen, out);
}

void hkdf_expand(const u8 prk[32], const u8 *info, u32 ilen,
                 u8 *out, u32 olen)
{
    u8 t[32 + 256 + 1];
    u8 block[32];
    u32 tlen = 0;
    u8 counter = 1;
    u32 done = 0;

    while (done < olen) {
        u32 at = 0;
        for (u32 i = 0; i < tlen; i++) t[at++] = block[i];
        for (u32 i = 0; i < ilen && i < 256; i++) t[at++] = info[i];
        t[at++] = counter++;
        hmac_sha256(prk, 32, t, at, block);
        tlen = 32;

        u32 take = olen - done;
        if (take > 32) take = 32;
        for (u32 i = 0; i < take; i++) out[done + i] = block[i];
        done += take;
    }
}
