/*
 * pipe.c -- objects, words, work and kernels between machines, UDP datagrams.
 * - SEEK/HERE plain: name, work flag, free memory, key claim, version, up to four other machines heard
 * - HELLO/WELCOME: x25519 handshake signed with the door key (ed25519); the key identifies the node, one row in the nodes table
 * - SEALED: AES-128-GCM records, one key per direction, counter per record, replays dropped
 * - OFFER/CHUNK/HAVE/TAKEN: windowed transfer directly from and into objects, up to 8 MiB; kind 4 = kernel image
 * - a kernel is installed only when the sender's nodes row contains "update"; work runs only with "work | welcomed" or the row's "work"
 * - heartbeat: SEEK to every node with an address every 30 s; the "network" page is rewritten every 2 s
 * - identity is trust on first use; a different key from a known address is rejected
 */
#include <eb/pipe.h>
#include <eb/nodes.h>
#include <eb/net.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/time.h>
#include <eb/crypto.h>
#include <eb/string.h>
#include <eb/msg.h>
#include <eb/proc.h>
#include <eb/standard.h>
#include <eb/pmm.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/ssh.h>
#include <eb/fat.h>
#include <eb/version.h>
#include <eb/cc.h>
#include <eb/ld.h>
#include <eb/term.h>

#define MAGIC     0x58504245u        /* "EBPX", little-endian */
#define K_OFFER   1
#define K_CHUNK   2
#define K_TAKEN   3
#define K_SEEK    4                  /* who else is on this wire? */
#define K_HERE    5                  /* i am, and this is my name */
#define K_HELLO   6                  /* a knock: my fresh public key */
#define K_WELCOME 7                  /* the answer: mine, in return */
#define K_SEALED  8                  /* an envelope around any of the rest */
#define K_ASK     9                  /* a job: run this text, answer me */
#define K_ANSWER  10                 /* what became of a job */
#define K_SAY     11                 /* a word for the person there */
#define K_HAVE    12                 /* how far a transfer has come */
#define K_ROTATE  13                 /* my door key is renewed: old, new, each signed by the other */
#define K_VOUCH   14                 /* i recognise this node's key: voucher, vouchee, address, name, signed by the voucher */

/* One chunk fills one datagram: 20 bytes of envelope, 24 of chunk head,
 * 16 of tag, under the 1400 the wire takes. */
#define CHUNK_MAX      1336
#define INNER_MAX      (24 + CHUNK_MAX)
#define CARRY_MAX_BYTES (8u << 20)
#define WINDOW_CHUNKS  24            /* chunks in flight beyond the last progress heard */
#define HAVE_EVERY     8             /* the receiver reports every so many chunks */
#define KERNEL_MIN     65536

/* What a TAKEN says, and why a refusal. */
#define T_TAKEN   0
#define T_REFUSED 1
#define T_BUSY    2
#define R_NOT_ALLOWED 1
#define R_TOO_BIG     2
#define R_UNKNOWN     3

/* Far work. A recipe fits one sealed datagram; the budget is what the
 * asking side grants and the interpreter over there enforces. */
#define RECIPE_MAX   1024
#define ASK_BUDGET_S 20

#define SAY_MAX 200

#define DESK_JOBS 4
#define PART_MAX  8
#define FASK_MAX  4
#define CAND_MAX  8

#define A_OK      0
#define A_NOWORK  1
#define A_LATE    2
#define A_BUSY    3
#define A_SILENT  4
#define A_NOINPUT 5                  /* the ask names an input that never arrived */

/* Flags in an OFFER and in an ASK. */
#define F_FOR_WORK 1                 /* offer: an input for work, held for the ask that follows */
#define F_HAS_INPUT 1                /* ask: hand the held input to the script as its third gift */
#define F_COMPILED  2                /* ask: the recipe is c source; compile it there and run the image */
#define INPUTS_MAX 4                 /* inputs held, one per asking machine */

/* Wire kinds, deliberately not the kernel's type ids. */
#define W_TEXT    1
#define W_BYTES   2
#define W_PICTURE 3
#define W_KERNEL  4

#define SECOND 1000000000ULL
#define MS     1000000ULL
#define BEAT_S  30                   /* the heartbeat to known nodes */
#define QUIET_S 90                   /* unheard this long: quiet */

static object *arrivals;

void pipe_arrivals_set(object *list)
{
    if (arrivals) obj_release(arrivals);
    arrivals = list;
    if (arrivals) obj_retain(arrivals);
}

object *pipe_arrivals(void) { return arrivals; }

static u32 rd32(const u8 *p);
static u64 rd64(const u8 *p);
static void wr32(u8 *p, u32 v);
static void wr64(u8 *p, u64 v);
static void rotate_message(u8 m[80], const u8 oldp[32], const u8 newp[32]);
static void vouch_message(u8 m[110], const u8 vk[32], const u8 ek[32],
                          const u8 ip[4], u16 port, const char name[24]);

/* ------------------------------------------------------------------ */
/* Small tools                                                         */
/* ------------------------------------------------------------------ */

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) |
           ((u32)p[3] << 24);
}

static u64 rd64(const u8 *p)
{
    return (u64)rd32(p) | ((u64)rd32(p + 4) << 32);
}

static void wr32(u8 *p, u32 v)
{
    p[0] = (u8)v; p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

static void wr64(u8 *p, u64 v)
{
    wr32(p, (u32)v);
    wr32(p + 4, (u32)(v >> 32));
}

static u32 put(char *buf, u32 at, const char *s)
{
    while (*s) buf[at++] = *s++;
    return at;
}

static u32 put_dec(char *buf, u32 at, u64 v)
{
    char tmp[24];
    u32 n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) buf[at++] = tmp[--n];
    return at;
}

static u32 put_pad(char *buf, u32 at, const char *s, u32 width)
{
    u32 n = 0;
    while (s[n]) buf[at++] = s[n++];
    while (n < width) { buf[at++] = ' '; n++; }
    return at;
}

static u32 put_ip(char *buf, u32 at, const u8 ip[4])
{
    for (u32 i = 0; i < 4; i++) {
        if (i) buf[at++] = '.';
        at = put_dec(buf, at, ip[i]);
    }
    return at;
}

