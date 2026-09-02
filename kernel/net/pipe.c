/*
 * pipe.c -- objects crossing between machines, sealed.
 *
 * The wire format is small datagrams over udp. Discovery speaks
 * plainly: a SEEK asks who is there, a HERE answers with a name --
 * names are claims either way, so there is nothing in them to
 * protect. Everything that carries substance travels sealed.
 *
 * The seal is a knock: HELLO carries a fresh x25519 public key and a
 * session number, WELCOME answers with the other side's fresh key,
 * and both ends derive AES-128-GCM keys from the shared secret --
 * one key per direction, a counter per record, replay refused. From
 * then on OFFER, CHUNK and TAKEN ride inside SEALED envelopes; a
 * plain one arriving is turned away. The honest limit, same as the
 * one https carries here: the knock proves privacy against the road,
 * not the identity of who answered it. Keys are fresh at every
 * knock and kept nowhere.
 *
 * An OFFER names the transfer: what kind of thing, what it calls
 * itself, how many bytes. CHUNKs carry the payload in order. When
 * the last byte is in, the receiver answers TAKEN, and the sender
 * stops worrying. No answer means the whole offer is made again, a
 * few times, and then the journal says so; a pipe that loses
 * something silently would be worse than no pipe.
 *
 * Only three kinds cross: text, bytes, pictures. The kinds are named
 * on the wire by their own small numbers rather than by this kernel's
 * type ids, so two machines of different ages still understand one
 * another. Nothing that runs and nothing that grants can be sent,
 * because the wire carries substance, never authority.
 */
#include <eb/pipe.h>
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

#define CHUNK_MAX 1024
#define CARRY_MAX_BYTES 65536
#define INNER_MAX (40 + CHUNK_MAX)     /* the widest inner packet: an ask */

/* Far work. A recipe fits one sealed datagram; the budget is what the
 * asking side grants and the interpreter over there enforces. */
#define RECIPE_MAX   1024
#define ASK_BUDGET_S 20

/* A spoken word: short enough to fit one sealed datagram with room
 * to spare, long enough for a sentence worth saying. */
#define SAY_MAX 200

/* The desk: how many jobs may queue, how many parts a job may be
 * divided into, how many asks fly at once, how many machines are
 * remembered as candidates. */
#define DESK_JOBS 4
#define PART_MAX  8
#define FASK_MAX  4
#define CAND_MAX  8

/* What an answer says about its job. */
#define A_OK     0                   /* here is the result */
#define A_NOWORK 1                   /* this machine takes no work */
#define A_LATE   2                   /* it ran out of time */
#define A_BUSY   3                   /* already working on something */
#define A_SILENT 4                   /* it ended without answering */

/* Wire kinds, deliberately not the kernel's type ids. */
#define W_TEXT    1
#define W_BYTES   2
#define W_PICTURE 3

#define SECOND 1000000000ULL

static object *arrivals;

void pipe_arrivals_set(object *list)
{
    if (arrivals) obj_release(arrivals);
    arrivals = list;
    if (arrivals) obj_retain(arrivals);
}

object *pipe_arrivals(void) { return arrivals; }

/* ------------------------------------------------------------------ */
/* The line                                                            */
/* ------------------------------------------------------------------ */

/* One running conversation, kept as an ordinary text: the kernel
 * appends what is said, in the order it was said, and the person
 * holds it read-only -- a talk is a record too. */
static object *line_obj;

void pipe_line_set(object *t)
{
    if (line_obj) obj_release(line_obj);
    line_obj = t;
    if (line_obj) obj_retain(line_obj);
}

object *pipe_line(void) { return line_obj; }

/* Words already heard, so a datagram that arrives twice does not
 * stand on the line twice. */
static u64 say_heard[8];
static u32 say_heard_at;

/* A word waiting for a seal that is still being knocked for. */
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

    /* A full page: the older half of the talk makes room, cut at a
     * line boundary so no word survives torn in the middle. */
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

static u32 rd32(const u8 *p);
static u64 rd64(const u8 *p);
static void wr32(u8 *p, u32 v);
static void wr64(u8 *p, u64 v);

/* ------------------------------------------------------------------ */
/* Company on the wire                                                 */
/* ------------------------------------------------------------------ */

#define FOUND_MAX 8

static struct {
    bool used;
    u8   ip[4];
    char name[24];
    bool works;                      /* says it takes far work */
    u32  free_mib;                   /* says it has this much air */
} found[FOUND_MAX];

static u64 scan_until_ns;            /* while set, seeks go out */
static u64 scan_last_call_ns;

void pipe_scan(void)
{
    for (u32 i = 0; i < FOUND_MAX; i++) found[i].used = false;
    scan_until_ns = time_ns() + 3ULL * 1000000000ULL;
    scan_last_call_ns = 0;
    journal_says("pipe", "calling out on the wire");
}

bool pipe_scanning(void)
{
    return time_ns() < scan_until_ns;
}

u32 pipe_found_count(void)
{
    u32 n = 0;
    for (u32 i = 0; i < FOUND_MAX; i++) if (found[i].used) n++;
    return n;
}

bool pipe_found_at(u32 i, u8 ip[4], char name[24],
                   bool *works, u32 *free_mib)
{
    u32 n = 0;
    for (u32 k = 0; k < FOUND_MAX; k++) {
        if (!found[k].used) continue;
        if (n == i) {
            for (u32 j = 0; j < 4; j++) ip[j] = found[k].ip[j];
            for (u32 j = 0; j < 24; j++) name[j] = found[k].name[j];
            if (works) *works = found[k].works;
            if (free_mib) *free_mib = found[k].free_mib;
            return true;
        }
        n++;
    }
    return false;
}

