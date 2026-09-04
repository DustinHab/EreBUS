/*
 * x25519.c -- key agreement, RFC 7748, TweetNaCl arrangement.
 * - field elements as sixteen 16-bit limbs in 64-bit words; Montgomery ladder with constant steps and swaps
 */
#include <eb/crypto.h>
#include "gf25519.h"

static const gf gf121665 = { 0xDB41, 1 };

void car25519(gf o)
{
    for (i32 i = 0; i < 16; i++) {
        o[i] += (1LL << 16);
        i64 c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

/* Conditional swap on b in {0,1}, chosen without branching on b. */
void sel25519(gf p, gf q, i32 b)
{
    i64 c = ~(b - 1);
    for (i32 i = 0; i < 16; i++) {
        i64 t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

void pack25519(u8 *o, const gf n)
{
    gf m, t;
    for (i32 i = 0; i < 16; i++) t[i] = n[i];
    car25519(t); car25519(t); car25519(t);
    for (i32 j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (i32 i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i-1] >> 16) & 1);
            m[i-1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        i32 b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (i32 i = 0; i < 16; i++) {
        o[2*i]     = (u8)(t[i] & 0xff);
        o[2*i + 1] = (u8)(t[i] >> 8);
    }
}

void unpack25519(gf o, const u8 *n)
{
    for (i32 i = 0; i < 16; i++) o[i] = n[2*i] + ((i64)n[2*i + 1] << 8);
    o[15] &= 0x7fff;
}

void A(gf o, const gf a, const gf b)
{ for (i32 i = 0; i < 16; i++) o[i] = a[i] + b[i]; }

void Z(gf o, const gf a, const gf b)
{ for (i32 i = 0; i < 16; i++) o[i] = a[i] - b[i]; }

void M(gf o, const gf a, const gf b)
{
    i64 t[31];
    for (i32 i = 0; i < 31; i++) t[i] = 0;
    for (i32 i = 0; i < 16; i++)
        for (i32 j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    for (i32 i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (i32 i = 0; i < 16; i++) o[i] = t[i];
    car25519(o);
    car25519(o);
}

void S(gf o, const gf a) { M(o, a, a); }

/* The field inverse by Fermat: a^(p-2). */
void inv25519(gf o, const gf i)
{
    gf c;
    for (i32 a = 0; a < 16; a++) c[a] = i[a];
    for (i32 a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i);
    }
    for (i32 a = 0; a < 16; a++) o[a] = c[a];
}

void x25519(u8 out[32], const u8 scalar[32], const u8 point[32])
{
    u8 z[32];
    gf a, b, c, d, e, f, x;

    for (i32 i = 0; i < 31; i++) z[i] = scalar[i];
    z[31] = (scalar[31] & 127) | 64;
    z[0] &= 248;

    unpack25519(x, point);
    for (i32 i = 0; i < 16; i++) { b[i] = x[i]; d[i] = a[i] = c[i] = 0; }
    a[0] = d[0] = 1;

    for (i32 i = 254; i >= 0; i--) {
        i64 r = (z[i >> 3] >> (i & 7)) & 1;
        sel25519(a, b, (i32)r);
        sel25519(c, d, (i32)r);
        A(e, a, c); Z(a, a, c);
        A(c, b, d); Z(b, b, d);
        S(d, e); S(f, a);
        M(a, c, a); M(c, b, e);
        A(e, a, c); Z(a, a, c);
        S(b, a); Z(c, d, f);
        M(a, c, gf121665); A(a, a, d);
        M(c, c, a); M(a, d, f);
        M(d, b, x); S(b, e);
        sel25519(a, b, (i32)r);
        sel25519(c, d, (i32)r);
    }
    inv25519(c, c);
    M(a, a, c);
    pack25519(out, a);
}

void x25519_base(u8 out[32], const u8 scalar[32])
{
    u8 base[32] = { 9 };
    x25519(out, scalar, base);
}
