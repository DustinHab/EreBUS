/*
 * net_fuzz.c -- libFuzzer entry for the wire: frames, streams and radio frames into net.c, ssh.c and wifi.c.
 * - the first byte picks the scene; the rest is what the far side sends
 *   0  raw frames, one after another, into the pump (arp, ip, udp, tcp, icmp, dns and dhcp parsers)
 *   1  a fetch of http://10.0.2.2/x: the harness plays the peer's tcp (arp, syn-ack, acks) and hands the
 *      input over as the response stream (the tcp receive path, the http response parser, redirects)
 *   2  a visitor at the door: the harness plays the client's tcp and hands the input over as the ssh stream
 *      (version exchange, kexinit, the packets, authentication with every signature passing, the channel)
 *   3  the air: joined to "fuzznet", the input as radio frames (beacons, authentication, association,
 *      the four-way handshake with every mic passing, data frames)
 *   4  a fetch of http://example.com/x: the input is the dns answer, then the response stream
 * - a frame in the input is [u16 length][bytes]; a stream is taken whole
 * - the ciphers and message authentication are stubs that pass, so the parsers behind them see the input
 * - built and run by tools/fuzz/run.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <eb/net.h>
#include <eb/wifi.h>
#include <eb/ssh.h>
#include <eb/crypto.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/msg.h>
#include <eb/thread.h>
#include <eb/term.h>
#include <eb/standard.h>
#include <eb/time.h>
#include <eb/fb.h>
#include <eb/pipe.h>

void kprintf(const char *fmt, ...) { (void)fmt; }
void journal_says(const char *who, const char *what) { (void)who; (void)what; }
const color fb_inks[16] = { 0 };

/* --- a fake object store ------------------------------------------------------ */

struct object {
    type_id type; u64 size; u8 *data; u64 nslots;
    object **slot; char **slot_name; u32 *slot_rights;
    char name[OBJ_NAME_MAX]; u32 refs;
};
object *obj_create(type_id type, u64 payload_size, u64 slot_count)
{
    if (payload_size > (64u << 20)) return NULL;
    object *o = calloc(1, sizeof *o);
    o->type = type; o->size = payload_size;
    o->data = calloc(1, payload_size ? payload_size : 1);
    o->nslots = slot_count;
    o->slot = calloc(slot_count ? slot_count : 1, sizeof(object *));
    o->slot_name = calloc(slot_count ? slot_count : 1, sizeof(char *));
    o->slot_rights = calloc(slot_count ? slot_count : 1, sizeof(u32));
    o->refs = 1;
    return o;
}
void obj_retain(object *o) { if (o) o->refs++; }
void obj_release(object *o)
{
    if (!o || --o->refs) return;
    for (u64 i = 0; i < o->nslots; i++) { if (o->slot[i]) obj_release(o->slot[i]); free(o->slot_name[i]); }
    free(o->slot); free(o->slot_name); free(o->slot_rights); free(o->data); free(o);
}
type_id obj_type(const object *o) { return o ? o->type : TYPE_NULL; }
void *obj_data(object *o) { return o ? o->data : NULL; }
u64 obj_size(const object *o) { return o ? o->size : 0; }
u64 obj_slots(const object *o) { return o ? o->nslots : 0; }
const char *obj_name(const object *o) { return o ? o->name : ""; }
void obj_set_name(object *o, const char *name)
{ if (!o) return; strncpy(o->name, name ? name : "", OBJ_NAME_MAX - 1); o->name[OBJ_NAME_MAX - 1] = 0; }
void obj_touch(object *o) { (void)o; }
bool obj_set_slot(object *o, u64 i, object *t, u32 rights)
{
    if (!o || i >= o->nslots) return false;
    if (t) obj_retain(t);
    if (o->slot[i]) obj_release(o->slot[i]);
    o->slot[i] = t; o->slot_rights[i] = rights;
    return true;
}
object *obj_get_slot(object *o, u64 i) { return (o && i < o->nslots) ? o->slot[i] : NULL; }
u32 obj_slot_rights(object *o, u64 i) { return (o && i < o->nslots) ? o->slot_rights[i] : 0; }
const char *obj_slot_name(object *o, u64 i) { return (o && i < o->nslots && o->slot_name[i]) ? o->slot_name[i] : ""; }
bool obj_set_slot_name(object *o, u64 i, const char *name)
{
    if (!o || i >= o->nslots) return false;
    free(o->slot_name[i]);
    size_t n = strlen(name) + 1;
    o->slot_name[i] = malloc(n);
    memcpy(o->slot_name[i], name, n);
    return true;
}

