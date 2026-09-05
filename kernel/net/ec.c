/*
 * ec.c -- ECDSA verification over NIST P-256 and P-384.
 * - field and scalar arithmetic on bn.c in Montgomery form; points in Jacobian coordinates, a = -3 on both curves
 * - one code path, the curve a table of parameters; a public point is checked to lie on its curve before use
 * - verification only: no private key ever passes through here, so timing is not hidden
 */
#include <eb/pki.h>
#include <eb/bn.h>

typedef struct {
    u32       limbs, bytes;
    const u8 *p, *p2, *n, *n2, *b, *gx, *gy;    /* big-endian; p2 = p - 2, n2 = n - 2 */
} ec_params;

static const u8 P256_P[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static const u8 P256_P2[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFD };
static const u8 P256_N[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x51 };
static const u8 P256_N2[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x4F };
static const u8 P256_B[32] = {
    0x5A,0xC6,0x35,0xD8,0xAA,0x3A,0x93,0xE7,0xB3,0xEB,0xBD,0x55,0x76,0x98,0x86,0xBC,
    0x65,0x1D,0x06,0xB0,0xCC,0x53,0xB0,0xF6,0x3B,0xCE,0x3C,0x3E,0x27,0xD2,0x60,0x4B };
static const u8 P256_GX[32] = {
    0x6B,0x17,0xD1,0xF2,0xE1,0x2C,0x42,0x47,0xF8,0xBC,0xE6,0xE5,0x63,0xA4,0x40,0xF2,
    0x77,0x03,0x7D,0x81,0x2D,0xEB,0x33,0xA0,0xF4,0xA1,0x39,0x45,0xD8,0x98,0xC2,0x96 };
static const u8 P256_GY[32] = {
    0x4F,0xE3,0x42,0xE2,0xFE,0x1A,0x7F,0x9B,0x8E,0xE7,0xEB,0x4A,0x7C,0x0F,0x9E,0x16,
    0x2B,0xCE,0x33,0x57,0x6B,0x31,0x5E,0xCE,0xCB,0xB6,0x40,0x68,0x37,0xBF,0x51,0xF5 };

static const u8 P384_P[48] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF };
static const u8 P384_P2[48] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFD };
static const u8 P384_N[48] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC7,0x63,0x4D,0x81,0xF4,0x37,0x2D,0xDF,
    0x58,0x1A,0x0D,0xB2,0x48,0xB0,0xA7,0x7A,0xEC,0xEC,0x19,0x6A,0xCC,0xC5,0x29,0x73 };
static const u8 P384_N2[48] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC7,0x63,0x4D,0x81,0xF4,0x37,0x2D,0xDF,
    0x58,0x1A,0x0D,0xB2,0x48,0xB0,0xA7,0x7A,0xEC,0xEC,0x19,0x6A,0xCC,0xC5,0x29,0x71 };
static const u8 P384_B[48] = {
    0xB3,0x31,0x2F,0xA7,0xE2,0x3E,0xE7,0xE4,0x98,0x8E,0x05,0x6B,0xE3,0xF8,0x2D,0x19,
    0x18,0x1D,0x9C,0x6E,0xFE,0x81,0x41,0x12,0x03,0x14,0x08,0x8F,0x50,0x13,0x87,0x5A,
    0xC6,0x56,0x39,0x8D,0x8A,0x2E,0xD1,0x9D,0x2A,0x85,0xC8,0xED,0xD3,0xEC,0x2A,0xEF };
static const u8 P384_GX[48] = {
    0xAA,0x87,0xCA,0x22,0xBE,0x8B,0x05,0x37,0x8E,0xB1,0xC7,0x1E,0xF3,0x20,0xAD,0x74,
    0x6E,0x1D,0x3B,0x62,0x8B,0xA7,0x9B,0x98,0x59,0xF7,0x41,0xE0,0x82,0x54,0x2A,0x38,
    0x55,0x02,0xF2,0x5D,0xBF,0x55,0x29,0x6C,0x3A,0x54,0x5E,0x38,0x72,0x76,0x0A,0xB7 };
