/*
 * ssh.c -- ssh version 2 server: the terminal reached over the network.
 * - curve25519-sha256, ssh-ed25519 (host and visitor), aes128-gcm@openssh.com, publickey only
 * - allowed keys: "door |" lines in the settings; the client's user name is only recorded
 * - stream from net.c; words go to a terminal session of the visitor's own
 * - several visitors at once, one session per door slot; client-driven rekeying is honoured
 * - limits: no compression, no forwarding; shell or one command; the door is for people, not for distributed work (that is the pipe)
 * - while the session takes bytes ("receive"), channel data goes to it unread and the window is refilled per chunk
 */
#include <eb/ssh.h>
#include <eb/net.h>
#include <eb/crypto.h>
#include <eb/base64.h>
#include <eb/term.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/string.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/io.h>

#define V_S "SSH-2.0-erebus_1"

#define MSG_DISCONNECT          1
#define MSG_IGNORE              2
#define MSG_UNIMPLEMENTED       3
#define MSG_DEBUG               4
#define MSG_SERVICE_REQUEST     5
#define MSG_SERVICE_ACCEPT      6
#define MSG_KEXINIT            20
#define MSG_NEWKEYS            21
#define MSG_KEX_ECDH_INIT      30
#define MSG_KEX_ECDH_REPLY     31
#define MSG_USERAUTH_REQUEST   50
#define MSG_USERAUTH_FAILURE   51
#define MSG_USERAUTH_SUCCESS   52
#define MSG_USERAUTH_PK_OK     60
#define MSG_GLOBAL_REQUEST     80
#define MSG_REQUEST_FAILURE    82
#define MSG_CHANNEL_OPEN       90
#define MSG_CHANNEL_OPEN_OK    91
#define MSG_CHANNEL_OPEN_FAIL  92
#define MSG_CHANNEL_WINDOW     93
#define MSG_CHANNEL_DATA       94
#define MSG_CHANNEL_EOF        96
#define MSG_CHANNEL_CLOSE      97
#define MSG_CHANNEL_REQUEST    98
#define MSG_CHANNEL_SUCCESS    99
#define MSG_CHANNEL_FAILURE   100

#define SECOND 1000000000ULL

enum {
    ST_IDLE,        /* nobody at the door */
    ST_VERSION,     /* waiting for the client's version line */
    ST_KEXINIT,     /* waiting for the client's KEXINIT */
    ST_ECDH,        /* waiting for the client's public point */
    ST_NEWKEYS,     /* keys derived; waiting for their NEWKEYS */
    ST_AUTH,        /* the seal stands; who is this? */
    ST_OPEN,        /* let in; waiting for the session channel */
    ST_LIVE,        /* a shell or a command, running */
    ST_CLOSING      /* we said close; waiting for theirs */
};

static u8   host_seed[32], host_pub[32];
static bool host_ready;

/* One visitor per session, bound to the door slot of the same index.
 * cur is the session being serviced; every reference below goes through
 * it, so the machinery reads as if there were one visitor while serving
 * several. Must not exceed the door's slot count. */
#define SSH_SESSIONS 4

static struct {
    u8   stage;
    u64  visit;
    u64  born_ns;

    u8   in[20000];                  /* bytes from the door, unparsed */
    u32  in_len;

    char vc[128];                    /* the client's version line */
    u32  vc_len;
    u8   ic[4096];                   /* their KEXINIT payload */
    u32  ic_len;
    u8   is[512];                    /* ours */
    u32  is_len;

    bool enc_in, enc_out;
    u8   key_in[16], key_out[16];
    u8   iv_in[12], iv_out[12];
    u8   session_id[32];
    bool have_sid;                   /* the session id is set once, at the first exchange */
    bool rekeying;                   /* a second exchange, mid-session, keeping the channel */
    u8   nkey_in[16], niv_in[12];    /* the next incoming key, applied on their NEWKEYS */

    char user[32];

    bool chan;                       /* the session channel stands */
    u32  rid;                        /* their number for it */
    u32  rwin;                       /* what they will still take */
    u32  eaten;                      /* what we took, not yet re-offered */
    bool pty, shell, closing_sent;
    u64  close_ns;

    char line[200];                  /* the visitor's gathering line */
    u32  llen;
    u8   esc;                        /* inside an escape sequence */
    term_session *ts;
    u64  taken;                      /* of the transcript, sent so far */

    u8   pend[32768];                /* wire bytes waiting for room */
    u32  pend_len;
} sess[SSH_SESSIONS];

static u32 cur;                      /* the session being serviced */
#define ssh sess[cur]

/* ------------------------------------------------------------------ */
/* Bytes in order                                                      */
/* ------------------------------------------------------------------ */

static u32 be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void put32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);  p[3] = (u8)v;
}

/* A payload being written. */
static u8  pb[9000];
static u32 pl;

static void pw_byte(u8 b)          { if (pl < sizeof(pb)) pb[pl++] = b; }
static void pw_u32(u32 v)          { if (pl + 4 <= sizeof(pb)) { put32(pb + pl, v); pl += 4; } }
static void pw_bytes(const void *d, u32 n)
{
    if (pl + n > sizeof(pb)) return;
    memcpy(pb + pl, d, n);
    pl += n;
}
static void pw_str(const void *d, u32 n) { pw_u32(n); pw_bytes(d, n); }
static void pw_cstr(const char *s)       { u32 n = 0; while (s[n]) n++; pw_str(s, n); }