/* --- ports, capabilities, threads: nothing else runs --------------------------------- */

object *port_create(u64 capacity) { (void)capacity; return obj_create(9, 0, 0); }
bool port_try_receive(domain *to, cap_handle h, message *out) { (void)to; (void)h; (void)out; return false; }
cap_handle cap_insert(domain *d, object *o, u32 r) { (void)d; (void)o; (void)r; return 1; }
object *cap_lookup(domain *d, cap_handle h, u32 r) { (void)d; (void)h; (void)r; return NULL; }
bool cap_revoke(domain *d, cap_handle h) { (void)d; (void)h; return true; }
thread *thread_create(const char *n, thread_entry e, void *a, domain *d) { (void)n; (void)e; (void)a; (void)d; return NULL; }
void pipe_prepare(struct domain *k) { (void)k; }
void pipe_service(void) { }
void pipe_input(const u8 src[4], u16 sport, const u8 *data, u32 len) { (void)src; (void)sport; (void)data; (void)len; }
bool pipe_ask(object *o, bool w) { (void)o; (void)w; return false; }
void update_tick(void) { }
bool crypto_selftest(void) { return true; }
bool tls_schedule_selftest(void) { return true; }
bool tls_pki_selftest(void) { return true; }
u32  pki_builtin_count(void) { return 0; }
bool tls_get(const u8 addr[4], const char *host, u32 hlen, const char *path, u32 plen, u8 *out, u32 max, u32 *got)
{ (void)addr; (void)host; (void)hlen; (void)path; (void)plen; (void)out; (void)max; *got = 0; return false; }
bool tls_exchange(const u8 addr[4], const char *host, u32 hlen, const u8 *req, u32 rlen,
                  const u8 *body, u32 blen, u8 *out, u32 max, u32 *got)
{ (void)addr; (void)host; (void)hlen; (void)req; (void)rlen; (void)body; (void)blen; (void)out; (void)max; *got = 0; return false; }
void tls_session_drop(void) { }
bool tls_last_verified(void) { return false; }
const char *tls_last_reason(void) { return ""; }
void time_set_unix(u64 s) { (void)s; }

static object *served;
object *system_served(void) { return served; }

/* --- settings ------------------------------------------------------------------------- */

bool settings_address(u8 ip[4]) { (void)ip; return false; }
void settings_name(char *out, u32 max) { strncpy(out, "fuzzbox", max); out[max - 1] = 0; }
u32  settings_door_count(void) { return 1; }
bool settings_door_key(u32 i, u8 out[32]) { (void)i; memset(out, 0x11, 32); return true; }
bool settings_wlan(const char *ssid, char *pass, u32 max) { (void)ssid; (void)pass; (void)max; return false; }
bool settings_remember_wlan(const char *ssid, const char *pass) { (void)ssid; (void)pass; return true; }

/* --- the terminal behind the door ------------------------------------------------------ */

static int dummy_session;
term_session *term_open(void) { return (term_session *)&dummy_session; }
void term_close(term_session *s) { (void)s; }
void term_line(term_session *s, const char *line) { (void)s; (void)line; }
const char *term_out(term_session *s, u64 *len) { (void)s; *len = 0; return ""; }
u64  term_total(term_session *s) { (void)s; return 0; }
bool term_secret(term_session *s) { (void)s; return false; }
bool term_taking(term_session *s) { (void)s; return false; }
u32  term_take_bytes(term_session *s, const u8 *d, u32 n) { (void)s; (void)d; (void)n; return 0; }

/* --- the primitives, transparent ------------------------------------------------------------ */

