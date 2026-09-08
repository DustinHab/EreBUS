/*
 * tls_fuzz.c -- libFuzzer entry for the tls client: the bytes are what the server sends back.
 * - tcp is a stub that hands the input out in chunks of varying size; writes go nowhere
 * - the record cipher and the hashes are stubs, so a record opens as it came and the Finished check passes
 *   on zeros: the handshake parsers, the certificate list and the application data path all see the input
 * - the certificate checker is the real one (fuzzed on its own by pki_fuzz)
 * - built and run by tools/fuzz/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/net.h>
#include <eb/crypto.h>
#include <eb/settings.h>
#include <eb/time.h>

void kprintf(const char *fmt, ...) { (void)fmt; }

/* --- the wire: the input, chunked -------------------------------------- */

static const uint8_t *in_data;
static size_t in_len, in_at;
static u32 chunk_rule;

bool tcp_open(const u8 addr[4], u16 port) { (void)addr; (void)port; return true; }
bool tcp_write(const u8 *buf, u32 len)   { (void)buf; (void)len; return true; }
void tcp_close(void)                     { }
bool tcp_eof(void)                       { return in_at >= in_len; }

/* No connection is ever kept in the fuzzer: every exchange is a fresh
 * handshake, which is the path under test. */
bool net_conn_alive(const u8 a[4], u16 p, bool s) { (void)a; (void)p; (void)s; return false; }
void net_conn_keep(const u8 a[4], u16 p, bool s)  { (void)a; (void)p; (void)s; }
void net_conn_drop(void)                          { }
bool http_keepable(const http_progress *p, const u8 *buf, u32 len) { (void)p; (void)buf; (void)len; return false; }

i32 tcp_read(u8 *buf, u32 max)
{
    if (in_at >= in_len) return 0;
    /* chunk sizes from the rule byte: 1, 5, 17, 100, 1000, or everything */
    static const u32 sizes[8] = { 1, 5, 17, 100, 1000, 65536, 3, 64 };
    u32 want = sizes[chunk_rule & 7];
    chunk_rule = chunk_rule * 1103515245u + 12345u;
    if (want > max) want = max;
    size_t left = in_len - in_at;
    if (want > left) want = (u32)left;
    memcpy(buf, in_data + in_at, want);
    in_at += want;
    return (i32)want;
}

/* Whether the headers have ended and a Content-Length body is complete:
 * enough of net.c's rule for the fetch loop to stop. */
bool http_response_complete(http_progress *p, const u8 *buf, u32 len)
{
    (void)p;
    for (u32 i = 3; i < len; i++)
        if (buf[i - 3] == '\r' && buf[i - 2] == '\n' && buf[i - 1] == '\r' && buf[i] == '\n')
            return len > 4000;
    return false;
}

/* --- the primitives the record layer leans on, made transparent ---------- */

void aes128_gcm_seal(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen,
                     const u8 *pt, u32 len, u8 *ct, u8 tag[16])
{
    (void)key; (void)iv; (void)aad; (void)alen;
    if (ct != pt) memmove(ct, pt, len);
    memset(tag, 0, 16);
}

bool aes128_gcm_open(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen,
                     const u8 *ct, u32 len, const u8 tag[16], u8 *pt)
{
    (void)key; (void)iv; (void)aad; (void)alen; (void)tag;
    if (pt != ct) memmove(pt, ct, len);
    return true;
}

/* A hash that is a hash in shape only: the transcript and the schedule
 * become deterministic garbage, the Finished value becomes zeros. */
void sha256_init(sha256_ctx *c) { memset(c, 0, sizeof *c); }
void sha256_update(sha256_ctx *c, const void *data, u64 len)
{
    const u8 *d = (const u8 *)data;
    for (u64 i = 0; i < len; i++) c->h[i & 7] = c->h[i & 7] * 31u + d[i];
    c->total += len;
}
void sha256_final(sha256_ctx *c, u8 out[32]) { memcpy(out, c->h, 32); }
void sha256(const void *data, u64 len, u8 out[32])
{
    sha256_ctx c; sha256_init(&c); sha256_update(&c, data, len); sha256_final(&c, out);
}
void hmac_sha256(const u8 *key, u32 klen, const void *msg, u64 mlen, u8 out[32])
{ (void)key; (void)klen; (void)msg; (void)mlen; memset(out, 0, 32); }
void hkdf_extract(const u8 *salt, u32 slen, const u8 *ikm, u32 ilen, u8 out[32])
{ (void)salt; (void)slen; (void)ikm; (void)ilen; memset(out, 0, 32); }
void hkdf_expand(const u8 prk[32], const u8 *info, u32 ilen, u8 *out, u32 olen)
{ (void)prk; (void)info; (void)ilen; memset(out, 0, olen); }

void rand_bytes(u8 *out, u32 len) { for (u32 i = 0; i < len; i++) out[i] = (u8)(i * 7 + 3); }
u64  time_unix(void) { return 1788609600ULL; }

/* --- settings ------------------------------------------------------------ */

bool      settings_tls_strict(void)           { return false; }
u32       settings_authority_count(void)      { return 0; }
const u8 *settings_authority(u32 i, u32 *len) { (void)i; *len = 0; return NULL; }

/* --- the run --------------------------------------------------------------- */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 200000) return 0;
    chunk_rule = data[0];
    in_data = data + 1;
    in_len = size - 1;
    in_at = 0;

    static u8 out[65536];
    u32 got = 0;
    static const u8 addr[4] = { 10, 0, 2, 100 };
    tls_get(addr, "example.com", 11, "/index.html", 11, out, sizeof out, &got);
    if (got > sizeof out) abort();
    (void)tls_last_verified();
    (void)tls_last_reason();
    return 0;
}
