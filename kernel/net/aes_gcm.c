/*
 * aes_gcm.c -- AES-128 in Galois/Counter mode, software only (vector units stay off).
 * - constant-time: the S-box is computed by inversion in GF(2^8), not
 *   looked up, so no secret byte indexes memory; ShiftRows is a fixed
 *   permutation and MixColumns is arithmetic (xtime). GHASH multiplies
 *   without a data-dependent branch. Nothing secret steers a memory
 *   access or a branch, so there is no table-timing side channel.
 * - this is slower than a table lookup; runtime is not the constraint
 *   here. The computed S-box is checked against the reference table for
 *   all 256 inputs at boot (aes_sbox_ct_ok), so an error cannot hide.
 */
#include <eb/crypto.h>
#include <eb/cpu.h>
#include <eb/io.h>
#include <eb/spin.h>

/* ------------------------------------------------------------------ */
/* AES-128                                                             */
/* ------------------------------------------------------------------ */

/* The reference S-box. Used only to check the computed one at boot
 * (aes_sbox_ct_ok); the cipher itself never indexes it, so no secret
 * ever selects a row of it. */
static const u8 sbox[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

static u8 xtime(u8 x) { return (u8)((x << 1) ^ ((x >> 7) * 0x1b)); }

/* Multiply in GF(2^8) modulo x^8 + x^4 + x^3 + x + 1 (0x11b), constant
 * time: eight fixed rounds, every conditional turned into a mask, no
 * branch or lookup on the operands. */
static u8 gf_mul8(u8 a, u8 b)
{
    u8 p = 0;
    for (u32 i = 0; i < 8; i++) {
        p ^= (u8)(0 - (b & 1)) & a;              /* add a when b's low bit is set */
        u8 hi = (u8)(0 - (a >> 7));               /* 0xff when a's high bit is set */
        a = (u8)(a << 1) ^ (u8)(hi & 0x1b);       /* times x, reduced */
        b >>= 1;
    }
    return p;
}

/* The multiplicative inverse in GF(2^8): x^254 = x^-1 for x != 0, and
 * 0 maps to 0 (which is what the S-box wants). Built by the
 * Itoh-Tsujii chain x -> x^(2^7 - 1) -> squared, so the exponent is
 * fixed and the work does not depend on x. */
static u8 gf_inv8(u8 x)
{
    u8 a = x;                                     /* x^(2^1 - 1) */
    for (u32 i = 0; i < 6; i++)
        a = gf_mul8(gf_mul8(a, a), x);            /* -> x^(2^7 - 1) = x^127 */
    return gf_mul8(a, a);                         /* x^254 */
}

static u8 rotl8(u8 x, u32 n) { return (u8)((x << n) | (x >> (8 - n))); }

/* The forward S-box, computed: inverse in GF(2^8) then the affine map
 * s = v ^ rotl(v,1) ^ rotl(v,2) ^ rotl(v,3) ^ rotl(v,4) ^ 0x63. */
static u8 sbox_ct(u8 x)
{
    u8 v = gf_inv8(x);
    return (u8)(v ^ rotl8(v, 1) ^ rotl8(v, 2) ^ rotl8(v, 3) ^ rotl8(v, 4) ^ 0x63);
}

/* The inverse S-box, computed: the inverse affine map first
 * (s = rotl(x,1) ^ rotl(x,3) ^ rotl(x,6) ^ 0x05), then inversion in
 * GF(2^8). Used by the RFC 3394 key unwrap, the one place AES runs
 * backwards. Constant time, no table. */
static u8 sbox_inv_ct(u8 x)
{
    u8 v = (u8)(rotl8(x, 1) ^ rotl8(x, 3) ^ rotl8(x, 6) ^ 0x05);
    return gf_inv8(v);
}
u8 aes_sbox_inv(u8 x) { return sbox_inv_ct(x); }

/* Checked at boot: the computed forward S-box equals the reference for
 * every input, and the computed inverse undoes it for every input --
 * which, given the forward is correct, is the whole inverse table proved
 * without carrying one. TLS stays down if either fails. */
bool aes_sbox_ct_ok(void)
{
    for (u32 i = 0; i < 256; i++) {
        u8 s = sbox_ct((u8)i);
        if (s != sbox[i]) return false;
        if (sbox_inv_ct(s) != (u8)i) return false;
    }
    return true;
}

static void aes128_expand(aes_key *k, const u8 key[16])
{
    for (u32 i = 0; i < 16; i++) k->rk[i] = key[i];
    u8 rcon = 1;
    for (u32 i = 16; i < 176; i += 4) {
        u8 t[4];
        for (u32 j = 0; j < 4; j++) t[j] = k->rk[i - 4 + j];
        if (i % 16 == 0) {
            u8 tmp = t[0];
            t[0] = (u8)(sbox_ct(t[1]) ^ rcon);
            t[1] = sbox_ct(t[2]);
            t[2] = sbox_ct(t[3]);
            t[3] = sbox_ct(tmp);
            rcon = xtime(rcon);
        }
        for (u32 j = 0; j < 4; j++) k->rk[i + j] = k->rk[i - 16 + j] ^ t[j];
    }
}

static void aes128_encrypt(const aes_key *k, const u8 in[16], u8 out[16])
{
    u8 s[16];
    for (u32 i = 0; i < 16; i++) s[i] = in[i] ^ k->rk[i];

    for (u32 round = 1; round <= 10; round++) {
        u8 t[16];
        for (u32 i = 0; i < 16; i++) t[i] = sbox_ct(s[i]);

        /* ShiftRows: row r rotates left by r, in column-major order. */
        u8 sr[16];
        sr[0]=t[0];  sr[4]=t[4];  sr[8]=t[8];   sr[12]=t[12];
        sr[1]=t[5];  sr[5]=t[9];  sr[9]=t[13];  sr[13]=t[1];
        sr[2]=t[10]; sr[6]=t[14]; sr[10]=t[2];  sr[14]=t[6];
        sr[3]=t[15]; sr[7]=t[3];  sr[11]=t[7];  sr[15]=t[11];

        if (round < 10) {
            for (u32 c = 0; c < 4; c++) {
                u8 *col = sr + c * 4;
                u8 a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                u8 x = a0 ^ a1 ^ a2 ^ a3;
                col[0] ^= x ^ xtime((u8)(a0 ^ a1));
                col[1] ^= x ^ xtime((u8)(a1 ^ a2));
                col[2] ^= x ^ xtime((u8)(a2 ^ a3));
                col[3] ^= x ^ xtime((u8)(a3 ^ a0));
            }
        }
        for (u32 i = 0; i < 16; i++) s[i] = sr[i] ^ k->rk[round * 16 + i];
    }
    for (u32 i = 0; i < 16; i++) out[i] = s[i];
}

/* ------------------------------------------------------------------ */
/* GHASH: multiply in GF(2^128), the GCM way                           */
/* ------------------------------------------------------------------ */

static u64 be64(const u8 *p)
{
    u64 v = 0;
    for (u32 i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

static void put_be64(u8 *p, u64 v)
{
    for (u32 i = 0; i < 8; i++) p[i] = (u8)(v >> (56 - i * 8));
}

/* Carry-less multiply in GF(2^128), the GCM convention: right shift,
 * with 0xe1 folded into the top when a bit falls off the bottom. */
static void gf_mul(u8 x[16], const u8 y[16])
{
    u64 v0 = be64(y), v1 = be64(y + 8);
    u64 z0 = 0, z1 = 0;

    for (u32 i = 0; i < 128; i++) {
        /* The bit index i runs 0..127 and is public; only the bit's
         * value is secret, so it is folded in with a mask, not a branch. */
        u64 bit = (u64)((x[i >> 3] >> (7 - (i & 7))) & 1);
        u64 m = (u64)0 - bit;                 /* all ones when the bit is set */
        z0 ^= v0 & m; z1 ^= v1 & m;
        u64 lsb = v1 & 1;
        v1 = (v1 >> 1) | (v0 << 63);
        v0 >>= 1;
        v0 ^= 0xe100000000000000ULL & ((u64)0 - lsb);
    }
    put_be64(x, z0);
    put_be64(x + 8, z1);
}

static void ghash(const u8 H[16], const u8 *data, u32 len, u8 y[16])
{
    u32 i = 0;
    while (i + 16 <= len) {
        for (u32 j = 0; j < 16; j++) y[j] ^= data[i + j];
        gf_mul(y, H);
        i += 16;
    }
    if (i < len) {
        for (u32 j = 0; i + j < len; j++) y[j] ^= data[i + j];
        gf_mul(y, H);
    }
}

/* ------------------------------------------------------------------ */
/* GCM                                                                 */
/* ------------------------------------------------------------------ */

static void gcm_core(const u8 key[16], const u8 iv[12],
                     const u8 *aad, u32 alen,
                     const u8 *in, u32 len, u8 *out, u8 tag[16],
                     bool decrypt)
{
    aes_key k;
    aes128_expand(&k, key);

    u8 H[16] = { 0 };
    aes128_encrypt(&k, H, H);

    /* Counter blocks: J0 is the iv with a 1 in the low word; the
     * keystream runs from J0+1, the tag mask comes from J0 itself. */
    u8 ctr[16];
    for (u32 i = 0; i < 12; i++) ctr[i] = iv[i];
    ctr[12] = 0; ctr[13] = 0; ctr[14] = 0; ctr[15] = 1;

    u8 j0[16];
    for (u32 i = 0; i < 16; i++) j0[i] = ctr[i];

    /* The tag authenticates the aad and the ciphertext, each padded to
     * a block, then the two bit-lengths. When opening, the ciphertext
     * is the input and must be hashed BEFORE the counter loop turns it
     * into plaintext -- the two can be the same buffer, and often are. */
    u8 y[16] = { 0 };
    ghash(H, aad, alen, y);
    if (decrypt) ghash(H, in, len, y);

    for (u32 off = 0; off < len; off += 16) {
        /* ++counter, then encrypt it into a keystream block. */
        for (i32 i = 15; i >= 12; i--) { if (++ctr[i]) break; }
        u8 ks[16];
        aes128_encrypt(&k, ctr, ks);
        u32 take = len - off < 16 ? len - off : 16;
        for (u32 i = 0; i < take; i++) out[off + i] = in[off + i] ^ ks[i];
    }

    if (!decrypt) ghash(H, out, len, y);

    u8 lens[16] = { 0 };
    u64 abits = (u64)alen * 8, cbits = (u64)len * 8;
    for (u32 i = 0; i < 8; i++) lens[7 - i]  = (u8)(abits >> (i * 8));
    for (u32 i = 0; i < 8; i++) lens[15 - i] = (u8)(cbits >> (i * 8));
    for (u32 i = 0; i < 16; i++) y[i] ^= lens[i];
    gf_mul(y, H);

    u8 mask[16];
    aes128_encrypt(&k, j0, mask);
    for (u32 i = 0; i < 16; i++) tag[i] = y[i] ^ mask[i];
}

/* The block cipher on its own, for the modes that live elsewhere:
 * counter mode with a cbc mac for the wireless frames, key wrapping
 * for the group key. */
void aes128_setkey(aes_key *k, const u8 key[16]) { aes128_expand(k, key); }
void aes128_block(const aes_key *k, const u8 in[16], u8 out[16]) { aes128_encrypt(k, in, out); }

void aes128_gcm_seal(const u8 key[16], const u8 iv[12],
                     const u8 *aad, u32 alen,
                     const u8 *pt, u32 len, u8 *ct, u8 tag[16])
{
    gcm_core(key, iv, aad, alen, pt, len, ct, tag, false);
}

bool aes128_gcm_open(const u8 key[16], const u8 iv[12],
                     const u8 *aad, u32 alen,
                     const u8 *ct, u32 len, const u8 tag[16], u8 *pt)
{
    /* The tag is computed over the ciphertext, so it can be checked
     * before a single plaintext byte is handed back -- which is the
     * whole discipline of authenticated decryption. gcm_core writes
     * plaintext as it goes; the tag it returns is compared, and a
     * mismatch means the caller must throw the plaintext away. */
    u8 want[16];
    gcm_core(key, iv, aad, alen, ct, len, pt, want, true);

    u8 diff = 0;
    for (u32 i = 0; i < 16; i++) diff |= want[i] ^ tag[i];
    if (diff) {
        for (u32 i = 0; i < len; i++) pt[i] = 0;
        return false;
    }
    return true;
}

/* Compares two byte strings without an early exit, so the time taken
 * does not reveal how many leading bytes matched. For anything a mismatch
 * must not be timed on -- a message tag, a key confirmation. */
bool ct_equal(const void *a, const void *b, u32 len)
{
    const u8 *x = (const u8 *)a, *y = (const u8 *)b;
    u8 diff = 0;
    for (u32 i = 0; i < len; i++) diff |= (u8)(x[i] ^ y[i]);
    return diff == 0;
}

/* ------------------------------------------------------------------ */
/* Randomness                                                          */
/* ------------------------------------------------------------------ */

static bool rdrand64(u64 *v)
{
    u8 ok = 0;
    __asm__ volatile ("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok != 0;
}

static bool rdseed64(u64 *v)
{
    u8 ok = 0;
    __asm__ volatile ("rdseed %0; setc %1" : "=r"(*v), "=qm"(ok) :: "cc");
    return ok != 0;
}

/* A hash-based entropy pool.
 *
 * The pool accumulates whatever true randomness the hardware offers --
 * RDSEED first, the seeded source; then RDRAND -- folded together with
 * the cycle counter through SHA-256, so no output rests on a single
 * sample. Each request stirs in fresh entropy and then returns
 * H(pool || counter): the counter keeps successive blocks distinct, and
 * hashing the pool rather than emitting it means an output never reveals
 * the pool it came from. On a machine with neither RDSEED nor RDRAND the
 * cycle counter is all there is, which is weak and predictable -- the
 * hardware's limit, said plainly, not a choice this makes. */
static u8       entropy_pool[32];
static u64      rng_counter;
static bool     rng_ready;
static bool     have_rdrand, have_rdseed;
static spinlock rng_lock;

static void pool_stir(void)
{
    u64 s[4] = { 0, 0, rdtsc(), rng_counter };
    if (have_rdseed)
        for (u32 t = 0; t < 32 && !rdseed64(&s[0]); t++) __asm__ volatile ("pause");
    if (have_rdrand)
        for (u32 t = 0; t < 10 && !rdrand64(&s[1]); t++) { }

    u8 buf[32 + sizeof(s)];
    for (u32 i = 0; i < 32; i++) buf[i] = entropy_pool[i];
    for (u32 i = 0; i < sizeof(s); i++) buf[32 + i] = ((const u8 *)s)[i];
    sha256(buf, sizeof(buf), entropy_pool);
}

void rand_bytes(u8 *out, u32 len)
{
    u64 flags = spin_lock_irq(&rng_lock);

    if (!rng_ready) {
        cpu_info c;
        cpu_detect(&c);
        have_rdrand = c.rdrand;
        have_rdseed = c.rdseed;
        for (u32 i = 0; i < 24; i++) pool_stir();   /* fill the pool before first use */
        rng_ready = true;
    }

    u32 i = 0;
    while (i < len) {
        pool_stir();                                /* fresh entropy per block */
        u8 buf[32 + 8], block[32];
        for (u32 j = 0; j < 32; j++) buf[j] = entropy_pool[j];
        u64 ctr = ++rng_counter;
        for (u32 j = 0; j < 8; j++) buf[32 + j] = (u8)(ctr >> (j * 8));
        sha256(buf, sizeof(buf), block);            /* output = H(pool || counter) */
        for (u32 j = 0; j < 32 && i < len; j++) out[i++] = block[j];
    }

    spin_unlock_irq(&rng_lock, flags);
}
