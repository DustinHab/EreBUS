/*
 * tls.c -- TLS 1.3 client, one suite: X25519 + AES-128-GCM-SHA256.
 * - ClientHello, server flight, RFC 8446 schedule, Finished both ways; then a sealed pipe for http
 * - certificate and signature are read past and NOT verified (no RSA/ECDSA, no root store); the shell says so
 * - one connection at a time on net.c's tcp stream
 */
#include <eb/net.h>
#include <eb/crypto.h>
#include <eb/string.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/io.h>

#define REC_HANDSHAKE 22
#define REC_APPDATA   23
#define REC_ALERT     21
#define REC_CCS       20

#define HS_CLIENT_HELLO 1
#define HS_SERVER_HELLO 2
#define HS_ENCRYPTED_EXT 8
#define HS_CERT         11
#define HS_CERT_VERIFY  15
#define HS_FINISHED     20
#define HS_NEW_TICKET   4

static bool verified;
bool tls_last_verified(void) { return verified; }

static void expand_label(const u8 secret[32], const char *label,
                         const u8 *ctx, u32 ctxlen, u8 *out, u32 olen);
static void derive_secret(const u8 secret[32], const char *label,
                          const u8 thash[32], u8 out[32]);

/* The key schedule, checked against RFC 8448's worked example. If this
 * matches, the labels, the extract/expand steps and their order are
 * right, and any handshake failure lies elsewhere. */
bool tls_schedule_selftest(void)
{
    static const u8 shared[32] = {
        0x8b,0xd4,0x05,0x4f,0xb5,0x5b,0x9d,0x63,0xfd,0xfb,0xac,0xf9,
        0xf0,0x4b,0x9f,0x0d,0x35,0xe6,0xd6,0x3f,0x53,0x75,0x63,0xef,
        0xd4,0x62,0x72,0x90,0x0f,0x89,0x49,0x2d };
    static const u8 thash[32] = {
        0x86,0x0c,0x06,0xed,0xc0,0x78,0x58,0xee,0x8e,0x78,0xf0,0xe7,
        0x42,0x8c,0x58,0xed,0xd6,0xb4,0x3f,0x2c,0xa3,0xe6,0xe9,0x5f,
        0x02,0xed,0x06,0x3c,0xf0,0xe1,0xca,0xd8 };
    static const u8 want_shs[32] = {
        0xb6,0x7b,0x7d,0x69,0x0c,0xc1,0x6c,0x4e,0x75,0xe5,0x42,0x13,
        0xcb,0x2d,0x37,0xb4,0xe9,0xc9,0x12,0xbc,0xde,0xd9,0x10,0x5d,
        0x42,0xbe,0xfd,0x59,0xd3,0x91,0xad,0x38 };
    static const u8 want_key[16] = {
        0x3f,0xce,0x51,0x60,0x09,0xc2,0x17,0x27,
        0xd0,0xf2,0xe4,0xe8,0x6e,0xe4,0x03,0xbc };
    static const u8 want_iv[12] = {
        0x5d,0x31,0x3e,0xb2,0x67,0x12,0x76,0xee,0x13,0x00,0x0b,0x30 };

    u8 empty_hash[32];
    sha256("", 0, empty_hash);

    u8 early[32], derived[32], hs[32], shs[32], key[16], iv[12];
    hkdf_extract(NULL, 0, (const u8[32]){0}, 32, early);
    derive_secret(early, "derived", empty_hash, derived);
    hkdf_extract(derived, 32, shared, 32, hs);
    derive_secret(hs, "s hs traffic", thash, shs);
    expand_label(shs, "key", NULL, 0, key, 16);
    expand_label(shs, "iv", NULL, 0, iv, 12);

    for (u32 i = 0; i < 32; i++) if (shs[i] != want_shs[i]) return false;
    for (u32 i = 0; i < 16; i++) if (key[i] != want_key[i]) return false;
    for (u32 i = 0; i < 12; i++) if (iv[i] != want_iv[i]) return false;
    return true;
}

/* ------------------------------------------------------------------ */
/* Key schedule helpers                                                */
/* ------------------------------------------------------------------ */

