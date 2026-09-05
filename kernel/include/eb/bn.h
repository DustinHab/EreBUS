/*
 * bn.h -- fixed-capacity big numbers for signature checking: arithmetic modulo an odd number in Montgomery form.
 * - little-endian 64-bit limbs, up to 4096 bits; the modulus fixes the limb count of every operand
 * - verification only: nothing here holds a secret, and nothing here is constant-time
 */
#ifndef EB_BN_H
#define EB_BN_H

#include <eb/types.h>

#define BN_MAX 64                      /* limbs: 4096 bits */

typedef struct {
    u32 n;                             /* limbs in the modulus */
    u64 m[BN_MAX];                     /* the modulus */
    u64 rr[BN_MAX];                    /* R^2 mod m, R = 2^(64 n) */
    u64 m0;                            /* -m^-1 mod 2^64 */
} bn_mont;

/* An odd modulus of 1..BN_MAX limbs, big-endian bytes; leading zeros are allowed. */
bool bn_mont_init(bn_mont *c, const u8 *mod_be, u32 len);

void bn_mont_mul(const bn_mont *c, u64 *out, const u64 *a, const u64 *b);   /* a b R^-1 mod m */
void bn_mont_enter(const bn_mont *c, u64 *out, const u64 *a);               /* a -> a R */
void bn_mont_leave(const bn_mont *c, u64 *out, const u64 *a);               /* a R -> a */
void bn_mont_one(const bn_mont *c, u64 *out);                               /* 1, in Montgomery form */
/* base^exp with base and result in Montgomery form; the exponent big-endian. */
void bn_mont_pow(const bn_mont *c, u64 *out, const u64 *base, const u8 *exp_be, u32 elen);

/* Plain modular addition and subtraction of operands below m. */
void bn_mod_add(const bn_mont *c, u64 *out, const u64 *a, const u64 *b);
void bn_mod_sub(const bn_mont *c, u64 *out, const u64 *a, const u64 *b);

bool bn_from_be(u64 *out, u32 n, const u8 *p, u32 len);   /* false when the bytes need more than n limbs */
void bn_to_be(const u64 *a, u32 n, u8 *out, u32 len);     /* the low len bytes, big-endian */
int  bn_cmp(const u64 *a, const u64 *b, u32 n);
bool bn_is_zero(const u64 *a, u32 n);

#endif /* EB_BN_H */
