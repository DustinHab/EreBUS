#ifndef EB_CRYPTO_H
#define EB_CRYPTO_H

#include <eb/types.h>

/* The primitives under the seal, written here and tested at start-up
 * against published vectors like every other layer of this system.
 * One hash, one curve, one cipher: exactly what one TLS 1.3 suite
 * needs, because every primitive beyond the needed ones is attack
 * surface wearing a feature's clothes.
 *
 * The AES is software, tables and all. The kernel keeps the vector
 * units off everywhere -- there is no SSE state to save because
 * nothing may use it -- and honest table lookups beat a half-done
 * hardware path. Table timing is a real side channel; for a
 * single-person machine speaking outward it is noted, bounded, and
 * accepted rather than hidden. */

typedef struct {
    u32 h[8];
    u8  buf[64];
    u64 total;
    u32 fill;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, u64 len);
void sha256_final(sha256_ctx *c, u8 out[32]);
void sha256(const void *data, u64 len, u8 out[32]);

void hmac_sha256(const u8 *key, u32 klen,
                 const void *msg, u64 mlen, u8 out[32]);
void hkdf_extract(const u8 *salt, u32 slen,
                  const u8 *ikm, u32 ilen, u8 out[32]);
void hkdf_expand(const u8 prk[32], const u8 *info, u32 ilen,
                 u8 *out, u32 olen);

/* The wider hash, for the signature that is defined over it. */
typedef struct {
    u64 h[8];
    u8  buf[128];
    u64 total;
    u32 fill;
} sha512_ctx;

void sha512_init(sha512_ctx *c);
void sha512_update(sha512_ctx *c, const void *data, u64 len);
void sha512_final(sha512_ctx *c, u8 out[64]);
void sha512(const void *data, u64 len, u8 out[64]);

/* Curve25519 scalar multiplication, RFC 7748. */
void x25519(u8 out[32], const u8 scalar[32], const u8 point[32]);
void x25519_base(u8 out[32], const u8 scalar[32]);

/* Ed25519 signatures, RFC 8032: a public key from a 32-byte seed, a
 * signature over a message, and its check. The name the seal never
 * had: whoever holds the seed is whoever signs. */
void ed25519_public(u8 pk[32], const u8 seed[32]);
void ed25519_sign(u8 sig[64], const u8 seed[32], const u8 pk[32],
                  const void *msg, u32 len);
bool ed25519_verify(const u8 pk[32], const void *msg, u32 len,
                    const u8 sig[64]);

/* AES-128-GCM, seal and open. A 96-bit nonce, a 128-bit tag. */
void aes128_gcm_seal(const u8 key[16], const u8 iv[12],
                     const u8 *aad, u32 alen,
                     const u8 *pt, u32 len, u8 *ct, u8 tag[16]);
bool aes128_gcm_open(const u8 key[16], const u8 iv[12],
                     const u8 *aad, u32 alen,
                     const u8 *ct, u32 len, const u8 tag[16], u8 *pt);

/* Random bytes: rdrand where the processor offers it, and a hash
 * chain over the cycle counter where it does not. */
void rand_bytes(u8 *out, u32 len);

/* Known-answer tests for all of the above. TLS refuses to exist when
 * these fail: a seal that cannot prove itself seals nothing. */
bool crypto_selftest(void);

#endif /* EB_CRYPTO_H */