static bool ip4_same(const u8 *a, const u8 *b)
{
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

static void found_note(const u8 *ip, const u8 *name, u32 nmax,
                       bool works, u32 free_mib)
{
    u32 slot = FOUND_MAX;
    for (u32 i = 0; i < FOUND_MAX; i++) {
        if (found[i].used && ip4_same(found[i].ip, ip)) { slot = i; break; }
        if (!found[i].used && slot == FOUND_MAX) slot = i;
    }
    if (slot == FOUND_MAX) return;

    found[slot].used = true;
    for (u32 i = 0; i < 4; i++) found[slot].ip[i] = ip[i];
    u32 n = 0;
    while (n < 23 && n < nmax && name[n]) {
        char c = (char)name[n];
        found[slot].name[n] = (c >= 0x20 && c < 0x7F) ? c : ' ';
        n++;
    }
    found[slot].name[n] = 0;
    found[slot].works = works;
    found[slot].free_mib = free_mib;
}

/* magic, kind, pad, the name -- and what the machine offers: whether
 * it takes far work, and how much air it has. Claims like the name,
 * but useful ones: choosing a machine to ask goes better knowing who
 * is willing. */
static void say_who(u8 kind, const u8 dst[4], u16 dport)
{
    u8 pkt[40];
    wr32(pkt, MAGIC);
    pkt[4] = kind; pkt[5] = pkt[6] = pkt[7] = 0;
    char nm[24];
    settings_name(nm, sizeof(nm));
    for (u32 i = 0; i < 24; i++) pkt[8 + i] = (u8)nm[i];
    pkt[32] = settings_work() ? 1 : 0;
    pkt[33] = pkt[34] = pkt[35] = 0;
    wr32(pkt + 36, (u32)(pmm_free_frames() / 256));
    net_udp_send(dst, PIPE_PORT, dport, pkt, 40);
}

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
/* The seal                                                            */
/* ------------------------------------------------------------------ */

/* A session: one knock's worth of keys. Fresh at every knock, gone
 * two minutes after the last word, held nowhere else. */
#define SEAL_MAX 4

typedef struct {
    bool used;
    bool we_knocked;
    u8   ip[4];
    u16  port;
    u32  sid;
    u8   my_pub[32];                 /* to answer a repeated knock */
    u8   key_out[16], iv_out[12];
    u8   key_in[16],  iv_in[12];
    u64  ctr_out;                    /* starts at one; zero never sent */
    u64  ctr_in_seen;
    u64  last_ns;
    bool proven;                     /* the other side signed with a key we hold it to */
    u8   answer[140];                /* our WELCOME, to answer a repeated knock alike */
    u32  answer_len;
} sealrec;

static sealrec seal[SEAL_MAX];

/* The knock we currently have out, if any. */
static struct {
    bool active;
    u8   ip[4];
    u16  port;
    u32  sid;
    u8   priv[32], pub[32];
    u32  tries;
    u64  last_ns;
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
    /* A new knock from a machine replaces what it had: the freshest
     * keys are the ones both sides are holding. */
    sealrec *s = seal_by_ip(ip);
    if (s) return s;
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (!seal[i].used) return &seal[i];

    /* All busy: take the longest-quiet one. */
    sealrec *old = &seal[0];
    for (u32 i = 1; i < SEAL_MAX; i++)
        if (seal[i].last_ns < old->last_ns) old = &seal[i];
    return old;
}

/* Both sides derive the same four secrets from the shared point and
 * the two public keys, then keep the pair that speaks their way. */
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

/* Wraps one inner packet in an envelope and sends it. The header is
 * bound in as associated data, so nothing about the envelope can be
 * bent without the tag saying so. */
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

static void knock_begin(const u8 *ip, u16 port)
{
    if (knock.active && ip4_same(knock.ip, ip)) return;
    knock.active = true;
    for (u32 i = 0; i < 4; i++) knock.ip[i] = ip[i];
    knock.port = port;
    rand_bytes((u8 *)&knock.sid, 4);
    rand_bytes(knock.priv, 32);
    x25519_base(knock.pub, knock.priv);
    knock.tries = 0;
    knock.last_ns = 0;
}

/* What a signature under a knock covers: the session and the fresh
 * key, under a fixed word, so a signature cannot be lifted from one
 * exchange into another. The answer covers both fresh keys. */
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

/* A knock or its answer: the fresh key and, when the machine has an
 * identity, that identity's key and a signature over the exchange.
 * 44 bytes without, 140 with; an older machine reads the first 44
 * and ignores the rest. */
static u32 build_hello(u8 *pkt)
{
    wr32(pkt, MAGIC);
    pkt[4] = K_HELLO; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, knock.sid);
    memcpy(pkt + 12, knock.pub, 32);
    if (!ssh_identity(pkt + 44)) return 44;
    u8 msg[46];
    hello_message(msg, knock.sid, knock.pub);
    if (!ssh_sign(msg, sizeof(msg), pkt + 76)) return 44;
    return 140;
}

static u32 build_welcome(u8 *pkt, u32 sid, const u8 *their_eph, const u8 *my_eph)
{
    wr32(pkt, MAGIC);
    pkt[4] = K_WELCOME; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, sid);
    memcpy(pkt + 12, my_eph, 32);
    if (!ssh_identity(pkt + 44)) return 44;
    u8 msg[80];
    welcome_message(msg, sid, their_eph, my_eph);
    if (!ssh_sign(msg, sizeof(msg), pkt + 76)) return 44;
    return 140;
}