/* HKDF-Expand-Label, the shape TLS 1.3 wraps every derivation in. */
static void expand_label(const u8 secret[32], const char *label,
                         const u8 *ctx, u32 ctxlen, u8 *out, u32 olen)
{
    u8 info[2 + 1 + 32 + 1 + 32];
    u32 at = 0;
    info[at++] = (u8)(olen >> 8);
    info[at++] = (u8)olen;

    u8 ll = 6;
    const char *l = label;
    while (*l) ll++, l++;
    info[at++] = ll;
    const char *pre = "tls13 ";
    for (u32 i = 0; i < 6; i++) info[at++] = (u8)pre[i];
    for (u32 i = 0; label[i]; i++) info[at++] = (u8)label[i];

    info[at++] = (u8)ctxlen;
    for (u32 i = 0; i < ctxlen; i++) info[at++] = ctx[i];

    hkdf_expand(secret, info, at, out, olen);
}

static void derive_secret(const u8 secret[32], const char *label,
                          const u8 thash[32], u8 out[32])
{
    expand_label(secret, label, thash, 32, out, 32);
}

/* One traffic epoch: the key, the base nonce, and the record counter. */
typedef struct {
    u8  key[16];
    u8  iv[12];
    u64 seq;
} epoch;

static void epoch_from(const u8 secret[32], epoch *e)
{
    expand_label(secret, "key", NULL, 0, e->key, 16);
    expand_label(secret, "iv", NULL, 0, e->iv, 12);
    e->seq = 0;
}

/* The per-record nonce: the base iv with the sequence counted into its
 * low eight bytes. */
static void nonce_of(const epoch *e, u8 out[12])
{
    for (u32 i = 0; i < 12; i++) out[i] = e->iv[i];
    for (u32 i = 0; i < 8; i++)
        out[11 - i] ^= (u8)(e->seq >> (i * 8));
}

/* ------------------------------------------------------------------ */
/* Records                                                             */
/* ------------------------------------------------------------------ */

/* Reads exactly n bytes from the tcp stream, pumping as needed. */
static bool read_exact(u8 *buf, u32 n)
{
    u32 got = 0;
    while (got < n) {
        i32 r = tcp_read(buf + got, n - got);
        if (r <= 0) return false;
        got += (u32)r;
    }
    return true;
}

/* One TLS record off the wire: its type, and its body into buf. */
static i32 read_record(u8 *type, u8 *buf, u32 max)
{
    u8 hdr[5];
    if (!read_exact(hdr, 5)) return -1;
    *type = hdr[0];
    u32 len = ((u32)hdr[3] << 8) | hdr[4];
    if (len > max) return -1;
    if (!read_exact(buf, len)) return -1;
    return (i32)len;
}

/* A plaintext record onto the wire. */
static bool write_record(u8 type, const u8 *body, u32 len)
{
    u8 hdr[5] = { type, 0x03, 0x03, (u8)(len >> 8), (u8)len };
    if (!tcp_write(hdr, 5)) return false;
    return tcp_write(body, len);
}

/* An encrypted record: the inner plaintext is body || real_type, then
 * sealed; the record on the wire is stamped application_data. */
static bool write_encrypted(epoch *e, u8 inner_type,
                            const u8 *body, u32 len)
{
    u8 inner[2048];
    if (len + 1 > sizeof(inner)) return false;
    for (u32 i = 0; i < len; i++) inner[i] = body[i];
    inner[len] = inner_type;
    u32 ilen = len + 1;

    u32 rlen = ilen + 16;
    u8 rec[5 + 2048 + 16];
    rec[0] = REC_APPDATA; rec[1] = 0x03; rec[2] = 0x03;
    rec[3] = (u8)(rlen >> 8); rec[4] = (u8)rlen;

    u8 nonce[12];
    nonce_of(e, nonce);
    aes128_gcm_seal(e->key, nonce, rec, 5, inner, ilen,
                    rec + 5, rec + 5 + ilen);
    e->seq++;
    return tcp_write(rec, 5 + rlen);
}

/* Decrypts an application_data record in place, returning the inner
 * length with the trailing content-type byte stripped into *type. */
static i32 decrypt_record(epoch *e, const u8 *hdr5, u8 *body, u32 len,
                          u8 *type)
{
    if (len < 16) return -1;
    u8 nonce[12];
    nonce_of(e, nonce);
    u32 plen = len - 16;
    if (!aes128_gcm_open(e->key, nonce, hdr5, 5, body, plen,
                         body + plen, body))
        return -1;
    e->seq++;

    /* Strip zero padding; the last non-zero byte is the real type. */
    while (plen > 0 && body[plen - 1] == 0) plen--;
    if (plen == 0) return -1;
    *type = body[plen - 1];
    return (i32)(plen - 1);
}

/* Reads a record and, if it is encrypted, opens it; a bare CCS record
 * is skipped. Returns inner length and sets *type to the content type. */