void ed25519_public(u8 pk[32], const u8 seed[32]) { for (u32 i = 0; i < 32; i++) pk[i] = (u8)(seed[i] ^ 0x5A); }
void ed25519_sign(u8 sig[64], const u8 seed[32], const u8 pk[32], const void *msg, u32 len)
{ (void)seed; (void)pk; (void)msg; (void)len; memset(sig, 0x42, 64); }
bool ed25519_verify(const u8 pk[32], const void *msg, u32 len, const u8 sig[64])
{ (void)pk; (void)msg; (void)len; (void)sig; return true; }
void aes128_gcm_seal(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen, const u8 *pt, u32 len, u8 *ct, u8 tag[16])
{ (void)key; (void)iv; (void)aad; (void)alen; if (ct != pt) memmove(ct, pt, len); memset(tag, 0, 16); }
bool aes128_gcm_open(const u8 key[16], const u8 iv[12], const u8 *aad, u32 alen, const u8 *ct, u32 len, const u8 tag[16], u8 *pt)
{ (void)key; (void)iv; (void)aad; (void)alen; (void)tag; if (pt != ct) memmove(pt, ct, len); return true; }
void aes128_setkey(aes_key *k, const u8 key[16]) { memcpy(k->rk, key, 16); }
void aes128_block(const aes_key *k, const u8 in[16], u8 out[16]) { for (u32 i = 0; i < 16; i++) out[i] = in[i] ^ k->rk[i]; }
void aes_ccm_seal(const aes_key *k, const u8 nonce[13], const u8 *aad, u32 alen, const u8 *in, u32 len, u8 *out, u8 tag[8])
{ (void)k; (void)nonce; (void)aad; (void)alen; if (out != in) memmove(out, in, len); memset(tag, 0, 8); }
bool aes_ccm_open(const aes_key *k, const u8 nonce[13], const u8 *aad, u32 alen, const u8 *in, u32 len, const u8 tag[8], u8 *out)
{ (void)k; (void)nonce; (void)aad; (void)alen; (void)tag; if (out != in) memmove(out, in, len); return true; }
bool aes_unwrap(const u8 kek[16], const u8 *in, u32 len, u8 *out) { (void)kek; if (len < 8) return false; memcpy(out, in + 8, len - 8); return true; }
void sha1(const void *data, u64 len, u8 out[20]) { (void)data; (void)len; memset(out, 0, 20); }
void hmac_sha1(const u8 *key, u32 klen, const void *data, u64 len, u8 out[20]) { (void)key; (void)klen; (void)data; (void)len; memset(out, 0, 20); }
void pbkdf2_hmac_sha1(const u8 *pass, u32 plen, const u8 *salt, u32 slen, u32 rounds, u8 *out, u32 olen)
{ (void)pass; (void)plen; (void)salt; (void)slen; (void)rounds; memset(out, 1, olen); }
void prf_sha1(const u8 *key, u32 klen, const char *label, const u8 *data, u32 dlen, u8 *out, u32 olen)
{ (void)key; (void)klen; (void)label; (void)data; (void)dlen; memset(out, 2, olen); }

static u64 clock_ns;
u64  time_ns(void) { clock_ns += 1000000; return clock_ns; }
static u32 rnd;
void rand_bytes(u8 *out, u32 len) { for (u32 i = 0; i < len; i++) { rnd = rnd * 1103515245u + 12345u; out[i] = (u8)(rnd >> 16); } }

/* --- the wire and the far side ------------------------------------------------------------------ */

static const u8 OUR_MAC[6]  = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static const u8 PEER_MAC[6] = { 0x52, 0x54, 0x00, 0x00, 0x00, 0x02 };
static const u8 PEER_IP[4]  = { 10, 0, 2, 2 };
static const u8 OUR_IP[4]   = { 0, 0, 0, 0 };     /* no lease: whatever the card accepts is looked at */

#define QMAX 64
static struct { u8 *d; u32 n; } queue[QMAX];
static u32 qhead, qtail;

static void enqueue(const u8 *d, u32 n)
{
    if (qtail - qhead >= QMAX || n > 2048) return;
    u8 *c = malloc(n);
    memcpy(c, d, n);
    queue[qtail % QMAX].d = c;
    queue[qtail % QMAX].n = n;
    qtail++;
}

static u16 csum(const u8 *p, u32 len, u32 seed)
{
    u32 s = seed;
    for (u32 i = 0; i + 1 < len; i += 2) s += ((u32)p[i] << 8) | p[i + 1];
    if (len & 1) s += (u32)p[len - 1] << 8;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (u16)~s;
}