static const u8 P384_GY[48] = {
    0x36,0x17,0xDE,0x4A,0x96,0x26,0x2C,0x6F,0x5D,0x9E,0x98,0xBF,0x92,0x92,0xDC,0x29,
    0xF8,0xF4,0x1D,0xBD,0x28,0x9A,0x14,0x7C,0xE9,0xDA,0x31,0x13,0xB5,0xF0,0xB8,0xC0,
    0x0A,0x60,0xB1,0xCE,0x1D,0x7E,0x81,0x9D,0x7A,0x43,0x1D,0x7C,0x90,0xEA,0x0E,0x5F };

static const ec_params CURVES[2] = {
    { 4, 32, P256_P, P256_P2, P256_N, P256_N2, P256_B, P256_GX, P256_GY },
    { 6, 48, P384_P, P384_P2, P384_N, P384_N2, P384_B, P384_GX, P384_GY },
};

typedef struct { u64 v[6]; } fe;                /* a field element in Montgomery form */
typedef struct { fe x, y, z; } pt;              /* Jacobian; z == 0 is the point at infinity */

typedef struct {
    const ec_params *k;
    bn_mont Fp, Fn;
    fe   B, ONE;
    pt   G;
    bool ready;
} ec_ctx;

static ec_ctx ctx[2];                           /* kept off the small thread stacks */

static void fmul(const ec_ctx *c, fe *o, const fe *a, const fe *b) { bn_mont_mul(&c->Fp, o->v, a->v, b->v); }
static void fsqr(const ec_ctx *c, fe *o, const fe *a)             { bn_mont_mul(&c->Fp, o->v, a->v, a->v); }
static void fadd(const ec_ctx *c, fe *o, const fe *a, const fe *b) { bn_mod_add(&c->Fp, o->v, a->v, b->v); }
static void fsub(const ec_ctx *c, fe *o, const fe *a, const fe *b) { bn_mod_sub(&c->Fp, o->v, a->v, b->v); }
static bool fzero(const ec_ctx *c, const fe *a)                    { return bn_is_zero(a->v, c->k->limbs); }
static bool feq(const ec_ctx *c, const fe *a, const fe *b)         { return bn_cmp(a->v, b->v, c->k->limbs) == 0; }

static void fe_enter(const ec_ctx *c, fe *o, const u8 *be, u32 len)
{
    u64 t[6];
    bn_from_be(t, c->k->limbs, be, len);
    bn_mont_enter(&c->Fp, o->v, t);
}

static void pt_infinity(const ec_ctx *c, pt *o)
{
    o->x = c->ONE; o->y = c->ONE;
    for (u32 i = 0; i < c->k->limbs; i++) o->z.v[i] = 0;
}

static const ec_ctx *setup(u32 curve)
{
    if (curve > 1) return NULL;
    ec_ctx *c = &ctx[curve];
    if (c->ready) return c;
    c->k = &CURVES[curve];
    bn_mont_init(&c->Fp, c->k->p, c->k->bytes);
    bn_mont_init(&c->Fn, c->k->n, c->k->bytes);
    fe_enter(c, &c->B, c->k->b, c->k->bytes);
    fe_enter(c, &c->G.x, c->k->gx, c->k->bytes);
    fe_enter(c, &c->G.y, c->k->gy, c->k->bytes);
    bn_mont_one(&c->Fp, c->ONE.v);
    c->G.z = c->ONE;
    c->ready = true;
    return c;
}

