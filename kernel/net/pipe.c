/*
 * pipe.c -- objects crossing between machines.
 *
 * The wire format is three small datagrams over udp. An OFFER names
 * the transfer: what kind of thing, what it calls itself, how many
 * bytes. CHUNKs carry the payload in order. When the last byte is in,
 * the receiver answers TAKEN, and the sender stops worrying. No
 * answer means the whole offer is made again, a few times, and then
 * the journal says so; a pipe that loses something silently would be
 * worse than no pipe.
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
#include <eb/fmt.h>
#include <eb/io.h>

#define MAGIC     0x58504245u        /* "EBPX", little-endian */
#define K_OFFER   1
#define K_CHUNK   2
#define K_TAKEN   3
#define K_SEEK    4                  /* who else is on this wire? */
#define K_HERE    5                  /* i am, and this is my name */

#define CHUNK_MAX 1024
#define CARRY_MAX_BYTES 65536

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
    u32  sent_at_try;                /* bytes acknowledged... simple: whole-offer retry */
    u64  last_try_ns;
    bool taken;
    u8   data[CARRY_MAX_BYTES];
} out;

bool pipe_post(object *o)
{
    if (!o) return false;

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

static void send_offer_and_chunks(void)
{
    u8 peer[4];
    u16 pp;
    if (!settings_peer(peer, &pp)) { out.busy = false; return; }

    u8 pkt[56];
    wr32(pkt, MAGIC);
    pkt[4] = K_OFFER; pkt[5] = pkt[6] = pkt[7] = 0;
    wr64(pkt + 8, out.id);
    wr32(pkt + 16, out.kind);
    wr32(pkt + 20, out.len);
    for (u32 i = 0; i < 32; i++)
        pkt[24 + i] = (i < OBJ_NAME_MAX) ? (u8)out.name[i] : 0;
    net_udp_send(peer, PIPE_PORT, pp, pkt, 56);

    u8 cp[24 + CHUNK_MAX];
    for (u32 off = 0; off < out.len; off += CHUNK_MAX) {
        u32 dlen = out.len - off < CHUNK_MAX ? out.len - off : CHUNK_MAX;
        wr32(cp, MAGIC);
        cp[4] = K_CHUNK; cp[5] = cp[6] = cp[7] = 0;
        wr64(cp + 8, out.id);
        wr32(cp + 16, off);
        wr32(cp + 20, dlen);
        for (u32 i = 0; i < dlen; i++) cp[24 + i] = out.data[off + i];
        net_udp_send(peer, PIPE_PORT, pp, cp, 24 + dlen);
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

void pipe_input(const u8 src[4], u16 sport, const u8 *p, u32 len)
{
    if (len < 16 || rd32(p) != MAGIC) return;
    u8 kind = p[4];
    u64 id = rd64(p + 8);

    if (kind == K_TAKEN) {
        if (out.busy && id == out.id) {
            out.taken = true;
        }
        return;
    }

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

    if (kind == K_OFFER && len >= 56) {
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
            net_udp_send(in.from, PIPE_PORT,
                         in.from_port ? in.from_port : PIPE_PORT,
                         ack, 16);
            arrival_done();
        }
    }
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

    /* A half-arrived thing whose sender went quiet is let go. */
    if (in.active && time_ns() - in.started_ns > 10 * SECOND)
        in.active = false;

    if (!out.busy) return;

    if (out.taken) {
        kprintf("pipe: carried %u bytes across\n", out.len);
        journal_says("pipe", "carried it across");
        out.busy = false;
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
    send_offer_and_chunks();
}