static i32 read_open(epoch *e, u8 *buf, u32 max, u8 *type)
{
    for (;;) {
        u8 hdr[5];
        if (!read_exact(hdr, 5)) return -1;
        u32 len = ((u32)hdr[3] << 8) | hdr[4];
        if (len > max) return -1;
        if (!read_exact(buf, len)) return -1;

        if (hdr[0] == REC_CCS) continue;     /* middlebox noise, ignore */
        if (hdr[0] == REC_APPDATA)
            return decrypt_record(e, hdr, buf, len, type);
        *type = hdr[0];                       /* plaintext alert/handshake */
        return (i32)len;
    }
}

/* ------------------------------------------------------------------ */
/* The handshake                                                       */
/* ------------------------------------------------------------------ */

static u8 hs_priv[32], hs_pub[32];

/* ClientHello. Returns its length written into out. */
static u32 build_hello(u8 *out, const char *host, u32 hlen,
                       const u8 random[32])
{
    u8 body[512];
    u32 b = 0;

    body[b++] = 0x03; body[b++] = 0x03;              /* legacy version */
    for (u32 i = 0; i < 32; i++) body[b++] = random[i];

    body[b++] = 32;                                  /* a session id, for looks */
    for (u32 i = 0; i < 32; i++) body[b++] = random[i] ^ 0x5a;

    body[b++] = 0x00; body[b++] = 0x02;              /* one cipher suite */
    body[b++] = 0x13; body[b++] = 0x01;              /* AES_128_GCM_SHA256 */

    body[b++] = 0x01; body[b++] = 0x00;              /* null compression */

    /* Extensions. */
    u32 ext_at = b;
    b += 2;                                          /* length, filled later */

    /* supported_versions: TLS 1.3 */
    body[b++]=0x00; body[b++]=0x2b; body[b++]=0x00; body[b++]=0x03;
    body[b++]=0x02; body[b++]=0x03; body[b++]=0x04;

    /* supported_groups: x25519 */
    body[b++]=0x00; body[b++]=0x0a; body[b++]=0x00; body[b++]=0x04;
    body[b++]=0x00; body[b++]=0x02; body[b++]=0x00; body[b++]=0x1d;

    /* signature_algorithms: offered so the server will speak, though
     * its signature is not checked here. */
    body[b++]=0x00; body[b++]=0x0d; body[b++]=0x00; body[b++]=0x08;
    body[b++]=0x00; body[b++]=0x06;
    body[b++]=0x04; body[b++]=0x03;                  /* ecdsa_secp256r1_sha256 */
    body[b++]=0x08; body[b++]=0x04;                  /* rsa_pss_rsae_sha256 */
    body[b++]=0x04; body[b++]=0x01;                  /* rsa_pkcs1_sha256 */

    /* key_share: our x25519 public value */
    body[b++]=0x00; body[b++]=0x33;
    body[b++]=0x00; body[b++]=0x26;                  /* ext len */
    body[b++]=0x00; body[b++]=0x24;                  /* client shares len */
    body[b++]=0x00; body[b++]=0x1d;                  /* x25519 */
    body[b++]=0x00; body[b++]=0x20;                  /* key len 32 */
    for (u32 i = 0; i < 32; i++) body[b++] = hs_pub[i];

    /* server_name */
    if (hlen > 0) {
        u32 snlen = hlen + 5;
        body[b++]=0x00; body[b++]=0x00;
        body[b++]=(u8)((snlen) >> 8); body[b++]=(u8)snlen;
        body[b++]=(u8)((hlen + 3) >> 8); body[b++]=(u8)(hlen + 3);
        body[b++]=0x00;                              /* host_name */
        body[b++]=(u8)(hlen >> 8); body[b++]=(u8)hlen;
        for (u32 i = 0; i < hlen; i++) body[b++] = (u8)host[i];
    }

    u32 ext_len = b - ext_at - 2;
    body[ext_at]   = (u8)(ext_len >> 8);
    body[ext_at+1] = (u8)ext_len;

    /* Wrap as a Handshake message. */
    out[0] = HS_CLIENT_HELLO;
    out[1] = (u8)(b >> 16); out[2] = (u8)(b >> 8); out[3] = (u8)b;
    for (u32 i = 0; i < b; i++) out[4 + i] = body[i];
    return b + 4;
}