static void knock_send(void)
{
    u8 pkt[140];
    u32 n = build_hello(pkt);
    net_udp_send(knock.ip, PIPE_PORT, knock.port, pkt, n);
}

/* Holds the key a machine came with against what the settings
 * remember for its address. The first meeting is believed and written
 * down; from then on the machine at that address must be the one that
 * proved itself then. -1: not that machine, or no key shown where one
 * is remembered -- turned away. 0: the machine remembered, or nothing
 * remembered and nothing shown. 1: met for the first time, remembered
 * now. */
static i32 identity_verdict(const u8 *ip, const u8 *idkey)
{
    u8 known[32];
    bool have = settings_known(ip, known);
    if (!idkey) {
        if (!have) return 0;
        kprintf("pipe: %u.%u.%u.%u comes without its key, though one is remembered for it; turned away\n",
                ip[0], ip[1], ip[2], ip[3]);
        journal_says("pipe", "a machine came without the key remembered for it; turned away");
        return -1;
    }
    char fp[64];
    ssh_fingerprint_of(idkey, fp);
    if (have) {
        if (memcmp(known, idkey, 32) == 0) return 0;
        kprintf("pipe: %u.%u.%u.%u comes with a key that is not the one remembered for it (%s); turned away\n",
                ip[0], ip[1], ip[2], ip[3], fp);
        journal_says("pipe", "a machine came with a key that is not the one remembered for it; turned away");
        return -1;
    }
    if (settings_remember(ip, idkey)) {
        kprintf("pipe: %u.%u.%u.%u is met for the first time; its key %s is remembered now\n",
                ip[0], ip[1], ip[2], ip[3], fp);
        journal_says("pipe", "a machine was met for the first time; its key is remembered");
    } else {
        kprintf("pipe: no room in the settings to remember %u.%u.%u.%u\n",
                ip[0], ip[1], ip[2], ip[3]);
        journal_says("pipe", "the settings have no room left to remember a machine");
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Far work: the desk                                                  */
/* ------------------------------------------------------------------ */

/* A text can be asked of another machine: it travels as a recipe,
 * runs over there in the interpreter with a way home and a clock and
 * nothing else, and what it answers comes back correlated by job.
 * The far machine only works at all when its settings welcome it.
 *
 * The desk is where such asks queue. A task whose first line says
 * "split P from LO to HI" is divided: P parts, each carrying its
 * stretch of the range, handed round-robin to the machines that
 * answered the scan willing -- and the parts' answers, numbers by
 * contract, are summed into one. A task without the line is one
 * part, asked of the settings' peer, its answer taken as it is.
 * Either way the result is written back into the task itself when
 * the task came writable, and laid into arrivals when it did not.
 * Programs reach the desk through the wire, which makes asking far
 * work a capability like everything else here. */

static domain *pipe_kdom;

void pipe_prepare(domain *k) { pipe_kdom = k; }

/* Part states. */
#define P_PEND 0
#define P_RUN  1
#define P_DONE 2

/* Job states. */
#define DJ_FRESH 0
#define DJ_SCAN  1
#define DJ_RUN   2

typedef struct {
    bool    used;
    u32     no;
    object *task;                  /* retained until the job ends */
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
    u64     pwait_ns[PART_MAX];    /* not before */
    i64     presult[PART_MAX];
    u8      raw[24];               /* a lone part's answer, as said */

    u8      cand[CAND_MAX][4];
    u16     cand_port[CAND_MAX];
    u32     cand_count;
    u32     next_cand;
} desk_job;

static desk_job desk[DESK_JOBS];
static u32 desk_no;

/* The asks in flight, at most one per part. */
static struct {
    bool active;
    u8   ip[4];
    u16  port;
    u64  id;
    u32  job, part;
    u32  tries;
    u64  last_ns, started_ns;
} fask[FASK_MAX];

/* The job we are running for someone else, if any. One at a time:
 * a worker is a neighbour lending a hand, not a queue. */
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

/* The last answer given, kept to repeat when the same ask comes
 * again -- a lost answer must cost a resend, never a second run. */
static struct {
    bool valid;
    u64  id;
    u8   status;
    u8   text[24];
} gave;

/* "job N..." -- the journal's half of the correlation. */
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

static void answer_send(sealrec *s, u64 id, u8 status, const u8 text[24])
{
    u8 pkt[40];
    wr32(pkt, MAGIC);
    pkt[4] = K_ANSWER; pkt[5] = status; pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, id);
    for (u32 i = 0; i < 24; i++) pkt[16 + i] = text[i];
    seal_send(s, pkt, 40);
}

/* One part's stretch of the job's range: the range is dealt like
 * cards, the first parts taking the leftovers. */
static void part_range(const desk_job *j, u32 part, i64 *lo, i64 *hi)
{
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
    pkt[4] = K_ASK; pkt[5] = pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, fask[fi].id);
    wr32(pkt + 16, j->len);
    wr32(pkt + 20, ASK_BUDGET_S);
    wr64(pkt + 24, (u64)plo);
    wr64(pkt + 32, (u64)phi);
    memcpy(pkt + 40, j->recipe, j->len);
    seal_send(s, pkt, 40 + j->len);
}

