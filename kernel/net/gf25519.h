/*
 * gf25519.h -- the field under both curves, shared between x25519.c
 * and ed25519.c. Private to the net directory: nothing above the
 * two curve files needs a field element.
 */
#ifndef EB_GF25519_H
#define EB_GF25519_H

#include <eb/types.h>

typedef i64 gf[16];

void car25519(gf o);
void sel25519(gf p, gf q, i32 b);
void pack25519(u8 *o, const gf n);
void unpack25519(gf o, const u8 *n);
void A(gf o, const gf a, const gf b);
void Z(gf o, const gf a, const gf b);
void M(gf o, const gf a, const gf b);
void S(gf o, const gf a);
void inv25519(gf o, const gf i);

#endif /* EB_GF25519_H */