/* A payload being read. Anything short marks it bad; the caller
 * checks once at the end rather than after every field. */
typedef struct { const u8 *p; u32 len, at; bool bad; } reader;

static u8 rd_byte(reader *r)
{
    if (r->at + 1 > r->len) { r->bad = true; return 0; }
    return r->p[r->at++];
}

static u32 rd_u32(reader *r)
{
    if (r->at + 4 > r->len) { r->bad = true; return 0; }
    u32 v = be32(r->p + r->at);
    r->at += 4;
    return v;
}

static const u8 *rd_str(reader *r, u32 *n)
{
    u32 len = rd_u32(r);
    if (r->bad || r->at + len > r->len) { r->bad = true; *n = 0; return r->p; }
    const u8 *s = r->p + r->at;
    r->at += len;
    *n = len;
    return s;
}

static bool str_is(const u8 *s, u32 n, const char *word)
{
    u32 w = 0;
    while (word[w]) w++;
    return w == n && memcmp(s, word, n) == 0;
}

/* Does a comma-separated name-list hold this name, whole? */
static bool list_has(const u8 *s, u32 n, const char *name)
{
    u32 w = 0;
    while (name[w]) w++;
    u32 i = 0;
    while (i <= n) {
        u32 j = i;
        while (j < n && s[j] != ',') j++;
        if (j - i == w && memcmp(s + i, name, w) == 0) return true;
        i = j + 1;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Packets on the wire                                                 */
/* ------------------------------------------------------------------ */

static void pend_bytes(const u8 *d, u32 n)
{
    if (ssh.pend_len + n > sizeof(ssh.pend)) {
        /* More than the visitor can take before answering: the honest
         * end is a dropped visit, not silently dropped words. */
        kprintf("ssh:  the client is too far behind; closing\n");
        door_close(cur);
        return;
    }
    memcpy(ssh.pend + ssh.pend_len, d, n);
    ssh.pend_len += n;
}

static void iv_step(u8 iv[12])
{
    for (i32 i = 11; i >= 4; i--) if (++iv[i]) break;
}

/* Wraps the payload in pb[] and queues it. Before the keys stand,
 * the binary packet of RFC 4253; after, the AEAD shape of RFC 5647:
 * the length in the clear as the associated data, the rest sealed,
 * the tag behind. */
static void send_packet(void)
{
    static u8 wire[9200];
    u32 n = pl;
    if (!ssh.enc_out) {
        u32 pad = 8 - ((5 + n) % 8);
        if (pad < 4) pad += 8;
        u32 L = 1 + n + pad;
        put32(wire, L);
        wire[4] = (u8)pad;
        memcpy(wire + 5, pb, n);
        rand_bytes(wire + 5 + n, pad);
        pend_bytes(wire, 4 + L);
    } else {
        u32 pad = 16 - ((1 + n) % 16);
        if (pad < 4) pad += 16;
        u32 L = 1 + n + pad;
        static u8 plain[9100];
        plain[0] = (u8)pad;
        memcpy(plain + 1, pb, n);
        rand_bytes(plain + 1 + n, pad);
        put32(wire, L);
        aes128_gcm_seal(ssh.key_out, ssh.iv_out, wire, 4, plain, L,
                        wire + 4, wire + 4 + L);
        iv_step(ssh.iv_out);
        pend_bytes(wire, 4 + L + 16);
    }
    pl = 0;
}

static void say_disconnect(u32 reason, const char *why)
{
    pl = 0;
    pw_byte(MSG_DISCONNECT);
    pw_u32(reason);
    pw_cstr(why);
    pw_cstr("");
    send_packet();
}

/* ------------------------------------------------------------------ */
/* The host's key                                                      */
/* ------------------------------------------------------------------ */

void ssh_make_key(u8 key[64])
{
    rand_bytes(key, 32);
    ed25519_public(key + 32, key);
}

void ssh_init(const u8 key[64])
{
    memcpy(host_seed, key, 32);
    memcpy(host_pub, key + 32, 32);
    /* The public half is recomputed rather than trusted: a snapshot
     * that came back with a bent pair would otherwise sign as
     * nobody, and the client would say so in a way that blames the
     * wrong party. */
    u8 check[32];
    ed25519_public(check, host_seed);
    memcpy(host_pub, check, 32);
    host_ready = true;
}

/* K_S: the blob every client keeps in its known_hosts. */
static u32 host_blob(u8 out[64])
{
    put32(out, 11);
    memcpy(out + 4, "ssh-ed25519", 11);
    put32(out + 15, 32);
    memcpy(out + 19, host_pub, 32);
    return 51;
}

/* The fingerprint of any ed25519 key, in the form ssh-keygen prints:
 * the hash of the standard blob around it, so a key shown here and a
 * key shown there can be compared by eye. */
void ssh_fingerprint_of(const u8 pub[32], char out[64])
{
    u8 blob[64], h[32];
    put32(blob, 11);
    memcpy(blob + 4, "ssh-ed25519", 11);
    put32(blob + 15, 32);
    memcpy(blob + 19, pub, 32);
    sha256(blob, 51, h);
    const char *pre = "SHA256:";
    u32 at = 0;
    while (pre[at]) { out[at] = pre[at]; at++; }
    at += base64_encode(h, 32, out + at, false);
    out[at] = 0;
}

void ssh_fingerprint(char out[64])
{
    ssh_fingerprint_of(host_pub, out);
}

/* The door's key is the machine's identity, for the pipe as for the
 * door: one key, one fingerprint, shown at boot. */
bool ssh_identity(u8 pub[32])
{
    if (!host_ready) return false;
    memcpy(pub, host_pub, 32);
    return true;
}

bool ssh_sign(const void *msg, u32 len, u8 sig[64])
{
    if (!host_ready) return false;
    ed25519_sign(sig, host_seed, host_pub, msg, len);
    return true;
}

bool ssh_key_bytes(u8 out[64])
{
    if (!host_ready) return false;
    memcpy(out, host_seed, 32);
    memcpy(out + 32, host_pub, 32);
    return true;
}

bool ssh_public_line(char out[96])
{
    if (!host_ready) return false;
    u8 blob[64];
    u32 n = host_blob(blob);
    const char *pre = "ssh-ed25519 ";
    u32 at = 0;
    while (pre[at]) { out[at] = pre[at]; at++; }
    at += base64_encode(blob, n, out + at, true);
    out[at] = 0;
    return true;
}

/* ------------------------------------------------------------------ */
/* The visit                                                           */
/* ------------------------------------------------------------------ */

static void visit_end(const char *why)
{
    if (ssh.stage != ST_IDLE && why)
        kprintf("ssh:  %s\n", why);
    if (ssh.ts) term_close(ssh.ts);
    if (ssh.user[0] && ssh.stage >= ST_OPEN) {
        char note[64];
        u32 at = 0;
        const char *a = "session closed for ";
        while (a[at]) { note[at] = a[at]; at++; }
        for (u32 i = 0; ssh.user[i] && at < sizeof(note) - 1; i++)
            note[at++] = ssh.user[i];
        note[at] = 0;
        journal_says("ssh", note);
    }
    memset(&ssh, 0, sizeof(ssh));
    ssh.stage = ST_IDLE;
}

static void send_kexinit(void)
{
    pl = 0;
    pw_byte(MSG_KEXINIT);
    u8 cookie[16];
    rand_bytes(cookie, 16);
    pw_bytes(cookie, 16);
    pw_cstr("curve25519-sha256,curve25519-sha256@libssh.org");
    pw_cstr("ssh-ed25519");
    pw_cstr("aes128-gcm@openssh.com");
    pw_cstr("aes128-gcm@openssh.com");
    pw_cstr("hmac-sha2-256");
    pw_cstr("hmac-sha2-256");
    pw_cstr("none");
    pw_cstr("none");
    pw_cstr("");
    pw_cstr("");
    pw_byte(0);
    pw_u32(0);
    ssh.is_len = pl < sizeof(ssh.is) ? pl : sizeof(ssh.is);
    memcpy(ssh.is, pb, ssh.is_len);
    send_packet();
}

static void visit_begin(void)
{
    visit_end(NULL);
    ssh.visit = door_visit(cur);
    ssh.born_ns = time_ns();
    ssh.stage = ST_VERSION;
    static const char line[] = V_S "\r\n";
    pend_bytes((const u8 *)line, sizeof(line) - 1);
    send_kexinit();
}

/* ------------------------------------------------------------------ */
/* The exchange                                                        */
/* ------------------------------------------------------------------ */

static void h_str(sha256_ctx *c, const void *d, u32 n)
{
    u8 l[4];
    put32(l, n);
    sha256_update(c, l, 4);
    sha256_update(c, d, n);
}

/* One derived key: HASH(K || H || letter || session_id), first bytes.
 * K goes in as the mpint it is on the wire, length and all -- the
 * same bytes the exchange hash took, which is what makes both sides
 * arrive at the same keys. */
static void derive(const u8 *kmp, u32 klen, const u8 H[32], char letter,
                   u8 *out, u32 n)
{
    sha256_ctx c;
    u8 full[32];
    sha256_init(&c);
    u8 l[4];
    put32(l, klen);
    sha256_update(&c, l, 4);
    sha256_update(&c, kmp, klen);
    sha256_update(&c, H, 32);
    sha256_update(&c, &letter, 1);
    sha256_update(&c, ssh.session_id, 32);
    sha256_final(&c, full);
    memcpy(out, full, n);
}

static bool on_kexinit(const u8 *p, u32 n)
{
    if (n > sizeof(ssh.ic)) return false;
    memcpy(ssh.ic, p, n);
    ssh.ic_len = n;

    reader r = { p, n, 1, false };
    r.at += 16;                                   /* the cookie */
    u32 ln;
    const u8 *kex  = rd_str(&r, &ln); u32 kexn = ln;
    const u8 *hk   = rd_str(&r, &ln); u32 hkn = ln;
    const u8 *ec   = rd_str(&r, &ln); u32 ecn = ln;
    const u8 *es   = rd_str(&r, &ln); u32 esn = ln;
    if (r.bad) return false;

    bool ok = (list_has(kex, kexn, "curve25519-sha256") ||
               list_has(kex, kexn, "curve25519-sha256@libssh.org")) &&
              list_has(hk, hkn, "ssh-ed25519") &&
              list_has(ec, ecn, "aes128-gcm@openssh.com") &&
              list_has(es, esn, "aes128-gcm@openssh.com");
    if (!ok) {
        say_disconnect(3, "supported: curve25519-sha256, ssh-ed25519, aes128-gcm@openssh.com");
        return false;
    }
    ssh.stage = ST_ECDH;
    return true;
}

static bool on_ecdh_init(const u8 *p, u32 n)
{
    reader r = { p, n, 1, false };
    u32 qn;
    const u8 *qc = rd_str(&r, &qn);
    if (r.bad || qn != 32) return false;

    u8 priv[32], qs[32], k[32];
    rand_bytes(priv, 32);
    x25519_base(qs, priv);
    x25519(k, priv, qc);
    memset(priv, 0, 32);

    /* K as an mpint: the bytes as they are, taken as a big-endian
     * number -- leading zeros dropped, a zero byte in front when the
     * top bit is set. */
    u8 kmp[33];
    u32 klen = 0;
    u32 skip = 0;
    while (skip < 31 && k[skip] == 0) skip++;
    if (k[skip] & 0x80) kmp[klen++] = 0;
    memcpy(kmp + klen, k + skip, 32 - skip);
    klen += 32 - skip;

    u8 ks[64];
    u32 ksn = host_blob(ks);

    /* The exchange hash, and with it the session's id. */
    sha256_ctx c;
    sha256_init(&c);
    h_str(&c, ssh.vc, ssh.vc_len);
    h_str(&c, V_S, sizeof(V_S) - 1);
    h_str(&c, ssh.ic, ssh.ic_len);
    h_str(&c, ssh.is, ssh.is_len);
    h_str(&c, ks, ksn);
    h_str(&c, qc, 32);
    h_str(&c, qs, 32);
    h_str(&c, kmp, klen);
    u8 H[32];
    sha256_final(&c, H);
    /* The session id is the first exchange hash and never changes; a
     * rekey computes a new H for the new keys but keeps the old id, so
     * derive() below mixes the new H with the lasting id. */
    if (!ssh.have_sid) { memcpy(ssh.session_id, H, 32); ssh.have_sid = true; }

    u8 sig[64];
    ed25519_sign(sig, host_seed, host_pub, H, 32);

    pl = 0;
    pw_byte(MSG_KEX_ECDH_REPLY);
    pw_str(ks, ksn);
    pw_str(qs, 32);
    u8 sigblob[4 + 11 + 4 + 64];
    put32(sigblob, 11);
    memcpy(sigblob + 4, "ssh-ed25519", 11);
    put32(sigblob + 15, 64);
    memcpy(sigblob + 19, sig, 64);
    pw_str(sigblob, sizeof(sigblob));
    send_packet();

    pl = 0;
    pw_byte(MSG_NEWKEYS);
    send_packet();

    /* Our side switches to the new keys right after this NEWKEYS (sent
     * just above under the old ones). Their side keeps sending under the
     * old incoming key until their own NEWKEYS, so the new incoming key
     * waits in a stash and is put in place then -- during a rekey the
     * old key is still needed to open their NEWKEYS itself. */
    derive(kmp, klen, H, 'B', ssh.iv_out, 12);
    derive(kmp, klen, H, 'D', ssh.key_out, 16);
    derive(kmp, klen, H, 'A', ssh.niv_in, 12);
    derive(kmp, klen, H, 'C', ssh.nkey_in, 16);
    memset(k, 0, 32);
    memset(kmp, 0, sizeof(kmp));
    ssh.enc_out = true;
    ssh.stage = ST_NEWKEYS;
    return true;
}

/* ------------------------------------------------------------------ */
/* The login                                                           */
/* ------------------------------------------------------------------ */

static void auth_failure(void)
{
    pl = 0;
    pw_byte(MSG_USERAUTH_FAILURE);
    pw_cstr("publickey");
    pw_byte(0);
    send_packet();
}

static bool key_may_enter(const u8 key[32])
{
    u32 n = settings_door_count();
    for (u32 i = 0; i < n; i++) {
        u8 k[32];
        if (settings_door_key(i, k) && memcmp(k, key, 32) == 0) return true;
    }
    return false;
}

static bool on_userauth(const u8 *p, u32 n)
{
    reader r = { p, n, 1, false };
    u32 un, sn, mn;
    const u8 *user = rd_str(&r, &un);
    const u8 *svc  = rd_str(&r, &sn);
    const u8 *meth = rd_str(&r, &mn);
    if (r.bad) return false;

    u32 ul = 0;
    for (u32 i = 0; i < un && ul < sizeof(ssh.user) - 1; i++)
        if (user[i] >= 0x20 && user[i] < 0x7F) ssh.user[ul++] = (char)user[i];
    ssh.user[ul] = 0;

    if (!str_is(svc, sn, "ssh-connection") || !str_is(meth, mn, "publickey")) {
        auth_failure();
        return true;
    }

    u8 has_sig = rd_byte(&r);
    u32 an, bn;
    const u8 *alg  = rd_str(&r, &an);
    const u8 *blob = rd_str(&r, &bn);
    if (r.bad) return false;

    bool shape = str_is(alg, an, "ssh-ed25519") && bn == 51 &&
                 be32(blob) == 11 && memcmp(blob + 4, "ssh-ed25519", 11) == 0 &&
                 be32(blob + 15) == 32;
    if (!shape || !key_may_enter(blob + 19)) {
        if (settings_door_count() == 0)
            kprintf("ssh:  %s: no door key in the settings\n",
                    ssh.user);
        else
            kprintf("ssh:  %s presented a key not in the settings\n",
                    ssh.user);
        auth_failure();
        return true;
    }

    if (!has_sig) {
        /* The key would do; now prove it. */
        pl = 0;
        pw_byte(MSG_USERAUTH_PK_OK);
        pw_str(alg, an);
        pw_str(blob, bn);
        send_packet();
        return true;
    }

    u32 sgn;
    const u8 *sigblob = rd_str(&r, &sgn);
    if (r.bad || sgn != 4 + 11 + 4 + 64 || be32(sigblob) != 11 ||
        memcmp(sigblob + 4, "ssh-ed25519", 11) != 0 ||
        be32(sigblob + 15) != 64) {
        auth_failure();
        return true;
    }

    /* What they signed: the session's id and the request itself up
     * to the signature. */
    u8 msg[512];
    u32 m = 0;
    put32(msg + m, 32); m += 4;
    memcpy(msg + m, ssh.session_id, 32); m += 32;
    msg[m++] = MSG_USERAUTH_REQUEST;
    put32(msg + m, un); m += 4;
    if (m + un + sn + mn + an + bn + 40 > sizeof(msg)) { auth_failure(); return true; }
    memcpy(msg + m, user, un); m += un;
    put32(msg + m, sn); m += 4; memcpy(msg + m, svc, sn); m += sn;
    put32(msg + m, mn); m += 4; memcpy(msg + m, meth, mn); m += mn;
    msg[m++] = 1;
    put32(msg + m, an); m += 4; memcpy(msg + m, alg, an); m += an;
    put32(msg + m, bn); m += 4; memcpy(msg + m, blob, bn); m += bn;

    if (!ed25519_verify(blob + 19, msg, m, sigblob + 19)) {
        kprintf("ssh:  %s: signature check failed\n", ssh.user);
        auth_failure();
        return true;
    }

    pl = 0;
    pw_byte(MSG_USERAUTH_SUCCESS);
    send_packet();
    ssh.stage = ST_OPEN;

    u8 ip[4];
    door_peer(cur, ip);
    kprintf("ssh:  %s logged in from %u.%u.%u.%u\n",
            ssh.user, ip[0], ip[1], ip[2], ip[3]);
    char note[64];
    u32 at = 0;
    for (u32 i = 0; ssh.user[i] && at < 30; i++) note[at++] = ssh.user[i];
    const char *tail = " logged in";
    for (u32 i = 0; tail[i] && at < sizeof(note) - 1; i++) note[at++] = tail[i];
    note[at] = 0;
    journal_says("ssh", note);
    return true;
}

/* ------------------------------------------------------------------ */
/* The session                                                         */
/* ------------------------------------------------------------------ */

static void chan_data(const u8 *d, u32 n)
{
    while (n) {
        u32 part = n < 4096 ? n : 4096;
        if (part > ssh.rwin) part = ssh.rwin;
        if (part == 0) return;         /* they asked for silence */
        pl = 0;
        pw_byte(MSG_CHANNEL_DATA);
        pw_u32(ssh.rid);
        pw_str(d, part);
        send_packet();
        ssh.rwin -= part;
        d += part;
        n -= part;
    }
}

static void chan_text(const char *s)
{
    u32 n = 0;
    while (s[n]) n++;
    chan_data((const u8 *)s, n);
}

/* What the terminal said since last time, with the echo of the
 * command left off -- the visitor typed it and saw it. On a pty the
 * line ends need a carriage return before them. */
static void flush_transcript(void)
{
    u64 total = term_total(ssh.ts);
    if (total <= ssh.taken) return;
    u64 delta = total - ssh.taken;
    u64 len;
    const char *out = term_out(ssh.ts, &len);
    if (delta > len) delta = len;
    const char *p = out + len - delta;
    ssh.taken = total;

    if (delta >= 2 && p[0] == '>' && p[1] == ' ') {
        while (delta && *p != '\n') { p++; delta--; }
        if (delta) { p++; delta--; }
    }

    static u8 buf[4096];
    u32 at = 0;
    for (u64 i = 0; i < delta; i++) {
        if (at + 2 > sizeof(buf)) { chan_data(buf, at); at = 0; }
        if (p[i] == '\n' && ssh.pty) buf[at++] = '\r';
        buf[at++] = (u8)p[i];
    }
    if (at) chan_data(buf, at);
}

static void session_close(void)
{
    if (ssh.closing_sent) return;
    pl = 0;
    pw_byte(MSG_CHANNEL_REQUEST);
    pw_u32(ssh.rid);
    pw_cstr("exit-status");
    pw_byte(0);
    pw_u32(0);
    send_packet();
    pl = 0;
    pw_byte(MSG_CHANNEL_EOF);
    pw_u32(ssh.rid);
    send_packet();
    pl = 0;
    pw_byte(MSG_CHANNEL_CLOSE);
    pw_u32(ssh.rid);
    send_packet();
    ssh.closing_sent = true;
    ssh.close_ns = time_ns();
    ssh.stage = ST_CLOSING;
}

static void run_line(void)
{
    ssh.line[ssh.llen] = 0;
    ssh.llen = 0;
    term_line(ssh.ts, ssh.line);
    flush_transcript();
    if (ssh.pty) chan_text("> ");
}

/* Keys from a shell: letters gather, a return runs them, the
 * ordinary editing keys do what they do everywhere. Without a pty
 * nothing is echoed; the other side is a pipe, not a person. */
static void on_keys(const u8 *d, u32 n)
{
    for (u32 i = 0; i < n; i++) {
        /* Bytes owed to a text: they go in as they are, and the window
         * they came through is opened again by as much, or the other
         * side stops sending at a megabyte and waits for a word that
         * would never come. */
        if (term_taking(ssh.ts)) {
            u32 used = term_take_bytes(ssh.ts, d + i, n - i);
            if (used) {
                pl = 0;
                pw_byte(MSG_CHANNEL_WINDOW);
                pw_u32(ssh.rid);
                pw_u32(used);
                send_packet();
                if (!term_taking(ssh.ts)) flush_transcript();
                i += used - 1;
                continue;
            }
        }
        u8 c = d[i];
        if (ssh.esc) {
            if (c >= 0x40 && c <= 0x7E && c != '[') ssh.esc = 0;
            continue;
        }
        if (c == 0x1B) { ssh.esc = 1; continue; }
        if (c == '\r' || c == '\n') {
            if (ssh.pty) chan_text("\r\n");
            run_line();
            continue;
        }
        if (c == 0x7F || c == 0x08) {
            if (ssh.llen) {
                ssh.llen--;
                if (ssh.pty) chan_text("\b \b");
            }
            continue;
        }
        if (c == 0x03) {                 /* ctrl-c: the line is dropped */
            ssh.llen = 0;
            if (ssh.pty) chan_text("^C\r\n> ");
            continue;
        }
        if (c == 0x04) {                 /* ctrl-d on an empty line: leave */
            if (ssh.llen == 0) { if (ssh.pty) chan_text("\r\n"); session_close(); return; }
            continue;
        }
        if (c >= 0x20 && c < 0x7F && ssh.llen < sizeof(ssh.line) - 1) {
            ssh.line[ssh.llen++] = (char)c;
            if (ssh.pty) {
                u8 shown = term_secret(ssh.ts) ? '*' : c;   /* a passphrase shows as dots */
                chan_data(&shown, 1);
            }
        }
    }
}

static bool on_channel_open(const u8 *p, u32 n)
{
    reader r = { p, n, 1, false };
    u32 tn;
    const u8 *type = rd_str(&r, &tn);
    u32 sender = rd_u32(&r);
    u32 win = rd_u32(&r);
    rd_u32(&r);                                   /* their max packet */
    if (r.bad) return false;

    if (ssh.chan || !str_is(type, tn, "session")) {
        pl = 0;
        pw_byte(MSG_CHANNEL_OPEN_FAIL);
        pw_u32(sender);
        pw_u32(ssh.chan ? 4 : 3);
        pw_cstr(ssh.chan ? "only one session" : "only session channels");
        pw_cstr("");
        send_packet();
        return true;
    }

    ssh.ts = term_open();
    if (!ssh.ts) {
        pl = 0;
        pw_byte(MSG_CHANNEL_OPEN_FAIL);
        pw_u32(sender);
        pw_u32(4);
        pw_cstr("no terminal session is free");
        pw_cstr("");
        send_packet();
        return true;
    }

    ssh.chan = true;
    ssh.rid = sender;
    ssh.rwin = win;
    ssh.eaten = 0;
    pl = 0;
    pw_byte(MSG_CHANNEL_OPEN_OK);
    pw_u32(sender);
    pw_u32(0);
    pw_u32(1u << 20);
    pw_u32(16384);
    send_packet();
    ssh.stage = ST_LIVE;
    return true;
}

static bool on_channel_request(const u8 *p, u32 n)
{
    reader r = { p, n, 1, false };
    rd_u32(&r);                                   /* our channel: 0 */
    u32 tn;
    const u8 *type = rd_str(&r, &tn);
    u8 want = rd_byte(&r);
    if (r.bad || !ssh.chan) return false;

    bool ok = false;
    if (str_is(type, tn, "pty-req")) {
        ssh.pty = true;
        ok = true;
    } else if (str_is(type, tn, "env")) {
        ok = true;                                /* taken and ignored */
    } else if (str_is(type, tn, "shell")) {
        if (!ssh.shell) {
            ssh.shell = true;
            ok = true;
        }
    } else if (str_is(type, tn, "exec")) {
        u32 cn;
        const u8 *cmd = rd_str(&r, &cn);
        if (!r.bad && !ssh.shell) {
            ok = true;
            ssh.shell = true;
            /* One command, its answer, and the door: the greeting is
             * not part of the answer. */
            ssh.taken = term_total(ssh.ts);
            u32 l = 0;
            for (u32 i = 0; i < cn && l < sizeof(ssh.line) - 1; i++)
                if (cmd[i] >= 0x20 && cmd[i] < 0x7F) ssh.line[l++] = (char)cmd[i];
            ssh.llen = l;
            if (want) {
                pl = 0;
                pw_byte(MSG_CHANNEL_SUCCESS);
                pw_u32(ssh.rid);
                send_packet();
                want = 0;
            }
            run_line();
            session_close();
            return true;
        }
    } else if (str_is(type, tn, "window-change") ||
               str_is(type, tn, "signal")) {
        ok = true;
    }

    if (want) {
        pl = 0;
        pw_byte(ok ? MSG_CHANNEL_SUCCESS : MSG_CHANNEL_FAILURE);
        pw_u32(ssh.rid);
        send_packet();
    }

    if (ok && str_is(type, tn, "shell")) {
        /* The greeting, then the prompt. */
        flush_transcript();
        if (ssh.pty) chan_text("> ");
    }
    return true;
}

static bool on_channel_data(const u8 *p, u32 n)
{
    reader r = { p, n, 1, false };
    rd_u32(&r);
    u32 dn;
    const u8 *d = rd_str(&r, &dn);
    if (r.bad || !ssh.chan) return false;

    ssh.eaten += dn;
    if (ssh.eaten > (1u << 19)) {
        pl = 0;
        pw_byte(MSG_CHANNEL_WINDOW);
        pw_u32(ssh.rid);
        pw_u32(ssh.eaten);
        send_packet();
        ssh.eaten = 0;
    }
    if (ssh.shell && ssh.stage == ST_LIVE) on_keys(d, dn);
    return true;
}

/* A rekey the client asked for, mid-session: answer its KEXINIT with
 * ours and run the exchange again. The channel and the session id stand;
 * only the keys change. */
static bool begin_rekey(const u8 *p, u32 n)
{
    ssh.rekeying = true;
    send_kexinit();              /* our KEXINIT, kept as I_S for the new hash */
    return on_kexinit(p, n);     /* theirs; moves to ST_ECDH */
}

/* ------------------------------------------------------------------ */
/* One payload in                                                      */
/* ------------------------------------------------------------------ */

static bool on_payload(const u8 *p, u32 n)
{
    if (n == 0) return false;
    u8 t = p[0];

    if (t == MSG_IGNORE || t == MSG_DEBUG || t == MSG_UNIMPLEMENTED) return true;
    if (t == MSG_DISCONNECT) { door_close(cur); visit_end("client disconnected"); return true; }

    switch (ssh.stage) {
    case ST_KEXINIT:
        if (t == MSG_KEXINIT) return on_kexinit(p, n);
        return false;
    case ST_ECDH:
        if (t == MSG_KEX_ECDH_INIT) return on_ecdh_init(p, n);
        if (t == MSG_KEXINIT) return true;        /* theirs, arriving late */
        return false;
    case ST_NEWKEYS:
        if (t == MSG_NEWKEYS) {
            /* Their NEWKEYS: from the next packet on, the incoming key is
             * the new one. */
            memcpy(ssh.key_in, ssh.nkey_in, 16);
            memcpy(ssh.iv_in, ssh.niv_in, 12);
            ssh.enc_in = true;
            /* After the first exchange the login follows; after a rekey
             * the session simply carries on where it left off. */
            if (ssh.rekeying) kprintf("ssh:  rekeyed with %s\n", ssh.user);
            ssh.stage = ssh.rekeying ? ST_LIVE : ST_AUTH;
            ssh.rekeying = false;
            return true;
        }
        return false;
    case ST_AUTH:
        if (t == MSG_SERVICE_REQUEST) {
            reader r = { p, n, 1, false };
            u32 sn;
            const u8 *s = rd_str(&r, &sn);
            if (r.bad || !str_is(s, sn, "ssh-userauth")) return false;
            pl = 0;
            pw_byte(MSG_SERVICE_ACCEPT);
            pw_cstr("ssh-userauth");
            send_packet();
            return true;
        }
        if (t == MSG_USERAUTH_REQUEST) return on_userauth(p, n);
        if (t == MSG_KEXINIT) { say_disconnect(2, "rekeying is not supported"); return false; }
        return false;
    case ST_OPEN:
    case ST_LIVE:
    case ST_CLOSING:
        if (t == MSG_CHANNEL_OPEN)    return on_channel_open(p, n);
        if (t == MSG_CHANNEL_REQUEST) return on_channel_request(p, n);
        if (t == MSG_CHANNEL_DATA)    return on_channel_data(p, n);
        if (t == MSG_CHANNEL_WINDOW) {
            reader r = { p, n, 1, false };
            rd_u32(&r);
            u32 more = rd_u32(&r);
            if (!r.bad) ssh.rwin += more;
            return true;
        }
        if (t == MSG_CHANNEL_EOF) {
            if (ssh.stage == ST_LIVE) session_close();
            return true;
        }
        if (t == MSG_CHANNEL_CLOSE) {
            if (!ssh.closing_sent) session_close();
            door_close(cur);
            visit_end("session ended");
            return true;
        }
        if (t == MSG_GLOBAL_REQUEST) {
            reader r = { p, n, 1, false };
            u32 gn;
            rd_str(&r, &gn);
            u8 want = rd_byte(&r);
            if (!r.bad && want) {
                pl = 0;
                pw_byte(MSG_REQUEST_FAILURE);
                send_packet();
            }
            return true;
        }
        if (t == MSG_KEXINIT) {
            if (ssh.stage == ST_LIVE) return begin_rekey(p, n);
            say_disconnect(2, "rekey only in an open session");
            return false;
        }
        return true;                              /* anything else: let it pass */
    default:
        return false;
    }
}

/* Cuts one packet from the front of in[] and hands its payload up.
 * Answers false when nothing whole is there yet. */
static bool take_packet(void)
{
    if (ssh.stage == ST_VERSION) {
        u32 i = 0;
        while (i < ssh.in_len && ssh.in[i] != '\n') i++;
        if (i >= ssh.in_len) {
            if (ssh.in_len >= 255) { door_close(cur); visit_end("no version line came"); }
            return false;
        }
        u32 l = i;
        if (l && ssh.in[l - 1] == '\r') l--;
        if (l > sizeof(ssh.vc)) l = sizeof(ssh.vc);
        memcpy(ssh.vc, ssh.in, l);
        ssh.vc_len = l;
        memmove(ssh.in, ssh.in + i + 1, ssh.in_len - i - 1);
        ssh.in_len -= i + 1;
        if (l < 8 || memcmp(ssh.vc, "SSH-2.0-", 8) != 0) {
            door_close(cur);
            visit_end("not ssh 2");
            return false;
        }
        ssh.stage = ST_KEXINIT;
        return true;
    }

    if (ssh.in_len < 4) return false;
    u32 L = be32(ssh.in);
    if (L < 8 || L > 18000) { door_close(cur); visit_end("invalid packet length"); return false; }

    static u8 plain[18100];
    const u8 *payload;
    u32 plen;
    u32 whole;

    if (ssh.enc_in) {
        whole = 4 + L + 16;
        if (ssh.in_len < whole) return false;
        if (!aes128_gcm_open(ssh.key_in, ssh.iv_in, ssh.in, 4, ssh.in + 4, L,
                             ssh.in + 4 + L, plain)) {
            door_close(cur);
            visit_end("packet authentication failed");
            return false;
        }
        iv_step(ssh.iv_in);
        u8 pad = plain[0];
        if ((u32)pad + 1 > L) { door_close(cur); visit_end("invalid padding"); return false; }
        payload = plain + 1;
        plen = L - 1 - pad;
    } else {
        whole = 4 + L;
        if (ssh.in_len < whole) return false;
        u8 pad = ssh.in[4];
        if ((u32)pad + 1 > L) { door_close(cur); visit_end("invalid padding"); return false; }
        memcpy(plain, ssh.in + 5, L - 1 - pad);
        payload = plain;
        plen = L - 1 - pad;
    }

    memmove(ssh.in, ssh.in + whole, ssh.in_len - whole);
    ssh.in_len -= whole;

    if (!on_payload(payload, plen) && ssh.stage != ST_IDLE) {
        say_disconnect(2, "unexpected message");
        door_close(cur);
        visit_end("message out of order");
    }
    return ssh.stage != ST_IDLE;
}

/* ------------------------------------------------------------------ */

/* One session's turn, on the door slot of the same index. */
static void ssh_service_one(void)
{
    /* A knock, or a new knock over an old visit: begin again. */
    if (door_alive(cur) && (ssh.stage == ST_IDLE || ssh.visit != door_visit(cur)))
        visit_begin();

    if (ssh.stage == ST_IDLE) return;

    if (!door_alive(cur)) {
        visit_end("connection lost");
        return;
    }

    /* Words waiting for room on the wire. */
    while (ssh.pend_len) {
        u32 room = door_room(cur);
        if (room == 0) break;
        u32 n = ssh.pend_len < room ? ssh.pend_len : room;
        if (n > 4096) n = 4096;
        if (!door_write(cur, ssh.pend, n)) break;
        memmove(ssh.pend, ssh.pend + n, ssh.pend_len - n);
        ssh.pend_len -= n;
    }

    /* Words from the wire. */
    u32 got = door_read(cur, ssh.in + ssh.in_len, sizeof(ssh.in) - ssh.in_len);
    ssh.in_len += got;
    while (take_packet()) { }
    if (ssh.stage == ST_IDLE) return;

    if (ssh.in_len >= sizeof(ssh.in)) {
        door_close(cur);
        visit_end("packet too large");
        return;
    }

    /* Patience has an end: a visit that never gets to the terminal,
     * and a close the visitor never answers. */
    u64 now = time_ns();
    if (ssh.stage < ST_LIVE && now - ssh.born_ns > 40 * SECOND) {
        say_disconnect(2, "too slow");
        door_close(cur);
        visit_end("login timeout");
        return;
    }
    if (ssh.stage == ST_CLOSING && ssh.pend_len == 0 &&
        now - ssh.close_ns > 2 * SECOND) {
        door_close(cur);
        visit_end("session ended");
        return;
    }
    if (door_finished(cur) && ssh.pend_len == 0) {
        door_close(cur);
        visit_end("connection closed");
    }
}

void ssh_service(void)
{
    if (!host_ready) return;
    u32 n = door_count();
    if (n > SSH_SESSIONS) n = SSH_SESSIONS;
    for (cur = 0; cur < n; cur++) ssh_service_one();
    cur = 0;
}