/* Appends "= <answer>" to the task, the way a hand would write the
 * result under the sum. Failing quietly would hide the outcome, so a
 * page with no room says so in the journal instead. */
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
    if (ok) {
        char line[64];
        u32 at = put(line, 0, " answers: ");
        at = put(line, at, text);
        line[at] = 0;
        job_says(j->no, line);
        kprintf("pipe: job %u answers: %s\n", j->no, text);

        if (j->writable) task_append(j->task, text);
        else             lay_answer(j->no, (const u8 *)text);
    } else {
        char line[64];
        u32 at = put(line, 0, ": ");
        at = put(line, at, why);
        line[at] = 0;
        job_says(j->no, line);
        kprintf("pipe: job %u failed: %s\n", j->no, why);

        char fb[64];
        u32 fat = put(fb, 0, "nothing (");
        fat = put(fb, fat, why);
        fat = put(fb, fat, ")");
        fb[fat] = 0;
        if (j->writable) task_append(j->task, fb);
    }

    desk_clear_fasks((u32)(j - desk));
    if (j->task) obj_release(j->task);
    j->task = NULL;
    j->used = false;
}

/* Takes a task in: parses its first line for a split, strips old
 * answers off its tail, and queues it. The one entry the chip, the
 * wire and the foreman all use. */
bool pipe_ask(object *o, bool writable)
{
    if (!o) return false;

    if (!net_crypto_ok()) {
        journal_says("pipe", "the seal cannot prove itself; "
                             "nothing goes");
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
        journal_says("pipe", "the desk is full; it will take more "
                             "later");
        return false;
    }

    const u8 *d = (const u8 *)obj_data(o);
    u64 size = obj_size(o);
    u64 len = 0;
    if (d) while (len < size && d[len]) len++;

    /* Old answers are history, not part of the recipe: trailing lines
     * that begin "= " are left behind. */
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
        journal_says("pipe", "there is nothing in it to ask");
        return false;
    }

    /* The first line may divide the work: "split P from LO to HI".
     * The numbers are read wherever they stand in the line. */
    u64 eol = 0;
    while (eol < len && d[eol] != '\n') eol++;

    u32 parts = 1;
    i64 lo = 0, hi = 0;
    u64 recipe_at = 0;
    bool split = (eol >= 5 && d[0]=='s' && d[1]=='p' && d[2]=='l' &&
                  d[3]=='i' && d[4]=='t');
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
        journal_says("pipe", "the task has no recipe under its line");
        return false;
    }
    u64 rlen = len - recipe_at;
    if (rlen > RECIPE_MAX) {
        journal_says("pipe", "that recipe is too long to send");
        return false;
    }

    memset(j, 0, sizeof(*j));
    memcpy(j->recipe, d + recipe_at, rlen);
    j->len = (u32)rlen;
    j->parts = parts;
    j->lo = lo;
    j->hi = hi;
    j->writable = writable;
    j->no = ++desk_no;
    j->state = DJ_FRESH;
    j->task = o;
    obj_retain(o);
    j->used = true;

    if (parts > 1) {
        char line[48];
        u32 at = put(line, 0, ": divided into ");
        at = put_dec(line, at, parts);
        at = put(line, at, " parts");
        line[at] = 0;
        job_says(j->no, line);
    } else {
        job_says(j->no, " asked of the peer");
    }
    kprintf("pipe: job %u queued, %u bytes, %u part%s\n",
            j->no, j->len, parts, parts == 1 ? "" : "s");
    return true;
}

/* ------------------------------------------------------------------ */
/* Sending                                                             */
/* ------------------------------------------------------------------ */

/* One thing in flight at a time. The substance is copied here when
 * the shell posts it, so the object itself is free to change or go
 * the moment the click is done. */
static struct {
    bool busy;
    u8   kind;
    char name[OBJ_NAME_MAX];
    u32  len;
    u64  id;
    u32  tries;
    u64  last_try_ns;
    bool taken;
    u8   data[CARRY_MAX_BYTES];
} out;

bool pipe_post(object *o)
{
    if (!o) return false;

    if (!net_crypto_ok()) {
        journal_says("pipe", "the seal cannot prove itself; "
                             "nothing goes");
        return false;
    }

    u8 kind = wire_kind_of(obj_type(o));
    if (kind == 0) {
        journal_says("pipe", "only plain things can cross");
        return false;
    }

    u8 peer[4];
    u16 pp;
    if (!settings_peer(peer, &pp)) {
        journal_says("pipe", "no peer is named in the settings");
        return false;
    }

    if (out.busy) {
        journal_says("pipe", "still carrying the last thing");
        return false;
    }

    const u8 *d = (const u8 *)obj_data(o);
    u64 size = obj_size(o);
    if (!d || size == 0 || size > CARRY_MAX_BYTES) {
        journal_says("pipe", "that does not fit through");
        return false;
    }

    /* Texts travel as far as their words, not their whole room. */
    u32 len = (u32)size;
    if (obj_type(o) == TYPE_TEXT) {
        u32 n = 0;
        while (n < size && d[n]) n++;
        len = n ? n : 1;
    }

    out.kind = kind;
    out.len = len;
    for (u32 i = 0; i < len; i++) out.data[i] = d[i];

    const char *nm = obj_name(o);
    u32 ni = 0;
    if (nm) while (nm[ni] && ni < OBJ_NAME_MAX - 1) {
        out.name[ni] = nm[ni];
        ni++;
    }
    out.name[ni] = 0;

    rand_bytes((u8 *)&out.id, 8);
    out.tries = 0;
    out.last_try_ns = 0;
    out.taken = false;
    out.busy = true;
    return true;
}

