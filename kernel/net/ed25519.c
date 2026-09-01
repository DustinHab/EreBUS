/*
 * ed25519.c -- the signature, RFC 8032.
 *
 * The same TweetNaCl arrangement as the key agreement, on the same
 * field: the twisted Edwards curve in extended coordinates, a
 * ladder that takes the same steps whatever the secret is, and the
 * scalar arithmetic modulo the group order done in bytes. What it
 * buys is the one thing the seal never had: a name. A host key that
 * signs the exchange proves which machine answered; a client key that
 * signs the login proves who is knocking. Checked against the RFC's
 * own vector at start-up, like everything else under the seal.
 */
#include <eb/crypto.h>
#include <eb/string.h>
#include "gf25519.h"

static const gf gf0;
static const gf gf1 = { 1 };
static const gf D = {
    0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
    0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203 };
static const gf D2 = {
    0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
    0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406 };
static const gf X = {
    0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
    0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169 };
static const gf Y = {
    0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666 };
static const gf I = {
    0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
    0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83 };

static void set25519(gf r, const gf a)
{
    for (i32 i = 0; i < 16; i++) r[i] = a[i];
}

static u8 par25519(const gf a)
{
    u8 d[32];
    pack25519(d, a);
    return d[0] & 1;
}

/* 0 when equal, in constant time. */
static i32 neq25519(const gf a, const gf b)
{
    u8 c[32], d[32];
    pack25519(c, a);
    pack25519(d, b);
    u32 diff = 0;
    for (i32 i = 0; i < 32; i++) diff |= (u32)(c[i] ^ d[i]);
    return diff != 0;
}

static void pow2523(gf o, const gf i)
{
    gf c;
    set25519(c, i);
    for (i32 a = 250; a >= 0; a--) {
        S(c, c);
        if (a != 1) M(c, c, i);
    }
    set25519(o, c);
}

static void add(gf p[4], gf q[4])
{
    gf a, b, c, d, t, e, f, g, h;
    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(d, p[2], q[2]);
    A(d, d, d);
    Z(e, b, a);
    Z(f, d, c);
    A(g, d, c);
    A(h, b, a);
    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

static void cswap(gf p[4], gf q[4], u8 b)
{
    for (i32 i = 0; i < 4; i++) sel25519(p[i], q[i], b);
}

static void pack(u8 *r, gf p[4])
{
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= (u8)(par25519(tx) << 7);
}

static void scalarmult(gf p[4], gf q[4], const u8 *s)
{
    set25519(p[0], gf0);
    set25519(p[1], gf1);
    set25519(p[2], gf1);
    set25519(p[3], gf0);
    for (i32 i = 255; i >= 0; --i) {
        u8 b = (u8)((s[i / 8] >> (i & 7)) & 1);
        cswap(p, q, b);
        add(q, p);
        add(p, p);
        cswap(p, q, b);
    }
}

static void scalarbase(gf p[4], const u8 *s)
{
    gf q[4];
    set25519(q[0], X);
    set25519(q[1], Y);
    set25519(q[2], gf1);
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

/* The group order, as bytes, for the reductions below. */
static const u64 L[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10 };

static void modL(u8 *r, i64 x[64])
{
    i64 carry, i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * (i64)L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry << 8;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; ++j) {
        x[j] += carry - (x[31] >> 4) * (i64)L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; ++j) x[j] -= carry * (i64)L[j];
    for (i = 0; i < 32; ++i) {
        x[i + 1] += x[i] >> 8;
        r[i] = (u8)(x[i] & 255);
    }
}

static void reduce(u8 *r)
{
    i64 x[64];
    for (i32 i = 0; i < 64; i++) x[i] = (i64)r[i];
    for (i32 i = 0; i < 64; i++) r[i] = 0;
    modL(r, x);
}

void ed25519_public(u8 pk[32], const u8 seed[32])
{
    u8 d[64];
    gf p[4];
    sha512(seed, 32, d);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;
    scalarbase(p, d);
    pack(pk, p);
    memset(d, 0, 64);
}

void ed25519_sign(u8 sig[64], const u8 seed[32], const u8 pk[32],
                  const void *msg, u32 len)
{
    u8 d[64], h[64], r[64];
    i64 x[64];
    gf p[4];

    sha512(seed, 32, d);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;

    /* r = H(prefix || M), reduced; R = rB. */
    sha512_ctx c;
    sha512_init(&c);
    sha512_update(&c, d + 32, 32);
    sha512_update(&c, msg, len);
    sha512_final(&c, r);
    reduce(r);
    scalarbase(p, r);
    pack(sig, p);

    /* h = H(R || A || M), reduced; S = r + h a. */
    sha512_init(&c);
    sha512_update(&c, sig, 32);
    sha512_update(&c, pk, 32);
    sha512_update(&c, msg, len);
    sha512_final(&c, h);
    reduce(h);

    for (i32 i = 0; i < 64; i++) x[i] = 0;
    for (i32 i = 0; i < 32; i++) x[i] = (i64)r[i];
    for (i32 i = 0; i < 32; i++)
        for (i32 j = 0; j < 32; j++)
            x[i + j] += (i64)h[i] * (i64)d[j];
    modL(sig + 32, x);

    memset(d, 0, 64);
    memset(r, 0, 64);
}

static i32 unpackneg(gf r[4], const u8 p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    set25519(r[2], gf1);
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);

    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);

    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) M(r[0], r[0], I);

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) return -1;

    if (par25519(r[0]) == (p[31] >> 7)) Z(r[0], gf0, r[0]);

    M(r[3], r[0], r[1]);
    return 0;
}

bool ed25519_verify(const u8 pk[32], const void *msg, u32 len,
                    const u8 sig[64])
{
    u8 t[32], h[64];
    gf p[4], q[4];

    if (unpackneg(q, pk)) return false;

    sha512_ctx c;
    sha512_init(&c);
    sha512_update(&c, sig, 32);
    sha512_update(&c, pk, 32);
    sha512_update(&c, msg, len);
    sha512_final(&c, h);
    reduce(h);

    scalarmult(p, q, h);
    scalarbase(q, sig + 32);
    add(p, q);
    pack(t, p);

    u32 diff = 0;
    for (i32 i = 0; i < 32; i++) diff |= (u32)(sig[i] ^ t[i]);
    return diff == 0;
}
