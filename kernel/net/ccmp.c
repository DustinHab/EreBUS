/*
 * ccmp.c -- AES-CCM as 802.11 seals frames, and AES key unwrap.
 * - one key: CBC-MAC over header and plaintext, counter stream over both
 * - nonce 13 bytes (sender address + packet number), tag 8 bytes
 */
#include <eb/crypto.h>
#include <eb/string.h>

/* The cbc mac over: B0 (flags, nonce, length), the header with its
 * length in front, padded; the plaintext, padded. */
static void ccm_mac(const aes_key *k, const u8 nonce[13],
                    const u8 *aad, u32 alen, const u8 *in, u32 len, u8 x[16])
{
    u8 b[16];
    /* flags: adata present, M = 8 -> (8-2)/2 = 3 in bits 5:3, L = 2 -> 1 */
    b[0] = (u8)(0x40 | (3 << 3) | 1);
    memcpy(b + 1, nonce, 13);
    b[14] = (u8)(len >> 8);
    b[15] = (u8)len;
    aes128_block(k, b, x);

    /* the header: two bytes of length, then the bytes, padded to 16 */
    u32 at = 0;
    memset(b, 0, 16);
    b[0] = (u8)(alen >> 8);
    b[1] = (u8)alen;
    at = 2;
    for (u32 i = 0; i < alen; i++) {
        b[at++] = aad[i];
        if (at == 16) {
            for (u32 j = 0; j < 16; j++) x[j] ^= b[j];
            aes128_block(k, x, x);
            memset(b, 0, 16);
            at = 0;
        }
    }
    if (at) {
        for (u32 j = 0; j < 16; j++) x[j] ^= b[j];
        aes128_block(k, x, x);
    }

    /* the payload, padded */
    for (u32 off = 0; off < len; off += 16) {
        memset(b, 0, 16);
        u32 take = len - off < 16 ? len - off : 16;
        memcpy(b, in + off, take);
        for (u32 j = 0; j < 16; j++) x[j] ^= b[j];
        aes128_block(k, x, x);
    }
}

/* Counter block i: flags L-1, the nonce, the counter. */
static void ccm_ctr(const aes_key *k, const u8 nonce[13], u16 i, u8 out[16])
{
    u8 a[16];
    a[0] = 1;
    memcpy(a + 1, nonce, 13);
    a[14] = (u8)(i >> 8);
    a[15] = (u8)i;
    aes128_block(k, a, out);
}

void aes_ccm_seal(const aes_key *k, const u8 nonce[13],
                  const u8 *aad, u32 alen, const u8 *in, u32 len,
                  u8 *out, u8 tag[8])
{
    u8 x[16], s[16];
    ccm_mac(k, nonce, aad, alen, in, len, x);
    ccm_ctr(k, nonce, 0, s);
    for (u32 j = 0; j < 8; j++) tag[j] = x[j] ^ s[j];
    for (u32 off = 0, i = 1; off < len; off += 16, i++) {
        ccm_ctr(k, nonce, (u16)i, s);
        u32 take = len - off < 16 ? len - off : 16;
        for (u32 j = 0; j < take; j++) out[off + j] = in[off + j] ^ s[j];
    }
}

bool aes_ccm_open(const aes_key *k, const u8 nonce[13],
                  const u8 *aad, u32 alen, const u8 *in, u32 len,
                  const u8 tag[8], u8 *out)
{
    u8 x[16], s[16];
    for (u32 off = 0, i = 1; off < len; off += 16, i++) {
        ccm_ctr(k, nonce, (u16)i, s);
        u32 take = len - off < 16 ? len - off : 16;
        for (u32 j = 0; j < take; j++) out[off + j] = in[off + j] ^ s[j];
    }
    ccm_mac(k, nonce, aad, alen, out, len, x);
    ccm_ctr(k, nonce, 0, s);
    u8 diff = 0;
    for (u32 j = 0; j < 8; j++) diff |= (u8)(tag[j] ^ x[j] ^ s[j]);
    if (diff) { memset(out, 0, len); return false; }
    return true;
}

/* --- the inverse cipher ----------------------------------------------- */

/* Unwrapping runs AES backwards, which the sealing modes never need. The
 * inverse S-box is computed in constant time (aes_sbox_inv), like the
 * forward one, so the group-key unwrap holds no secret-indexed table. */

static u8 gmul(u8 a, u8 b)
{
    u8 p = 0;
    while (b) {
        if (b & 1) p ^= a;
        a = (u8)((a << 1) ^ ((a >> 7) * 0x1b));
        b >>= 1;
    }
    return p;
}

static void aes128_block_inverse(const aes_key *k, const u8 in[16], u8 out[16])
{
    u8 s[16];
    for (u32 i = 0; i < 16; i++) s[i] = in[i] ^ k->rk[160 + i];
    for (i32 round = 9; round >= 0; round--) {
        /* inverse shift rows */
        u8 t[16];
        for (u32 c = 0; c < 4; c++)
            for (u32 r = 0; r < 4; r++)
                t[((c + r) % 4) * 4 + r] = s[c * 4 + r];
        /* inverse sub bytes, add round key */
        for (u32 i = 0; i < 16; i++) s[i] = aes_sbox_inv(t[i]) ^ k->rk[round * 16 + i];
        if (round == 0) break;
        /* inverse mix columns */
        for (u32 c = 0; c < 4; c++) {
            u8 a0 = s[c * 4], a1 = s[c * 4 + 1], a2 = s[c * 4 + 2], a3 = s[c * 4 + 3];
            s[c * 4]     = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
            s[c * 4 + 1] = gmul(a0, 9)  ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
            s[c * 4 + 2] = gmul(a0, 13) ^ gmul(a1, 9)  ^ gmul(a2, 14) ^ gmul(a3, 11);
            s[c * 4 + 3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9)  ^ gmul(a3, 14);
        }
    }
    memcpy(out, s, 16);
}

/* RFC 3394, the unwrapping direction only: n blocks of eight bytes
 * come out of n+1, and the first register must end as A6 repeated. */
bool aes_unwrap(const u8 kek[16], const u8 *in, u32 len, u8 *out)
{
    if (len < 24 || len % 8) return false;
    u32 n = len / 8 - 1;
    aes_key k;
    aes128_setkey(&k, kek);

    u8 a[8];
    memcpy(a, in, 8);
    memcpy(out, in + 8, n * 8);
    for (i32 j = 5; j >= 0; j--) {
        for (u32 i = n; i >= 1; i--) {
            u64 t = (u64)n * (u64)j + i;
            u8 b[16], d[16];
            memcpy(b, a, 8);
            for (u32 q = 0; q < 8; q++) b[q] ^= (u8)(t >> (56 - 8 * q));
            memcpy(b + 8, out + (i - 1) * 8, 8);
            aes128_block_inverse(&k, b, d);
            memcpy(a, d, 8);
            memcpy(out + (i - 1) * 8, d + 8, 8);
        }
    }
    for (u32 q = 0; q < 8; q++) if (a[q] != 0xA6) return false;
    return true;
}