static void send_offer_and_chunks(sealrec *s)
{
    u8 pkt[56];
    wr32(pkt, MAGIC);
    pkt[4] = K_OFFER; pkt[5] = pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, out.id);
    wr32(pkt + 16, out.kind);
    wr32(pkt + 20, out.len);
    for (u32 i = 0; i < 32; i++)
        pkt[24 + i] = (i < OBJ_NAME_MAX) ? (u8)out.name[i] : 0;
    seal_send(s, pkt, 56);

    u8 cp[24 + CHUNK_MAX];
    for (u32 off = 0; off < out.len; off += CHUNK_MAX) {
        u32 dlen = out.len - off < CHUNK_MAX ? out.len - off : CHUNK_MAX;
        wr32(cp, MAGIC);
        cp[4] = K_CHUNK; cp[5] = cp[6] = cp[7] = 0;
        wr64(cp + 8, out.id);
        wr32(cp + 16, off);
        wr32(cp + 20, dlen);
        for (u32 i = 0; i < dlen; i++) cp[24 + i] = out.data[off + i];
        seal_send(s, cp, 24 + dlen);
        net_breathe();                    /* let acks and everyone else in */
    }
}

/* ------------------------------------------------------------------ */
/* Receiving                                                           */
/* ------------------------------------------------------------------ */

static struct {
    bool active;
    u64  id;
    u8   kind;
    char name[OBJ_NAME_MAX];
    u32  total, have;
    u64  started_ns;
    u8   from[4];
    u16  from_port;
    u8   data[CARRY_MAX_BYTES];
} in;

static u64 last_taken_id;            /* to re-ack a lost TAKEN */

/* Lays a finished object into the arrivals list. Read and write,
 * never grant onward by default: what came over the wire is
 * material, and handing it around further stays a decision, not a
 * reflex. Takes over the caller's reference on success. */
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

/* A job's answer becomes a small text in arrivals, so the result is
 * material to work with and not only a line that scrolls away. */
static void lay_answer(u32 no, const u8 *text)
{
    u32 tl = 0;
    while (tl < 24 && text[tl]) tl++;
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

static void arrival_done(void)
{
    type_id t = local_kind_of(in.kind);
    if (t == TYPE_NULL || !arrivals) { in.active = false; return; }

    /* Texts get a byte of quiet at the end so they stay editable;
     * bytes and pictures arrive exactly as sent. */
    u64 room = in.total + (t == TYPE_TEXT ? 512 : 0);
    object *o = obj_create(t, room, 0);
    if (!o) { in.active = false; return; }

    u8 *d = (u8 *)obj_data(o);
    for (u32 i = 0; i < in.total; i++) d[i] = in.data[i];
    if (in.name[0]) obj_set_name(o, in.name);

    if (!arrivals_place(o)) {
        obj_release(o);
        in.active = false;
        return;
    }

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
        const char *un = "something unnamed";
        while (*un && sa < sizeof(said) - 1) said[sa++] = *un++;
    }
    said[sa] = 0;
    journal_says("pipe", said);
    in.active = false;
}

/* One plaintext packet out of an opened envelope. Everything with
 * substance in it lands here and nowhere else. */
/* ------------------------------------------------------------------ */
/* Saying a word                                                       */
/* ------------------------------------------------------------------ */

/* Puts one spoken word into every sealed session there is. Answers
 * how many doors it went through -- zero means nobody could hear. */
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

    /* What was said stands on the line either way; whether anyone
     * heard it is the journal's to report. */
    line_append("you", text);

    if (say_wire(text)) return true;

    /* No seal is standing. With a named peer the knock goes out and
     * the word waits by the door; without one there is no door. */
    u8 peer[4];
    u16 pp;
    if (!settings_peer(peer, &pp)) {
        journal_says("pipe", "nobody is on the line, and no peer is named");
        return false;
    }
    u32 n = 0;
    while (text[n] && n < SAY_MAX) { saypend.text[n] = text[n]; n++; }
    saypend.text[n] = 0;
    saypend.active = true;
    saypend.born_ns = time_ns();
    knock_begin(peer, pp);
    journal_says("pipe", "knocking first; the word will follow");
    return true;
}

