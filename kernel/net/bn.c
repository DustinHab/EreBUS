/*
 * bn.c -- big numbers modulo an odd number, Montgomery form (CIOS multiplication).
 * - the products use the compiler's 128-bit integer, which is one mul instruction; no division anywhere
 * - out may alias an input in every operation: results are built in a local and copied last
 */
#include <eb/bn.h>

typedef unsigned __int128 u128;

bool bn_from_be(u64 *out, u32 n, const u8 *p, u32 len)
{
    for (u32 i = 0; i < n; i++) out[i] = 0;
    while (len > 0 && p[0] == 0) { p++; len--; }
    if (len > n * 8) return false;
    for (u32 i = 0; i < len; i++) {
        u32 at = len - 1 - i;              /* byte position from the right */
        out[at / 8] |= (u64)p[i] << (8 * (at % 8));
    }
    return true;
}

void bn_to_be(const u64 *a, u32 n, u8 *out, u32 len)
{
    for (u32 i = 0; i < len; i++) {
        u32 at = len - 1 - i;
        out[i] = at / 8 < n ? (u8)(a[at / 8] >> (8 * (at % 8))) : 0;
    }
}

int bn_cmp(const u64 *a, const u64 *b, u32 n)
{
    for (u32 i = n; i-- > 0;)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

bool bn_is_zero(const u64 *a, u32 n)
{
    for (u32 i = 0; i < n; i++) if (a[i]) return false;
    return true;
}

static void copy(u64 *out, const u64 *a, u32 n)
{
    for (u32 i = 0; i < n; i++) out[i] = a[i];
}

/* out = a + b, the carry out returned. */
static u64 add_n(u64 *out, const u64 *a, const u64 *b, u32 n)
{
    u64 carry = 0;
    for (u32 i = 0; i < n; i++) {
        u128 s = (u128)a[i] + b[i] + carry;
        out[i] = (u64)s;
        carry = (u64)(s >> 64);
    }
    return carry;
}

/* out = a - b, the borrow out returned. */
static u64 sub_n(u64 *out, const u64 *a, const u64 *b, u32 n)
{
    u64 borrow = 0;
    for (u32 i = 0; i < n; i++) {
        u128 d = (u128)a[i] - b[i] - borrow;
        out[i] = (u64)d;
        borrow = (u64)(d >> 64) & 1;
    }
    return borrow;
}

void bn_mod_add(const bn_mont *c, u64 *out, const u64 *a, const u64 *b)
{
    u64 t[BN_MAX];
    u64 carry = add_n(t, a, b, c->n);
    if (carry || bn_cmp(t, c->m, c->n) >= 0) sub_n(t, t, c->m, c->n);
    copy(out, t, c->n);
}

void bn_mod_sub(const bn_mont *c, u64 *out, const u64 *a, const u64 *b)
{
    u64 t[BN_MAX];
    if (sub_n(t, a, b, c->n)) add_n(t, t, c->m, c->n);
    copy(out, t, c->n);
}

bool bn_mont_init(bn_mont *c, const u8 *mod_be, u32 len)
{
    while (len > 0 && mod_be[0] == 0) { mod_be++; len--; }
    if (len == 0 || len > BN_MAX * 8) return false;
    c->n = (len + 7) / 8;
    bn_from_be(c->m, c->n, mod_be, len);
    if (!(c->m[0] & 1)) return false;

    /* -m^-1 mod 2^64 by Newton's iteration: an odd number is its own
     * inverse modulo 8, and every step doubles the correct bits. */
    u64 inv = c->m[0];
    for (u32 i = 0; i < 6; i++) inv *= 2 - c->m[0] * inv;
    c->m0 = (u64)0 - inv;

    /* R^2 mod m: one, doubled 128 n times with a reduction each time. */
    u64 x[BN_MAX];
    for (u32 i = 0; i < c->n; i++) x[i] = 0;
    x[0] = 1;
    if (bn_cmp(x, c->m, c->n) >= 0) sub_n(x, x, c->m, c->n);
    for (u32 i = 0; i < 128 * c->n; i++) {
        u64 carry = add_n(x, x, x, c->n);
        if (carry || bn_cmp(x, c->m, c->n) >= 0) sub_n(x, x, c->m, c->n);
    }
    copy(c->rr, x, c->n);
    return true;
}

void bn_mont_mul(const bn_mont *c, u64 *out, const u64 *a, const u64 *b)
{
    u32 n = c->n;
    u64 t[BN_MAX + 2];
    for (u32 i = 0; i < n + 2; i++) t[i] = 0;

    for (u32 i = 0; i < n; i++) {
        u64 carry = 0;
        for (u32 j = 0; j < n; j++) {
            u128 s = (u128)a[j] * b[i] + t[j] + carry;
            t[j] = (u64)s;
            carry = (u64)(s >> 64);
        }
        u128 s = (u128)t[n] + carry;
        t[n] = (u64)s;
        t[n + 1] = (u64)(s >> 64);

        u64 m = t[0] * c->m0;
        u128 s0 = (u128)m * c->m[0] + t[0];
        carry = (u64)(s0 >> 64);
        for (u32 j = 1; j < n; j++) {
            u128 s1 = (u128)m * c->m[j] + t[j] + carry;
            t[j - 1] = (u64)s1;
            carry = (u64)(s1 >> 64);
        }
        u128 s2 = (u128)t[n] + carry;
        t[n - 1] = (u64)s2;
        t[n] = t[n + 1] + (u64)(s2 >> 64);
    }

    if (t[n] || bn_cmp(t, c->m, n) >= 0) sub_n(t, t, c->m, n);
    copy(out, t, n);
}

void bn_mont_enter(const bn_mont *c, u64 *out, const u64 *a)
{
    bn_mont_mul(c, out, a, c->rr);
}

void bn_mont_leave(const bn_mont *c, u64 *out, const u64 *a)
{
    u64 one[BN_MAX];
    for (u32 i = 0; i < c->n; i++) one[i] = 0;
    one[0] = 1;
    bn_mont_mul(c, out, a, one);
}

void bn_mont_one(const bn_mont *c, u64 *out)
{
    u64 one[BN_MAX];
    for (u32 i = 0; i < c->n; i++) one[i] = 0;
    one[0] = 1;
    bn_mont_enter(c, out, one);
}

void bn_mont_pow(const bn_mont *c, u64 *out, const u64 *base, const u8 *exp_be, u32 elen)
{
    u64 r[BN_MAX];
    bn_mont_one(c, r);
    bool started = false;
    for (u32 i = 0; i < elen; i++) {
        for (i32 bit = 7; bit >= 0; bit--) {
            if (started) bn_mont_mul(c, r, r, r);
            if ((exp_be[i] >> bit) & 1) {
                bn_mont_mul(c, r, r, base);
                started = true;
            }
        }
    }
    copy(out, r, c->n);
}