/* dbl-2001-b: doubling with a = -3. */
static void pt_double(const ec_ctx *c, pt *o, const pt *p)
{
    if (fzero(c, &p->z)) { *o = *p; return; }
    fe delta, gamma, beta, alpha, t1, t2, x3, y3, z3;
    fsqr(c, &delta, &p->z);
    fsqr(c, &gamma, &p->y);
    fmul(c, &beta, &p->x, &gamma);
    fsub(c, &t1, &p->x, &delta);
    fadd(c, &t2, &p->x, &delta);
    fmul(c, &t1, &t1, &t2);
    fadd(c, &alpha, &t1, &t1);
    fadd(c, &alpha, &alpha, &t1);                 /* 3 (x - delta)(x + delta) */

    fsqr(c, &t1, &alpha);
    fadd(c, &t2, &beta, &beta);
    fadd(c, &t2, &t2, &t2);
    fadd(c, &t2, &t2, &t2);                       /* 8 beta */
    fsub(c, &x3, &t1, &t2);

    fadd(c, &t1, &p->y, &p->z);
    fsqr(c, &t1, &t1);
    fsub(c, &t1, &t1, &gamma);
    fsub(c, &z3, &t1, &delta);

    fadd(c, &t1, &beta, &beta);
    fadd(c, &t1, &t1, &t1);                       /* 4 beta */
    fsub(c, &t1, &t1, &x3);
    fmul(c, &t1, &alpha, &t1);
    fsqr(c, &t2, &gamma);
    fadd(c, &t2, &t2, &t2);
    fadd(c, &t2, &t2, &t2);
    fadd(c, &t2, &t2, &t2);                       /* 8 gamma^2 */
    fsub(c, &y3, &t1, &t2);

    o->x = x3; o->y = y3; o->z = z3;
}

/* add-2007-bl: general addition, with the doubling and infinity cases. */
static void pt_add(const ec_ctx *c, pt *o, const pt *p, const pt *q)
{
    if (fzero(c, &p->z)) { *o = *q; return; }
    if (fzero(c, &q->z)) { *o = *p; return; }
    fe z1z1, z2z2, u1, u2, s1, s2, h, r, i, j, v, t, x3, y3, z3;
    fsqr(c, &z1z1, &p->z);
    fsqr(c, &z2z2, &q->z);
    fmul(c, &u1, &p->x, &z2z2);
    fmul(c, &u2, &q->x, &z1z1);
    fmul(c, &s1, &p->y, &q->z);
    fmul(c, &s1, &s1, &z2z2);
    fmul(c, &s2, &q->y, &p->z);
    fmul(c, &s2, &s2, &z1z1);
    fsub(c, &h, &u2, &u1);
    fsub(c, &r, &s2, &s1);
    if (fzero(c, &h)) {
        if (fzero(c, &r)) { pt_double(c, o, p); return; }
        pt_infinity(c, o);
        return;
    }
    fadd(c, &r, &r, &r);                          /* 2 (s2 - s1) */
    fadd(c, &i, &h, &h);
    fsqr(c, &i, &i);                              /* (2 h)^2 */
    fmul(c, &j, &h, &i);
    fmul(c, &v, &u1, &i);

    fsqr(c, &t, &r);
    fsub(c, &t, &t, &j);
    fsub(c, &t, &t, &v);
    fsub(c, &x3, &t, &v);                         /* r^2 - j - 2 v */

    fsub(c, &t, &v, &x3);
    fmul(c, &t, &r, &t);
    fmul(c, &s1, &s1, &j);
    fadd(c, &s1, &s1, &s1);
    fsub(c, &y3, &t, &s1);                        /* r (v - x3) - 2 s1 j */

    fadd(c, &t, &p->z, &q->z);
    fsqr(c, &t, &t);
    fsub(c, &t, &t, &z1z1);
    fsub(c, &t, &t, &z2z2);
    fmul(c, &z3, &t, &h);

    o->x = x3; o->y = y3; o->z = z3;
}

/* k P, k a plain scalar of the curve's width, most significant bit first. */
static void pt_mul(const ec_ctx *c, pt *o, const pt *p, const u64 *k)
{
    pt r;
    pt_infinity(c, &r);
    for (i32 bit = (i32)(c->k->limbs * 64) - 1; bit >= 0; bit--) {
        pt_double(c, &r, &r);
        if ((k[bit / 64] >> (bit % 64)) & 1) pt_add(c, &r, &r, p);
    }
    *o = r;
}

/* The affine x of a point, as a plain number. False at infinity. */
static bool pt_affine_x(const ec_ctx *c, const pt *p, u64 *x)
{
    if (fzero(c, &p->z)) return false;
    fe zi, zi2, t;
    bn_mont_pow(&c->Fp, zi.v, p->z.v, c->k->p2, c->k->bytes);
    fsqr(c, &zi2, &zi);
    fmul(c, &t, &p->x, &zi2);
    bn_mont_leave(&c->Fp, x, t.v);
    return true;
}