/* Pulls the server's x25519 share out of a ServerHello body. */
static bool parse_server_hello(const u8 *m, u32 len, u8 server_pub[32])
{
    if (len < 38) return false;
    u32 at = 2 + 32;                       /* version, random */
    if (at >= len) return false;
    u8 sid = m[at++]; at += sid;           /* session id echo */
    at += 2;                               /* cipher suite */
    at += 1;                               /* compression */
    if (at + 2 > len) return false;
    u32 ext_len = ((u32)m[at] << 8) | m[at+1]; at += 2;
    u32 end = at + ext_len;
    if (end > len) end = len;

    while (at + 4 <= end) {
        u16 et = ((u16)m[at] << 8) | m[at+1];
        u16 el = ((u16)m[at+2] << 8) | m[at+3];
        at += 4;
        if (at + el > end) break;
        if (et == 0x0033) {                /* key_share */
            u32 p = at;
            /* group (2) + len (2) + key */
            if (el >= 4) {
                u16 klen = ((u16)m[p+2] << 8) | m[p+3];
                if (klen == 32 && p + 4 + 32 <= at + el) {
                    for (u32 i = 0; i < 32; i++) server_pub[i] = m[p+4+i];
                    return true;
                }
            }
        }
        at += el;
    }
    return false;
}

/* ------------------------------------------------------------------ */

/* The whole errand: connect, handshake, request, read, and hand back
 * the decrypted http response in out[]. */