static bool ip4_same(const u8 *a, const u8 *b)
{
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

static void copy_why(char *why, u32 max, const char *text)
{
    if (!why || !max) return;
    u32 i = 0;
    while (text[i] && i + 1 < max) { why[i] = text[i]; i++; }
    why[i] = 0;
}

static void lay_answer(u32 no, const u8 *text);

static u8 wire_kind_of(type_id t)
{
    if (t == TYPE_TEXT) return W_TEXT;
    if (t == TYPE_BYTES) return W_BYTES;
    if (t == TYPE_PICTURE) return W_PICTURE;
    return 0;
}

static type_id local_kind_of(u8 w)
{
    if (w == W_TEXT) return TYPE_TEXT;
    if (w == W_BYTES) return TYPE_BYTES;
    if (w == W_PICTURE) return TYPE_PICTURE;
    return TYPE_NULL;
}

/* ------------------------------------------------------------------ */
/* The line                                                            */
/* ------------------------------------------------------------------ */

static object *line_obj;

void pipe_line_set(object *t)
{
    if (line_obj) obj_release(line_obj);
    line_obj = t;
    if (line_obj) obj_retain(line_obj);
}

object *pipe_line(void) { return line_obj; }

static u64 say_heard[8];
static u32 say_heard_at;

static struct {
    bool active;
    u64  born_ns;
    char text[SAY_MAX + 1];
} saypend;

static void line_append(const char *who, const char *what)
{
    if (!line_obj || !who || !what) return;

    char ln[256];
    u64 at = 0;
    for (u64 i = 0; who[i] && at < 30; i++) {
        char c = who[i];
        ln[at++] = (c >= 0x20 && c < 0x7F) ? c : ' ';
    }
    while (at > 0 && ln[at - 1] == ' ') at--;
    ln[at++] = ':';
    ln[at++] = ' ';
    ln[at++] = ' ';
    for (u64 i = 0; what[i] && at < sizeof(ln) - 2; i++) {
        char c = what[i];
        ln[at++] = (c >= 0x20 && c < 0x7F) ? c : ' ';
    }
    while (at > 0 && ln[at - 1] == ' ') at--;
    ln[at++] = '\n';

    u64 flags = irq_save();
    u8 *d = (u8 *)obj_data(line_obj);
    u64 size = obj_size(line_obj);
    if (!d || size < sizeof(ln) + 2) { irq_restore(flags); return; }
    u64 len = 0;
    while (len < size && d[len]) len++;

    if (len + at + 1 > size) {
        u64 from = len / 2;
        while (from < len && d[from] != '\n') from++;
        if (from < len) from++;
        memmove(d, d + from, len - from);
        len -= from;
        memset(d + len, 0, size - len);
    }
    memcpy(d + len, ln, at);
    d[len + at] = 0;
    irq_restore(flags);
    obj_touch(line_obj);
}

/* ------------------------------------------------------------------ */
/* The ledger                                                          */
/* ------------------------------------------------------------------ */

/* A durable record of far-work jobs: what was asked and what came back,
 * one line each, kept in a read-only text on the system shelf. It
 * outlives the desk (which holds only jobs in flight) and the journal's
 * ring, so a machine can be asked later what work it gave out or did. */
static object *ledger_obj;

void pipe_ledger_set(object *t)
{
    if (ledger_obj) obj_release(ledger_obj);
    ledger_obj = t;
    if (ledger_obj) obj_retain(ledger_obj);
}

object *pipe_ledger(void) { return ledger_obj; }

static void ledger_append(const char *what)
{
    if (!ledger_obj || !what) return;

    char ln[160];
    u64 at = 0;
    for (u64 i = 0; what[i] && at < sizeof(ln) - 2; i++) {
        char c = what[i];
        ln[at++] = (c >= 0x20 && c < 0x7F) ? c : ' ';
    }
    while (at > 0 && ln[at - 1] == ' ') at--;
    ln[at++] = '\n';

    u64 flags = irq_save();
    u8 *d = (u8 *)obj_data(ledger_obj);
    u64 size = obj_size(ledger_obj);
    if (!d || size < sizeof(ln) + 2) { irq_restore(flags); return; }
    u64 len = 0;
    while (len < size && d[len]) len++;

    if (len + at + 1 > size) {                 /* full: drop the older half */
        u64 from = len / 2;
        while (from < len && d[from] != '\n') from++;
        if (from < len) from++;
        memmove(d, d + from, len - from);
        len -= from;
        memset(d + len, 0, size - len);
    }
    memcpy(d + len, ln, at);
    d[len + at] = 0;
    irq_restore(flags);
    obj_touch(ledger_obj);
}

/* ------------------------------------------------------------------ */
/* Company on the wire                                                 */
/* ------------------------------------------------------------------ */

/* Machines heard on the wire, by address: name, flags, key claim,
 * version, time of the last datagram. "found" = heard since the last
 * scan began. */
#define HEARD_MAX 16

typedef struct {
    bool used;
    u8   ip[4];
    u16  port;
    char name[24];
    bool works;
    u32  free_mib;
    bool has_key;
    u8   key[32];
    char version[24];
    u64  seen_ns;
    u16  up_min;                     /* the node's uptime in minutes, as it said */
    bool quiet_said;                 /* the journal has said it went quiet */
} heardrec;

static heardrec heard[HEARD_MAX];

static u64 scan_until_ns;
static u64 scan_last_call_ns;
static u64 scan_started_ns;

void pipe_scan(void)
{
    scan_started_ns = time_ns();
    scan_until_ns = scan_started_ns + 3ULL * SECOND;
    scan_last_call_ns = 0;
    journal_says("pipe", "scan: SEEK sent");
}

bool pipe_scanning(void)
{
    return time_ns() < scan_until_ns;
}

static bool found_is(const heardrec *h)
{
    return h->used && scan_started_ns && h->seen_ns >= scan_started_ns;
}

u32 pipe_found_count(void)
{
    u32 n = 0;
    for (u32 i = 0; i < HEARD_MAX; i++) if (found_is(&heard[i])) n++;
    return n;
}

bool pipe_found_at(u32 i, u8 ip[4], char name[24],
                   bool *works, u32 *free_mib)
{
    u32 n = 0;
    for (u32 k = 0; k < HEARD_MAX; k++) {
        if (!found_is(&heard[k])) continue;
        if (n == i) {
            for (u32 j = 0; j < 4; j++) ip[j] = heard[k].ip[j];
            for (u32 j = 0; j < 24; j++) name[j] = heard[k].name[j];
            if (works) *works = heard[k].works;
            if (free_mib) *free_mib = heard[k].free_mib;
            return true;
        }
        n++;
    }
    return false;
}

static heardrec *heard_by_ip(const u8 *ip)
{
    for (u32 i = 0; i < HEARD_MAX; i++)
        if (heard[i].used && ip4_same(heard[i].ip, ip)) return &heard[i];
    return NULL;
}

u64 pipe_seen_ago_s(const u8 ip[4])
{
    heardrec *h = heard_by_ip(ip);
    if (!h || !h->seen_ns) return ~0ULL;
    return (time_ns() - h->seen_ns) / SECOND;
}

static heardrec *heard_note(const u8 *ip, u16 port, const u8 *name, u32 nmax,
                            bool works, u32 free_mib)
{
    heardrec *h = heard_by_ip(ip);
    if (!h) {
        for (u32 i = 0; i < HEARD_MAX && !h; i++)
            if (!heard[i].used) h = &heard[i];
    }
    if (!h) {
        h = &heard[0];
        for (u32 i = 1; i < HEARD_MAX; i++)
            if (heard[i].seen_ns < h->seen_ns) h = &heard[i];
        memset(h, 0, sizeof(*h));
    }
    h->used = true;
    for (u32 i = 0; i < 4; i++) h->ip[i] = ip[i];
    h->port = port ? port : PIPE_PORT;
    u32 n = 0;
    while (n < 23 && n < nmax && name[n]) {
        char c = (char)name[n];
        h->name[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        n++;
    }
    h->name[n] = 0;
    h->works = works;
    h->free_mib = free_mib;
    h->seen_ns = time_ns();
    if (h->quiet_said) {
        /* Heard again after the journal had said it went quiet. */
        i32 row = nodes_by_address(h->ip);
        if (row >= 0) {
            char nm[24], line[64];
            nodes_name_at((u32)row, nm);
            u32 at = put(line, 0, "node ");
            at = put(line, at, nm);
            at = put(line, at, " is back");
            line[at] = 0;
            journal_says("pipe", line);
            kprintf("pipe: %s\n", line);
        }
        h->quiet_said = false;
    }
    return h;
}

/* SEEK/HERE: magic, kind, pad, name (24); work flag for the recipient;
 * free memory in MiB; own key (32, unverified claim); version (24);
 * count and up to four (ip, port) pairs heard within QUIET_S, for
 * discovery across routers. Older versions read the first 40 bytes. */
static void say_who(u8 kind, const u8 dst[4], u16 dport)
{
    u8 pkt[128];
    memset(pkt, 0, sizeof(pkt));
    wr32(pkt, MAGIC);
    pkt[4] = kind;
    char nm[24];
    settings_name(nm, sizeof(nm));
    for (u32 i = 0; i < 24; i++) pkt[8 + i] = (u8)nm[i];

    bool works = settings_work();
    if (!works) {
        i32 row = nodes_by_address(dst);
        if (row >= 0 && (nodes_may_at((u32)row) & NODE_MAY_WORK)) works = true;
    }
    pkt[32] = works ? 1 : 0;
    /* uptime in minutes at 34..35; older readers stop at byte 32 */
    u64 upm = time_ns() / (60ULL * SECOND);
    if (upm > 65535) upm = 65535;
    pkt[34] = (u8)upm; pkt[35] = (u8)(upm >> 8);
    wr32(pkt + 36, (u32)(pmm_free_frames() / 256));
    ssh_identity(pkt + 40);
    for (u32 i = 0; i < 23 && erebus_version[i]; i++) pkt[72 + i] = (u8)erebus_version[i];

    u8 self[4] = { 0, 0, 0, 0 };
    net_own_address(self);
    u32 n = 0;
    u64 now = time_ns();
    for (u32 i = 0; i < HEARD_MAX && n < 4; i++) {
        heardrec *h = &heard[i];
        if (!h->used || now - h->seen_ns > QUIET_S * SECOND) continue;
        if (ip4_same(h->ip, dst) || ip4_same(h->ip, self)) continue;
        memcpy(pkt + 97 + n * 6, h->ip, 4);
        pkt[101 + n * 6] = (u8)h->port;
        pkt[102 + n * 6] = (u8)(h->port >> 8);
        n++;
    }
    pkt[96] = (u8)n;
    net_udp_send(dst, PIPE_PORT, dport, pkt, 97 + 6 * n);
}

/* An address reported by another machine: one SEEK at most per minute,
 * and only when it has not been heard directly within QUIET_S. */
static struct { u8 ip[4]; u64 ns; } gossip_asked[8];
static u32 gossip_at;

static void gossip_consider(const u8 *ip, u16 port)
{
    u8 self[4];
    if (net_own_address(self) && ip4_same(ip, self)) return;
    if (ip[0] == 0 || ip[0] == 255) return;
    heardrec *h = heard_by_ip(ip);
    u64 now = time_ns();
    if (h && now - h->seen_ns < QUIET_S * SECOND) return;
    for (u32 i = 0; i < 8; i++)
        if (ip4_same(gossip_asked[i].ip, ip) && now - gossip_asked[i].ns < 60 * SECOND)
            return;
    memcpy(gossip_asked[gossip_at].ip, ip, 4);
    gossip_asked[gossip_at].ns = now;
    gossip_at = (gossip_at + 1) % 8;
    say_who(K_SEEK, ip, port ? port : PIPE_PORT);
}

/* ------------------------------------------------------------------ */
/* The seal                                                            */
/* ------------------------------------------------------------------ */

#define SEAL_MAX 8

typedef struct {
    bool used;
    bool we_knocked;
    u8   ip[4];
    u16  port;
    u32  sid;
    u8   my_pub[32];
    u8   key_out[16], iv_out[12];
    u8   key_in[16],  iv_in[12];
    u64  ctr_out;
    u64  ctr_in_seen;
    u64  last_ns;
    bool proven;                     /* the other side signed with its door key */
    u8   idkey[32];                  /* that key */
    u8   answer[188];
    u32  answer_len;
} sealrec;

#define KNOCK_PLAIN  44              /* a knock without an identity */
#define KNOCK_SIGNED 140             /* with the door key and a signature */
#define KNOCK_NAMED  188             /* and the name and version claimed after them */

static sealrec seal[SEAL_MAX];

static struct {
    bool active;
    u8   ip[4];
    u16  port;
    u32  sid;
    u8   priv[32], pub[32];
    u32  tries;
    u64  last_ns;
    i32  expect;                     /* the node row that must answer, or -1 */
} knock;

static sealrec *seal_find(const u8 *ip, u32 sid)
{
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (seal[i].used && seal[i].sid == sid &&
            ip4_same(seal[i].ip, ip))
            return &seal[i];
    return NULL;
}

static sealrec *seal_by_ip(const u8 *ip)
{
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (seal[i].used && ip4_same(seal[i].ip, ip))
            return &seal[i];
    return NULL;
}

static sealrec *seal_slot_for(const u8 *ip)
{
    sealrec *s = seal_by_ip(ip);
    if (s) return s;
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (!seal[i].used) return &seal[i];
    sealrec *old = &seal[0];
    for (u32 i = 1; i < SEAL_MAX; i++)
        if (seal[i].last_ns < old->last_ns) old = &seal[i];
    return old;
}

/* Whether the node behind a session has a right: authenticated, and
 * its nodes row contains the right. */
static bool may(const sealrec *s, u32 right)
{
    if (!s || !s->proven) return false;
    i32 i = nodes_by_key(s->idkey);
    return i >= 0 && (nodes_may_at((u32)i) & right) != 0;
}

/* A machine's name for log lines: its nodes row, else the name it sent
 * in HERE, else its address. */
static void name_of(const u8 *ip, const sealrec *s, char out[24])
{
    i32 i = (s && s->proven) ? nodes_by_key(s->idkey) : nodes_by_address(ip);
    if (i >= 0 && nodes_name_at((u32)i, out)) return;
    heardrec *h = heard_by_ip(ip);
    if (h && h->name[0]) { memcpy(out, h->name, 24); return; }
    u32 at = put_ip(out, 0, ip);
    out[at] = 0;
}

static void seal_derive(sealrec *s, bool we_knocked,
                        const u8 hello_pub[32],
                        const u8 welcome_pub[32],
                        const u8 shared[32])
{
    u8 salt[64], prk[32];
    memcpy(salt, hello_pub, 32);
    memcpy(salt + 32, welcome_pub, 32);
    hkdf_extract(salt, 64, shared, 32, prk);

    u8 kk[16], ki[12], ak[16], ai[12];
    hkdf_expand(prk, (const u8 *)"pipe knocker key", 16, kk, 16);
    hkdf_expand(prk, (const u8 *)"pipe knocker iv", 15, ki, 12);
    hkdf_expand(prk, (const u8 *)"pipe answerer key", 17, ak, 16);
    hkdf_expand(prk, (const u8 *)"pipe answerer iv", 16, ai, 12);

    if (we_knocked) {
        memcpy(s->key_out, kk, 16); memcpy(s->iv_out, ki, 12);
        memcpy(s->key_in,  ak, 16); memcpy(s->iv_in,  ai, 12);
    } else {
        memcpy(s->key_out, ak, 16); memcpy(s->iv_out, ai, 12);
        memcpy(s->key_in,  kk, 16); memcpy(s->iv_in,  ki, 12);
    }
    s->we_knocked = we_knocked;
    s->ctr_out = 1;
    s->ctr_in_seen = 0;
    s->last_ns = time_ns();
    s->used = true;
}

static void seal_nonce(u8 out[12], const u8 iv[12], u64 ctr)
{
    for (u32 i = 0; i < 12; i++) out[i] = iv[i];
    for (u32 i = 0; i < 8; i++) out[4 + i] ^= (u8)(ctr >> (8 * i));
}

static void seal_send(sealrec *s, const u8 *inner, u32 ilen)
{
    if (!s || ilen == 0 || ilen > INNER_MAX) return;

    u8 pkt[20 + INNER_MAX + 16];
    wr32(pkt, MAGIC);
    pkt[4] = K_SEALED; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, s->sid);
    wr64(pkt + 12, s->ctr_out);

    u8 nonce[12];
    seal_nonce(nonce, s->iv_out, s->ctr_out);
    aes128_gcm_seal(s->key_out, nonce, pkt, 20, inner, ilen,
                    pkt + 20, pkt + 20 + ilen);
    s->ctr_out++;
    s->last_ns = time_ns();
    net_udp_send(s->ip, PIPE_PORT, s->port, pkt, 20 + ilen + 16);
}

static void knock_begin(const u8 *ip, u16 port, i32 expect)
{
    if (knock.active && ip4_same(knock.ip, ip)) return;
    knock.active = true;
    for (u32 i = 0; i < 4; i++) knock.ip[i] = ip[i];
    knock.port = port;
    knock.expect = expect;
    rand_bytes((u8 *)&knock.sid, 4);
    rand_bytes(knock.priv, 32);
    x25519_base(knock.pub, knock.priv);
    knock.tries = 0;
    knock.last_ns = 0;
}

static void hello_message(u8 *m, u32 sid, const u8 *eph)
{
    memcpy(m, "EBPX hello", 10);
    wr32(m + 10, sid);
    memcpy(m + 14, eph, 32);
}

static void welcome_message(u8 *m, u32 sid, const u8 *their_eph, const u8 *my_eph)
{
    memcpy(m, "EBPX welcome", 12);
    wr32(m + 12, sid);
    memcpy(m + 16, their_eph, 32);
    memcpy(m + 48, my_eph, 32);
}

/* Name and version appended to a signed HELLO/WELCOME. Unverified,
 * like the ones in HERE; used to name the row written at the first
 * handshake. */
static u32 add_claims(u8 *pkt)
{
    memset(pkt + KNOCK_SIGNED, 0, KNOCK_NAMED - KNOCK_SIGNED);
    char nm[24];
    settings_name(nm, sizeof(nm));
    for (u32 i = 0; i < 23 && nm[i]; i++) pkt[KNOCK_SIGNED + i] = (u8)nm[i];
    for (u32 i = 0; i < 23 && erebus_version[i]; i++) pkt[KNOCK_SIGNED + 24 + i] = (u8)erebus_version[i];
    return KNOCK_NAMED;
}

/* HELLO and WELCOME: the ephemeral x25519 key; with an identity also
 * the identity key, a signature over the exchange, and the claimed name
 * and version. 44 bytes without an identity, 188 with; older versions
 * read the first 44 or 140. */
static u32 build_hello(u8 *pkt)
{
    wr32(pkt, MAGIC);
    pkt[4] = K_HELLO; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, knock.sid);
    memcpy(pkt + 12, knock.pub, 32);
    if (!ssh_identity(pkt + 44)) return KNOCK_PLAIN;
    u8 msg[46];
    hello_message(msg, knock.sid, knock.pub);
    if (!ssh_sign(msg, sizeof(msg), pkt + 76)) return KNOCK_PLAIN;
    return add_claims(pkt);
}

static u32 build_welcome(u8 *pkt, u32 sid, const u8 *their_eph, const u8 *my_eph)
{
    wr32(pkt, MAGIC);
    pkt[4] = K_WELCOME; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, sid);
    memcpy(pkt + 12, my_eph, 32);
    if (!ssh_identity(pkt + 44)) return KNOCK_PLAIN;
    u8 msg[80];
    welcome_message(msg, sid, their_eph, my_eph);
    if (!ssh_sign(msg, sizeof(msg), pkt + 76)) return KNOCK_PLAIN;
    return add_claims(pkt);
}

static void knock_send(void)
{
    u8 pkt[KNOCK_NAMED];
    u32 n = build_hello(pkt);
    net_udp_send(knock.ip, PIPE_PORT, knock.port, pkt, n);
}

/* Name, version and key from a signed handshake, written into the
 * heard cache. */
