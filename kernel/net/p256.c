/*
 * p256.c -- ECDSA verification over NIST P-256 (secp256r1) with SHA-256.
 * - field and scalar arithmetic on bn.c in Montgomery form; points in Jacobian coordinates, a = -3
 * - a public point is checked to lie on the curve before it is used
 * - verification only: no private key ever passes through here, so timing is not hidden
 */
#include <eb/pki.h>
#include <eb/bn.h>

static const u8 P_BE[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static const u8 P_MINUS_2[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFD };
static const u8 N_BE[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x51 };
static const u8 N_MINUS_2[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x4F };
static const u8 B_BE[32] = {
    0x5A,0xC6,0x35,0xD8,0xAA,0x3A,0x93,0xE7,0xB3,0xEB,0xBD,0x55,0x76,0x98,0x86,0xBC,
    0x65,0x1D,0x06,0xB0,0xCC,0x53,0xB0,0xF6,0x3B,0xCE,0x3C,0x3E,0x27,0xD2,0x60,0x4B };
static const u8 GX_BE[32] = {
    0x6B,0x17,0xD1,0xF2,0xE1,0x2C,0x42,0x47,0xF8,0xBC,0xE6,0xE5,0x63,0xA4,0x40,0xF2,
    0x77,0x03,0x7D,0x81,0x2D,0xEB,0x33,0xA0,0xF4,0xA1,0x39,0x45,0xD8,0x98,0xC2,0x96 };
static const u8 GY_BE[32] = {
    0x4F,0xE3,0x42,0xE2,0xFE,0x1A,0x7F,0x9B,0x8E,0xE7,0xEB,0x4A,0x7C,0x0F,0x9E,0x16,
    0x2B,0xCE,0x33,0x57,0x6B,0x31,0x5E,0xCE,0xCB,0xB6,0x40,0x68,0x37,0xBF,0x51,0xF5 };

typedef struct { u64 v[4]; } fe;                /* a field element in Montgomery form */
typedef struct { fe x, y, z; } pt;              /* Jacobian; z == 0 is the point at infinity */

static bn_mont Fp, Fn;
static fe  B, ONE;
static pt  G;
static bool ready;

static void fmul(fe *o, const fe *a, const fe *b) { bn_mont_mul(&Fp, o->v, a->v, b->v); }
static void fsqr(fe *o, const fe *a)             { bn_mont_mul(&Fp, o->v, a->v, a->v); }
static void fadd(fe *o, const fe *a, const fe *b) { bn_mod_add(&Fp, o->v, a->v, b->v); }
static void fsub(fe *o, const fe *a, const fe *b) { bn_mod_sub(&Fp, o->v, a->v, b->v); }
static bool fzero(const fe *a)                   { return bn_is_zero(a->v, 4); }
static bool feq(const fe *a, const fe *b)        { return bn_cmp(a->v, b->v, 4) == 0; }

static void fe_enter(fe *o, const u8 be[32])
{
    u64 t[4];
    bn_from_be(t, 4, be, 32);
    bn_mont_enter(&Fp, o->v, t);
}

static void setup(void)
{
    if (ready) return;
    bn_mont_init(&Fp, P_BE, 32);
    bn_mont_init(&Fn, N_BE, 32);
    fe_enter(&B, B_BE);
    fe_enter(&G.x, GX_BE);
    fe_enter(&G.y, GY_BE);
    bn_mont_one(&Fp, ONE.v);
    G.z = ONE;
    ready = true;
}

/* dbl-2001-b: doubling with a = -3. */
static void pt_double(pt *o, const pt *p)
{
    if (fzero(&p->z)) { *o = *p; return; }
    fe delta, gamma, beta, alpha, t1, t2, x3, y3, z3;
    fsqr(&delta, &p->z);
    fsqr(&gamma, &p->y);
    fmul(&beta, &p->x, &gamma);
    fsub(&t1, &p->x, &delta);
    fadd(&t2, &p->x, &delta);
    fmul(&t1, &t1, &t2);
    fadd(&alpha, &t1, &t1);
    fadd(&alpha, &alpha, &t1);                    /* 3 (x - delta)(x + delta) */

    fsqr(&t1, &alpha);
    fadd(&t2, &beta, &beta);
    fadd(&t2, &t2, &t2);
    fadd(&t2, &t2, &t2);                          /* 8 beta */
    fsub(&x3, &t1, &t2);

    fadd(&t1, &p->y, &p->z);
    fsqr(&t1, &t1);
    fsub(&t1, &t1, &gamma);
    fsub(&z3, &t1, &delta);

    fadd(&t1, &beta, &beta);
    fadd(&t1, &t1, &t1);                          /* 4 beta */
    fsub(&t1, &t1, &x3);
    fmul(&t1, &alpha, &t1);
    fsqr(&t2, &gamma);
    fadd(&t2, &t2, &t2);
    fadd(&t2, &t2, &t2);
    fadd(&t2, &t2, &t2);                          /* 8 gamma^2 */
    fsub(&y3, &t1, &t2);

    o->x = x3; o->y = y3; o->z = z3;
}

/* add-2007-bl: general addition, with the doubling and infinity cases. */
static void pt_add(pt *o, const pt *p, const pt *q)
{
    if (fzero(&p->z)) { *o = *q; return; }
    if (fzero(&q->z)) { *o = *p; return; }
    fe z1z1, z2z2, u1, u2, s1, s2, h, r, i, j, v, t, x3, y3, z3;
    fsqr(&z1z1, &p->z);
    fsqr(&z2z2, &q->z);
    fmul(&u1, &p->x, &z2z2);
    fmul(&u2, &q->x, &z1z1);
    fmul(&s1, &p->y, &q->z);
    fmul(&s1, &s1, &z2z2);
    fmul(&s2, &q->y, &p->z);
    fmul(&s2, &s2, &z1z1);
    fsub(&h, &u2, &u1);
    fsub(&r, &s2, &s1);
    if (fzero(&h)) {
        if (fzero(&r)) { pt_double(o, p); return; }
        o->x = ONE; o->y = ONE;
        for (u32 k = 0; k < 4; k++) o->z.v[k] = 0;
        return;
    }
    fadd(&r, &r, &r);                             /* 2 (s2 - s1) */
    fadd(&i, &h, &h);
    fsqr(&i, &i);                                 /* (2 h)^2 */
    fmul(&j, &h, &i);
    fmul(&v, &u1, &i);

    fsqr(&t, &r);
    fsub(&t, &t, &j);
    fsub(&t, &t, &v);
    fsub(&x3, &t, &v);                            /* r^2 - j - 2 v */

    fsub(&t, &v, &x3);
    fmul(&t, &r, &t);
    fmul(&s1, &s1, &j);
    fadd(&s1, &s1, &s1);
    fsub(&y3, &t, &s1);                           /* r (v - x3) - 2 s1 j */

    fadd(&t, &p->z, &q->z);
    fsqr(&t, &t);
    fsub(&t, &t, &z1z1);
    fsub(&t, &t, &z2z2);
    fmul(&z3, &t, &h);

    o->x = x3; o->y = y3; o->z = z3;
}

/* k P, k a plain 256-bit scalar, most significant bit first. */
static void pt_mul(pt *o, const pt *p, const u64 k[4])
{
    pt r;
    r.x = ONE; r.y = ONE;
    for (u32 i = 0; i < 4; i++) r.z.v[i] = 0;
    for (i32 bit = 255; bit >= 0; bit--) {
        pt_double(&r, &r);
        if ((k[bit / 64] >> (bit % 64)) & 1) pt_add(&r, &r, p);
    }
    *o = r;
}

/* The affine x of a point, as a plain number. False at infinity. */
static bool pt_affine_x(const pt *p, u64 x[4])
{
    if (fzero(&p->z)) return false;
    fe zi, zi2, t;
    bn_mont_pow(&Fp, zi.v, p->z.v, P_MINUS_2, 32);
    fsqr(&zi2, &zi);
    fmul(&t, &p->x, &zi2);
    bn_mont_leave(&Fp, x, t.v);
    return true;
}

/* Loads an uncompressed point; false unless both coordinates are below p
 * and the point satisfies the curve equation. */
static bool pt_load(pt *o, const u8 pub[65])
{
    if (pub[0] != 0x04) return false;
    u64 x[4], y[4], p[4];
    bn_from_be(x, 4, pub + 1, 32);
    bn_from_be(y, 4, pub + 33, 32);
    bn_from_be(p, 4, P_BE, 32);
    if (bn_cmp(x, p, 4) >= 0 || bn_cmp(y, p, 4) >= 0) return false;
    bn_mont_enter(&Fp, o->x.v, x);
    bn_mont_enter(&Fp, o->y.v, y);
    o->z = ONE;

    fe lhs, rhs, t;
    fsqr(&lhs, &o->y);
    fsqr(&t, &o->x);
    fmul(&rhs, &t, &o->x);                        /* x^3 */
    fadd(&t, &o->x, &o->x);
    fadd(&t, &t, &o->x);                          /* 3 x */
    fsub(&rhs, &rhs, &t);
    fadd(&rhs, &rhs, &B);
    return feq(&lhs, &rhs);
}

bool p256_point_ok(const u8 pub[65])
{
    setup();
    pt q;
    return pt_load(&q, pub);
}

bool p256_verify(const u8 pub[65], const u8 hash[32],
                 const u8 *r, u32 rlen, const u8 *s, u32 slen)
{
    setup();
    u64 n[4], rv[4], sv[4], ev[4];
    bn_from_be(n, 4, N_BE, 32);
    if (!bn_from_be(rv, 4, r, rlen) || !bn_from_be(sv, 4, s, slen)) return false;
    if (bn_is_zero(rv, 4) || bn_cmp(rv, n, 4) >= 0) return false;
    if (bn_is_zero(sv, 4) || bn_cmp(sv, n, 4) >= 0) return false;

    pt q;
    if (!pt_load(&q, pub)) return false;

    /* e: the hash as a number, reduced once (it is below 2 n). */
    bn_from_be(ev, 4, hash, 32);
    if (bn_cmp(ev, n, 4) >= 0) bn_mod_sub(&Fn, ev, ev, n);

    /* w = s^-1; u1 = e w; u2 = r w -- all modulo n. */
    u64 sm[4], wm[4], em[4], rm[4], u1[4], u2[4];
    bn_mont_enter(&Fn, sm, sv);
    bn_mont_pow(&Fn, wm, sm, N_MINUS_2, 32);
    bn_mont_enter(&Fn, em, ev);
    bn_mont_enter(&Fn, rm, rv);
    bn_mont_mul(&Fn, u1, em, wm);
    bn_mont_leave(&Fn, u1, u1);
    bn_mont_mul(&Fn, u2, rm, wm);
    bn_mont_leave(&Fn, u2, u2);

    pt a, b, sum;
    pt_mul(&a, &G, u1);
    pt_mul(&b, &q, u2);
    pt_add(&sum, &a, &b);

    u64 x[4];
    if (!pt_affine_x(&sum, x)) return false;
    if (bn_cmp(x, n, 4) >= 0) bn_mod_sub(&Fn, x, x, n);
    return bn_cmp(x, rv, 4) == 0;
}