bool tls_get(const u8 addr[4], const char *host, u32 hlen,
             const char *path, u32 plen, u8 *out, u32 max, u32 *got)
{
    verified = false;
    *got = 0;

    if (!tcp_open(addr, 443)) return false;

    /* Our ephemeral key pair. */
    rand_bytes(hs_priv, 32);
    hs_priv[0] &= 248; hs_priv[31] &= 127; hs_priv[31] |= 64;
    x25519_base(hs_pub, hs_priv);

    u8 random[32];
    rand_bytes(random, 32);

    /* The transcript hash runs over every handshake message body. */
    sha256_ctx tr;
    sha256_init(&tr);

    u8 hello[600];
    u32 hello_len = build_hello(hello, host, hlen, random);
    sha256_update(&tr, hello, hello_len);
    if (!write_record(REC_HANDSHAKE, hello, hello_len)) { tcp_close(); return false; }

    static u8 rec[18432];

    /* ServerHello arrives in the clear. */
    u8 rtype;
    i32 rlen = read_record(&rtype, rec, sizeof(rec));
    if (rlen < 4 || rtype != REC_HANDSHAKE || rec[0] != HS_SERVER_HELLO) {
        tcp_close(); return false;
    }
    u32 sh_len = ((u32)rec[1] << 16) | ((u32)rec[2] << 8) | rec[3];
    sha256_update(&tr, rec, 4 + sh_len);

    u8 server_pub[32];
    if (!parse_server_hello(rec + 4, sh_len, server_pub)) {
        tcp_close(); return false;
    }

    /* The shared secret, and the handshake schedule from it. */
    u8 shared[32];
    x25519(shared, hs_priv, server_pub);

    u8 empty_hash[32];
    sha256("", 0, empty_hash);

    u8 early[32], derived[32], hs_secret[32];
    hkdf_extract(NULL, 0, (const u8[32]){0}, 32, early);
    derive_secret(early, "derived", empty_hash, derived);
    hkdf_extract(derived, 32, shared, 32, hs_secret);

    u8 th_cs[32];
    { sha256_ctx c = tr; sha256_final(&c, th_cs); }

    u8 c_hs[32], s_hs[32];
    derive_secret(hs_secret, "c hs traffic", th_cs, c_hs);
    derive_secret(hs_secret, "s hs traffic", th_cs, s_hs);

    epoch s_ep, c_ep;
    epoch_from(s_hs, &s_ep);
    epoch_from(c_hs, &c_ep);

    /* The server's encrypted flight: EncryptedExtensions, Certificate,
     * CertificateVerify, Finished. Each is hashed into the transcript;
     * the certificate and its signature are read past, not checked. */
    u8 server_fin[32];
    bool have_fin = false;
    u8 th_before_fin[32];

    /* A flight may pack several handshake messages into one record, so
     * messages are reassembled across records into a small buffer. */
    static u8 hsbuf[18432];
    u32 hb = 0, hp = 0;

    for (u32 guard = 0; guard < 64 && !have_fin; guard++) {
        u8 ct;
        i32 n = read_open(&s_ep, rec, sizeof(rec), &ct);
        if (n < 0) { tcp_close(); return false; }
        if (ct == REC_ALERT) { tcp_close(); return false; }
        if (ct != REC_HANDSHAKE) continue;

        if (hb + (u32)n > sizeof(hsbuf)) { tcp_close(); return false; }
        for (i32 i = 0; i < n; i++) hsbuf[hb++] = rec[i];

        /* Peel whole handshake messages out of what has accumulated. */
        while (hp + 4 <= hb) {
            u32 mlen = ((u32)hsbuf[hp+1] << 16) |
                       ((u32)hsbuf[hp+2] << 8) | hsbuf[hp+3];
            if (hp + 4 + mlen > hb) break;      /* the rest is still coming */
            u8 mtype = hsbuf[hp];

            if (mtype == HS_FINISHED) {
                /* The transcript up to but not including this message
                 * is what the server's verify_data covers. */
                sha256_ctx c = tr;
                sha256_final(&c, th_before_fin);
                for (u32 i = 0; i < 32 && i < mlen; i++)
                    server_fin[i] = hsbuf[hp + 4 + i];
                have_fin = true;
            }

            sha256_update(&tr, hsbuf + hp, 4 + mlen);
            hp += 4 + mlen;
            if (have_fin) break;
        }
    }
    if (!have_fin) { tcp_close(); return false; }

    /* Check the server's Finished: proof the handshake was not tampered
     * with, though not proof of who the server is. */
    u8 s_fk[32], expect[32];
    expand_label(s_hs, "finished", NULL, 0, s_fk, 32);
    hmac_sha256(s_fk, 32, th_before_fin, 32, expect);
    verified = true;
    for (u32 i = 0; i < 32; i++) if (expect[i] != server_fin[i]) verified = false;
    if (!verified) { tcp_close(); return false; }

    /* Application keys, from the master secret and the full transcript
     * through the server's Finished. */
    u8 th_after_fin[32];
    { sha256_ctx c = tr; sha256_final(&c, th_after_fin); }

    u8 derived2[32], master[32];
    derive_secret(hs_secret, "derived", empty_hash, derived2);
    hkdf_extract(derived2, 32, (const u8[32]){0}, 32, master);

    u8 c_ap[32], s_ap[32];
    derive_secret(master, "c ap traffic", th_after_fin, c_ap);
    derive_secret(master, "s ap traffic", th_after_fin, s_ap);

    /* Our Finished, sealed under the client handshake epoch. */
    u8 c_fk[32], cfin[32];
    expand_label(c_hs, "finished", NULL, 0, c_fk, 32);
    hmac_sha256(c_fk, 32, th_after_fin, 32, cfin);

    u8 fin_msg[36];
    fin_msg[0] = HS_FINISHED; fin_msg[1] = 0; fin_msg[2] = 0; fin_msg[3] = 32;
    for (u32 i = 0; i < 32; i++) fin_msg[4 + i] = cfin[i];

    u8 ccs[1] = { 0x01 };
    write_record(REC_CCS, ccs, 1);           /* compat, ignored by peers */
    if (!write_encrypted(&c_ep, REC_HANDSHAKE, fin_msg, 36)) { tcp_close(); return false; }

    /* The channel is open. Send the request, read the answer. */
    epoch s_app, c_app;
    epoch_from(s_ap, &s_app);
    epoch_from(c_ap, &c_app);

    char req[1400];
    u32 at = 0;
    const char *a = "GET ";
    while (*a) req[at++] = *a++;
    if (plen == 0) req[at++] = '/';
    for (u32 i = 0; i < plen && at < 1280; i++) req[at++] = path[i];
    a = " HTTP/1.0\r\nHost: ";
    while (*a) req[at++] = *a++;
    for (u32 i = 0; i < hlen && at < 1340; i++) req[at++] = (char)host[i];
    a = "\r\nUser-Agent: erebus/0.1\r\nConnection: close\r\n\r\n";
    while (*a) req[at++] = *a++;

    if (!write_encrypted(&c_app, REC_APPDATA, (const u8 *)req, at)) {
        tcp_close(); return false;
    }

    u32 total = 0;
    for (u32 guard = 0; guard < 4096; guard++) {
        u8 ct;
        i32 n = read_open(&s_app, rec, sizeof(rec), &ct);
        if (n < 0) break;                    /* end, reset, or stall */
        if (ct == REC_APPDATA) {
            for (i32 i = 0; i < n && total < max; i++) out[total++] = rec[i];
            if (total >= max) break;
        } else if (ct == REC_ALERT) {
            break;                           /* close_notify or a gripe */
        }
        /* handshake records here are session tickets: ignore them. */
    }

    *got = total;
    tcp_close();
    return total > 0;
}