static void knock_claims(const u8 *ip, u16 port, const u8 *p, u32 len)
{
    if (len < KNOCK_NAMED) return;
    heardrec *h = heard_by_ip(ip);
    if (!h) h = heard_note(ip, port, p + KNOCK_SIGNED, 24, false, 0);
    else if (!h->name[0]) {
        u32 n = 0;
        while (n < 23 && p[KNOCK_SIGNED + n]) {
            char c = (char)p[KNOCK_SIGNED + n];
            h->name[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            n++;
        }
        h->name[n] = 0;
    }
    if (!h->version[0]) {
        u32 n = 0;
        while (n < 23 && p[KNOCK_SIGNED + 24 + n]) {
            char c = (char)p[KNOCK_SIGNED + 24 + n];
            h->version[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            n++;
        }
        h->version[n] = 0;
    }
    h->has_key = true;
    memcpy(h->key, p + 44, 32);
}

/* Identity check against the nodes table. Known key: its row is
 * updated (address, version, name if still unnamed). Unknown key from
 * an address bound to another row: rejected until that row is removed.
 * Unknown key otherwise: a new row. No key: rejected when the address
 * is bound to a row, else an unauthenticated session. Returns -1
 * rejected, 0 known or unauthenticated, 1 first handshake. */
static i32 identity_verdict(const u8 *ip, u16 port, const u8 *idkey)
{
    nodes_apply();
    i32 at = nodes_by_address(ip);
    if (!idkey) {
        if (at < 0) return 0;
        kprintf("pipe: %u.%u.%u.%u sent no identity key, but the address is bound to a node; rejected\n",
                ip[0], ip[1], ip[2], ip[3]);
        journal_says("pipe", "a machine without a key at a bound address was rejected");
        return -1;
    }
    char fp[64];
    ssh_fingerprint_of(idkey, fp);
    i32 me = nodes_by_key(idkey);
    if (me < 0 && at >= 0) {
        kprintf("pipe: %u.%u.%u.%u: key %s is not the one remembered for this address; rejected\n",
                ip[0], ip[1], ip[2], ip[3], fp);
        journal_says("pipe", "a different key from a known address was rejected");
        return -1;
    }
    heardrec *h = heard_by_ip(ip);
    i32 row = nodes_meet(h ? h->name : NULL, idkey, ip, port,
                         h ? h->version : NULL, false);
    if (row < 0) {
        kprintf("pipe: no room in nodes for %u.%u.%u.%u (%s)\n",
                ip[0], ip[1], ip[2], ip[3], fp);
        journal_says("pipe", "the nodes table is full");
        return 0;
    }
    return me < 0 ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Where the pipe points                                               */
/* ------------------------------------------------------------------ */

/* The settings' peer: an address and port, or a node's name looked up
 * in the nodes table -- or among the machines heard, for one not met
 * yet. `node` is the row that must answer, or -1. */
static bool target_resolve(u8 ip[4], u16 *port, i32 *node)
{
    if (node) *node = -1;
    if (settings_peer(ip, port)) {
        if (node) *node = nodes_by_address(ip);
        return true;
    }
    char nm[24];
    if (!settings_peer_name(nm, sizeof(nm))) return false;
    nodes_apply();
    i32 i = nodes_by_name(nm);
    if (i >= 0) {
        if (!nodes_address_at((u32)i, ip, port)) return false;
        if (node) *node = i;
        return true;
    }
    for (u32 k = 0; k < HEARD_MAX; k++) {
        if (!heard[k].used) continue;
        u32 j = 0;
        while (heard[k].name[j] && nm[j] &&
               ((heard[k].name[j] | 32) == (nm[j] | 32))) j++;
        if (heard[k].name[j] || nm[j]) continue;
        memcpy(ip, heard[k].ip, 4);
        *port = heard[k].port;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Far work: the desk                                                  */
/* ------------------------------------------------------------------ */

static domain *pipe_kdom;

void pipe_prepare(domain *k) { pipe_kdom = k; }

#define P_PEND 0
#define P_RUN  1
#define P_DONE 2

#define DJ_FRESH 0
#define DJ_SCAN  1
#define DJ_RUN   2

typedef struct {
    bool    used;
    u32     no;
    object *task;
    bool    writable;
    u8      state;
    u64     scan_until_ns;
    u64     deadline_ns;

    u32     parts;
    i64     lo, hi;
    u32     len;
    u8      recipe[RECIPE_MAX];

    u8      pstate[PART_MAX];
    u32     ptries[PART_MAX];
    u64     pwait_ns[PART_MAX];
    i64     presult[PART_MAX];
    char    ptext[PART_MAX][25];   /* each part's answer as said */
    u8      pby[PART_MAX][4];      /* who answered it */
    bool    pverified[PART_MAX];   /* the answer carried a good signature over the door key */
    u8      raw[24];

    u8      cand[CAND_MAX][4];
    u16     cand_port[CAND_MAX];
    u8      istate[CAND_MAX];      /* the input: 0 not sent, 1 going, 2 there, 3 failed */
    u8      itries[CAND_MAX];
    u32     cand_count;
    u32     next_cand;

    object *input;                 /* an object every worker gets ahead of its part, or NULL */
    u8      input_kind;
    u32     input_len;
    bool    compiled;              /* the recipe is c source, compiled and run on each worker */
    u32     quorum;                /* 0: an ordinary job; N: the same task on N distinct machines, answers compared */
} desk_job;

static desk_job desk[DESK_JOBS];
static u32 desk_no;

static struct {
    bool active;
    u8   ip[4];
    u16  port;
    u64  id;
    u32  job, part;
    u32  tries;
    u64  last_ns, started_ns;
} fask[FASK_MAX];

static struct {
    bool       active;
    u64        id;
    u8         from[4];
    u64        started_ns;
    u64        budget_s;
    object    *prog;
    object    *port;
    cap_handle recv;
} workj;

static struct {
    bool valid;
    u64  id;
    u8   status;
    u8   text[24];
} gave;

static void job_says(u32 no, const char *tail)
{
    char line[48];
    u32 at = 0;
    const char *p = "job ";
    while (*p) line[at++] = *p++;

    char d[10];
    u32 nd = 0;
    if (no == 0) d[nd++] = '0';
    while (no && nd < 10) { d[nd++] = (char)('0' + no % 10); no /= 10; }
    while (nd && at < 46) line[at++] = d[--nd];

    while (*tail && at < 47) line[at++] = *tail++;
    line[at] = 0;
    journal_says("pipe", line);
}

/* An answer, signed with this machine's door key over the job id, the
 * status and the answer bytes. The seal already keeps the wire private;
 * the signature binds the answer to the node's lasting identity, so it
 * is proof of who produced it and stays proof if it is stored or passed
 * on. A machine without a door key sends the answer unsigned (40 bytes),
 * and the asking side treats it as unverified. */
static void answer_send(sealrec *s, u64 id, u8 status, const u8 text[24])
{
    u8 pkt[104];
    wr32(pkt, MAGIC);
    pkt[4] = K_ANSWER; pkt[5] = status; pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, id);
    for (u32 i = 0; i < 24; i++) pkt[16 + i] = text[i];

    u8 msg[33];
    wr64(msg, id);
    msg[8] = status;
    for (u32 i = 0; i < 24; i++) msg[9 + i] = text[i];
    if (ssh_sign(msg, sizeof(msg), pkt + 40))
        seal_send(s, pkt, 104);
    else
        seal_send(s, pkt, 40);
}

/* Whether a received answer packet carries a good signature from the
 * key this session was proven with. */
static bool answer_verified(const sealrec *s, u64 id, const u8 *p, u32 len)
{
    if (!s || !s->proven || len < 104) return false;
    u8 msg[33];
    wr64(msg, id);
    msg[8] = p[5];
    for (u32 i = 0; i < 24; i++) msg[9 + i] = p[16 + i];
    return ed25519_verify(s->idkey, msg, sizeof(msg), p + 40);
}

static void part_range(const desk_job *j, u32 part, i64 *lo, i64 *hi)
{
    /* A quorum is not a division: every machine runs the whole task, so
     * every part carries the whole range (none, in practice). */
    if (j->quorum) { *lo = j->lo; *hi = j->hi; return; }
    if (j->parts <= 1 || j->hi < j->lo) { *lo = j->lo; *hi = j->hi; return; }
    u64 n = (u64)(j->hi - j->lo + 1);
    u64 base = n / j->parts, rem = n % j->parts;
    u64 from = (u64)part * base + (part < rem ? part : rem);
    u64 count = base + (part < rem ? 1 : 0);
    *lo = j->lo + (i64)from;
    *hi = *lo + (i64)(count ? count - 1 : 0);
}

static void fask_send(sealrec *s, u32 fi)
{
    desk_job *j = &desk[fask[fi].job];
    i64 plo, phi;
    part_range(j, fask[fi].part, &plo, &phi);

    u8 pkt[40 + RECIPE_MAX];
    wr32(pkt, MAGIC);
    pkt[4] = K_ASK;
    pkt[5] = (j->input ? F_HAS_INPUT : 0) | (j->compiled ? F_COMPILED : 0);
    pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, fask[fi].id);
    wr32(pkt + 16, j->len);
    wr32(pkt + 20, ASK_BUDGET_S);
    wr64(pkt + 24, (u64)plo);
    wr64(pkt + 32, (u64)phi);
    memcpy(pkt + 40, j->recipe, j->len);
    seal_send(s, pkt, 40 + j->len);
}

static void task_append(object *task, const char *tail)
{
    u8 *d = (u8 *)obj_data(task);
    if (!d) return;
    u64 size = obj_size(task);
    u64 len = 0;
    while (len < size && d[len]) len++;

    u64 need = 3;
    for (u32 i = 0; tail[i]; i++) need++;
    if (len + need + 1 > size) {
        journal_says("pipe", "the task has no room for its answer");
        return;
    }

    if (len && d[len - 1] != '\n') d[len++] = '\n';
    d[len++] = '=';
    d[len++] = ' ';
    for (u32 i = 0; tail[i]; i++) d[len++] = (u8)tail[i];
    d[len++] = '\n';
    d[len] = 0;
    obj_touch(task);
}

static void desk_clear_fasks(u32 job)
{
    for (u32 i = 0; i < FASK_MAX; i++)
        if (fask[i].active && fask[i].job == job) fask[i].active = false;
}

static void job_end(desk_job *j, bool ok, const char *text,
                    const char *why)
{
    /* A line for the ledger either way: the job's number, what it was
     * (a compiled c task or a recipe), and its result or why it failed. */
    char led[128];
    u32 la = put(led, 0, "job ");
    la = put_dec(led, la, j->no);
    la = put(led, la, j->compiled ? " (code): " : ": ");

    if (ok) {
        char line[64];
        u32 at = put(line, 0, " answers: ");
        at = put(line, at, text);
        line[at] = 0;
        job_says(j->no, line);
        kprintf("pipe: job %u answers: %s\n", j->no, text);

        la = put(led, la, text);
        led[la] = 0;
        ledger_append(led);

        if (j->writable) task_append(j->task, text);
        else             lay_answer(j->no, (const u8 *)text);
    } else {
        char line[64];
        u32 at = put(line, 0, ": ");
        at = put(line, at, why);
        line[at] = 0;
        job_says(j->no, line);
        kprintf("pipe: job %u failed: %s\n", j->no, why);

        la = put(led, la, "failed, ");
        la = put(led, la, why);
        led[la] = 0;
        ledger_append(led);

        char an[96];
        u32 aat = put(an, 0, "job ");
        aat = put_dec(an, aat, j->no);
        aat = put(an, aat, " failed: ");
        aat = put(an, aat, why);
        an[aat] = 0;
        attention_note("pipe", an);

        char fb[64];
        u32 fat = put(fb, 0, "nothing (");
        fat = put(fb, fat, why);
        fat = put(fb, fat, ")");
        fb[fat] = 0;
        if (j->writable) task_append(j->task, fb);
    }

    desk_clear_fasks((u32)(j - desk));
    if (j->task) obj_release(j->task);
    if (j->input) obj_release(j->input);
    j->task = NULL;
    j->input = NULL;
    j->used = false;
}

/* Strikes a candidate off a job: its parts go elsewhere. */
static void cand_strike(desk_job *j, u32 c)
{
    for (u32 k = c; k + 1 < j->cand_count; k++) {
        memcpy(j->cand[k], j->cand[k + 1], 4);
        j->cand_port[k] = j->cand_port[k + 1];
        j->istate[k] = j->istate[k + 1];
        j->itries[k] = j->itries[k + 1];
    }
    if (j->cand_count) j->cand_count--;
}

/* The names of the machines that answered a job's parts, each once:
 * "alpha, beta". */
static u32 put_answerers(const desk_job *j, char *buf, u32 at, u32 max)
{
    u32 named = 0;
    for (u32 p = 0; p < j->parts; p++) {
        bool seen = false;
        for (u32 q = 0; q < p; q++)
            if (ip4_same(j->pby[q], j->pby[p])) seen = true;
        if (seen) continue;
        char nm[24];
        name_of(j->pby[p], seal_by_ip(j->pby[p]), nm);

        /* A name is followed by "(unverified)" when any part it answered
         * did not carry a good signature -- so a reader is never left to
         * assume a bare name was proven. */
        bool ver = true;
        for (u32 q = 0; q < j->parts; q++)
            if (ip4_same(j->pby[q], j->pby[p]) && !j->pverified[q]) ver = false;

        if (at + 40 >= max) break;
        if (named++) at = put(buf, at, ", ");
        at = put(buf, at, nm);
        if (!ver) at = put(buf, at, " (unverified)");
    }
    return at;
}

static bool ask_take(object *o, bool writable, object *input,
                     bool compiled, u32 quorum);

bool pipe_ask(object *o, bool writable)
{
    return ask_take(o, writable, NULL, false, 0);
}

bool pipe_ask_code(object *o, bool writable)
{
    return ask_take(o, writable, NULL, true, 0);
}

bool pipe_ask_full(object *o, bool writable, object *input,
                   bool compiled, u32 quorum)
{
    if (input) {
        if (wire_kind_of(obj_type(input)) == 0) {
            journal_says("pipe", "a work input must be a text, bytes or a picture");
            return false;
        }
        if (obj_size(input) == 0 || obj_size(input) > CARRY_MAX_BYTES) {
            journal_says("pipe", "the input exceeds 8 MiB");
            return false;
        }
    }
    return ask_take(o, writable, input, compiled, quorum);
}

bool pipe_ask_ex(object *o, bool writable, bool compiled, u32 quorum)
{
    return pipe_ask_full(o, writable, NULL, compiled, quorum);
}

bool pipe_ask_with(object *o, bool writable, object *input)
{
    return pipe_ask_full(o, writable, input, false, 0);
}

static bool ask_take(object *o, bool writable, object *input,
                     bool compiled, u32 quorum)
{
    if (!o) return false;

    if (!net_crypto_ok()) {
        journal_says("pipe", "crypto self test failed; "
                             "the pipe is disabled");
        return false;
    }
    if (obj_type(o) != TYPE_TEXT) {
        journal_says("pipe", "only a text can be asked");
        return false;
    }

    desk_job *j = NULL;
    for (u32 i = 0; i < DESK_JOBS; i++)
        if (!desk[i].used) { j = &desk[i]; break; }
    if (!j) {
        journal_says("pipe", "the desk is full "
                             "(4 jobs)");
        return false;
    }

    const u8 *d = (const u8 *)obj_data(o);
    u64 size = obj_size(o);
    u64 len = 0;
    if (d) while (len < size && d[len]) len++;

    for (;;) {
        u64 end = len;
        while (end > 0 && d[end - 1] == '\n') end--;
        u64 start = end;
        while (start > 0 && d[start - 1] != '\n') start--;
        if (end > start + 1 && d[start] == '=' && d[start + 1] == ' ')
            len = start;
        else
            break;
    }
    while (len > 0 && d[len - 1] == '\n') len--;
    if (len == 0) {
        journal_says("pipe", "the task is empty");
        return false;
    }

    u64 eol = 0;
    while (eol < len && d[eol] != '\n') eol++;

    u32 parts = 1;
    i64 lo = 0, hi = 0;
    u64 recipe_at = 0;
    /* A quorum runs the whole task on N machines and compares; it is not
     * divided, so a split line is not read and the recipe is the whole
     * text. */
    if (quorum > PART_MAX) quorum = PART_MAX;
    bool split = !quorum &&
                 (eol >= 5 && d[0]=='s' && d[1]=='p' && d[2]=='l' &&
                  d[3]=='i' && d[4]=='t');
    if (quorum) parts = quorum;
    if (split) {
        i64 nums[3];
        u32 got = 0;
        for (u64 i = 5; i < eol && got < 3; i++) {
            bool neg = (d[i] == '-' && i + 1 < eol &&
                        d[i+1] >= '0' && d[i+1] <= '9');
            if (neg) i++;
            if (d[i] < '0' || d[i] > '9') continue;
            i64 v = 0;
            while (i < eol && d[i] >= '0' && d[i] <= '9')
                v = v * 10 + (d[i++] - '0');
            nums[got++] = neg ? -v : v;
        }
        if (got < 3 || nums[0] < 1) {
            journal_says("pipe", "the split line wants: "
                                 "split P from LO to HI");
            return false;
        }
        parts = (u32)(nums[0] > PART_MAX ? PART_MAX : nums[0]);
        lo = nums[1];
        hi = nums[2];
        recipe_at = eol + 1;
    }

    if (recipe_at >= len) {
        journal_says("pipe", "no recipe after the split line");
        return false;
    }
    u64 rlen = len - recipe_at;
    if (rlen > RECIPE_MAX) {
        journal_says("pipe", "the recipe exceeds 1024 bytes");
        return false;
    }

    memset(j, 0, sizeof(*j));
    memcpy(j->recipe, d + recipe_at, rlen);
    j->len = (u32)rlen;
    j->parts = parts;
    j->lo = lo;
    j->hi = hi;
    j->writable = writable;
    j->compiled = compiled;
    j->quorum = quorum;
    j->no = ++desk_no;
    j->state = DJ_FRESH;
    j->task = o;
    obj_retain(o);
    if (input) {
        j->input = input;
        obj_retain(input);
        j->input_kind = wire_kind_of(obj_type(input));
        u64 isz = obj_size(input);
        if (obj_type(input) == TYPE_TEXT) {
            const u8 *id = (const u8 *)obj_data(input);
            u64 n = 0;
            while (n < isz && id[n]) n++;
            isz = n ? n : 1;
        }
        j->input_len = (u32)isz;
    }
    j->used = true;

    if (quorum) {
        char line[48];
        u32 at = put(line, 0, ": a quorum of ");
        at = put_dec(line, at, quorum);
        at = put(line, at, j->compiled ? " machines (code)" : " machines");
        line[at] = 0;
        job_says(j->no, line);
    } else if (parts > 1) {
        char line[48];
        u32 at = put(line, 0, ": divided into ");
        at = put_dec(line, at, parts);
        at = put(line, at, " parts");
        if (input) at = put(line, at, ", with an input");
        line[at] = 0;
        job_says(j->no, line);
    } else {
        job_says(j->no, input ? " asked of the peer, with an input" : " asked of the peer");
    }
    kprintf("pipe: job %u queued, %u bytes, %s %u%s\n",
            j->no, j->len, quorum ? "quorum" : "parts",
            quorum ? quorum : parts,
            input ? ", with an input" : "");
    return true;
}

/* ------------------------------------------------------------------ */
/* Sending                                                             */
/* ------------------------------------------------------------------ */

/* One outgoing transfer at a time, read directly from its source: a
 * retained object, or the running kernel's bytes. */
static struct {
    bool     busy;
    u8       kind;
    char     name[OBJ_NAME_MAX];
    u32      len;
    u64      id;
    object  *obj;
    const u8 *raw;
    u8       to[4];
    u16      to_port;
    i32      node;
    bool     offered;
    u32      acked, sent;
    u32      stalls, busy_tries;
    u64      started_ns, last_progress_ns;
    bool     taken;
    u8       taken_status, taken_reason;
    i32      work_job;               /* the desk job this input goes ahead of, or -1 */
    u32      work_cand;
} out;

/* Queue for 'update all': node rows, and the image to send to each. */
static struct {
    i32     rows[NODES_MAX];
    u32     n, at;
    object *image;
} upq;

static const u8 *out_bytes(void)
{
    return out.obj ? (const u8 *)obj_data(out.obj) : out.raw;
}

/* End of an outgoing transfer. For a work input, the desk's state for
 * that candidate becomes delivered or failed. */
static void out_end(bool ok)
{
    if (out.work_job >= 0 && out.work_job < DESK_JOBS &&
        desk[out.work_job].used && out.work_cand < CAND_MAX)
        desk[out.work_job].istate[out.work_cand] = ok ? 2 : 3;
    if (out.obj) obj_release(out.obj);
    out.obj = NULL;
    out.raw = NULL;
    out.busy = false;
    out.work_job = -1;
}

static bool out_begin(u8 kind, const char *name, object *obj, const u8 *raw,
                      u32 len, const u8 to[4], u16 to_port, i32 node)
{
    if (out.busy) return false;
    memset(&out, 0, sizeof(out));
    out.work_job = -1;
    out.kind = kind;
    out.len = len;
    out.obj = obj;
    if (obj) obj_retain(obj);
    out.raw = raw;
    memcpy(out.to, to, 4);
    out.to_port = to_port;
    out.node = node;
    u32 ni = 0;
    if (name) while (name[ni] && ni < OBJ_NAME_MAX - 1) { out.name[ni] = name[ni]; ni++; }
    out.name[ni] = 0;
    rand_bytes((u8 *)&out.id, 8);
    out.started_ns = time_ns();
    out.busy = true;
    return true;
}

bool pipe_post(object *o)
{
    if (!o) return false;

    if (!net_crypto_ok()) {
        journal_says("pipe", "crypto self test failed; "
                             "the pipe is disabled");
        return false;
    }

    u8 kind = wire_kind_of(obj_type(o));
    if (kind == 0) {
        journal_says("pipe", "only text, bytes and pictures can be sent");
        return false;
    }

    u8 peer[4];
    u16 pp;
    i32 node;
    if (!target_resolve(peer, &pp, &node)) {
        journal_says("pipe", "no peer is named in the settings");
        return false;
    }

    if (out.busy) {
        journal_says("pipe", "a transfer is still in progress");
        return false;
    }

    const u8 *d = (const u8 *)obj_data(o);
    u64 size = obj_size(o);
    if (!d || size == 0 || size > CARRY_MAX_BYTES) {
        journal_says("pipe", "the object exceeds 8 MiB");
        return false;
    }

    /* Texts travel as far as their words, not their whole room. */
    u32 len = (u32)size;
    if (obj_type(o) == TYPE_TEXT) {
        u32 n = 0;
        while (n < size && d[n]) n++;
        len = n ? n : 1;
    }

    return out_begin(kind, obj_name(o), o, NULL, len, peer, pp, node);
}

static bool looks_like_kernel(const u8 *d, u64 len)
{
    return d && len >= KERNEL_MIN && len <= CARRY_MAX_BYTES &&
           d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F';
}

static bool update_start(i32 row, object *image, char *why, u32 max)
{
    u8 ip[4];
    u16 port;
    char nm[24];
    nodes_name_at((u32)row, nm);
    if (!nodes_address_at((u32)row, ip, &port)) {
        copy_why(why, max, "no address known for that node.");
        return false;
    }
    const u8 *src;
    u64 len;
    if (image) {
        src = (const u8 *)obj_data(image);
        len = obj_size(image);
    } else {
        const u8 *l, *k;
        u64 ls, ks;
        if (!system_boot_files(&l, &ls, &k, &ks)) {
            copy_why(why, max, "the running kernel's bytes are not available; use 'update <node> with <kernel.elf>'.");
            return false;
        }
        src = k;
        len = ks;
    }
    if (!looks_like_kernel(src, len)) {
        copy_why(why, max, "not a kernel image (kernel.elf).");
        return false;
    }
    if (!out_begin(W_KERNEL, "kernel.elf", image, image ? NULL : src,
                   (u32)len, ip, port, row)) {
        copy_why(why, max, "a transfer is in progress.");
        return false;
    }
    char line[64];
    u32 at = put(line, 0, "sending the kernel to ");
    at = put(line, at, nm);
    line[at] = 0;
    journal_says("pipe", line);
    kprintf("pipe: sending a kernel of %u bytes to %s (%u.%u.%u.%u)\n",
            (u32)len, nm, ip[0], ip[1], ip[2], ip[3]);
    return true;
}

bool pipe_update(const char *node, object *image, char *why, u32 max)
{
    if (!net_crypto_ok()) { copy_why(why, max, "crypto self test failed; the pipe is disabled."); return false; }
    nodes_apply();
    i32 row = nodes_by_name(node);
    if (row < 0) { copy_why(why, max, "no node of that name in nodes.  'nodes' lists them."); return false; }
    if (out.busy) { copy_why(why, max, "a transfer is in progress; retry later."); return false; }
    return update_start(row, image, why, max);
}

u32 pipe_update_all(object *image, char *why, u32 max)
{
    if (!net_crypto_ok()) { copy_why(why, max, "crypto self test failed; the pipe is disabled."); return 0; }
    nodes_apply();
    if (upq.image) obj_release(upq.image);
    upq.image = image;
    if (image) obj_retain(image);
    upq.n = upq.at = 0;
    for (u32 i = 0; i < nodes_count(); i++)
        if (nodes_address_at(i, NULL, NULL)) upq.rows[upq.n++] = (i32)i;
    if (upq.n == 0) {
        copy_why(why, max, "no node with an address in nodes.");
        if (upq.image) obj_release(upq.image);
        upq.image = NULL;
        return 0;
    }
    return upq.n;
}

static void send_offer(sealrec *s)
{
    u8 pkt[56];
    wr32(pkt, MAGIC);
    pkt[4] = K_OFFER; pkt[5] = out.work_job >= 0 ? F_FOR_WORK : 0; pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, out.id);
    wr32(pkt + 16, out.kind);
    wr32(pkt + 20, out.len);
    for (u32 i = 0; i < 32; i++)
        pkt[24 + i] = (i < OBJ_NAME_MAX) ? (u8)out.name[i] : 0;
    seal_send(s, pkt, 56);
}

static void send_chunk(sealrec *s, u32 off)
{
    u8 cp[24 + CHUNK_MAX];
    u32 dlen = out.len - off < CHUNK_MAX ? out.len - off : CHUNK_MAX;
    wr32(cp, MAGIC);
    cp[4] = K_CHUNK; cp[5] = cp[6] = cp[7] = 0;
    wr64(cp + 8, out.id);
    wr32(cp + 16, off);
    wr32(cp + 20, dlen);
    memcpy(cp + 24, out_bytes() + off, dlen);
    seal_send(s, cp, 24 + dlen);
}

/* ------------------------------------------------------------------ */
/* Receiving                                                           */
/* ------------------------------------------------------------------ */

static struct {
    bool    active;
    u64     id;
    u8      kind;
    char    name[OBJ_NAME_MAX];
    u32     total, have;
    u32     since_have;
    u64     started_ns, last_have_ns;
    u8      from[4];
    u16     from_port;
    object *obj;
    bool    for_work;
} in;

static u64 last_taken_id;
static u64 restart_at_ns;

/* Work inputs received ahead of an ASK, one per sending address, held
 * for up to five minutes. */
static struct {
    bool    used;
    u8      from[4];
    object *obj;
    u64     ns;
} inputs[INPUTS_MAX];

static object *input_from(const u8 *ip)
{
    for (u32 i = 0; i < INPUTS_MAX; i++)
        if (inputs[i].used && ip4_same(inputs[i].from, ip)) return inputs[i].obj;
    return NULL;
}

static void input_hold(const u8 *ip, object *o)
{
    u32 slot = INPUTS_MAX;
    for (u32 i = 0; i < INPUTS_MAX; i++) {
        if (inputs[i].used && ip4_same(inputs[i].from, ip)) { slot = i; break; }
        if (!inputs[i].used && slot == INPUTS_MAX) slot = i;
    }
    if (slot == INPUTS_MAX) {
        slot = 0;
        for (u32 i = 1; i < INPUTS_MAX; i++)
            if (inputs[i].ns < inputs[slot].ns) slot = i;
    }
    if (inputs[slot].used && inputs[slot].obj) obj_release(inputs[slot].obj);
    inputs[slot].used = true;
    memcpy(inputs[slot].from, ip, 4);
    inputs[slot].obj = o;
    inputs[slot].ns = time_ns();
}

static void in_drop(void)
{
    if (in.obj) obj_release(in.obj);
    in.obj = NULL;
    in.active = false;
}

static void send_taken(sealrec *s, u64 id, u8 status, u8 reason)
{
    u8 ack[16];
    wr32(ack, MAGIC);
    ack[4] = K_TAKEN; ack[5] = status; ack[6] = reason; ack[7] = 0;
    wr64(ack + 8, id);
    seal_send(s, ack, 16);
}

static void send_have(sealrec *s, u64 id, u32 have)
{
    u8 pkt[20];
    wr32(pkt, MAGIC);
    pkt[4] = K_HAVE; pkt[5] = pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, id);
    wr32(pkt + 16, have);
    seal_send(s, pkt, 20);
    in.last_have_ns = time_ns();
}

static bool arrivals_place(object *o)
{
    if (!arrivals) return false;

    u64 n = obj_slots(arrivals), spot = n;
    for (u64 i = 0; i < n; i++)
        if (!obj_get_slot(arrivals, i)) { spot = i; break; }
    if (spot == n && !obj_grow_slots(arrivals, n + 1)) return false;

    obj_set_slot(arrivals, spot, o, CAP_READ | CAP_WRITE);
    obj_release(o);
    obj_touch(arrivals);
    return true;
}

static void lay_answer(u32 no, const u8 *text)
{
    u32 tl = 0;
    while (tl < 400 && text[tl]) tl++;
    if (tl == 0) return;

    object *o = obj_create(TYPE_TEXT, tl + 512, 0);
    if (!o) return;
    memcpy(obj_data(o), text, tl);

    char nm[16];
    u32 at = 0;
    const char *p = "answer ";
    while (*p) nm[at++] = *p++;
    char d[10];
    u32 nd = 0;
    if (no == 0) d[nd++] = '0';
    while (no && nd < 10) { d[nd++] = (char)('0' + no % 10); no /= 10; }
    while (nd && at < 15) nm[at++] = d[--nd];
    nm[at] = 0;
    obj_set_name(o, nm);

    if (!arrivals_place(o)) obj_release(o);
}

/* Transfer complete. Kernel: installed for the next start, restart in
 * 3 s. Work input: held for the ASK. Anything else: placed in
 * arrivals. */
static void arrival_done(sealrec *s)
{
    char who[24];
    name_of(in.from, s, who);

    if (in.kind == W_KERNEL) {
        const u8 *d = (const u8 *)obj_data(in.obj);
        char why[120];
        if (!looks_like_kernel(d, in.total)) {
            kprintf("pipe: the kernel from %s is not an elf image; dropped\n", who);
            journal_says("pipe", "a received kernel was not an elf image; dropped");
        } else if (!fat_install_kernel(d, in.total, why, sizeof(why))) {
            kprintf("pipe: the kernel from %s could not be installed: %s\n", who, why);
            journal_says("pipe", "a received kernel could not be installed");
        } else {
            kprintf("pipe: a kernel of %u bytes came from %s; installed for the next start; restarting in 3 s\n",
                    in.total, who);
            char line[80];
            u32 at = put(line, 0, "a kernel came from ");
            at = put(line, at, who);
            at = put(line, at, "; installed; restarting");
            line[at] = 0;
            journal_says("pipe", line);
            restart_at_ns = time_ns() + 3 * SECOND;
        }
        in_drop();
        return;
    }

    if (in.name[0]) obj_set_name(in.obj, in.name);
    object *o = in.obj;
    in.obj = NULL;
    in.active = false;

    if (in.for_work) {
        input_hold(in.from, o);
        kprintf("pipe: an input of %u bytes for work came from %s\n", in.total, who);
        return;
    }

    if (!arrivals) { obj_release(o); return; }
    if (!arrivals_place(o)) { obj_release(o); return; }

    kprintf("pipe: %u bytes arrived from %u.%u.%u.%u\n",
            in.total, in.from[0], in.from[1], in.from[2], in.from[3]);

    char said[48];
    u32 sa = 0;
    const char *pre = "arrived: ";
    while (*pre) said[sa++] = *pre++;
    if (in.name[0])
        for (u32 i = 0; in.name[i] && sa < sizeof(said) - 1; i++)
            said[sa++] = in.name[i];
    else {
        const char *un = "unnamed";
        while (*un && sa < sizeof(said) - 1) said[sa++] = *un++;
    }
    said[sa] = 0;
    journal_says("pipe", said);
}

/* ------------------------------------------------------------------ */
/* Saying a word                                                       */
/* ------------------------------------------------------------------ */

static u32 say_wire(const char *text)
{
    u32 tl = 0;
    while (text[tl] && tl < SAY_MAX) tl++;
    if (tl == 0) return 0;

    u8 pkt[44 + SAY_MAX];
    wr32(pkt, MAGIC);
    pkt[4] = K_SAY; pkt[5] = pkt[6] = pkt[7] = 0;
    u64 id;
    rand_bytes((u8 *)&id, 8);
    wr64(pkt + 8, id);
    char nm[24];
    settings_name(nm, sizeof(nm));
    for (u32 i = 0; i < 24; i++) pkt[16 + i] = (u8)nm[i];
    wr32(pkt + 40, tl);
    memcpy(pkt + 44, text, tl);

    u32 sent = 0;
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (seal[i].used) { seal_send(&seal[i], pkt, 44 + tl); sent++; }
    return sent;
}

bool pipe_say(const char *text)
{
    if (!text || !text[0]) return false;

    line_append("you", text);

    if (say_wire(text)) return true;

    u8 peer[4];
    u16 pp;
    i32 node;
    if (!target_resolve(peer, &pp, &node)) {
        journal_says("pipe", "no session and no peer named");
        return false;
    }
    u32 n = 0;
    while (text[n] && n < SAY_MAX) { saypend.text[n] = text[n]; n++; }
    saypend.text[n] = 0;
    saypend.active = true;
    saypend.born_ns = time_ns();
    knock_begin(peer, pp, node);
    journal_says("pipe", "handshake first; the line follows");
    return true;
}

/* ------------------------------------------------------------------ */
/* What arrives inside an envelope                                     */
/* ------------------------------------------------------------------ */

static const char *refusal_words(u8 reason)
{
    if (reason == R_NOT_ALLOWED) return "no update right granted there";
    if (reason == R_TOO_BIG)     return "size refused";
    return "declined";
}

static void inner_input(const u8 src[4], u16 sport, sealrec *s,
                        const u8 *p, u32 len)
{
    if (len < 16 || rd32(p) != MAGIC) return;
    u8 kind = p[4];
    u64 id = rd64(p + 8);

    if (kind == K_TAKEN) {
        if (out.busy && id == out.id && !out.taken) {
            out.taken = true;
            out.taken_status = p[5];
            out.taken_reason = p[6];
        }
        return;
    }

    if (kind == K_HAVE && len >= 20) {
        if (!out.busy || id != out.id) return;
        u32 have = rd32(p + 16);
        if (have > out.len) return;
        if (have > out.acked || !out.offered) {
            out.acked = have;
            out.last_progress_ns = time_ns();
            out.stalls = 0;
        }
        if (out.sent < out.acked) out.sent = out.acked;
        return;
    }

    if (kind == K_OFFER && len >= 56) {
        if (id == last_taken_id && last_taken_id) {
            send_taken(s, id, T_TAKEN, 0);
            return;
        }
        if (in.active && id == in.id) {
            /* the same offer again: answer with the current position */
            send_have(s, id, in.have);
            return;
        }
        if (in.active) {
            send_taken(s, id, T_BUSY, 0);
            return;
        }

        u32 wk = rd32(p + 16);
        u32 total = rd32(p + 20);
        char who[24];
        name_of(src, s, who);

        if (wk == W_KERNEL) {
            if (!may(s, NODE_MAY_UPDATE)) {
                kprintf("pipe: %s offers a kernel, but may not update this machine; declined\n", who);
                char line[72];
                u32 at = put(line, 0, "node ");
                at = put(line, at, who);
                at = put(line, at, " offered a kernel; it may not update this machine");
                line[at] = 0;
                journal_says("pipe", line);
                send_taken(s, id, T_REFUSED, R_NOT_ALLOWED);
                return;
            }
            if (total < KERNEL_MIN || total > CARRY_MAX_BYTES) {
                send_taken(s, id, T_REFUSED, R_TOO_BIG);
                return;
            }
        } else {
            if (local_kind_of((u8)wk) == TYPE_NULL) { send_taken(s, id, T_REFUSED, R_UNKNOWN); return; }
            if (total == 0 || total > CARRY_MAX_BYTES) { send_taken(s, id, T_REFUSED, R_TOO_BIG); return; }
        }

        type_id t = wk == W_KERNEL ? TYPE_BYTES : local_kind_of((u8)wk);
        u64 room = total + (t == TYPE_TEXT ? 512 : 0);
        object *o = obj_create(t, room, 0);
        if (!o) { send_taken(s, id, T_REFUSED, R_TOO_BIG); return; }
        if (wk == W_KERNEL) obj_set_transient(o, true);

        memset(&in, 0, sizeof(in));
        in.active = true;
        in.id = id;
        in.kind = (u8)wk;
        in.for_work = (p[5] & F_FOR_WORK) != 0 && wk != W_KERNEL;
        in.total = total;
        in.started_ns = time_ns();
        for (u32 i = 0; i < 4; i++) in.from[i] = src[i];
        in.from_port = sport;
        in.obj = o;
        u32 ni = 0;
        while (ni < OBJ_NAME_MAX - 1 && p[24 + ni]) {
            char c = (char)p[24 + ni];
            in.name[ni] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            ni++;
        }
        in.name[ni] = 0;
        if (wk == W_KERNEL) {
            kprintf("pipe: receiving a kernel of %u bytes from %s\n", total, who);
            journal_says("pipe", "receiving a kernel from a node with the update right");
        }
        send_have(s, id, 0);
        return;
    }

    if (kind == K_CHUNK && len >= 24) {
        if (!in.active || id != in.id) return;
        u32 off = rd32(p + 16);
        u32 dlen = rd32(p + 20);
        if (dlen == 0 || dlen > CHUNK_MAX || 24 + dlen > len) return;
        if (off != in.have) {
            /* a gap, or an old chunk again: say where we stand, not too often */
            if (off > in.have && time_ns() - in.last_have_ns > 100 * MS)
                send_have(s, id, in.have);
            return;
        }
        if (off + dlen > in.total) return;

        memcpy((u8 *)obj_data(in.obj) + off, p + 24, dlen);
        in.have = off + dlen;
        in.since_have++;

        if (in.have == in.total) {
            send_taken(s, in.id, T_TAKEN, 0);
            last_taken_id = in.id;
            arrival_done(s);
        } else if (in.since_have >= HAVE_EVERY) {
            in.since_have = 0;
            send_have(s, id, in.have);
        }
        return;
    }

    if (kind == K_SAY && len >= 44) {
        u32 tl = rd32(p + 40);
        if (tl == 0 || tl > SAY_MAX || 44 + tl > len) return;

        for (u32 i = 0; i < 8; i++)
            if (say_heard[i] == id) return;
        say_heard[say_heard_at] = id;
        say_heard_at = (say_heard_at + 1) % 8;

        char nm[25];
        u32 n = 0;
        while (n < 24 && p[16 + n]) {
            char c = (char)p[16 + n];
            nm[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            n++;
        }
        nm[n] = 0;
        if (!nm[0]) {
            const char *fallback = "someone";
            for (n = 0; fallback[n]; n++) nm[n] = fallback[n];
            nm[n] = 0;
        }

        char tx[SAY_MAX + 1];
        for (u32 i = 0; i < tl; i++) {
            char c = (char)p[44 + i];
            tx[i] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        }
        tx[tl] = 0;

        line_append(nm, tx);

        char note[64];
        u32 a = 0;
        while (nm[a] && a < 24) { note[a] = nm[a]; a++; }
        const char *tail = " wrote on the line";
        for (u32 i = 0; tail[i] && a < sizeof(note) - 1; i++)
            note[a++] = tail[i];
        note[a] = 0;
        journal_says("pipe", note);
        return;
    }

    /* A job. Taken when the settings welcome work from everyone, or the
     * node's row lets this one; one at a time; the same ask again is
     * the sender retrying. */
    if (kind == K_ASK && len >= 40) {
        u32 rlen = rd32(p + 16);
        u64 budget = rd32(p + 20);
        i64 wlo = (i64)rd64(p + 24);
        i64 whi = (i64)rd64(p + 32);
        if (rlen == 0 || rlen > RECIPE_MAX || 40 + rlen > len) return;

        if (workj.active && id == workj.id) return;
        if (gave.valid && id == gave.id) {
            answer_send(s, id, gave.status, gave.text);
            return;
        }

        static const u8 none[24] = { 0 };
        nodes_apply();
        if (!settings_work() && !may(s, NODE_MAY_WORK)) {
            answer_send(s, id, A_NOWORK, none);
            kprintf("pipe: turned away a job: work is refused\n");
            return;
        }
        if (workj.active) {
            answer_send(s, id, A_BUSY, none);
            return;
        }
        if (!pipe_kdom) return;

        /* The ASK may refer to an input sent ahead of it. */
        object *input = NULL;
        if (p[5] & F_HAS_INPUT) {
            input = input_from(src);
            if (!input) {
                answer_send(s, id, A_NOINPUT, none);
                kprintf("pipe: an ask refers to an input that was not received; answered\n");
                return;
            }
        }

        object *reply = port_create(4);
        if (!reply) return;
        cap_handle h = cap_insert(pipe_kdom, reply, CAP_READ);
        if (budget == 0 || budget > 60) budget = ASK_BUDGET_S;

        object *prog = NULL;
        if (p[5] & F_COMPILED) {
            /* A compiled job: build the c source into an image with the
             * in-kernel compiler, then run the image. The compiler's
             * tables are shared and not reentrant, so claim them; a busy
             * compiler answers "busy", and the asker retries. */
            if (!term_compile_claim()) {
                cap_revoke(pipe_kdom, h); obj_release(reply);
                answer_send(s, id, A_BUSY, none);
                return;
            }
            char *asmtext = lang_text_buffer();
            u8   *outimg  = lang_out_buffer();
            char cerr[128];
            i64 al = (asmtext && outimg)
                   ? cc_compile(p + 40, rlen, "task", NULL, NULL,
                                asmtext, LANG_TEXT_MAX, cerr, sizeof(cerr))
                   : -1;
            u32 ikind = 0;
            i64 imgn = (al >= 0)
                     ? lang_build_text((const u8 *)asmtext, (u64)al, false,
                                       outimg, LANG_OUT_MAX, &ikind, cerr, sizeof(cerr))
                     : -1;
            object *image = (imgn > 0 && ikind == LANG_IMAGE)
                          ? obj_create(TYPE_BYTES, (u64)imgn, 0) : NULL;
            if (image) memcpy(obj_data(image), outimg, (u64)imgn);
            term_compile_release();

            if (!image) {
                cap_revoke(pipe_kdom, h); obj_release(reply);
                answer_send(s, id, A_SILENT, none);
                kprintf("pipe: a compiled job would not build: %s\n",
                        cerr[0] ? cerr : "no image");
                return;
            }
            obj_set_name(image, "task code");
            prog = work_code_launch(image, reply, input);
            obj_release(image);
        } else {
            object *script = obj_create(TYPE_TEXT, rlen + 512, 0);
            if (!script) { cap_revoke(pipe_kdom, h); obj_release(reply); return; }
            memcpy(obj_data(script), p + 40, rlen);
            obj_set_name(script, "task text");
            prog = work_launch(script, reply, budget, wlo, whi, input);
            obj_release(script);
        }
        if (!prog) {
            cap_revoke(pipe_kdom, h);
            obj_release(reply);
            return;
        }

        workj.active = true;
        workj.id = id;
        for (u32 i = 0; i < 4; i++) workj.from[i] = src[i];
        workj.started_ns = time_ns();
        workj.budget_s = budget;
        workj.prog = prog;
        workj.port = reply;
        workj.recv = h;

        kprintf("pipe: running a job from %u.%u.%u.%u, %u bytes, "
                "%llu seconds\n",
                src[0], src[1], src[2], src[3], rlen, budget);
        journal_says("pipe", "running a job for another machine");
        return;
    }

    if (kind == K_ANSWER && len >= 40) {
        u32 fi = FASK_MAX;
        for (u32 i = 0; i < FASK_MAX; i++)
            if (fask[i].active && fask[i].id == id) { fi = i; break; }
        if (fi == FASK_MAX) return;

        u32 ji = fask[fi].job, part = fask[fi].part;
        fask[fi].active = false;
        if (ji >= DESK_JOBS || !desk[ji].used) return;
        desk_job *j = &desk[ji];
        u8 status = p[5];

        if (status == A_OK) {
            char tz[25];
            u32 ti = 0;
            while (ti < 24 && p[16 + ti]) {
                u8 c = p[16 + ti];
                tz[ti++] = (char)((c >= 0x20 && c < 0x7F) ? c : ' ');
            }
            tz[ti] = 0;

            i64 v = 0;
            bool neg = false, num = ti > 0;
            u32 di = 0;
            if (tz[0] == '-') { neg = true; di = 1; num = ti > 1; }
            for (; di < ti; di++) {
                if (tz[di] < '0' || tz[di] > '9') { num = false; break; }
                v = v * 10 + (tz[di] - '0');
            }
            if (neg) v = -v;

            j->pstate[part] = P_DONE;
            j->presult[part] = num ? v : 0;
            memcpy(j->ptext[part], tz, 25);
            memcpy(j->pby[part], fask[fi].ip, 4);
            j->pverified[part] = answer_verified(s, id, p, len);
            memcpy(j->raw, p + 16, 24);

            u32 done = 0;
            for (u32 i = 0; i < j->parts; i++)
                if (j->pstate[i] == P_DONE) done++;
            if (done < j->parts) return;

            /* The result: numeric part answers are summed, others are
             * concatenated in part order; the answering machines are
             * named. */
            static char text[400];
            u32 at = 0;
            if (j->quorum) {
                /* Every machine ran the whole task. The result is what a
                 * strict majority answered the same; only verified
                 * answers count toward the majority. */
                u32 best = 0, bestcnt = 0;
                for (u32 i = 0; i < j->parts; i++) {
                    if (!j->pverified[i]) continue;
                    u32 c = 0;
                    for (u32 k = 0; k < j->parts; k++)
                        if (j->pverified[k] &&
                            strcmp(j->ptext[i], j->ptext[k]) == 0) c++;
                    if (c > bestcnt) { bestcnt = c; best = i; }
                }
                if (bestcnt * 2 > j->parts) {
                    at = put(text, at, j->ptext[best][0] ? j->ptext[best]
                                                         : "nothing");
                    at = put(text, at, "  (agreed by ");
                    at = put_dec(text, at, bestcnt);
                    at = put(text, at, " of ");
                    at = put_dec(text, at, j->parts);
                    at = put(text, at, ")");
                    text[at] = 0;
                    job_end(j, true, text, NULL);
                } else {
                    /* No verified majority: name the distinct answers, so
                     * the disagreement is visible, not hidden. */
                    at = put(text, at, "no agreement -- ");
                    u32 named = 0;
                    for (u32 i = 0; i < j->parts &&
                                    at < sizeof(text) - 48; i++) {
                        bool seen = false;
                        for (u32 k = 0; k < i; k++)
                            if (strcmp(j->ptext[i], j->ptext[k]) == 0)
                                seen = true;
                        if (seen) continue;
                        if (named++) at = put(text, at, ", ");
                        char nm[24];
                        name_of(j->pby[i], seal_by_ip(j->pby[i]), nm);
                        at = put(text, at, nm);
                        at = put(text, at, j->pverified[i] ? " said "
                                                           : " said (unverified) ");
                        at = put(text, at, j->ptext[i][0] ? j->ptext[i]
                                                          : "nothing");
                    }
                    text[at] = 0;
                    job_end(j, false, NULL, text);
                }
                return;
            }
            if (j->parts == 1) {
                at = put(text, at, tz);
                at = put(text, at, " (by ");
                at = put_answerers(j, text, at, sizeof(text) - 2);
                at = put(text, at, ")");
            } else {
                bool all_num = true;
                for (u32 i = 0; i < j->parts; i++) {
                    const char *t = j->ptext[i];
                    u32 k = (t[0] == '-') ? 1 : 0;
                    if (!t[k]) all_num = false;
                    for (; t[k]; k++) if (t[k] < '0' || t[k] > '9') all_num = false;
                }
                if (all_num) {
                    i64 total = 0;
                    for (u32 i = 0; i < j->parts; i++)
                        total += j->presult[i];
                    u64 mag = total < 0 ? (u64)-total : (u64)total;
                    if (total < 0) text[at++] = '-';
                    at = put_dec(text, at, mag);
                } else {
                    for (u32 i = 0; i < j->parts; i++) {
                        if (i) text[at++] = ' ';
                        at = put(text, at, j->ptext[i]);
                    }
                }
                at = put(text, at, " (");
                at = put_dec(text, at, j->parts);
                at = put(text, at, " parts by ");
                at = put_answerers(j, text, at, sizeof(text) - 2);
                at = put(text, at, ")");
            }
            text[at] = 0;
            job_end(j, true, text, NULL);
            return;
        }

        if (status == A_NOINPUT) {
            /* The worker has no input for this ask: resend it once,
             * then remove the candidate. */
            for (u32 c = 0; c < j->cand_count; c++) {
                if (!ip4_same(j->cand[c], fask[fi].ip)) continue;
                if (++j->itries[c] >= 2) cand_strike(j, c);
                else j->istate[c] = 0;
                break;
            }
            if (j->cand_count == 0) {
                job_end(j, false, NULL, "the input never reached the machines");
                return;
            }
            j->pstate[part] = P_PEND;
            j->pwait_ns[part] = time_ns();
            return;
        }

        if (status == A_BUSY) {
            j->pstate[part] = P_PEND;
            j->pwait_ns[part] = time_ns() + 2 * SECOND;
            return;
        }

        if (status == A_NOWORK) {
            for (u32 c = 0; c < j->cand_count; c++) {
                if (!ip4_same(j->cand[c], fask[fi].ip)) continue;
                cand_strike(j, c);
                break;
            }
            if (j->cand_count == 0) {
                job_end(j, false, NULL, "no machine accepts work");
                return;
            }
            j->pstate[part] = P_PEND;
            j->pwait_ns[part] = time_ns();
            return;
        }

        job_end(j, false, NULL,
                status == A_LATE ? "it ran out of time"
                                 : "it ended without an answer");
        return;
    }
}

/* ------------------------------------------------------------------ */
/* What arrives on the port                                            */
/* ------------------------------------------------------------------ */

void pipe_input(const u8 src[4], u16 sport, const u8 *p, u32 len)
{
    if (len < 16 || rd32(p) != MAGIC) return;
    u8 kind = p[4];

    if (kind == K_SEEK || kind == K_HERE) {
        if (len < 32) return;
        u8 self[4];
        if (net_own_address(self) && ip4_same(src, self)) return;

        bool works = (len >= 40) && (p[32] & 1);
        u32 mib = (len >= 40) ? rd32(p + 36) : 0;
        heardrec *h = heard_note(src, sport, p + 8, 24, works, mib);
        if (len >= 40) h->up_min = (u16)(p[34] | (p[35] << 8));

        if (len >= 96) {
            bool any = false;
            for (u32 i = 0; i < 32; i++) if (p[40 + i]) any = true;
            if (any) { h->has_key = true; memcpy(h->key, p + 40, 32); }
            u32 n = 0;
            while (n < 23 && p[72 + n]) {
                char c = (char)p[72 + n];
                h->version[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
                n++;
            }
            h->version[n] = 0;
        }
        if (len >= 97) {
            u32 n = p[96];
            if (n > 4) n = 4;
            if (97 + 6 * n <= len)
                for (u32 i = 0; i < n; i++)
                    gossip_consider(p + 97 + i * 6,
                                    (u16)(p[101 + i * 6] | (p[102 + i * 6] << 8)));
        }
        if (kind == K_SEEK) say_who(K_HERE, src, sport);

        /* A known key claimed in a HERE: from the row's own address the
         * version is updated; from another address a handshake is
         * started, and a verified answer moves the row. */
        if (h->has_key) {
            nodes_apply();
            i32 i = nodes_by_key(h->key);
            if (i >= 0) {
                u8 rip[4];
                u16 rp;
                bool has = nodes_address_at((u32)i, rip, &rp);
                if (has && ip4_same(rip, src))
                    nodes_meet(h->name, h->key, src, h->port, h->version, false);
                else if (!seal_by_ip(src) && !knock.active && net_crypto_ok())
                    knock_begin(src, h->port, i);
            }
        }
        return;
    }

    if (kind == K_ROTATE && len >= 200) {
        /* A node renewing its key: the packet carries the old public key
         * and the new, each signed over both. We honour it only for a
         * node we already hold under the old key -- a stranger cannot
         * rotate anyone -- and both signatures must check. */
        const u8 *oldp = p + 8, *newp = p + 40, *so = p + 72, *sn = p + 136;
        u8 msg[80];
        rotate_message(msg, oldp, newp);
        if (!ed25519_verify(oldp, msg, sizeof(msg), so)) return;
        if (!ed25519_verify(newp, msg, sizeof(msg), sn)) return;
        nodes_apply();
        i32 i = nodes_by_key(oldp);
        if (i >= 0) nodes_rekey((u32)i, newp);
        return;
    }

    if (kind == K_VOUCH && len >= 166) {
        /* A node vouching for another's key. Honoured only from a node
         * we already hold and have marked 'may vouch' -- a stranger's
         * word pins nothing -- and the signature must check against the
         * voucher's key carried in the packet. The vouchee's key is
         * then pinned before we meet it, exactly as 'trust' does by
         * hand; no rights ride along, only recognition. */
        const u8 *vk = p + 8, *ek = p + 40, *ip = p + 72;
        u16 eport = (u16)p[76] | ((u16)p[77] << 8);
        char name[24];
        for (u32 k = 0; k < 24; k++) name[k] = (char)p[78 + k];
        name[23] = 0;
        const u8 *sig = p + 102;

        u8 msg[110];
        vouch_message(msg, vk, ek, ip, eport, name);
        if (!ed25519_verify(vk, msg, sizeof(msg), sig)) return;

        nodes_apply();
        i32 vi = nodes_by_key(vk);
        if (vi < 0 || !(nodes_may_at((u32)vi) & NODE_MAY_VOUCH)) return;
        if (nodes_by_key(ek) >= 0) return;         /* already known */

        bool has = (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
        if (nodes_meet(name, ek, has ? ip : NULL, eport, NULL, true) < 0) return;

        char vname[24];
        nodes_name_at((u32)vi, vname);
        char fp[64];
        ssh_fingerprint_of(ek, fp);
        kprintf("pipe: '%s' vouches for '%s'; key %s pinned before meeting\n",
                vname, name, fp);
        char line[80];
        u32 at = 0;
        for (u32 k = 0; vname[k] && at < 24; k++) line[at++] = vname[k];
        at = put(line, at, " vouches for ");
        for (u32 k = 0; name[k] && at < sizeof(line) - 16; k++) line[at++] = name[k];
        at = put(line, at, "; its key is pinned");
        line[at] = 0;
        journal_says("pipe", line);
        return;
    }

    if (kind == K_HELLO && len >= 44) {
        if (!net_crypto_ok()) return;
        u32 sid = rd32(p + 8);

        sealrec *s = seal_find(src, sid);
        if (s && !s->we_knocked) {
            net_udp_send(src, PIPE_PORT, sport, s->answer, s->answer_len);
            return;
        }

        const u8 *idkey = NULL;
        if (len >= KNOCK_SIGNED) {
            u8 msg[46];
            hello_message(msg, sid, p + 12);
            if (!ed25519_verify(p + 44, msg, sizeof(msg), p + 76)) {
                kprintf("pipe: the handshake from %u.%u.%u.%u has an invalid signature; ignored\n",
                        src[0], src[1], src[2], src[3]);
                return;
            }
            idkey = p + 44;
            knock_claims(src, sport ? sport : PIPE_PORT, p, len);
        }
        if (identity_verdict(src, sport ? sport : PIPE_PORT, idkey) < 0) return;

        u8 priv[32], pub[32], shared[32];
        rand_bytes(priv, 32);
        x25519_base(pub, priv);
        x25519(shared, priv, p + 12);

        s = seal_slot_for(src);
        for (u32 i = 0; i < 4; i++) s->ip[i] = src[i];
        s->port = sport ? sport : PIPE_PORT;
        s->sid = sid;
        s->proven = idkey != NULL;
        if (idkey) memcpy(s->idkey, idkey, 32); else memset(s->idkey, 0, 32);
        memcpy(s->my_pub, pub, 32);
        seal_derive(s, false, p + 12, pub, shared);
        memset(priv, 0, 32);
        memset(shared, 0, 32);

        s->answer_len = build_welcome(s->answer, sid, p + 12, pub);
        net_udp_send(src, PIPE_PORT, sport, s->answer, s->answer_len);

        kprintf("pipe: session with %u.%u.%u.%u, %s\n",
                s->ip[0], s->ip[1], s->ip[2], s->ip[3],
                s->proven ? "proven" : "unproven");
        return;
    }

    if (kind == K_WELCOME && len >= 44) {
        if (!knock.active) return;
        if (rd32(p + 8) != knock.sid) return;
        if (!ip4_same(src, knock.ip)) return;

        const u8 *idkey = NULL;
        if (len >= KNOCK_SIGNED) {
            u8 msg[80];
            welcome_message(msg, knock.sid, knock.pub, p + 12);
            if (!ed25519_verify(p + 44, msg, sizeof(msg), p + 76)) {
                kprintf("pipe: the answer from %u.%u.%u.%u has an invalid signature; ignored\n",
                        src[0], src[1], src[2], src[3]);
                return;
            }
            idkey = p + 44;
            knock_claims(src, knock.port, p, len);
        }
        i32 verdict = identity_verdict(src, knock.port, idkey);

        /* The handshake targeted a specific row: a different key, even
         * a known one, is rejected. */
        if (verdict >= 0 && knock.expect >= 0) {
            u8 ek[32];
            if (!idkey || !nodes_key_at((u32)knock.expect, ek) || memcmp(ek, idkey, 32) != 0) {
                char nm[24];
                if (!nodes_name_at((u32)knock.expect, nm)) nm[0] = 0;
                kprintf("pipe: the machine at %u.%u.%u.%u is not %s; transfer cancelled\n",
                        src[0], src[1], src[2], src[3], nm);
                journal_says("pipe", "the answering machine is not the intended node; nothing was sent");
                verdict = -1;
            }
        }
        if (verdict < 0) {
            knock.active = false;
            memset(knock.priv, 0, 32);
            if (out.busy && ip4_same(out.to, knock.ip)) {
                out_end(false);
                journal_says("pipe", "nothing was sent");
            }
            kprintf("pipe: nothing was sent to %u.%u.%u.%u\n",
                    knock.ip[0], knock.ip[1], knock.ip[2], knock.ip[3]);
            return;
        }

        u8 shared[32];
        x25519(shared, knock.priv, p + 12);

        sealrec *s = seal_slot_for(src);
        for (u32 i = 0; i < 4; i++) s->ip[i] = src[i];
        s->port = knock.port;
        s->sid = knock.sid;
        s->proven = idkey != NULL;
        if (idkey) memcpy(s->idkey, idkey, 32); else memset(s->idkey, 0, 32);
        memcpy(s->my_pub, knock.pub, 32);
        seal_derive(s, true, knock.pub, p + 12, shared);
        memset(shared, 0, 32);
        memset(knock.priv, 0, 32);
        knock.active = false;

        kprintf("pipe: session with %u.%u.%u.%u, %s\n",
                s->ip[0], s->ip[1], s->ip[2], s->ip[3],
                s->proven ? "proven" : "unproven");
        journal_says("pipe", !s->proven ? "session opened; the machine sent no identity"
                             : verdict == 1 ? "session opened; first handshake, key remembered"
                                            : "session opened with a known node");
        return;
    }

    if (kind == K_SEALED && len >= 36) {
        u32 sid = rd32(p + 8);
        u64 ctr = rd64(p + 12);

        sealrec *s = seal_find(src, sid);
        if (!s) return;
        if (ctr <= s->ctr_in_seen) return;

        static u8 inner[INNER_MAX];
        u32 ilen = len - 36;
        if (ilen == 0 || ilen > INNER_MAX) return;

        u8 nonce[12];
        seal_nonce(nonce, s->iv_in, ctr);
        if (!aes128_gcm_open(s->key_in, nonce, p, 20,
                             p + 20, ilen, p + 20 + ilen, inner))
            return;

        s->ctr_in_seen = ctr;
        s->last_ns = time_ns();
        heardrec *h = heard_by_ip(src);
        if (h) h->seen_ns = s->last_ns;
        inner_input(src, sport, s, inner, ilen);
        return;
    }

    if (kind == K_OFFER || kind == K_CHUNK || kind == K_TAKEN)
        kprintf("pipe: an unencrypted offer was ignored\n");
}

/* ------------------------------------------------------------------ */
/* The network page                                                    */
/* ------------------------------------------------------------------ */

static object *page;
static u64 page_ns;

void pipe_page_set(object *t)
{
    if (page) obj_release(page);
    page = t;
    if (page) obj_retain(page);
}

/* The door-key object, held so a renewal can write a fresh pair into it
 * and have it saved with the graph. */
static object *door_key_obj;

void pipe_door_key_set(object *t)
{
    if (door_key_obj) obj_release(door_key_obj);
    door_key_obj = t;
    if (door_key_obj) obj_retain(door_key_obj);
}

/* The message both keys sign in a rotation: a fixed label so the
 * signature cannot be mistaken for any other, then the old and new
 * public keys. */
static void rotate_message(u8 m[80], const u8 oldp[32], const u8 newp[32])
{
    static const char label[] = "erebus rotate v1";   /* 16 letters */
    memcpy(m, label, 16);
    memcpy(m + 16, oldp, 32);
    memcpy(m + 48, newp, 32);
}

static void rotate_send(const u8 dst[4], u16 dport, const u8 oldp[32],
                        const u8 newp[32], const u8 so[64], const u8 sn[64])
{
    u8 pkt[200];
    memset(pkt, 0, sizeof(pkt));
    wr32(pkt, MAGIC);
    pkt[4] = K_ROTATE;
    memcpy(pkt + 8, oldp, 32);
    memcpy(pkt + 40, newp, 32);
    memcpy(pkt + 72, so, 64);
    memcpy(pkt + 136, sn, 64);
    net_udp_send(dst, PIPE_PORT, dport, pkt, 200);
}

bool pipe_renew_key(void)
{
    if (!door_key_obj || obj_size(door_key_obj) < 64) {
        journal_says("pipe", "there is no door key to renew");
        return false;
    }
    u8 oldk[64], newk[64];
    if (!ssh_key_bytes(oldk)) return false;
    ssh_make_key(newk);

    u8 msg[80];
    rotate_message(msg, oldk + 32, newk + 32);
    u8 so[64], sn[64];
    ed25519_sign(so, oldk, oldk + 32, msg, sizeof(msg));
    ed25519_sign(sn, newk, newk + 32, msg, sizeof(msg));

    /* Install the new pair: written into the graph's door-key object and
     * given to ssh, so the door and the pipe both speak with it now. */
    memcpy(obj_data(door_key_obj), newk, 64);
    obj_touch(door_key_obj);
    ssh_init(newk);

    /* Tell every node we know: each announcement carries the old key and
     * the new, one vouching for the other, so the far side can move its
     * row without meeting us afresh. A node that does not hold the old
     * key ignores it and will meet the new key on its own later. */
    nodes_apply();
    u32 sent = 0;
    for (u32 i = 0; i < nodes_count(); i++) {
        u8 ip[4]; u16 port;
        if (nodes_address_at(i, ip, &port)) {
            rotate_send(ip, port, oldk + 32, newk + 32, so, sn);
            sent++;
        }
    }
    char fp[64];
    ssh_fingerprint(fp);
    kprintf("pipe: the door key is renewed; now %s; told %u node(s)\n", fp, sent);
    journal_says("pipe", "the door key was renewed; known nodes are told, "
                         "each announcement signed with the old key and the new");
    memset(oldk, 0, 64);
    memset(newk, 0, 64);
    return true;
}

/* The message a vouch signs: a fixed label, then the voucher's public
 * key, the vouchee's key, its address and name. A signature over this
 * cannot be lifted onto any other statement. */
static void vouch_message(u8 m[110], const u8 vk[32], const u8 ek[32],
                          const u8 ip[4], u16 port, const char name[24])
{
    static const char label[] = "erebus vouch v1";   /* 15 + NUL = 16 */
    memset(m, 0, 110);
    memcpy(m, label, 16);
    memcpy(m + 16, vk, 32);
    memcpy(m + 48, ek, 32);
    memcpy(m + 80, ip, 4);
    m[84] = (u8)(port & 0xFF);
    m[85] = (u8)(port >> 8);
    for (u32 i = 0; i < 24; i++) m[86 + i] = (u8)name[i];
}

static void vouch_send(const u8 dst[4], u16 dport, const u8 vk[32],
                       const u8 ek[32], const u8 ip[4], u16 eport,
                       const char name[24], const u8 sig[64])
{
    u8 pkt[176];
    memset(pkt, 0, sizeof(pkt));
    wr32(pkt, MAGIC);
    pkt[4] = K_VOUCH;
    memcpy(pkt + 8, vk, 32);
    memcpy(pkt + 40, ek, 32);
    memcpy(pkt + 72, ip, 4);
    pkt[76] = (u8)(eport & 0xFF);
    pkt[77] = (u8)(eport >> 8);
    for (u32 i = 0; i < 24; i++) pkt[78 + i] = (u8)name[i];
    memcpy(pkt + 102, sig, 64);
    net_udp_send(dst, PIPE_PORT, dport, pkt, 166);
}

/* Vouch for a node: sign a statement that its key is one we recognise
 * and send it to every other known node. A node that has marked this
 * machine 'vouch' pins the key before it ever meets it. */
bool pipe_vouch(u32 node)
{
    nodes_apply();
    if (node >= nodes_count()) return false;

    u8 ek[32], vk64[64];
    char name[24];
    if (!nodes_key_at(node, ek)) return false;
    nodes_name_at(node, name);
    u8 eip[4]; u16 eport;
    if (!nodes_address_at(node, eip, &eport)) { memset(eip, 0, 4); eport = 0; }

    if (!ssh_key_bytes(vk64)) {
        journal_says("pipe", "there is no door key to vouch with");
        return false;
    }

    u8 msg[110];
    vouch_message(msg, vk64 + 32, ek, eip, eport, name);
    u8 sig[64];
    ed25519_sign(sig, vk64, vk64 + 32, msg, sizeof(msg));

    u32 sent = 0;
    for (u32 i = 0; i < nodes_count(); i++) {
        if (i == node) continue;               /* the vouchee holds its own key */
        u8 ip[4]; u16 port;
        if (nodes_address_at(i, ip, &port)) {
            vouch_send(ip, port, vk64 + 32, ek, eip, eport, name, sig);
            sent++;
        }
    }
    char line[64];
    u32 at = put(line, 0, "vouched for ");
    for (u32 i = 0; name[i] && at < 40; i++) line[at++] = name[i];
    at = put(line, at, " to ");
    at = put_dec(line, at, sent);
    at = put(line, at, " node(s)");
    line[at] = 0;
    journal_says("pipe", line);
    memset(vk64, 0, 64);
    return true;
}

static void page_write(void)
{
    if (!page) return;
    u8 *d = (u8 *)obj_data(page);
    u64 size = obj_size(page);
    if (!d || size < 512) return;

    static char buf[4096];
    u64 now = time_ns();
    u32 at = put(buf, 0, "node          address            version         seen     free    up      work  seal\n");

    nodes_apply();
    u32 n = nodes_count();
    for (u32 i = 0; i < n && at + 200 < sizeof(buf); i++) {
        char nm[24], ver[24];
        u8 ip[4];
        u16 port;
        nodes_name_at(i, nm);
        nodes_version_at(i, ver);
        bool has = nodes_address_at(i, ip, &port);
        at = put_pad(buf, at, nm, 14);
        u32 col = at;
        if (has) { at = put_ip(buf, at, ip); buf[at++] = ' '; at = put_dec(buf, at, port); }
        else buf[at++] = '-';
        while (at < col + 19) buf[at++] = ' ';
        at = put_pad(buf, at, ver[0] ? ver : "-", 16);
        heardrec *h = has ? heard_by_ip(ip) : NULL;
        col = at;
        if (h && now - h->seen_ns < QUIET_S * SECOND) { at = put_dec(buf, at, (now - h->seen_ns) / SECOND); buf[at++] = 's'; }
        else at = put(buf, at, h ? "quiet" : "-");
        while (at < col + 9) buf[at++] = ' ';
        col = at;
        if (h) { at = put_dec(buf, at, h->free_mib); buf[at++] = 'M'; } else buf[at++] = '-';
        while (at < col + 8) buf[at++] = ' ';
        col = at;
        if (h && h->up_min) {
            if (h->up_min >= 60) { at = put_dec(buf, at, h->up_min / 60); buf[at++] = 'h'; }
            at = put_dec(buf, at, h->up_min % 60); buf[at++] = 'm';
        } else buf[at++] = '-';
        while (at < col + 8) buf[at++] = ' ';
        at = put_pad(buf, at, h ? (h->works ? "yes" : "no") : "-", 6);
        sealrec *s = has ? seal_by_ip(ip) : NULL;
        at = put(buf, at, s ? (s->proven ? "proven" : "sealed") : "-");
        buf[at++] = '\n';
    }
    if (n == 0) at = put(buf, at, "(no node met yet)\n");

    bool head = false;
    for (u32 k = 0; k < HEARD_MAX && at + 120 < sizeof(buf); k++) {
        heardrec *h = &heard[k];
        if (!h->used || now - h->seen_ns > QUIET_S * SECOND) continue;
        if (h->has_key && nodes_by_key(h->key) >= 0) continue;
        if (nodes_by_address(h->ip) >= 0) continue;
        if (!head) { at = put(buf, at, "\nheard, not authenticated:\n"); head = true; }
        at = put(buf, at, "  ");
        u32 col = at;
        at = put_ip(buf, at, h->ip);
        buf[at++] = ' ';
        at = put_dec(buf, at, h->port);
        while (at < col + 21) buf[at++] = ' ';
        at = put_pad(buf, at, h->name[0] ? h->name : "(no name)", 14);
        at = put_pad(buf, at, h->version[0] ? h->version : "-", 16);
        at = put_dec(buf, at, (now - h->seen_ns) / SECOND);
        at = put(buf, at, "s ago");
        if (h->works) { at = put(buf, at, ", takes work, "); at = put_dec(buf, at, h->free_mib); at = put(buf, at, "M free"); }
        buf[at++] = '\n';
    }

    at = put(buf, at, "\n");
    u32 queued = 0;
    for (u32 i = 0; i < DESK_JOBS; i++) if (desk[i].used) queued++;
    at = put(buf, at, "desk      ");
    at = put_dec(buf, at, queued);
    at = put(buf, at, queued == 1 ? " job\n" : " jobs\n");
    at = put(buf, at, "working   ");
    if (workj.active) { char who[24]; name_of(workj.from, seal_by_ip(workj.from), who); at = put(buf, at, "for "); at = put(buf, at, who); }
    else at = put(buf, at, "for nobody");
    buf[at++] = '\n';
    at = put(buf, at, "carrying  ");
    if (out.busy) {
        at = put(buf, at, out.name[0] ? out.name : "something");
        at = put(buf, at, " to ");
        char who[24];
        name_of(out.to, NULL, who);
        at = put(buf, at, who);
        at = put(buf, at, ": ");
        at = put_dec(buf, at, out.acked);
        at = put(buf, at, " of ");
        at = put_dec(buf, at, out.len);
        at = put(buf, at, " bytes");
    } else if (in.active) {
        at = put(buf, at, "in: ");
        at = put_dec(buf, at, in.have);
        at = put(buf, at, " of ");
        at = put_dec(buf, at, in.total);
        at = put(buf, at, " bytes");
    } else {
        at = put(buf, at, "nothing");
    }
    buf[at++] = '\n';

    if ((u64)at + 1 > size) at = (u32)size - 1;
    if (memcmp(d, buf, at) == 0 && d[at] == 0) return;
    memcpy(d, buf, at);
    memset(d + at, 0, size - at);
    obj_touch(page);
}

/* ------------------------------------------------------------------ */
/* The carrying                                                        */
/* ------------------------------------------------------------------ */

static void carry(void)
{
    if (!out.busy) return;
    u64 now = time_ns();
    char who[24];

    if (out.taken) {
        name_of(out.to, seal_by_ip(out.to), who);
        if (out.taken_status == T_TAKEN) {
            kprintf("pipe: carried %u bytes across, sealed\n", out.len);
            if (out.kind == W_KERNEL) {
                char line[72];
                u32 at = put(line, 0, "node ");
                at = put(line, at, who);
                at = put(line, at, " took the kernel; it installs it and restarts");
                line[at] = 0;
                journal_says("pipe", line);
            } else if (out.work_job >= 0) {
                journal_says("pipe", "the work input was delivered");
            } else {
                journal_says("pipe", "transfer complete");
            }
            out_end(true);
        } else if (out.taken_status == T_BUSY) {
            out.taken = false;
            out.offered = false;
            if (++out.busy_tries > 10) {
                journal_says("pipe", "the far side stayed busy; nothing was sent");
                kprintf("pipe: %s stayed busy\n", who);
                out_end(false);
            } else {
                out.last_progress_ns = now + 2 * SECOND;   /* wait before the next offer */
            }
        } else {
            kprintf("pipe: %s declined it (%s)\n", who, refusal_words(out.taken_reason));
            char line[80];
            u32 at = put(line, 0, "node ");
            at = put(line, at, who);
            at = put(line, at, " declined: ");
            at = put(line, at, refusal_words(out.taken_reason));
            line[at] = 0;
            journal_says("pipe", line);
            out_end(false);
        }
        return;
    }

    sealrec *s = seal_by_ip(out.to);
    if (!s) {
        knock_begin(out.to, out.to_port, out.node);
        return;                              /* the handshake has its own timeout */
    }

    if (!out.offered) {
        if (now < out.last_progress_ns) return;    /* holding off after a busy answer */
        out.acked = out.sent = 0;
        out.stalls = 0;
        send_offer(s);
        out.offered = true;
        out.last_progress_ns = now;
        return;
    }

    if (now - out.last_progress_ns > 700 * MS) {
        if (++out.stalls > 20) {
            name_of(out.to, s, who);
            journal_says("pipe", "the transfer stalled; cancelled");
            kprintf("pipe: %s went quiet after %u of %u bytes\n", who, out.acked, out.len);
            out_end(false);
            return;
        }
        out.sent = out.acked;
        out.last_progress_ns = now;
        send_offer(s);                       /* the receiver answers with its position */
    }

    u32 burst = 0;
    while (out.sent < out.len &&
           out.sent < out.acked + WINDOW_CHUNKS * CHUNK_MAX &&
           burst < WINDOW_CHUNKS) {
        u32 dlen = out.len - out.sent < CHUNK_MAX ? out.len - out.sent : CHUNK_MAX;
        send_chunk(s, out.sent);
        out.sent += dlen;
        burst++;
    }
}

/* ------------------------------------------------------------------ */

void pipe_service(void)
{
    u64 now = time_ns();

    if (now < scan_until_ns && now - scan_last_call_ns > SECOND) {
        scan_last_call_ns = now;
        static const u8 everyone[4] = { 255, 255, 255, 255 };
        say_who(K_SEEK, everyone, PIPE_PORT);

        u8 peer[4];
        u16 pp;
        if (settings_peer(peer, &pp)) say_who(K_SEEK, peer, pp);
    }

    /* Heartbeat: a SEEK to every node with an address. Keeps the
     * network page current and finds nodes that changed address. */
    static u64 beat_ns;
    if (now - beat_ns > BEAT_S * SECOND) {
        beat_ns = now;
        nodes_apply();
        for (u32 i = 0; i < nodes_count(); i++) {
            u8 ip[4];
            u16 port;
            if (nodes_address_at(i, ip, &port)) say_who(K_SEEK, ip, port);
        }
        u8 peer[4];
        u16 pp;
        if (settings_peer(peer, &pp) && nodes_by_address(peer) < 0) say_who(K_SEEK, peer, pp);

        /* A known node heard once and then not for a while has gone
         * quiet: say so once, so a fleet's silence is noticed rather
         * than merely shown. Coming back is said in heard_note. */
        for (u32 i = 0; i < HEARD_MAX; i++) {
            heardrec *h = &heard[i];
            if (!h->used || h->quiet_said) continue;
            if (now - h->seen_ns <= QUIET_S * SECOND) continue;
            i32 row = nodes_by_address(h->ip);
            if (row < 0) continue;
            char nm[24], line[64];
            nodes_name_at((u32)row, nm);
            u32 at = put(line, 0, "node ");
            at = put(line, at, nm);
            at = put(line, at, " went quiet");
            line[at] = 0;
            attention_note("pipe", line);
            kprintf("pipe: %s\n", line);
            h->quiet_said = true;
        }
    }

    for (u32 i = 0; i < SEAL_MAX; i++)
        if (seal[i].used && now - seal[i].last_ns > 120 * SECOND)
            seal[i].used = false;

    if (in.active && now - in.started_ns > 60 * SECOND &&
        now - in.last_have_ns > 15 * SECOND)
        in_drop();

    for (u32 i = 0; i < INPUTS_MAX; i++)
        if (inputs[i].used && now - inputs[i].ns > 300 * SECOND) {
            if (inputs[i].obj) obj_release(inputs[i].obj);
            inputs[i].obj = NULL;
            inputs[i].used = false;
        }

    if (knock.active) {
        if (!knock.last_ns || now - knock.last_ns >= 2 * SECOND) {
            if (knock.tries >= 3) {
                knock.active = false;
                if (out.busy && ip4_same(out.to, knock.ip)) {
                    journal_says("pipe", "no answer to the handshake");
                    kprintf("pipe: the handshake went unanswered\n");
                    out_end(false);
                }
            } else {
                knock.tries++;
                knock.last_ns = now;
                knock_send();
            }
        }
    }

    if (saypend.active) {
        bool door = false;
        for (u32 i = 0; i < SEAL_MAX; i++)
            if (seal[i].used) door = true;
        if (door) {
            saypend.active = false;
            say_wire(saypend.text);
        } else if (now - saypend.born_ns > 10 * SECOND) {
            saypend.active = false;
            journal_says("pipe", "no session for the line");
        }
    }

    if (workj.active) {
        message m;
        u8 text[24];
        u8 status = 0xFF;
        memset(text, 0, 24);

        if (pipe_kdom && port_try_receive(pipe_kdom, workj.recv, &m)) {
            if (m.ncaps > 0) {
                cap_revoke(pipe_kdom, m.caps[0]);
                if (m.ncaps > 1) cap_revoke(pipe_kdom, m.caps[1]);
            }
            if (m.tag == 0x54584554ULL) {          /* "TEXT" */
                for (u32 i = 0; i < 24; i++)
                    text[i] = (u8)((m.words[i / 8] >> ((i % 8) * 8))
                                   & 0xFF);
                status = A_OK;
            }
        } else if (!proc_is_running(workj.prog)) {
            u64 gone = (time_ns() - workj.started_ns) / SECOND;
            status = (gone + 1 >= workj.budget_s) ? A_LATE : A_SILENT;
        } else if (time_ns() - workj.started_ns >
                   (workj.budget_s + 5) * SECOND) {
            status = A_LATE;
            /* Still running past its deadline: a compiled job can spin
             * without ever reaching a syscall, so the interpreter's own
             * budget does not reach it. End the process; the scheduler
             * retires it at its next preemption. */
            proc_end(workj.prog);
        }

        if (status != 0xFF) {
            sealrec *s = seal_by_ip(workj.from);
            if (s) answer_send(s, workj.id, status, text);

            gave.valid = true;
            gave.id = workj.id;
            gave.status = status;
            memcpy(gave.text, text, 24);

            cap_revoke(pipe_kdom, workj.recv);
            obj_release(workj.port);
            obj_release(workj.prog);
            workj.active = false;

            kprintf("pipe: the job is answered (status %u)\n", status);
            journal_says("pipe", status == A_OK
                         ? "the job is done and answered"
                         : "the job ended without a result");
        }
    }

    {
        desk_job *j = NULL;
        for (u32 i = 0; i < DESK_JOBS; i++)
            if (desk[i].used) { j = &desk[i]; break; }

        if (j && j->state == DJ_FRESH) {
            j->deadline_ns = now +
                (60 + (u64)j->parts * (ASK_BUDGET_S + 10)) * SECOND;
            if (j->parts == 1) {
                u8 peer[4];
                u16 pp;
                i32 node;
                if (!target_resolve(peer, &pp, &node)) {
                    job_end(j, false, NULL,
                            "no peer is named in the settings");
                    j = NULL;
                } else {
                    memcpy(j->cand[0], peer, 4);
                    j->cand_port[0] = pp;
                    j->cand_count = 1;
                    j->state = DJ_RUN;
                }
            } else {
                pipe_scan();
                j->scan_until_ns = now + 4 * SECOND;
                j->state = DJ_SCAN;
            }
        }

        if (j && j->state == DJ_SCAN && now >= j->scan_until_ns) {
            j->cand_count = 0;
            u32 nf = pipe_found_count();
            for (u32 i = 0; i < nf && j->cand_count < CAND_MAX; i++) {
                u8 fip[4];
                char nm[24];
                bool wk;
                if (!pipe_found_at(i, fip, nm, &wk, NULL)) break;
                if (!wk) continue;
                memcpy(j->cand[j->cand_count], fip, 4);
                j->cand_port[j->cand_count] = PIPE_PORT;
                j->cand_count++;
            }
            if (j->cand_count == 0) {
                u8 peer[4];
                u16 pp;
                i32 node;
                if (target_resolve(peer, &pp, &node)) {
                    memcpy(j->cand[0], peer, 4);
                    j->cand_port[0] = pp;
                    j->cand_count = 1;
                }
            }
            if (j->cand_count == 0) {
                job_end(j, false, NULL, "no machine accepts work");
                j = NULL;
            } else if (j->quorum && j->cand_count < j->quorum) {
                char line[64];
                u32 at = put(line, 0, "not enough machines for a quorum of ");
                at = put_dec(line, at, j->quorum);
                at = put(line, at, " (");
                at = put_dec(line, at, j->cand_count);
                at = put(line, at, " answered)");
                line[at] = 0;
                job_end(j, false, NULL, line);
                j = NULL;
            } else {
                char line[48];
                u32 at = put(line, 0, "the desk deals to ");
                at = put_dec(line, at, j->cand_count);
                at = put(line, at, j->cand_count == 1 ? " machine"
                                                      : " machines");
                line[at] = 0;
                journal_says("pipe", line);
                j->state = DJ_RUN;
            }
        }

        if (j && j->state == DJ_RUN) {
            if (now > j->deadline_ns) {
                job_end(j, false, NULL,
                        "the job timed out");
            } else {
                for (u32 pi = 0; pi < j->parts; pi++) {
                    if (j->pstate[pi] != P_PEND) continue;
                    if (now < j->pwait_ns[pi]) continue;
                    if (j->cand_count == 0) break;

                    u32 fi = FASK_MAX;
                    for (u32 i = 0; i < FASK_MAX; i++)
                        if (!fask[i].active) { fi = i; break; }
                    if (fi == FASK_MAX) break;

                    /* A quorum wants each part on a distinct machine, so
                     * part i goes to candidate i; an ordinary job deals
                     * round-robin. */
                    u32 c = j->quorum ? pi : (j->next_cand % j->cand_count);
                    if (c >= j->cand_count) break;

                    /* The input is transferred before a machine's first
                     * part; a machine the input cannot reach gets no
                     * part. */
                    if (j->input && j->istate[c] != 2) {
                        if (j->istate[c] == 3) {
                            cand_strike(j, c);
                            if (j->cand_count == 0) {
                                job_end(j, false, NULL, "the input could not be delivered");
                                break;
                            }
                            continue;
                        }
                        if (j->istate[c] == 0 && !out.busy &&
                            out_begin(j->input_kind, obj_name(j->input), j->input, NULL,
                                      j->input_len, j->cand[c], j->cand_port[c], -1)) {
                            out.work_job = (i32)(j - desk);
                            out.work_cand = c;
                            j->istate[c] = 1;
                        }
                        break;               /* until it is there */
                    }
                    j->next_cand++;
                    fask[fi].active = true;
                    memcpy(fask[fi].ip, j->cand[c], 4);
                    fask[fi].port = j->cand_port[c];
                    rand_bytes((u8 *)&fask[fi].id, 8);
                    fask[fi].job = (u32)(j - desk);
                    fask[fi].part = pi;
                    fask[fi].tries = 0;
                    fask[fi].last_ns = 0;
                    fask[fi].started_ns = now;
                    j->pstate[pi] = P_RUN;
                }
            }
        }
    }

    for (u32 i = 0; i < FASK_MAX; i++) {
        if (!fask[i].active) continue;
        desk_job *j = &desk[fask[i].job];
        if (!j->used) { fask[i].active = false; continue; }
        bool give_up = false;

        sealrec *s = seal_by_ip(fask[i].ip);
        if (!s) {
            knock_begin(fask[i].ip, fask[i].port, -1);
            give_up = (now - fask[i].started_ns > 8 * SECOND);
        } else if (now - fask[i].started_ns >
                   (u64)(ASK_BUDGET_S + 10) * SECOND) {
            give_up = true;
        } else if (fask[i].tries < 3 &&
                   (!fask[i].last_ns ||
                    now - fask[i].last_ns >= 2 * SECOND)) {
            fask[i].tries++;
            fask[i].last_ns = now;
            fask_send(s, i);
        }

        if (give_up) {
            u32 part = fask[i].part;
            fask[i].active = false;
            j->ptries[part]++;
            if (j->ptries[part] >= 2) {
                job_end(j, false, NULL, "no answer came");
            } else {
                j->pstate[part] = P_PEND;
                j->pwait_ns[part] = now + SECOND;
            }
        }
    }

    /* 'update all': the next queued node when no transfer is in flight. */
    if (!out.busy && upq.at < upq.n) {
        char why[120];
        i32 row = upq.rows[upq.at++];
        if (!update_start(row, upq.image, why, sizeof(why))) {
            char nm[24];
            if (!nodes_name_at((u32)row, nm)) nm[0] = 0;
            kprintf("pipe: %s is skipped: %s\n", nm, why);
        }
        if (upq.at >= upq.n && upq.image) { obj_release(upq.image); upq.image = NULL; }
    }

    carry();

    if (restart_at_ns && now >= restart_at_ns) {
        restart_at_ns = 0;
        kprintf("pipe: restarting into the kernel that came through the pipe\n");
        system_restart();
    }

    if (now - page_ns > 2 * SECOND) {
        page_ns = now;
        page_write();
    }
}