/* A tcp segment from the peer to us. */
static void peer_tcp(u16 sport, u16 dport, u32 seq, u32 ack, u8 flags, const u8 *data, u32 dlen)
{
    static u8 f[1600];
    if (dlen > 1400) dlen = 1400;
    memcpy(f, OUR_MAC, 6); memcpy(f + 6, PEER_MAC, 6); f[12] = 0x08; f[13] = 0x00;
    u8 *ip = f + 14;
    u16 tot = (u16)(20 + 20 + dlen);
    memset(ip, 0, 20);
    ip[0] = 0x45; ip[2] = (u8)(tot >> 8); ip[3] = (u8)tot; ip[8] = 64; ip[9] = 6;
    memcpy(ip + 12, PEER_IP, 4); memcpy(ip + 16, OUR_IP, 4);
    u16 hc = csum(ip, 20, 0); ip[10] = (u8)(hc >> 8); ip[11] = (u8)hc;
    u8 *t = ip + 20;
    memset(t, 0, 20);
    t[0] = (u8)(sport >> 8); t[1] = (u8)sport; t[2] = (u8)(dport >> 8); t[3] = (u8)dport;
    t[4] = (u8)(seq >> 24); t[5] = (u8)(seq >> 16); t[6] = (u8)(seq >> 8); t[7] = (u8)seq;
    t[8] = (u8)(ack >> 24); t[9] = (u8)(ack >> 16); t[10] = (u8)(ack >> 8); t[11] = (u8)ack;
    t[12] = 0x50; t[13] = flags; t[14] = 0xFF; t[15] = 0xFF;
    if (dlen) memcpy(t + 20, data, dlen);
    enqueue(f, 14 + tot);
}

static void peer_udp(u16 sport, u16 dport, const u8 *data, u32 dlen)
{
    static u8 f[1600];
    if (dlen > 1400) dlen = 1400;
    memcpy(f, OUR_MAC, 6); memcpy(f + 6, PEER_MAC, 6); f[12] = 0x08; f[13] = 0x00;
    u8 *ip = f + 14;
    u16 tot = (u16)(20 + 8 + dlen);
    memset(ip, 0, 20);
    ip[0] = 0x45; ip[2] = (u8)(tot >> 8); ip[3] = (u8)tot; ip[8] = 64; ip[9] = 17;
    memcpy(ip + 12, PEER_IP, 4); memcpy(ip + 16, OUR_IP, 4);
    u16 hc = csum(ip, 20, 0); ip[10] = (u8)(hc >> 8); ip[11] = (u8)hc;
    u8 *u = ip + 20;
    u[0] = (u8)(sport >> 8); u[1] = (u8)sport; u[2] = (u8)(dport >> 8); u[3] = (u8)dport;
    u16 ul = (u16)(8 + dlen); u[4] = (u8)(ul >> 8); u[5] = (u8)ul; u[6] = 0; u[7] = 0;
    memcpy(u + 8, data, dlen);
    enqueue(f, 14 + tot);
}

/* The stream the far side has to deliver, and how far it got. */
static const u8 *stream;
static size_t stream_len, stream_at;
static u32 scene;

/* The peer's side of a tcp conversation. */
static struct {
    bool active, established, syn_sent;
    u16 our_port, peer_port;       /* our = the kernel's, peer = the harness's */
    u32 peer_seq, peer_ack;        /* what the harness sends next, what it expects */
    bool fin_sent;
} conv;

static void feed_stream(void)
{
    if (!conv.established || conv.fin_sent) return;
    for (u32 k = 0; k < 4 && stream_at < stream_len; k++) {
        u32 n = (u32)(stream_len - stream_at);
        /* varying segment sizes, from the data itself */
        u32 pick = (stream[stream_at] & 3);
        u32 want = pick == 0 ? 1 : pick == 1 ? 7 : pick == 2 ? 300 : 1200;
        if (n > want) n = want;
        peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, conv.peer_ack, 0x18, stream + stream_at, n);
        conv.peer_seq += n;
        stream_at += n;
    }
    if (stream_at >= stream_len && !conv.fin_sent) {
        peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, conv.peer_ack, 0x11, NULL, 0);
        conv.peer_seq++;
        conv.fin_sent = true;
    }
}