/* Loads an uncompressed point; false unless both coordinates are below p
 * and the point satisfies the curve equation. */
static bool pt_load(const ec_ctx *c, pt *o, const u8 *pub, u32 publen)
{
    u32 by = c->k->bytes;
    if (publen != 1 + 2 * by || pub[0] != 0x04) return false;
    u64 x[6], y[6], p[6];
    bn_from_be(x, c->k->limbs, pub + 1, by);
    bn_from_be(y, c->k->limbs, pub + 1 + by, by);
    bn_from_be(p, c->k->limbs, c->k->p, by);
    if (bn_cmp(x, p, c->k->limbs) >= 0 || bn_cmp(y, p, c->k->limbs) >= 0) return false;
    bn_mont_enter(&c->Fp, o->x.v, x);
    bn_mont_enter(&c->Fp, o->y.v, y);
    o->z = c->ONE;

    fe lhs, rhs, t;
    fsqr(c, &lhs, &o->y);
    fsqr(c, &t, &o->x);
    fmul(c, &rhs, &t, &o->x);                     /* x^3 */
    fadd(c, &t, &o->x, &o->x);
    fadd(c, &t, &t, &o->x);                       /* 3 x */
    fsub(c, &rhs, &rhs, &t);
    fadd(c, &rhs, &rhs, &c->B);
    return feq(c, &lhs, &rhs);
}

bool ec_point_ok(u32 curve, const u8 *pub, u32 publen)
{
    const ec_ctx *c = setup(curve);
    if (!c) return false;
    pt q;
    return pt_load(c, &q, pub, publen);
}

bool ec_verify(u32 curve, const u8 *pub, u32 publen, const u8 *hash, u32 hlen,
               const u8 *r, u32 rlen, const u8 *s, u32 slen)
{
    const ec_ctx *c = setup(curve);
    if (!c) return false;
    u32 L = c->k->limbs;

    u64 n[6], rv[6], sv[6], ev[6];
    bn_from_be(n, L, c->k->n, c->k->bytes);
    if (!bn_from_be(rv, L, r, rlen) || !bn_from_be(sv, L, s, slen)) return false;
    if (bn_is_zero(rv, L) || bn_cmp(rv, n, L) >= 0) return false;
    if (bn_is_zero(sv, L) || bn_cmp(sv, n, L) >= 0) return false;

    pt q;
    if (!pt_load(c, &q, pub, publen)) return false;

    /* e: the leftmost bits of the hash, as many as the curve is wide,
     * reduced once (it is below 2 n). */
    u32 take = hlen > c->k->bytes ? c->k->bytes : hlen;
    bn_from_be(ev, L, hash, take);
    if (bn_cmp(ev, n, L) >= 0) bn_mod_sub(&c->Fn, ev, ev, n);

    /* w = s^-1; u1 = e w; u2 = r w -- all modulo n. */
    u64 sm[6], wm[6], em[6], rm[6], u1[6], u2[6];
    bn_mont_enter(&c->Fn, sm, sv);
    bn_mont_pow(&c->Fn, wm, sm, c->k->n2, c->k->bytes);
    bn_mont_enter(&c->Fn, em, ev);
    bn_mont_enter(&c->Fn, rm, rv);
    bn_mont_mul(&c->Fn, u1, em, wm);
    bn_mont_leave(&c->Fn, u1, u1);
    bn_mont_mul(&c->Fn, u2, rm, wm);
    bn_mont_leave(&c->Fn, u2, u2);

    pt a, b, sum;
    pt_mul(c, &a, &c->G, u1);
    pt_mul(c, &b, &q, u2);
    pt_add(c, &sum, &a, &b);

    u64 x[6];
    if (!pt_affine_x(c, &sum, x)) return false;
    if (bn_cmp(x, n, L) >= 0) bn_mod_sub(&c->Fn, x, x, n);
    return bn_cmp(x, rv, L) == 0;
}