static void inner_input(const u8 src[4], u16 sport, sealrec *s,
                        const u8 *p, u32 len)
{
    if (len < 16 || rd32(p) != MAGIC) return;
    u8 kind = p[4];
    u64 id = rd64(p + 8);

    if (kind == K_TAKEN) {
        if (out.busy && id == out.id) out.taken = true;
        return;
    }

    if (kind == K_OFFER && len >= 56) {
        if (id == last_taken_id && last_taken_id) {
            /* Already taken; the ack must have gone missing. */
            u8 ack[16];
            wr32(ack, MAGIC);
            ack[4] = K_TAKEN; ack[5] = ack[6] = ack[7] = 0;
            wr64(ack + 8, id);
            seal_send(s, ack, 16);
            return;
        }

        u32 wk = rd32(p + 16);
        u32 total = rd32(p + 20);
        if (local_kind_of((u8)wk) == TYPE_NULL) return;
        if (total == 0 || total > CARRY_MAX_BYTES) return;

        in.active = true;
        in.id = id;
        in.kind = (u8)wk;
        in.total = total;
        in.have = 0;
        in.started_ns = time_ns();
        for (u32 i = 0; i < 4; i++) in.from[i] = src[i];
        in.from_port = sport;
        u32 ni = 0;
        while (ni < OBJ_NAME_MAX - 1 && p[24 + ni]) {
            char c = (char)p[24 + ni];
            in.name[ni] = (c >= 0x20 && c < 0x7F) ? c : ' ';
            ni++;
        }
        in.name[ni] = 0;
        return;
    }

    if (kind == K_CHUNK && len >= 24) {
        if (!in.active || id != in.id) return;
        u32 off = rd32(p + 16);
        u32 dlen = rd32(p + 20);
        if (off != in.have) return;          /* in order or not at all */
        if (dlen == 0 || dlen > CHUNK_MAX || 24 + dlen > len) return;
        if (off + dlen > in.total) return;

        for (u32 i = 0; i < dlen; i++) in.data[off + i] = p[24 + i];
        in.have = off + dlen;

        if (in.have == in.total) {
            u8 ack[16];
            wr32(ack, MAGIC);
            ack[4] = K_TAKEN; ack[5] = ack[6] = ack[7] = 0;
            wr64(ack + 8, in.id);
            seal_send(s, ack, 16);
            last_taken_id = in.id;
            arrival_done();
        }
        return;
    }

    /* A word for the person here. It goes on the line under the name
     * the sender wears; the same datagram said twice lands once. */
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

        /* The bottom row shows the journal's latest word, so this is
         * how a talk announces itself without a bell. */
        char note[64];
        u32 a = 0;
        while (nm[a] && a < 24) { note[a] = nm[a]; a++; }
        const char *tail = " spoke on the line";
        for (u32 i = 0; tail[i] && a < sizeof(note) - 1; i++)
            note[a++] = tail[i];
        note[a] = 0;
        journal_says("pipe", note);
        return;
    }

    /* A job. The same ask asked again is the sender retrying, not a
     * second job: while it runs it is ignored, once answered the
     * old answer is repeated. Fresh asks are taken only when the
     * settings welcome work, and one at a time. */
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
        if (!settings_work()) {
            answer_send(s, id, A_NOWORK, none);
            kprintf("pipe: turned away a job (work is refused)\n");
            return;
        }
        if (workj.active) {
            answer_send(s, id, A_BUSY, none);
            return;
        }
        if (!pipe_kdom) return;

        object *script = obj_create(TYPE_TEXT, rlen + 512, 0);
        if (!script) return;
        memcpy(obj_data(script), p + 40, rlen);
        obj_set_name(script, "visiting work");

        object *reply = port_create(4);
        if (!reply) { obj_release(script); return; }
        cap_handle h = cap_insert(pipe_kdom, reply, CAP_READ);

        if (budget == 0 || budget > 60) budget = ASK_BUDGET_S;
        object *prog = work_launch(script, reply, budget, wlo, whi);
        obj_release(script);             /* the program holds its words */
        if (!prog) {
            cap_revoke(pipe_kdom, h);
            obj_release(reply);
            return;
        }
        /* The launcher's hold on the program object is now the job's;
         * it goes when the job is cleared. */

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

    /* What became of one of our parts. */
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
            /* The text, and -- for a divided job -- the number it
             * spells. Parts answer numbers by contract; a part that
             * answers words ends the job honestly. */
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

            if (j->parts > 1 && !num) {
                job_end(j, false, NULL,
                        "a part answered words, not a number");
                return;
            }

            j->pstate[part] = P_DONE;
            j->presult[part] = v;
            memcpy(j->raw, p + 16, 24);

            u32 done = 0;
            for (u32 i = 0; i < j->parts; i++)
                if (j->pstate[i] == P_DONE) done++;
            if (done < j->parts) return;

            if (j->parts == 1) {
                job_end(j, true, tz, NULL);
            } else {
                i64 total = 0;
                for (u32 i = 0; i < j->parts; i++)
                    total += j->presult[i];

                char text[48];
                u32 at = 0;
                u64 mag = total < 0 ? (u64)-total : (u64)total;
                char dg[24];
                u32 nd = 0;
                if (mag == 0) dg[nd++] = '0';
                while (mag) { dg[nd++] = (char)('0' + mag % 10); mag /= 10; }
                if (total < 0) text[at++] = '-';
                while (nd) text[at++] = dg[--nd];
                at = put(text, at, " (");
                at = put_dec(text, at, j->parts);
                at = put(text, at, " parts)");
                text[at] = 0;
                job_end(j, true, text, NULL);
            }
            return;
        }

        if (status == A_BUSY) {
            /* A busy neighbour is not a failure; the part waits its
             * turn and the round-robin tries elsewhere meanwhile. */
            j->pstate[part] = P_PEND;
            j->pwait_ns[part] = time_ns() + 2 * SECOND;
            return;
        }

        if (status == A_NOWORK) {
            /* That machine is out. Strike it; with nobody left the
             * job says so. */
            for (u32 c = 0; c < j->cand_count; c++) {
                if (!ip4_same(j->cand[c], fask[fi].ip)) continue;
                for (u32 k = c; k + 1 < j->cand_count; k++)
                    memcpy(j->cand[k], j->cand[k + 1], 4);
                j->cand_count--;
                break;
            }
            if (j->cand_count == 0) {
                job_end(j, false, NULL, "nobody takes the work");
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

void pipe_input(const u8 src[4], u16 sport, const u8 *p, u32 len)
{
    if (len < 16 || rd32(p) != MAGIC) return;
    u8 kind = p[4];

    /* Company. A seeker is answered and remembered both -- one scan
     * and the two machines know each other. Our own voice, come back
     * around the wire, is not company. */
    if (kind == K_SEEK || kind == K_HERE) {
        if (len < 32) return;
        u8 self[4];
        if (net_own_address(self) && ip4_same(src, self)) return;

        bool works = (len >= 40) && (p[32] & 1);
        u32 mib = (len >= 40) ? rd32(p + 36) : 0;
        found_note(src, p + 8, 24, works, mib);
        if (kind == K_SEEK) say_who(K_HERE, src, sport);
        return;
    }

    /* A knock. Answer it with a fresh key of our own and keep the
     * session; the same knock asked twice gets the same answer, so a
     * lost WELCOME costs a retry, not an argument about keys. */
    if (kind == K_HELLO && len >= 44) {
        if (!net_crypto_ok()) return;
        u32 sid = rd32(p + 8);

        sealrec *s = seal_find(src, sid);
        if (s && !s->we_knocked) {
            /* the same knock again gets the same answer, so a lost
             * WELCOME costs a retry, not an argument about keys */
            net_udp_send(src, PIPE_PORT, sport, s->answer, s->answer_len);
            return;
        }

        /* Who knocks. A signed knock proves the key it carries, and
         * the key is held against what the settings remember. */
        const u8 *idkey = NULL;
        if (len >= 140) {
            u8 msg[46];
            hello_message(msg, sid, p + 12);
            if (!ed25519_verify(p + 44, msg, sizeof(msg), p + 76)) {
                kprintf("pipe: a knock from %u.%u.%u.%u carried a signature that does not check; ignored\n",
                        src[0], src[1], src[2], src[3]);
                return;
            }
            idkey = p + 44;
        }
        if (identity_verdict(src, idkey) < 0) return;

        u8 priv[32], pub[32], shared[32];
        rand_bytes(priv, 32);
        x25519_base(pub, priv);
        x25519(shared, priv, p + 12);

        s = seal_slot_for(src);
        for (u32 i = 0; i < 4; i++) s->ip[i] = src[i];
        s->port = sport ? sport : PIPE_PORT;
        s->sid = sid;
        s->proven = idkey != NULL;
        memcpy(s->my_pub, pub, 32);
        seal_derive(s, false, p + 12, pub, shared);
        memset(priv, 0, 32);
        memset(shared, 0, 32);

        s->answer_len = build_welcome(s->answer, sid, p + 12, pub);
        net_udp_send(src, PIPE_PORT, sport, s->answer, s->answer_len);

        /* The session's own copy, not src: src points into the card's
         * receive ring, and sending above may already have let that
         * slot be filled again. It printed another packet's bytes as
         * an address once -- the seal itself was never wrong, only
         * the report of it. */
        kprintf("pipe: sealed with %u.%u.%u.%u, %s\n",
                s->ip[0], s->ip[1], s->ip[2], s->ip[3],
                s->proven ? "proven" : "unproven");
        return;
    }

    /* The answer to our knock. */
    if (kind == K_WELCOME && len >= 44) {
        if (!knock.active) return;
        if (rd32(p + 8) != knock.sid) return;
        if (!ip4_same(src, knock.ip)) return;

        /* Who answered. */
        const u8 *idkey = NULL;
        if (len >= 140) {
            u8 msg[80];
            welcome_message(msg, knock.sid, knock.pub, p + 12);
            if (!ed25519_verify(p + 44, msg, sizeof(msg), p + 76)) {
                kprintf("pipe: the answer from %u.%u.%u.%u carried a signature that does not check; ignored\n",
                        src[0], src[1], src[2], src[3]);
                return;
            }
            idkey = p + 44;
        }
        i32 verdict = identity_verdict(src, idkey);
        if (verdict < 0) {
            /* Not the machine we knew. The knock is over, and what
             * waited on it does not go. */
            knock.active = false;
            memset(knock.priv, 0, 32);
            if (out.busy) {
                out.busy = false;
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
        memcpy(s->my_pub, knock.pub, 32);
        seal_derive(s, true, knock.pub, p + 12, shared);
        memset(shared, 0, 32);
        memset(knock.priv, 0, 32);
        knock.active = false;

        kprintf("pipe: sealed with %u.%u.%u.%u, %s\n",
                s->ip[0], s->ip[1], s->ip[2], s->ip[3],
                s->proven ? "proven" : "unproven");
        journal_says("pipe", !s->proven ? "the way is sealed, but the machine could not prove itself"
                             : verdict == 1 ? "the way is sealed; the machine is met for the first time and remembered"
                                            : "the way is sealed with the machine remembered");
        return;
    }

    /* An envelope. Refused unless it opens: wrong keys, a replayed
     * counter, or a bent header all fail the same quiet way. */
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
        inner_input(src, sport, s, inner, ilen);
        return;
    }

    /* Substance without a seal is turned away, not spoken to. */
    if (kind == K_OFFER || kind == K_CHUNK || kind == K_TAKEN)
        kprintf("pipe: a plain offer was turned away\n");
}

/* ------------------------------------------------------------------ */

void pipe_service(void)
{
    /* While a scan runs, the call goes out once a second: to everyone
     * on the wire, and to the named peer besides -- who may live past
     * a router where no broadcast reaches. */
    if (time_ns() < scan_until_ns &&
        time_ns() - scan_last_call_ns > SECOND) {
        scan_last_call_ns = time_ns();
        static const u8 everyone[4] = { 255, 255, 255, 255 };
        say_who(K_SEEK, everyone, PIPE_PORT);

        u8 peer[4];
        u16 pp;
        if (settings_peer(peer, &pp)) say_who(K_SEEK, peer, pp);
    }

    /* Sessions go quietly when nobody has spoken through them. */
    for (u32 i = 0; i < SEAL_MAX; i++)
        if (seal[i].used && time_ns() - seal[i].last_ns > 120 * SECOND)
            seal[i].used = false;

    /* A half-arrived thing whose sender went quiet is let go. */
    if (in.active && time_ns() - in.started_ns > 10 * SECOND)
        in.active = false;

    /* An unanswered knock is repeated, then given up on. A send that
     * waited on it goes with it; the desk's parts keep their own
     * clocks and simply try another machine. */
    if (knock.active) {
        u64 now = time_ns();
        if (!knock.last_ns || now - knock.last_ns >= 2 * SECOND) {
            if (knock.tries >= 3) {
                knock.active = false;
                if (out.busy) {
                    journal_says("pipe", "nobody answered the knock");
                    kprintf("pipe: the knock went unanswered\n");
                    out.busy = false;
                }
            } else {
                knock.tries++;
                knock.last_ns = now;
                knock_send();
            }
        }
    }

    /* A word that waited for the knock goes the moment any door is
     * open; one that waited too long is let go, and said so. */
    if (saypend.active) {
        bool door = false;
        for (u32 i = 0; i < SEAL_MAX; i++)
            if (seal[i].used) door = true;
        if (door) {
            saypend.active = false;
            say_wire(saypend.text);
        } else if (time_ns() - saypend.born_ns > 10 * SECOND) {
            saypend.active = false;
            journal_says("pipe", "the word found no open door");
        }
    }

    /* The job we run for someone else: its first told line is the
     * answer; a script that ends mute, or outstays the budget the
     * interpreter holds it to, is answered for. */
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
            /* The interpreter counts whole seconds of the day, so a
             * script it ended for time can look a breath early from
             * here; within a second of the budget is the budget. */
            u64 gone = (time_ns() - workj.started_ns) / SECOND;
            status = (gone + 1 >= workj.budget_s) ? A_LATE : A_SILENT;
        } else if (time_ns() - workj.started_ns >
                   (workj.budget_s + 5) * SECOND) {
            status = A_LATE;
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

    /* The desk walks its first queued job: find hands, deal parts,
     * keep the deadline. One job at a time -- the ones behind it
     * wait their turn, which is also what the journal promised. */
    {
        desk_job *j = NULL;
        for (u32 i = 0; i < DESK_JOBS; i++)
            if (desk[i].used) { j = &desk[i]; break; }

        if (j && j->state == DJ_FRESH) {
            u64 now = time_ns();
            j->deadline_ns = now +
                (60 + (u64)j->parts * (ASK_BUDGET_S + 10)) * SECOND;
            if (j->parts == 1) {
                u8 peer[4];
                u16 pp;
                if (!settings_peer(peer, &pp)) {
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
                /* Divided work goes to whoever is willing; the scan
                 * asks the wire who that is. */
                pipe_scan();
                j->scan_until_ns = now + 4 * SECOND;
                j->state = DJ_SCAN;
            }
        }

        if (j && j->state == DJ_SCAN && time_ns() >= j->scan_until_ns) {
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
                if (settings_peer(peer, &pp)) {
                    memcpy(j->cand[0], peer, 4);
                    j->cand_port[0] = pp;
                    j->cand_count = 1;
                }
            }
            if (j->cand_count == 0) {
                job_end(j, false, NULL, "nobody on the wire takes work");
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
            u64 now = time_ns();
            if (now > j->deadline_ns) {
                job_end(j, false, NULL,
                        "the work did not come back in time");
            } else {
                for (u32 pi = 0; pi < j->parts; pi++) {
                    if (j->pstate[pi] != P_PEND) continue;
                    if (now < j->pwait_ns[pi]) continue;
                    if (j->cand_count == 0) break;

                    u32 fi = FASK_MAX;
                    for (u32 i = 0; i < FASK_MAX; i++)
                        if (!fask[i].active) { fi = i; break; }
                    if (fi == FASK_MAX) break;

                    u32 c = j->next_cand % j->cand_count;
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

    /* Each flying part: knock while the way is not sealed, repeat
     * the ask a few times, and give the attempt up when its machine
     * stays quiet -- the part then waits for another turn, once,
     * before the job calls it lost. */
    for (u32 i = 0; i < FASK_MAX; i++) {
        if (!fask[i].active) continue;
        desk_job *j = &desk[fask[i].job];
        if (!j->used) { fask[i].active = false; continue; }
        u64 now = time_ns();
        bool give_up = false;

        sealrec *s = seal_by_ip(fask[i].ip);
        if (!s) {
            knock_begin(fask[i].ip, fask[i].port);
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

    if (!out.busy) return;

    if (out.taken) {
        kprintf("pipe: carried %u bytes across, sealed\n", out.len);
        journal_says("pipe", "carried it across, sealed");
        out.busy = false;
        return;
    }

    u8 peer[4];
    u16 pp;
    if (!settings_peer(peer, &pp)) { out.busy = false; return; }

    sealrec *s = seal_by_ip(peer);
    if (!s) {
        knock_begin(peer, pp);
        return;
    }

    u64 now = time_ns();
    if (out.last_try_ns && now - out.last_try_ns < 2 * SECOND) return;

    if (out.tries >= 3) {
        journal_says("pipe", "the far side did not take it");
        kprintf("pipe: gave up after %u tries\n", out.tries);
        out.busy = false;
        return;
    }

    out.tries++;
    out.last_try_ns = now;
    send_offer_and_chunks(s);
}
