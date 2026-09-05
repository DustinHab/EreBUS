/*
 * rsa.c -- RSA signature verification with SHA-256: PKCS#1 v1.5 (certificate chains) and PSS (TLS 1.3 CertificateVerify).
 * - the public operation is one exponentiation on bn.c; moduli of 2048 to 4096 bits, any exponent
 * - v1.5: the encoded message is compared whole against the expected bytes; PSS: salt of 32 bytes, MGF1 with SHA-256
 */
#include <eb/pki.h>
#include <eb/bn.h>
#include <eb/crypto.h>

static bn_mont M;                       /* a kibibyte; kept off the small thread stacks */

/* The DigestInfo of SHA-256, as PKCS#1 spells it out. */
static const u8 SHA256_INFO[19] = {
    0x30,0x31,0x30,0x0D,0x06,0x09,0x60,0x86,0x48,0x01,
    0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20 };

/* sig^e mod n as big-endian bytes, as long as the modulus. */
static bool rsa_public(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                       const u8 *sig, u32 siglen, u8 *em, u32 *emlen)
{
    while (nlen > 0 && n[0] == 0) { n++; nlen--; }
    while (elen > 0 && e[0] == 0) { e++; elen--; }
    if (nlen < 256 || nlen > 512 || elen == 0 || elen > nlen) return false;
    if (siglen != nlen) return false;
    if (!bn_mont_init(&M, n, nlen)) return false;

    u64 s[BN_MAX], t[BN_MAX];
    if (!bn_from_be(s, M.n, sig, siglen)) return false;
    if (bn_cmp(s, M.m, M.n) >= 0) return false;
    bn_mont_enter(&M, t, s);
    bn_mont_pow(&M, s, t, e, elen);
    bn_mont_leave(&M, t, s);
    bn_to_be(t, M.n, em, nlen);
    *emlen = nlen;
    return true;
}

bool rsa_verify_pkcs1_sha256(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                             const u8 hash[32], const u8 *sig, u32 siglen)
{
    u8 em[512];
    u32 k;
    if (!rsa_public(n, nlen, e, elen, sig, siglen, em, &k)) return false;

    /* 00 01 FF..FF 00 DigestInfo hash, with at least eight FF bytes. */
    u32 tlen = 19 + 32;
    if (k < tlen + 11) return false;
    if (em[0] != 0x00 || em[1] != 0x01) return false;
    u32 i = 2;
    while (i < k - tlen - 1) {
        if (em[i] != 0xFF) return false;
        i++;
    }
    if (em[i] != 0x00) return false;
    i++;
    for (u32 j = 0; j < 19; j++) if (em[i + j] != SHA256_INFO[j]) return false;
    for (u32 j = 0; j < 32; j++) if (em[i + 19 + j] != hash[j]) return false;
    return true;
}

/* MGF1 with SHA-256: the mask of the seed, counter by counter. */
static void mgf1(const u8 seed[32], u8 *out, u32 len)
{
    u8 block[36];
    for (u32 i = 0; i < 32; i++) block[i] = seed[i];
    u32 at = 0;
    for (u32 c = 0; at < len; c++) {
        block[32] = (u8)(c >> 24); block[33] = (u8)(c >> 16);
        block[34] = (u8)(c >> 8);  block[35] = (u8)c;
        u8 h[32];
        sha256(block, 36, h);
        for (u32 i = 0; i < 32 && at < len; i++) out[at++] = h[i];
    }
}

bool rsa_verify_pss_sha256(const u8 *n, u32 nlen, const u8 *e, u32 elen,
                           const u8 hash[32], const u8 *sig, u32 siglen)
{
    u8 em[512];
    u32 k;
    if (!rsa_public(n, nlen, e, elen, sig, siglen, em, &k)) return false;

    /* The bit length of the modulus decides the length of the encoded
     * message: one bit less, rounded up to bytes. */
    while (nlen > 0 && n[0] == 0) { n++; nlen--; }
    u32 lead = 0;
    for (u8 b = n[0]; !(b & 0x80); b = (u8)(b << 1)) lead++;
    u32 mod_bits = 8 * nlen - lead;
    u32 em_bits = mod_bits - 1;
    u32 em_len = (em_bits + 7) / 8;
    if (em_len > k) return false;
    for (u32 i = 0; i < k - em_len; i++) if (em[i] != 0) return false;
    const u8 *EM = em + (k - em_len);

    const u32 hlen = 32, slen = 32;
    if (em_len < hlen + slen + 2) return false;
    if (EM[em_len - 1] != 0xBC) return false;

    u32 dblen = em_len - hlen - 1;
    const u8 *masked = EM;
    const u8 *H = EM + dblen;
    u32 top = 8 * em_len - em_bits;               /* leading bits that must be zero */
    if (top && (masked[0] >> (8 - top)) != 0) return false;

    u8 db[512];
    mgf1(H, db, dblen);
    for (u32 i = 0; i < dblen; i++) db[i] ^= masked[i];
    if (top) db[0] &= (u8)(0xFF >> top);

    u32 pad = em_len - hlen - slen - 2;           /* zeros before the 01 */
    for (u32 i = 0; i < pad; i++) if (db[i] != 0) return false;
    if (db[pad] != 0x01) return false;
    const u8 *salt = db + pad + 1;

    /* H' = SHA-256(8 zeros, the message hash, the salt) must equal H. */
    u8 m2[8 + 32 + 32];
    for (u32 i = 0; i < 8; i++) m2[i] = 0;
    for (u32 i = 0; i < 32; i++) m2[8 + i] = hash[i];
    for (u32 i = 0; i < slen; i++) m2[40 + i] = salt[i];
    u8 h2[32];
    sha256(m2, sizeof m2, h2);
    for (u32 i = 0; i < 32; i++) if (h2[i] != H[i]) return false;
    return true;
}
