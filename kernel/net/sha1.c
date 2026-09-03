/*
 * sha1.c -- SHA-1 and what wireless security builds on it.
 *
 * SHA-1 is not a hash to sign anything with any more; WPA2 keeps it
 * only inside keyed constructions -- the passphrase derivation, the
 * key expansion, the handshake's integrity check -- where the known
 * weaknesses do not reach. So it is here, for that, and nothing else
 * should reach for it.
 */
#include <eb/crypto.h>
#include <eb/string.h>

static u32 rol(u32 v, u32 n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(sha1_ctx *c, const u8 *p)
{
    u32 w[80];
    for (u32 i = 0; i < 16; i++)
        w[i] = ((u32)p[i * 4] << 24) | ((u32)p[i * 4 + 1] << 16) |
               ((u32)p[i * 4 + 2] << 8) | (u32)p[i * 4 + 3];
    for (u32 i = 16; i < 80; i++)
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    u32 a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (u32 i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20)      { f = (b & cc) | (~b & d);            k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d;                     k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d);  k = 0x8F1BBCDC; }
        else             { f = b ^ cc ^ d;                     k = 0xCA62C1D6; }
        u32 t = rol(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

void sha1_init(sha1_ctx *c)
{
    c->h[0] = 0x67452301; c->h[1] = 0xEFCDAB89; c->h[2] = 0x98BADCFE;
    c->h[3] = 0x10325476; c->h[4] = 0xC3D2E1F0;
    c->len = 0;
    c->fill = 0;
}

void sha1_update(sha1_ctx *c, const void *data, u64 len)
{
    const u8 *p = (const u8 *)data;
    c->len += len;
    while (len) {
        u32 take = 64 - c->fill;
        if (take > len) take = (u32)len;
        memcpy(c->buf + c->fill, p, take);
        c->fill += take;
        p += take;
        len -= take;
        if (c->fill == 64) { sha1_block(c, c->buf); c->fill = 0; }
    }
}

void sha1_final(sha1_ctx *c, u8 out[20])
{
    u64 bits = c->len * 8;
    u8 pad = 0x80;
    sha1_update(c, &pad, 1);
    u8 zero = 0;
    while (c->fill != 56) sha1_update(c, &zero, 1);
    u8 l[8];
    for (u32 i = 0; i < 8; i++) l[i] = (u8)(bits >> (56 - 8 * i));
    sha1_update(c, l, 8);
    for (u32 i = 0; i < 5; i++) {
        out[i * 4] = (u8)(c->h[i] >> 24);
        out[i * 4 + 1] = (u8)(c->h[i] >> 16);
        out[i * 4 + 2] = (u8)(c->h[i] >> 8);
        out[i * 4 + 3] = (u8)c->h[i];
    }
}

void sha1(const void *data, u64 len, u8 out[20])
{
    sha1_ctx c;
    sha1_init(&c);
    sha1_update(&c, data, len);
    sha1_final(&c, out);
}

void hmac_sha1(const u8 *key, u32 klen, const void *data, u64 len, u8 out[20])
{
    u8 k[64], ipad[64], opad[64], inner[20];
    memset(k, 0, 64);
    if (klen > 64) sha1(key, klen, k);
    else memcpy(k, key, klen);
    for (u32 i = 0; i < 64; i++) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5C; }

    sha1_ctx c;
    sha1_init(&c);
    sha1_update(&c, ipad, 64);
    sha1_update(&c, data, len);
    sha1_final(&c, inner);
    sha1_init(&c);
    sha1_update(&c, opad, 64);
    sha1_update(&c, inner, 20);
    sha1_final(&c, out);
}

/* The passphrase into a key: the salt is the network's name, the
 * rounds are 4096, and a 32-byte key takes two blocks of it. Slow by
 * design, once per network. */
void pbkdf2_hmac_sha1(const u8 *pass, u32 plen, const u8 *salt, u32 slen,
                      u32 rounds, u8 *out, u32 olen)
{
    u8 block[20], u[20], s[64];
    u32 made = 0;
    for (u32 i = 1; made < olen; i++) {
        u32 sl = slen > 60 ? 60 : slen;
        memcpy(s, salt, sl);
        s[sl] = (u8)(i >> 24); s[sl + 1] = (u8)(i >> 16);
        s[sl + 2] = (u8)(i >> 8); s[sl + 3] = (u8)i;
        hmac_sha1(pass, plen, s, sl + 4, u);
        memcpy(block, u, 20);
        for (u32 r = 1; r < rounds; r++) {
            hmac_sha1(pass, plen, u, 20, u);
            for (u32 k = 0; k < 20; k++) block[k] ^= u[k];
        }
        u32 take = olen - made < 20 ? olen - made : 20;
        memcpy(out + made, block, take);
        made += take;
    }
}

/* The expansion of 802.11i: HMAC-SHA1(key, label || 0 || data || i)
 * for i from zero, as many blocks as the length asks for. */
void prf_sha1(const u8 *key, u32 klen, const char *label,
              const u8 *data, u32 dlen, u8 *out, u32 olen)
{
    u8 in[160], h[20];
    u32 ll = 0;
    while (label[ll]) ll++;
    if (ll + 1 + dlen + 1 > sizeof(in)) return;
    memcpy(in, label, ll);
    in[ll] = 0;
    memcpy(in + ll + 1, data, dlen);
    u32 made = 0;
    for (u8 i = 0; made < olen; i++) {
        in[ll + 1 + dlen] = i;
        hmac_sha1(key, klen, in, ll + 2 + dlen, h);
        u32 take = olen - made < 20 ? olen - made : 20;
        memcpy(out + made, h, take);
        made += take;
    }
}
