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

/* Unwrapping runs AES backwards, which the sealing modes never need;
 * the inverse rounds live here, with their own table. */
static const u8 inv_sbox[256] = {
0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d };

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
        for (u32 i = 0; i < 16; i++) s[i] = inv_sbox[t[i]] ^ k->rk[round * 16 + i];
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