/* What the kernel puts on the wire: the far side answers arp and tcp. */
static bool on_wire(const u8 *f, u32 len)
{
    if (len < 14) return true;
    u16 type = ((u16)f[12] << 8) | f[13];
    if (type == 0x0806 && len >= 42 && f[21] == 1) {            /* arp request */
        u8 r[42];
        memcpy(r, f + 6, 6); memcpy(r + 6, PEER_MAC, 6); r[12] = 0x08; r[13] = 0x06;
        memcpy(r + 14, f + 14, 28);
        r[21] = 2;
        memcpy(r + 22, PEER_MAC, 6); memcpy(r + 28, f + 38, 4);  /* the address asked for */
        memcpy(r + 32, f + 22, 6); memcpy(r + 38, f + 28, 4);
        enqueue(r, 42);
        return true;
    }
    if (type != 0x0800 || len < 34) return true;
    const u8 *ip = f + 14;
    u8 ihl = (u8)(ip[0] & 0x0F) * 4;
    if (ihl < 20 || 14u + ihl > len) return true;
    if (ip[9] == 17 && scene == 4) {
        /* a dns question: the stream's first part is the answer */
        const u8 *u = ip + ihl;
        if (14u + ihl + 8 > len) return true;
        u16 sport = ((u16)u[0] << 8) | u[1], dport = ((u16)u[2] << 8) | u[3];
        if (dport == 53 && stream_at == 0 && stream_len > 2) {
            u32 n = (u32)stream[0] | ((u32)stream[1] << 8);
            if (n > stream_len - 2) n = (u32)(stream_len - 2);
            static u8 ans[1400];
            u32 m = n > 1400 ? 1400 : n;
            memcpy(ans, stream + 2, m);
            /* the id has to match to be looked at; the input decides the rest */
            const u8 *q = u + 8;
            if (m >= 2 && 14u + ihl + 10 <= len) { ans[0] = q[0]; ans[1] = q[1]; }
            peer_udp(53, sport, ans, m);
            stream_at = 2 + n;
        }
        return true;
    }
    if (ip[9] != 6) return true;
    const u8 *t = ip + ihl;
    if (14u + ihl + 20 > len) return true;
    u16 sport = ((u16)t[0] << 8) | t[1], dport = ((u16)t[2] << 8) | t[3];
    u32 seq = ((u32)t[4] << 24) | ((u32)t[5] << 16) | ((u32)t[6] << 8) | t[7];
    u8 off = (u8)(t[12] >> 4) * 4, fl = t[13];
    u32 dlen = (14u + ihl + off <= len) ? len - 14 - ihl - off : 0;

    if ((fl & 0x02) && !(fl & 0x10) && (scene == 1 || scene == 4)) {   /* the kernel's syn */
        conv.active = true; conv.our_port = sport; conv.peer_port = dport;
        conv.peer_seq = 0x10000; conv.peer_ack = seq + 1;
        peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, conv.peer_ack, 0x12, NULL, 0);
        conv.peer_seq++;
        conv.established = true;
        return true;
    }
    if ((fl & 0x02) && (fl & 0x10) && scene == 2 && conv.syn_sent) {   /* the door's syn-ack */
        conv.peer_ack = seq + 1;
        conv.established = true;
        peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, conv.peer_ack, 0x10, NULL, 0);
        return true;
    }
    if (conv.established && sport == conv.our_port) {
        if (dlen) {
            conv.peer_ack = seq + dlen;
            peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, conv.peer_ack, 0x10, NULL, 0);
        }
        if (fl & 0x01) conv.peer_ack = seq + dlen + 1;
        feed_stream();
    }
    return true;
}

bool nic_send(const void *frame, u32 len) { return on_wire((const u8 *)frame, len); }
bool nic_send_raw(const void *frame, u32 len) { return on_wire((const u8 *)frame, len); }
const u8 *nic_mac(void) { return OUR_MAC; }
bool nic_up(void) { return true; }
bool nic_start(void) { return true; }
bool nic_wait(u64 ns) { (void)ns; return false; }
void nic_signal(void) { }
bool nic_interrupts(void) { return false; }
void nic_note_interrupts(bool yes) { (void)yes; }

/* Raw frames from the input, when the scene is that. */
static const u8 *raw; static size_t raw_len, raw_at;

