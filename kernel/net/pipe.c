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
#include <eb/fmt.h>
#include <eb/io.h>

#define MAGIC     0x58504245u        /* "EBPX", little-endian */
#define K_OFFER   1
#define K_CHUNK   2
#define K_TAKEN   3
#define K_SEEK    4                  /* who else is on this wire? */
#define K_HERE    5                  /* i am, and this is my name */
#define K_HELLO   6                  /* a knock: my fresh public key */
#define K_WELCOME 7                  /* the answer: mine, in return */
#define K_SEALED  8                  /* an envelope around any of the rest */

#define CHUNK_MAX 1024
#define CARRY_MAX_BYTES 65536
#define INNER_MAX (24 + CHUNK_MAX)

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

bool pipe_found_at(u32 i, u8 ip[4], char name[24])
{
    u32 n = 0;
    for (u32 k = 0; k < FOUND_MAX; k++) {
        if (!found[k].used) continue;
        if (n == i) {
            for (u32 j = 0; j < 4; j++) ip[j] = found[k].ip[j];
            for (u32 j = 0; j < 24; j++) name[j] = found[k].name[j];
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

static void found_note(const u8 *ip, const u8 *name, u32 nmax)
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
}

/* magic, kind, pad, then the name. */
static void say_who(u8 kind, const u8 dst[4], u16 dport)
{
    u8 pkt[32];
    wr32(pkt, MAGIC);
    pkt[4] = kind; pkt[5] = pkt[6] = pkt[7] = 0;
    char nm[24];
    settings_name(nm, sizeof(nm));
    for (u32 i = 0; i < 24; i++) pkt[8 + i] = (u8)nm[i];
    net_udp_send(dst, PIPE_PORT, dport, pkt, 32);
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

static void knock_send(void)
{
    u8 pkt[44];
    wr32(pkt, MAGIC);
    pkt[4] = K_HELLO; pkt[5] = pkt[6] = pkt[7] = 0;
    wr32(pkt + 8, knock.sid);
    memcpy(pkt + 12, knock.pub, 32);
    net_udp_send(knock.ip, PIPE_PORT, knock.port, pkt, 44);
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

    u64 n = obj_slots(arrivals), spot = n;
    for (u64 i = 0; i < n; i++)
        if (!obj_get_slot(arrivals, i)) { spot = i; break; }
    if (spot == n && !obj_grow_slots(arrivals, n + 1)) {
        obj_release(o);
        in.active = false;
        return;
    }

    /* Read and write, never grant onward by default: what came over
     * the wire is material, and handing it around further stays a
     * decision, not a reflex. */
    obj_set_slot(arrivals, spot, o, CAP_READ | CAP_WRITE);
    obj_release(o);
    obj_touch(arrivals);

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

        found_note(src, p + 8, 24);
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
            u8 pkt[44];
            wr32(pkt, MAGIC);
            pkt[4] = K_WELCOME; pkt[5] = pkt[6] = pkt[7] = 0;
            wr32(pkt + 8, sid);
            memcpy(pkt + 12, s->my_pub, 32);
            net_udp_send(src, PIPE_PORT, sport, pkt, 44);
            return;
        }

        u8 priv[32], pub[32], shared[32];
        rand_bytes(priv, 32);
        x25519_base(pub, priv);
        x25519(shared, priv, p + 12);

        s = seal_slot_for(src);
        for (u32 i = 0; i < 4; i++) s->ip[i] = src[i];
        s->port = sport ? sport : PIPE_PORT;
        s->sid = sid;
        memcpy(s->my_pub, pub, 32);
        seal_derive(s, false, p + 12, pub, shared);
        memset(priv, 0, 32);
        memset(shared, 0, 32);

        u8 pkt[44];
        wr32(pkt, MAGIC);
        pkt[4] = K_WELCOME; pkt[5] = pkt[6] = pkt[7] = 0;
        wr32(pkt + 8, sid);
        memcpy(pkt + 12, pub, 32);
        net_udp_send(src, PIPE_PORT, sport, pkt, 44);

        kprintf("pipe: sealed with %u.%u.%u.%u\n",
                src[0], src[1], src[2], src[3]);
        return;
    }

    /* The answer to our knock. */
    if (kind == K_WELCOME && len >= 44) {
        if (!knock.active) return;
        if (rd32(p + 8) != knock.sid) return;
        if (!ip4_same(src, knock.ip)) return;

        u8 shared[32];
        x25519(shared, knock.priv, p + 12);

        sealrec *s = seal_slot_for(src);
        for (u32 i = 0; i < 4; i++) s->ip[i] = src[i];
        s->port = knock.port;
        s->sid = knock.sid;
        memcpy(s->my_pub, knock.pub, 32);
        seal_derive(s, true, knock.pub, p + 12, shared);
        memset(shared, 0, 32);
        memset(knock.priv, 0, 32);
        knock.active = false;

        kprintf("pipe: sealed with %u.%u.%u.%u\n",
                src[0], src[1], src[2], src[3]);
        journal_says("pipe", "the way is sealed");
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

    /* An unanswered knock is repeated, then given up on -- and with
     * it goes whatever was waiting to travel. */
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