i32 nic_recv(void *out, u32 max)
{
    for (;;) {
        if (qhead == qtail) {
            if (scene == 0 && raw_at + 2 <= raw_len) {
                u32 n = (u32)raw[raw_at] | ((u32)raw[raw_at + 1] << 8);
                raw_at += 2;
                if (n > raw_len - raw_at) n = (u32)(raw_len - raw_at);
                enqueue(raw + raw_at, n);
                raw_at += n;
            } else if (scene == 3 && raw_at + 2 <= raw_len) {
                u32 n = (u32)raw[raw_at] | ((u32)raw[raw_at + 1] << 8);
                raw_at += 2;
                if (n > raw_len - raw_at) n = (u32)(raw_len - raw_at);
                static u8 f[2048];
                if (n > 2000) n = 2000;
                memcpy(f, OUR_MAC, 6); memcpy(f + 6, PEER_MAC, 6); f[12] = 0x88; f[13] = 0xB5;
                f[14] = 'R'; f[15] = 1; f[16] = (u8)-40; f[17] = 6;
                memcpy(f + 18, raw + raw_at, n);
                enqueue(f, 18 + n);
                raw_at += n;
            } else if (conv.established && stream_at < stream_len) {
                feed_stream();
            }
            if (qhead == qtail) return -1;
        }
        u8 *d = queue[qhead % QMAX].d; u32 n = queue[qhead % QMAX].n;
        qhead++;
        if (n >= 18 && d[12] == 0x88 && d[13] == 0xB5 && d[14] == 'R') {
            wifi_radio_input(d + 18, n - 18, (i8)d[16], d[17]);
            free(d);
            continue;
        }
        if (n > max) n = max;
        memcpy(out, d, n);
        free(d);
        return (i32)n;
    }
}

static void wire_reset(void)
{
    while (qhead != qtail) { free(queue[qhead % QMAX].d); qhead++; }
    memset(&conv, 0, sizeof conv);
    stream = NULL; stream_len = stream_at = 0;
    raw = NULL; raw_len = raw_at = 0;
    clock_ns = 0;
    rnd = 1;
}

/* --- the run ------------------------------------------------------------------------------ */

static bool prepared;

static void prepare(void)
{
    if (prepared) return;
    prepared = true;
    served = obj_create(TYPE_LIST, 0, 4);
    object *page = obj_create(TYPE_TEXT, 64, 0);
    memcpy(page->data, "hello from the fuzzer\n", 22);
    obj_set_slot(served, 0, page, 1);
    obj_set_slot_name(served, 0, "hello");
    object *pic = obj_create(TYPE_PICTURE, 8 + 16, 0);
    pic->data[0] = 4; pic->data[4] = 4;
    obj_set_slot(served, 1, pic, 1);
    obj_set_slot_name(served, 1, "pic");
    net_prepare(NULL);
    net_start();
    u8 key[64];
    memset(key, 9, 64);
    ssh_init(key);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 200000) return 0;
    prepare();
    wire_reset();
    scene = data[0] % 5;
    const u8 *in = data + 1;
    size_t n = size - 1;

    if (scene == 0) {
        raw = in; raw_len = n;
        for (u32 i = 0; i < 8; i++) { net_breathe(); ssh_service(); }
    } else if (scene == 1 || scene == 4) {
        stream = in; stream_len = n;
        static u8 out[65536];
        u32 off = 0, len = 0;
        bool secure = false;
        const char *url = scene == 1 ? "http://10.0.2.2/x" : "http://example.com/x";
        net_fetch(url, (u32)strlen(url), out, sizeof out, &off, &len, &secure);
        if (len > sizeof out || off > sizeof out) abort();
    } else if (scene == 2) {
        stream = in; stream_len = n;
        conv.active = true; conv.syn_sent = true;
        conv.our_port = 22; conv.peer_port = 40000;
        conv.peer_seq = 0x20000;
        peer_tcp(conv.peer_port, conv.our_port, conv.peer_seq, 0, 0x02, NULL, 0);
        conv.peer_seq++;
        for (u32 i = 0; i < 400 && (stream_at < stream_len || qhead != qtail || i < 8); i++) {
            net_breathe();
            ssh_service();
            if (conv.established && stream_at < stream_len) feed_stream();
        }
        for (u32 c = 0; c < door_count(); c++) door_close(c);
        for (u32 i = 0; i < 4; i++) { net_breathe(); ssh_service(); }
    } else if (scene == 3) {
        raw = in; raw_len = n;
        wifi_join("fuzznet", "password");
        for (u32 i = 0; i < 64; i++) { net_breathe(); wifi_poll(); }
        wifi_leave();
        for (u32 i = 0; i < 4; i++) { net_breathe(); wifi_poll(); }
    }
    return 0;
}
