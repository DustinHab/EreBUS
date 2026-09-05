/*
 * net.c -- client-side network: ARP, DHCP, DNS over UDP, TCP, HTTP/1.0; one conversation at a time.
 * - one thread; lost packets retried a few times, then the errand fails with its reason where the answer would go
 * - address by DHCP at start; a silent network gets the emulator defaults
 * - reachable only through a capability to its port; writes only where that capability may write
 */
#include <eb/net.h>
#include <eb/wifi.h>
#include <eb/pipe.h>
#include <eb/ssh.h>
#include <eb/standard.h>
#include <eb/fb.h>
#include <eb/crypto.h>
#include <eb/msg.h>
#include <eb/thread.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/io.h>
#include <eb/string.h>

/* Who we are and who serves us: filled in by DHCP, or by the
 * emulator's well-known arrangement when nobody answers. */
static u8 ip_ours[4];
static u8 ip_gw[4];
static u8 ip_dns[4];
static bool configured;
static bool relink_wanted;

static domain    *kdom;
static object    *service_port;
static cap_handle service_receive;
static bool       running;
static bool       crypto_good;

object *net_port(void)      { return service_port; }
bool    net_up(void)        { return running; }
bool    net_crypto_ok(void) { return crypto_good; }

bool net_own_address(u8 ip[4])
{
    if (!configured) return false;
    if (ip) for (u32 i = 0; i < 4; i++) ip[i] = ip_ours[i];
    return true;
}

/* How the last page came, for the shell to show a seal or its absence. */
static bool last_secure;
static bool last_verified;
bool net_last_secure(void)   { return last_secure; }
bool net_last_verified(void) { return last_verified; }

/* ------------------------------------------------------------------ */
/* Small tools                                                         */
/* ------------------------------------------------------------------ */

static bool ip4_eq(const u8 *a, const u8 *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

/* One's complement sum, the checksum of everything below http. */
static u16 csum(const void *data, u32 len, u32 seed)
{
    const u8 *d = (const u8 *)data;
    u32 sum = seed;
    while (len > 1) { sum += ((u32)d[0] << 8) | d[1]; d += 2; len -= 2; }
    if (len) sum += (u32)d[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (u16)~sum;
}

/* ------------------------------------------------------------------ */
/* Frames                                                              */
/* ------------------------------------------------------------------ */

#define ETH_IP  0x0800
#define ETH_ARP 0x0806

static u8 mac_gw[6];
static bool have_gw;

/* Neighbours answered for directly: a machine on our own street is
 * spoken to by its own door, not sent through the gateway -- which is
 * what lets two EreBUS machines on one wire find each other with no
 * router in between. */
#define ARP_CACHE 4
static struct {
    bool used;
    u8   ip[4];
    u8   mac[6];
} neigh[ARP_CACHE];

static bool on_link(const u8 *dst)
{
    return dst[0] == ip_ours[0] && dst[1] == ip_ours[1] &&
           dst[2] == ip_ours[2];
}

static u8 frame_out[1600];
static u8 frame_in[1600];

static void eth_head(u8 *f, const u8 *dst, u16 type)
{
    for (u32 i = 0; i < 6; i++) f[i] = dst[i];
    const u8 *us = nic_mac();
    for (u32 i = 0; i < 6; i++) f[6 + i] = us[i];
    f[12] = (u8)(type >> 8);
    f[13] = (u8)type;
}

static void arp_say(const u8 *dst_mac, const u8 *dst_ip, u16 op)
{
    u8 *f = frame_out;
    static const u8 everyone[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    eth_head(f, op == 1 ? everyone : dst_mac, ETH_ARP);

    u8 *a = f + 14;
    a[0] = 0; a[1] = 1;             /* ethernet */
    a[2] = 8; a[3] = 0;             /* ipv4 */
    a[4] = 6; a[5] = 4;             /* address sizes */
    a[6] = 0; a[7] = (u8)op;        /* 1 asks, 2 answers */
    const u8 *us = nic_mac();
    for (u32 i = 0; i < 6; i++) a[8 + i] = us[i];
    for (u32 i = 0; i < 4; i++) a[14 + i] = ip_ours[i];
    for (u32 i = 0; i < 6; i++) a[18 + i] = dst_mac[i];
    for (u32 i = 0; i < 4; i++) a[24 + i] = dst_ip[i];

    nic_send(f, 14 + 28);
}

/* ------------------------------------------------------------------ */
/* The pump: everything that arrives, sorted                           */
/* ------------------------------------------------------------------ */

/* What the current errand is waiting for, if anything. The pump fills
 * these; the waiting loops below watch them. */
static struct {
    bool  want_dns;
    u16   dns_id;
    u8    dns_addr[4];
    bool  dns_done;
} wait_dns;

/* The lease being negotiated. */
static struct {
    bool want;
    u32  xid;
    bool have_offer, have_ack;
    u8   offered[4], server[4], router[4], names[4];
} wait_dhcp;

static void dhcp_input(const u8 *p, u32 len)
{
    if (!wait_dhcp.want || len < 244) return;
    if (p[0] != 2) return;                               /* not an answer */
    u32 xid = ((u32)p[4] << 24) | ((u32)p[5] << 16) |
              ((u32)p[6] << 8) | p[7];
    if (xid != wait_dhcp.xid) return;
    if (p[236] != 0x63 || p[237] != 0x82 ||
        p[238] != 0x53 || p[239] != 0x63) return;

    u8 kind = 0;
    u8 router[4] = { 0 }, names[4] = { 0 }, server[4] = { 0 };

    u32 at = 240;
    while (at + 1 < len && p[at] != 255) {
        u8 opt = p[at], olen = p[at + 1];
        if (at + 2 + olen > len) break;
        const u8 *v = p + at + 2;
        if (opt == 53 && olen >= 1) kind = v[0];
        if (opt == 3 && olen >= 4) for (u32 i = 0; i < 4; i++) router[i] = v[i];
        if (opt == 6 && olen >= 4) for (u32 i = 0; i < 4; i++) names[i] = v[i];
        if (opt == 54 && olen >= 4) for (u32 i = 0; i < 4; i++) server[i] = v[i];
        at += 2 + olen;
    }

    if (kind == 2 && !wait_dhcp.have_offer) {            /* an offer */
        for (u32 i = 0; i < 4; i++) {
            wait_dhcp.offered[i] = p[16 + i];
            wait_dhcp.server[i] = server[i];
            wait_dhcp.router[i] = router[i];
            wait_dhcp.names[i] = names[i];
        }
        wait_dhcp.have_offer = true;
    } else if (kind == 5) {                              /* the lease */
        wait_dhcp.have_ack = true;
    }
}

/* The one tcp conversation. Arriving application bytes land in a ring
 * the reader drains; this is what lets a stream be pulled at whatever
 * pace suits, which plain http did not need but tls does -- records
 * arrive and must be handed up a few at a time. */
#define TCP_RING 49152
static struct {
    bool active;
    u8   remote_ip[4];
    u16  local_port, remote_port;
    u32  snd_nxt;                   /* next byte we would send */
    u32  snd_una;                   /* oldest unacknowledged */
    u32  rcv_nxt;                   /* next byte we expect */
    bool established;
    bool peer_done;                 /* their fin arrived */
    bool reset;

    u8   ring[TCP_RING];
    u32  head, tail;                /* tail - head bytes are waiting */
} tcb;

static u32 ring_used(void) { return tcb.tail - tcb.head; }

static void tcp_emit(u8 flags, const void *payload, u32 len);
static void door_input(const u8 src[4], const u8 *seg, u32 len);
static void door_service(void);

static void tcp_input(const u8 *seg, u32 len)
{
    if (len < 20) return;
    u16 sport = ((u16)seg[0] << 8) | seg[1];
    u16 dport = ((u16)seg[2] << 8) | seg[3];
    if (!tcb.active || dport != tcb.local_port ||
        sport != tcb.remote_port)
        return;

    u32 seq = ((u32)seg[4] << 24) | ((u32)seg[5] << 16) |
              ((u32)seg[6] << 8) | seg[7];
    u32 ack = ((u32)seg[8] << 24) | ((u32)seg[9] << 16) |
              ((u32)seg[10] << 8) | seg[11];
    u8  off = (u8)(seg[12] >> 4) * 4;
    u8  fl  = seg[13];
    if (off > len) return;

    if (fl & 0x04) { tcb.reset = true; return; }        /* rst */

    if (fl & 0x10) {                                    /* ack */
        if ((i32)(ack - tcb.snd_una) > 0) tcb.snd_una = ack;
    }

    if ((fl & 0x02) && !tcb.established) {              /* syn+ack */
        tcb.rcv_nxt = seq + 1;
        tcb.established = true;
        tcp_emit(0x10, NULL, 0);                        /* ack it */
        return;
    }

    const u8 *data = seg + off;
    u32 dlen = len - off;

    /* Only the next expected bytes are taken; anything out of order
     * is dropped and asked for again by the duplicate ack. Simple,
     * and on a link this short, sufficient. */
    if (dlen > 0) {
        if (seq == tcb.rcv_nxt) {
            u32 room = TCP_RING - ring_used();
            u32 take = dlen < room ? dlen : room;
            for (u32 i = 0; i < take; i++)
                tcb.ring[(tcb.tail + i) % TCP_RING] = data[i];
            tcb.tail += take;
            tcb.rcv_nxt += take;      /* only what we kept is acknowledged */
        }
        tcp_emit(0x10, NULL, 0);
    }

    if (fl & 0x01) {                                    /* fin */
        if (seq + dlen == tcb.rcv_nxt || dlen == 0) {
            if (!tcb.peer_done) {
                tcb.rcv_nxt = seq + dlen + 1;
                tcb.peer_done = true;
                tcp_emit(0x11, NULL, 0);                /* fin+ack back */
            }
        }
    }
}

static void dns_input(const u8 *p, u32 len)
{
    if (!wait_dns.want_dns || len < 12) return;
    u16 id = ((u16)p[0] << 8) | p[1];
    if (id != wait_dns.dns_id) return;

    u16 qd = ((u16)p[4] << 8) | p[5];
    u16 an = ((u16)p[6] << 8) | p[7];
    u32 at = 12;

    for (u16 q = 0; q < qd; q++) {                 /* skip the question */
        while (at < len && p[at]) at += p[at] + 1;
        at += 5;
    }
    for (u16 a = 0; a < an && at + 12 <= len; a++) {
        if ((p[at] & 0xC0) == 0xC0) at += 2;       /* compressed name */
        else { while (at < len && p[at]) at += p[at] + 1; at++; }

        if (at + 10 > len) return;
        u16 type = ((u16)p[at] << 8) | p[at + 1];
        u16 rdl  = ((u16)p[at + 8] << 8) | p[at + 9];
        at += 10;
        if (type == 1 && rdl == 4 && at + 4 <= len) {
            for (u32 i = 0; i < 4; i++) wait_dns.dns_addr[i] = p[at + i];
            wait_dns.dns_done = true;
            return;
        }
        at += rdl;
    }
}

static void sntp_input(const u8 *p, u32 len);
static void web_input(const u8 src[4], const u8 *seg, u32 len);

/* Sorts one arriving frame. Called until the wire is quiet. */
static void net_pump(void)
{
    for (;;) {
        i32 got = nic_recv(frame_in, sizeof(frame_in));
        if (got < 14) return;

        u16 type = ((u16)frame_in[12] << 8) | frame_in[13];
        const u8 *p = frame_in + 14;
        u32 plen = (u32)got - 14;

        if (type == ETH_ARP && plen >= 28) {
            if (p[7] == 1 && ip4_eq(p + 24, ip_ours)) {
                arp_say(p + 8, p + 14, 2);         /* they ask, we answer */
            } else if (p[7] == 2) {
                if (ip4_eq(p + 14, ip_gw)) {
                    for (u32 i = 0; i < 6; i++) mac_gw[i] = p[8 + i];
                    have_gw = true;
                }
                /* Any answering neighbour is worth remembering. */
                u32 slot = ARP_CACHE;
                for (u32 i = 0; i < ARP_CACHE; i++) {
                    if (neigh[i].used && ip4_eq(neigh[i].ip, p + 14))
                        { slot = i; break; }
                    if (!neigh[i].used && slot == ARP_CACHE) slot = i;
                }
                if (slot < ARP_CACHE) {
                    neigh[slot].used = true;
                    for (u32 i = 0; i < 4; i++) neigh[slot].ip[i] = p[14 + i];
                    for (u32 i = 0; i < 6; i++) neigh[slot].mac[i] = p[8 + i];
                }
            }
            continue;
        }

        if (type != ETH_IP || plen < 20) continue;

        /* Ours, or spoken to everyone -- and while we have no address
         * yet, everything the card accepted is worth a look, since
         * the lease that will name us arrives before the name. */
        static const u8 everyone4[4] = { 255, 255, 255, 255 };
        if (!ip4_eq(p + 16, ip_ours) &&
            !ip4_eq(p + 16, everyone4) &&
            configured)
            continue;
        u8 ihl = (u8)(p[0] & 0x0F) * 4;
        u16 tot = ((u16)p[2] << 8) | p[3];
        if (ihl < 20 || tot > plen) continue;
        u8 proto = p[9];
        const u8 *inner = p + ihl;
        u32 ilen = tot - ihl;

        if (proto == 6) {
            u16 tdport = ilen >= 4 ? (u16)(((u16)inner[2] << 8) | inner[3])
                                   : 0;
            if      (tdport == 80) web_input(p + 12, inner, ilen);
            else if (tdport == 22) door_input(p + 12, inner, ilen);
            else                   tcp_input(inner, ilen);
        } else if (proto == 17 && ilen >= 8) {
            u16 sport = ((u16)inner[0] << 8) | inner[1];
            u16 dport = ((u16)inner[2] << 8) | inner[3];
            if (sport == 53) dns_input(inner + 8, ilen - 8);
            if (sport == 123) sntp_input(inner + 8, ilen - 8);
            if (dport == 68) dhcp_input(inner + 8, ilen - 8);
            if (dport == PIPE_PORT)
                pipe_input(p + 12, sport, inner + 8, ilen - 8);
        } else if (proto == 1 && ilen >= 8 && inner[0] == 8) {
            /* An echo request: answer it. Being pingable costs one
             * buffer and makes the whole path checkable from outside.
             * The echo is the request's size; one that would not fit
             * the frame with its headers is left unanswered. */
            if (14 + 20 + ilen > sizeof(frame_out)) continue;
            u8 *f = frame_out;
            eth_head(f, frame_in + 6, ETH_IP);
            u8 *ip = f + 14;
            for (u32 i = 0; i < 20; i++) ip[i] = p[i];
            for (u32 i = 0; i < 4; i++) {
                ip[12 + i] = p[16 + i];
                ip[16 + i] = p[12 + i];
            }
            ip[8] = 64; ip[10] = 0; ip[11] = 0;
            u16 hc = csum(ip, 20, 0);
            ip[10] = (u8)(hc >> 8); ip[11] = (u8)hc;

            u8 *ic = ip + 20;
            for (u32 i = 0; i < ilen; i++) ic[i] = inner[i];
            ic[0] = 0; ic[2] = 0; ic[3] = 0;
            u16 cc = csum(ic, ilen, 0);
            ic[2] = (u8)(cc >> 8); ic[3] = (u8)cc;
            nic_send(f, 14 + 20 + ilen);
        }
    }
}

/* A short breath: look at the wire, let everyone else run. */
void net_breathe(void)
{
    net_pump();
    sched_yield();
}

/* ------------------------------------------------------------------ */
/* Sending upward: ip, udp, tcp                                        */
/* ------------------------------------------------------------------ */

static u16 ip_ident = 1;

static bool gateway_find(void);

/* The door to knock on for dst: a neighbour's own, or the gateway's.
 * A neighbour not yet known is asked for and waited on briefly; the
 * cache remembers the answer for everything after. */
static bool mac_for(const u8 *dst, u8 out_mac[6])
{
    /* Spoken to everyone: the broadcast door, no asking needed. */
    static const u8 everyone4[4] = { 255, 255, 255, 255 };
    if (ip4_eq(dst, everyone4)) {
        for (u32 i = 0; i < 6; i++) out_mac[i] = 0xFF;
        return true;
    }

    if (!on_link(dst)) {
        if (!gateway_find()) return false;
        for (u32 i = 0; i < 6; i++) out_mac[i] = mac_gw[i];
        return true;
    }

    for (u32 i = 0; i < ARP_CACHE; i++)
        if (neigh[i].used && ip4_eq(neigh[i].ip, dst)) {
            for (u32 j = 0; j < 6; j++) out_mac[j] = neigh[i].mac[j];
            return true;
        }

    static const u8 nobody[6] = { 0 };
    for (u32 try = 0; try < 3; try++) {
        arp_say(nobody, dst, 1);
        u64 from = time_ns();
        while (time_ns() - from < 500000000ULL) {
            net_pump();
            for (u32 i = 0; i < ARP_CACHE; i++)
                if (neigh[i].used && ip4_eq(neigh[i].ip, dst)) {
                    for (u32 j = 0; j < 6; j++) out_mac[j] = neigh[i].mac[j];
                    return true;
                }
            sched_yield();
        }
    }
    return false;
}

/* Builds and sends one ip packet to `dst`, by its own door when it is
 * on our street and through the gateway when it is not. */
static bool ip_send(u8 proto, const u8 *dst, const u8 *payload, u32 len)
{
    u8 door[6];
    if (!mac_for(dst, door)) return false;

    u8 *f = frame_out;
    eth_head(f, door, ETH_IP);
    u8 *ip = f + 14;
    u16 tot = (u16)(20 + len);

    ip[0] = 0x45; ip[1] = 0;
    ip[2] = (u8)(tot >> 8); ip[3] = (u8)tot;
    ip[4] = (u8)(ip_ident >> 8); ip[5] = (u8)ip_ident;
    ip_ident++;
    ip[6] = 0x40; ip[7] = 0;        /* do not fragment */
    ip[8] = 64;  ip[9] = proto;
    ip[10] = 0; ip[11] = 0;
    for (u32 i = 0; i < 4; i++) ip[12 + i] = ip_ours[i];
    for (u32 i = 0; i < 4; i++) ip[16 + i] = dst[i];
    u16 hc = csum(ip, 20, 0);
    ip[10] = (u8)(hc >> 8); ip[11] = (u8)hc;

    for (u32 i = 0; i < len; i++) ip[20 + i] = payload[i];
    return nic_send(f, 14 + 20 + len);
}

static u32 pseudo_seed(const u8 *dst, u8 proto, u32 len);

bool net_udp_send(const u8 dst[4], u16 sport, u16 dport,
                  const u8 *data, u32 len)
{
    if (!nic_up()) return false;
    if (len > 1400) return false;

    u8 u[8 + 1400];
    u16 ulen = (u16)(8 + len);
    u[0] = (u8)(sport >> 8); u[1] = (u8)sport;
    u[2] = (u8)(dport >> 8); u[3] = (u8)dport;
    u[4] = (u8)(ulen >> 8);  u[5] = (u8)ulen;
    u[6] = 0; u[7] = 0;
    for (u32 i = 0; i < len; i++) u[8 + i] = data[i];

    /* A real checksum, not the permitted zero: the zero survives the
     * emulator's own little services but not every road beyond them,
     * and a datagram that only works on friendly roads is a datagram
     * that fails exactly when it matters. */
    u16 c = csum(u, ulen, pseudo_seed(dst, 17, ulen));
    if (c == 0) c = 0xFFFF;
    u[6] = (u8)(c >> 8);
    u[7] = (u8)c;

    return ip_send(17, dst, u, ulen);
}

/* The pseudo-header seed tcp and udp checksums start from. */
static u32 pseudo_seed(const u8 *dst, u8 proto, u32 len)
{
    u32 sum = 0;
    sum += ((u32)ip_ours[0] << 8) | ip_ours[1];
    sum += ((u32)ip_ours[2] << 8) | ip_ours[3];
    sum += ((u32)dst[0] << 8) | dst[1];
    sum += ((u32)dst[2] << 8) | dst[3];
    sum += proto;
    sum += len;
    return sum;
}

static u8 seg_buf[1520];

static void tcp_emit(u8 flags, const void *payload, u32 len)
{
    u8 *t = seg_buf;
    bool syn = (flags & 0x02) != 0;
    u32 hlen = syn ? 24 : 20;

    t[0] = (u8)(tcb.local_port >> 8); t[1] = (u8)tcb.local_port;
    t[2] = (u8)(tcb.remote_port >> 8); t[3] = (u8)tcb.remote_port;
    u32 seq = tcb.snd_nxt;
    t[4] = (u8)(seq >> 24); t[5] = (u8)(seq >> 16);
    t[6] = (u8)(seq >> 8);  t[7] = (u8)seq;
    u32 ack = (flags & 0x10) ? tcb.rcv_nxt : 0;
    t[8] = (u8)(ack >> 24); t[9] = (u8)(ack >> 16);
    t[10] = (u8)(ack >> 8); t[11] = (u8)ack;
    t[12] = (u8)((hlen / 4) << 4);
    t[13] = flags;
    t[14] = 0x20; t[15] = 0x00;     /* an 8 KiB window, plenty here */
    t[16] = 0; t[17] = 0;
    t[18] = 0; t[19] = 0;
    if (syn) {                       /* say our segment size once */
        t[20] = 2; t[21] = 4; t[22] = 0x05; t[23] = 0xB4;
    }
    const u8 *pl = (const u8 *)payload;
    for (u32 i = 0; i < len; i++) t[hlen + i] = pl[i];

    u16 c = csum(t, hlen + len,
                 pseudo_seed(tcb.remote_ip, 6, hlen + len));
    t[16] = (u8)(c >> 8); t[17] = (u8)c;

    ip_send(6, tcb.remote_ip, t, hlen + len);
}

/* ------------------------------------------------------------------ */
/* The errands: resolve, converse, fetch                               */
/* ------------------------------------------------------------------ */

#define SECOND 1000000000ULL

/* One DHCP message, spoken from nowhere to everyone: we have no
 * address yet, which is the whole point of asking. */
static void dhcp_send(u8 kind)
{
    static const u8 everymac[6] = { 255, 255, 255, 255, 255, 255 };
    u8 b[320];
    for (u32 i = 0; i < sizeof(b); i++) b[i] = 0;

    b[0] = 1; b[1] = 1; b[2] = 6;                       /* ask, ethernet */
    b[4] = (u8)(wait_dhcp.xid >> 24);
    b[5] = (u8)(wait_dhcp.xid >> 16);
    b[6] = (u8)(wait_dhcp.xid >> 8);
    b[7] = (u8)wait_dhcp.xid;
    b[10] = 0x80;                       /* answer to everyone, please */
    const u8 *us = nic_mac();
    for (u32 i = 0; i < 6; i++) b[28 + i] = us[i];
    b[236] = 0x63; b[237] = 0x82; b[238] = 0x53; b[239] = 0x63;

    u32 at = 240;
    b[at++] = 53; b[at++] = 1; b[at++] = kind;   /* 1 asks, 3 takes */
    if (kind == 3) {
        b[at++] = 50; b[at++] = 4;
        for (u32 i = 0; i < 4; i++) b[at++] = wait_dhcp.offered[i];
        b[at++] = 54; b[at++] = 4;
        for (u32 i = 0; i < 4; i++) b[at++] = wait_dhcp.server[i];
    }
    b[at++] = 55; b[at++] = 3;
    b[at++] = 1; b[at++] = 3; b[at++] = 6;   /* mask, way out, names */
    b[at++] = 255;
    u32 blen = at < 300 ? 300 : at;          /* the old floor of bootp */

    u8 *f = frame_out;
    eth_head(f, everymac, ETH_IP);
    u8 *ip = f + 14;
    u16 tot = (u16)(20 + 8 + blen);
    ip[0] = 0x45; ip[1] = 0;
    ip[2] = (u8)(tot >> 8); ip[3] = (u8)tot;
    ip[4] = (u8)(ip_ident >> 8); ip[5] = (u8)ip_ident;
    ip_ident++;
    ip[6] = 0; ip[7] = 0; ip[8] = 64; ip[9] = 17;
    ip[10] = 0; ip[11] = 0;
    for (u32 i = 0; i < 4; i++) ip[12 + i] = 0;
    for (u32 i = 0; i < 4; i++) ip[16 + i] = 255;
    u16 hc = csum(ip, 20, 0);
    ip[10] = (u8)(hc >> 8); ip[11] = (u8)hc;

    u8 *u = ip + 20;
    u[0] = 0; u[1] = 68; u[2] = 0; u[3] = 67;
    u16 ulen = (u16)(8 + blen);
    u[4] = (u8)(ulen >> 8); u[5] = (u8)ulen;
    u[6] = 0; u[7] = 0;
    for (u32 i = 0; i < blen; i++) u[8 + i] = b[i];

    nic_send(f, 14 + tot);
}

/* Asks the network who we are. Whatever answers decides; a network
 * that stays silent gets the emulator's well-known arrangement, so a
 * test machine neither waits long nor lies about it. */
static void dhcp_run(void)
{
    /* A claimed address needs no landlord: the settings said who we
     * are, and on a wire with no dhcp -- two machines and a cable --
     * asking would only be six seconds of silence. */
    u8 claimed[4];
    if (settings_address(claimed)) {
        for (u32 i = 0; i < 4; i++) ip_ours[i] = claimed[i];
        ip_gw[0] = claimed[0]; ip_gw[1] = claimed[1];
        ip_gw[2] = claimed[2]; ip_gw[3] = 2;
        for (u32 i = 0; i < 4; i++) ip_dns[i] = ip_gw[i];
        configured = true;
        kprintf("net:  %u.%u.%u.%u by claim\n",
                ip_ours[0], ip_ours[1], ip_ours[2], ip_ours[3]);
        return;
    }

    wait_dhcp.want = true;
    wait_dhcp.xid = (u32)(time_ns() ^ 0x9E3779B9u) | 1;

    for (u32 try = 0; try < 3 && !wait_dhcp.have_offer; try++) {
        dhcp_send(1);
        u64 from = time_ns();
        while (time_ns() - from < 2 * SECOND && !wait_dhcp.have_offer)
            net_breathe();
    }
    for (u32 try = 0; try < 3 && wait_dhcp.have_offer &&
                      !wait_dhcp.have_ack; try++) {
        dhcp_send(3);
        u64 from = time_ns();
        while (time_ns() - from < 2 * SECOND && !wait_dhcp.have_ack)
            net_breathe();
    }
    wait_dhcp.want = false;

    if (wait_dhcp.have_ack) {
        for (u32 i = 0; i < 4; i++) {
            ip_ours[i] = wait_dhcp.offered[i];
            ip_gw[i] = wait_dhcp.router[i];
            ip_dns[i] = wait_dhcp.names[i];
        }
        /* A landlord who names no name servant means himself. */
        if (ip_dns[0] == 0)
            for (u32 i = 0; i < 4; i++) ip_dns[i] = ip_gw[i];
        configured = true;
        kprintf("net:  %u.%u.%u.%u by lease, way out %u.%u.%u.%u, "
                "names from %u.%u.%u.%u\n",
                ip_ours[0], ip_ours[1], ip_ours[2], ip_ours[3],
                ip_gw[0], ip_gw[1], ip_gw[2], ip_gw[3],
                ip_dns[0], ip_dns[1], ip_dns[2], ip_dns[3]);
        return;
    }

    ip_ours[0] = 10; ip_ours[1] = 0; ip_ours[2] = 2; ip_ours[3] = 15;
    ip_gw[0] = 10; ip_gw[1] = 0; ip_gw[2] = 2; ip_gw[3] = 2;
    ip_dns[0] = 10; ip_dns[1] = 0; ip_dns[2] = 2; ip_dns[3] = 3;
    configured = true;
    kprintf("net:  no dhcp answer; assuming 10.0.2.15 "
            "(emulator)\n");
}

static bool gateway_find(void)
{
    if (have_gw) return true;
    for (u32 try = 0; try < 5; try++) {
        static const u8 nobody[6] = { 0 };
        arp_say(nobody, ip_gw, 1);
        u64 from = time_ns();
        while (time_ns() - from < SECOND / 2) {
            net_breathe();
            if (have_gw) return true;
        }
    }
    return false;
}

static u16 udp_port_next = 40000;

static bool dns_resolve(const char *host, u32 hlen, u8 *out_ip)
{
    /* A name that is already an address resolves to itself. */
    u32 dots = 0, digits = 0;
    for (u32 i = 0; i < hlen; i++) {
        if (host[i] == '.') dots++;
        else if (host[i] >= '0' && host[i] <= '9') digits++;
    }
    if (dots == 3 && digits + dots == hlen) {
        u32 v = 0, at = 0;
        for (u32 i = 0; i <= hlen; i++) {
            if (i == hlen || host[i] == '.') { out_ip[at++] = (u8)v; v = 0; }
            else v = v * 10 + (u32)(host[i] - '0');
        }
        return true;
    }

    u8 q[320];
    u16 id = (u16)(time_ns() & 0xFFFF) | 1;
    q[0] = (u8)(id >> 8); q[1] = (u8)id;
    q[2] = 0x01; q[3] = 0;          /* a plain recursive question */
    q[4] = 0; q[5] = 1;
    q[6] = 0; q[7] = 0; q[8] = 0; q[9] = 0; q[10] = 0; q[11] = 0;

    u32 at = 12, mark = at++;
    for (u32 i = 0; i < hlen && at < 300; i++) {
        if (host[i] == '.') { q[mark] = (u8)(at - mark - 1); mark = at++; }
        else q[at++] = (u8)host[i];
    }
    q[mark] = (u8)(at - mark - 1);
    q[at++] = 0;
    q[at++] = 0; q[at++] = 1;       /* type a */
    q[at++] = 0; q[at++] = 1;       /* class in */

    u16 sport = udp_port_next++;
    u8 u[352];
    u32 ulen = 8 + at;
    u[0] = (u8)(sport >> 8); u[1] = (u8)sport;
    u[2] = 0; u[3] = 53;
    u[4] = (u8)(ulen >> 8); u[5] = (u8)ulen;
    u[6] = 0; u[7] = 0;             /* udp over ipv4 may skip its checksum */
    for (u32 i = 0; i < at; i++) u[8 + i] = q[i];

    wait_dns.want_dns = true;
    wait_dns.dns_id = id;
    wait_dns.dns_done = false;

    for (u32 try = 0; try < 3 && !wait_dns.dns_done; try++) {
        ip_send(17, ip_dns, u, ulen);
        u64 from = time_ns();
        while (time_ns() - from < 2 * SECOND && !wait_dns.dns_done)
            net_breathe();
    }
    wait_dns.want_dns = false;
    if (!wait_dns.dns_done) return false;
    for (u32 i = 0; i < 4; i++) out_ip[i] = wait_dns.dns_addr[i];
    return true;
}

/* ------------------------------------------------------------------ */
/* The clock from the net                                              */
/* ------------------------------------------------------------------ */

/* One SNTP question, and the answer sets the wall clock. The time
 * that comes back is utc; where the machine actually stands stays the
 * settings' clock line to say, on top. */
static void sntp_input(const u8 *p, u32 len)
{
    if (len < 48) return;

    /* The transmit timestamp: seconds since 1900, big-endian. */
    u64 secs = ((u64)p[40] << 24) | ((u64)p[41] << 16) |
               ((u64)p[42] << 8) | p[43];
    if (secs == 0) return;

    u32 day = (u32)(secs % 86400);
    time_set_wall(day / 3600, (day / 60) % 60, day % 60);
    kprintf("net:  the clock was set from the net (utc)\n");
    journal_says("system", "the clock was set from the net");
}

static void sntp_ask(void)
{
    u8 srv[4];
    if (!dns_resolve("pool.ntp.org", 12, srv)) return;

    u8 q[48];
    memset(q, 0, sizeof(q));
    q[0] = 0x23;                       /* version 4, a client asking */
    net_udp_send(srv, 40123, 123, q, 48);
}

/* ------------------------------------------------------------------ */
/* The served: a small web server                                      */
/* ------------------------------------------------------------------ */

/* One visitor at a time, one request per visit, and a reference as
 * the whole switch: while a list named "the served" exists, its
 * readable entries are pages on the local net; without the list the
 * port does not even answer. Substance goes out and nothing comes in
 * but the asking -- a browser is a reader here, never a hand. Best
 * effort on purpose: no retransmission, one segment run per answer,
 * which on the wire this serves is what a page load looks like. */

static struct {
    bool active, established;
    u8   ip[4];
    u16  port;
    u32  snd_nxt, rcv_nxt;
} web;

static u8 web_all[20480];            /* header and body, composed */
static u8 web_body[16600];

static void web_emit(u8 flags, const u8 *data, u32 len)
{
    static u8 t[1500];
    t[0] = 0; t[1] = 80;
    t[2] = (u8)(web.port >> 8); t[3] = (u8)web.port;
    t[4] = (u8)(web.snd_nxt >> 24); t[5] = (u8)(web.snd_nxt >> 16);
    t[6] = (u8)(web.snd_nxt >> 8);  t[7] = (u8)web.snd_nxt;
    t[8] = (u8)(web.rcv_nxt >> 24); t[9] = (u8)(web.rcv_nxt >> 16);
    t[10] = (u8)(web.rcv_nxt >> 8); t[11] = (u8)web.rcv_nxt;
    t[12] = 0x50;
    t[13] = flags;
    t[14] = 0x20; t[15] = 0x00;
    t[16] = 0; t[17] = 0;
    t[18] = 0; t[19] = 0;
    for (u32 i = 0; i < len; i++) t[20 + i] = data[i];

    u16 c = csum(t, 20 + len, pseudo_seed(web.ip, 6, 20 + len));
    t[16] = (u8)(c >> 8); t[17] = (u8)c;
    ip_send(6, web.ip, t, 20 + len);

    web.snd_nxt += len;
    if (flags & 0x03) web.snd_nxt += 1;      /* syn or fin took one */
}

static u32 wput(u8 *b, u32 at, const char *s)
{
    while (*s) b[at++] = (u8)*s++;
    return at;
}

static u32 wput_dec(u8 *b, u32 at, u64 v)
{
    char d[24];
    u32 n = 0;
    if (v == 0) d[n++] = '0';
    while (v) { d[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[at++] = (u8)d[--n];
    return at;
}

/* A petname into a page, with the four letters html cares about
 * dulled -- names are claims, and claims do not get to be markup. */
static u32 wput_name(u8 *b, u32 at, const char *s)
{
    for (u32 i = 0; s && s[i] && i < 40; i++) {
        char c = s[i];
        if (c == '<' || c == '>' || c == '&' || c == '"') c = '.';
        b[at++] = (u8)c;
    }
    return at;
}

/* A picture as a bmp: 8 bits per pixel, the sixteen inks as its
 * palette, rows bottom-up the way bmp wants them. */
static u32 web_bmp(object *pic, u8 *out, u32 max)
{
    const u8 *d = (const u8 *)obj_data(pic);
    if (!d || obj_size(pic) < 8) return 0;
    u32 w = (u32)d[0] | ((u32)d[1] << 8) | ((u32)d[2] << 16) |
            ((u32)d[3] << 24);
    u32 h = (u32)d[4] | ((u32)d[5] << 8) | ((u32)d[6] << 16) |
            ((u32)d[7] << 24);
    if (w == 0 || h == 0 || (u64)w * h + 8 > obj_size(pic)) return 0;

    u32 stride = (w + 3) & ~3u;
    u32 total = 14 + 40 + 16 * 4 + stride * h;
    if (total > max) return 0;

    memset(out, 0, total);
    out[0] = 'B'; out[1] = 'M';
    out[2] = (u8)total; out[3] = (u8)(total >> 8);
    out[4] = (u8)(total >> 16); out[5] = (u8)(total >> 24);
    u32 off = 14 + 40 + 64;
    out[10] = (u8)off; out[11] = (u8)(off >> 8);

    u8 *ih = out + 14;
    ih[0] = 40;
    ih[4] = (u8)w; ih[5] = (u8)(w >> 8);
    ih[8] = (u8)h; ih[9] = (u8)(h >> 8);
    ih[12] = 1;                              /* one plane */
    ih[14] = 8;                              /* bits per pixel */
    ih[32] = 16;                             /* colours used */

    u8 *pal = out + 14 + 40;
    for (u32 i = 0; i < 16; i++) {
        color c = fb_inks[i];
        pal[i * 4 + 0] = (u8)c;              /* blue */
        pal[i * 4 + 1] = (u8)(c >> 8);       /* green */
        pal[i * 4 + 2] = (u8)(c >> 16);      /* red */
    }

    u8 *px = out + off;
    for (u32 row = 0; row < h; row++) {
        const u8 *src = d + 8 + (u64)(h - 1 - row) * w;
        for (u32 col = 0; col < w; col++)
            px[row * stride + col] = (u8)(src[col] & 15);
    }
    return total;
}

/* Builds the answer for one asked path into web_all. */
static u32 web_answer(const char *path)
{
    object *served = system_served();
    const char *ctype = "text/plain";
    u32 blen = 0;
    bool found = (served != NULL);

    if (served && path[0] == '/' && path[1] == 0) {
        ctype = "text/html";
        u32 at = wput(web_body, 0,
                      "<!doctype html><html><head><title>");
        char nm[24];
        settings_name(nm, sizeof(nm));
        at = wput_name(web_body, at, nm);
        at = wput(web_body, at,
                  "</title></head><body><h1>what this machine "
                  "serves</h1><ul>");
        u32 n = 0;
        for (u64 i = 0; i < obj_slots(served); i++) {
            object *t = obj_get_slot(served, i);
            if (!t || !(obj_slot_rights(served, i) & CAP_READ)) continue;
            const char *label = obj_slot_name(served, i);
            if (!label) label = obj_name(t);
            at = wput(web_body, at, "<li><a href=\"/");
            at = wput_dec(web_body, at, n);
            at = wput(web_body, at, "\">");
            at = wput_name(web_body, at, label ? label : "unnamed");
            at = wput(web_body, at, "</a></li>");
            n++;
            if (at > sizeof(web_body) - 200) break;
        }
        at = wput(web_body, at, "</ul></body></html>");
        blen = at;
    } else if (served && path[0] == '/') {
        u32 want = 0;
        bool numeric = path[1] != 0;
        for (u32 i = 1; path[i]; i++) {
            if (path[i] < '0' || path[i] > '9') { numeric = false; break; }
            want = want * 10 + (u32)(path[i] - '0');
        }

        object *hit = NULL;
        if (numeric) {
            u32 n = 0;
            for (u64 i = 0; i < obj_slots(served) && !hit; i++) {
                object *t = obj_get_slot(served, i);
                if (!t || !(obj_slot_rights(served, i) & CAP_READ))
                    continue;
                if (n == want) hit = t;
                n++;
            }
        }

        if (!hit) {
            found = false;
        } else if (obj_type(hit) == TYPE_TEXT) {
            const u8 *d = (const u8 *)obj_data(hit);
            u64 size = obj_size(hit);
            u32 n = 0;
            while (d && n < size && d[n] && n < sizeof(web_body)) {
                web_body[n] = d[n];
                n++;
            }
            blen = n;
        } else if (obj_type(hit) == TYPE_PICTURE) {
            blen = web_bmp(hit, web_body, sizeof(web_body));
            if (blen) ctype = "image/bmp";
            else blen = wput(web_body, 0, "a picture too large to serve\n");
        } else if (obj_type(hit) == TYPE_BYTES) {
            ctype = "application/octet-stream";
            u64 size = obj_size(hit);
            if (size > sizeof(web_body)) size = sizeof(web_body);
            memcpy(web_body, obj_data(hit), size);
            blen = (u32)size;
        } else {
            blen = wput(web_body, 0,
                        "a list; open its entries individually\n");
        }
    } else {
        found = false;
    }

    u32 at = wput(web_all, 0, found ? "HTTP/1.0 200 OK\r\nContent-Type: "
                                    : "HTTP/1.0 404 Not Found\r\n"
                                      "Content-Type: ");
    if (!found) {
        blen = wput(web_body, 0, "nothing here\n");
        ctype = "text/plain";
    }
    at = wput(web_all, at, ctype);
    at = wput(web_all, at, "\r\nContent-Length: ");
    at = wput_dec(web_all, at, blen);
    at = wput(web_all, at, "\r\nConnection: close\r\n\r\n");
    memcpy(web_all + at, web_body, blen);
    return at + blen;
}

static void web_input(const u8 src[4], const u8 *seg, u32 len)
{
    if (len < 20) return;
    if (!system_served()) return;            /* the port is closed */

    u16 sport = ((u16)seg[0] << 8) | seg[1];
    u32 seq = ((u32)seg[4] << 24) | ((u32)seg[5] << 16) |
              ((u32)seg[6] << 8) | seg[7];
    u32 ack = ((u32)seg[8] << 24) | ((u32)seg[9] << 16) |
              ((u32)seg[10] << 8) | seg[11];
    u8  off = (u8)(seg[12] >> 4) * 4;
    u8  fl  = seg[13];
    if (off > len) return;
    (void)ack;

    if (fl & 0x04) { web.active = false; return; }

    /* A knock: answer it, taking over from any older visit. */
    if ((fl & 0x02) && !(fl & 0x10)) {
        for (u32 i = 0; i < 4; i++) web.ip[i] = src[i];
        web.port = sport;
        u32 iss;
        rand_bytes((u8 *)&iss, 4);
        web.snd_nxt = iss;
        web.rcv_nxt = seq + 1;
        web.active = true;
        web.established = false;
        web_emit(0x12, NULL, 0);             /* syn+ack */
        return;
    }

    if (!web.active || !ip4_eq(src, web.ip) || sport != web.port)
        return;

    if (fl & 0x01) {                         /* their fin: wave back */
        web.rcv_nxt = seq + 1;
        web_emit(0x10, NULL, 0);
        web.active = false;
        return;
    }

    if ((fl & 0x10) && !web.established) web.established = true;

    const u8 *data = seg + off;
    u32 dlen = len - off;
    if (dlen == 0 || !web.established) return;

    /* The first line is the whole conversation: "GET /path ...". */
    web.rcv_nxt = seq + dlen;

    char path[120];
    u32 pn = 0;
    if (dlen > 4 && data[0] == 'G' && data[1] == 'E' && data[2] == 'T' &&
        data[3] == ' ') {
        u32 i = 4;
        while (i < dlen && data[i] != ' ' && data[i] != '\r' &&
               pn < sizeof(path) - 1)
            path[pn++] = (char)data[i++];
    }
    path[pn] = 0;
    if (pn == 0) { web.active = false; return; }

    u32 total = web_answer(path);
    kprintf("web:  served %s to %u.%u.%u.%u, %u bytes\n",
            path, src[0], src[1], src[2], src[3], total);

    for (u32 o2 = 0; o2 < total; o2 += 1360) {
        u32 part = total - o2 < 1360 ? total - o2 : 1360;
        bool last = (o2 + part >= total);
        web_emit(last ? 0x19 : 0x18, web_all + o2, part);
    }
}

/* ------------------------------------------------------------------ */
/* The door: one visitor on port 22                                    */
/* ------------------------------------------------------------------ */

/* A server-side stream, for ssh to speak through. Unlike the web
 * server's single answer this is a conversation, so it keeps what
 * arrives in a ring until the protocol above drains it, keeps what
 * it sent until the visitor acknowledges it, and says a lost segment
 * again a few times before giving the visit up. One visitor at a
 * time: a second knock takes the door over, which for one person's
 * machine is the honest behaviour rather than a queue. */
#define DOOR_RX 32768
#define DOOR_TX 16384
#define DOORS   4                    /* how many visitors may stand at once */

typedef struct {
    bool active, established, peer_done, dead;
    u8   ip[4];
    u16  port;
    u32  snd_nxt, snd_una, rcv_nxt;
    u8   rx[DOOR_RX];
    u32  rx_head, rx_tail;           /* tail - head bytes are waiting */
    u8   tx[DOOR_TX];                /* byte for seq S lives at S % DOOR_TX */
    u64  sent_ns;
    u64  born_ns;
    u32  tries;
    u64  visits;
} doorconn;

static doorconn doors[DOORS];

u32 door_count(void) { return DOORS; }

static u32 door_waiting(const doorconn *d) { return d->rx_tail - d->rx_head; }

static void door_emit(doorconn *d, u8 flags, u32 seq, const u8 *data, u32 len)
{
    static u8 t[1500];
    t[0] = 0; t[1] = 22;
    t[2] = (u8)(d->port >> 8); t[3] = (u8)d->port;
    t[4] = (u8)(seq >> 24); t[5] = (u8)(seq >> 16);
    t[6] = (u8)(seq >> 8);  t[7] = (u8)seq;
    t[8] = (u8)(d->rcv_nxt >> 24); t[9] = (u8)(d->rcv_nxt >> 16);
    t[10] = (u8)(d->rcv_nxt >> 8); t[11] = (u8)d->rcv_nxt;
    t[12] = 0x50;
    t[13] = flags;
    u32 room = DOOR_RX - door_waiting(d);
    if (room > 65535) room = 65535;
    t[14] = (u8)(room >> 8); t[15] = (u8)room;
    t[16] = 0; t[17] = 0;
    t[18] = 0; t[19] = 0;
    for (u32 i = 0; i < len; i++) t[20 + i] = data[i];

    u16 c = csum(t, 20 + len, pseudo_seed(d->ip, 6, 20 + len));
    t[16] = (u8)(c >> 8); t[17] = (u8)c;
    ip_send(6, d->ip, t, 20 + len);
}

/* The connection this segment belongs to, by (address, port). */
static doorconn *door_by_peer(const u8 src[4], u16 sport)
{
    for (u32 i = 0; i < DOORS; i++)
        if (doors[i].active && ip4_eq(doors[i].ip, src) && doors[i].port == sport)
            return &doors[i];
    return NULL;
}

static void door_input(const u8 src[4], const u8 *seg, u32 len)
{
    if (len < 20) return;
    u16 sport = ((u16)seg[0] << 8) | seg[1];
    u32 seq = ((u32)seg[4] << 24) | ((u32)seg[5] << 16) |
              ((u32)seg[6] << 8) | seg[7];
    u32 ack = ((u32)seg[8] << 24) | ((u32)seg[9] << 16) |
              ((u32)seg[10] << 8) | seg[11];
    u8  off = (u8)(seg[12] >> 4) * 4;
    u8  fl  = seg[13];
    if (off > len) return;

    /* A knock: give it a free slot, its own slot again if it is a
     * repeated SYN, or the longest-idle slot when all are busy -- a
     * knock always gets in, and the one displaced is the stalest. */
    if ((fl & 0x02) && !(fl & 0x10)) {
        doorconn *d = door_by_peer(src, sport);
        if (!d) for (u32 i = 0; i < DOORS; i++)
            if (!doors[i].active) { d = &doors[i]; break; }
        if (!d) {
            d = &doors[0];
            for (u32 i = 1; i < DOORS; i++)
                if (doors[i].sent_ns < d->sent_ns) d = &doors[i];
        }
        for (u32 i = 0; i < 4; i++) d->ip[i] = src[i];
        d->port = sport;
        u32 iss;
        rand_bytes((u8 *)&iss, 4);
        d->rcv_nxt = seq + 1;
        d->snd_nxt = iss + 1;
        d->snd_una = iss;
        d->rx_head = d->rx_tail = 0;
        d->active = true;
        d->established = false;
        d->peer_done = false;
        d->dead = false;
        d->tries = 0;
        d->born_ns = time_ns();
        d->visits++;
        door_emit(d, 0x12, iss, NULL, 0);        /* syn+ack */
        return;
    }

    doorconn *d = door_by_peer(src, sport);
    if (!d) return;

    if (fl & 0x04) { d->dead = true; d->active = false; return; }

    if (fl & 0x10) {
        if ((i32)(ack - d->snd_una) > 0 &&
            (i32)(d->snd_nxt - ack) >= 0) {
            d->snd_una = ack;
            d->sent_ns = time_ns();
            d->tries = 0;
        }
        d->established = true;
    }

    const u8 *data = seg + off;
    u32 dlen = len - off;
    bool spoke = false;

    if (dlen && seq == d->rcv_nxt) {
        u32 room = DOOR_RX - door_waiting(d);
        u32 take = dlen < room ? dlen : room;
        for (u32 i = 0; i < take; i++)
            d->rx[(d->rx_tail + i) % DOOR_RX] = data[i];
        d->rx_tail += take;
        d->rcv_nxt += take;
        spoke = true;
    } else if (dlen) {
        spoke = true;                /* out of place: say where we are */
    }

    if ((fl & 0x01) && seq + dlen == d->rcv_nxt) {
        d->rcv_nxt += 1;
        d->peer_done = true;
        spoke = true;
    }

    if (spoke) door_emit(d, 0x10, d->snd_nxt, NULL, 0);
}

/* Says unacknowledged bytes again when they have gone quiet too
 * long, and gives a visit up when saying them does no good. */
static void door_service(void)
{
    for (u32 i = 0; i < DOORS; i++) {
        doorconn *d = &doors[i];
        if (!d->active || d->dead) continue;
        if (d->snd_nxt == d->snd_una) continue;
        if (time_ns() - d->sent_ns < 600000000ULL) continue;

        if (d->tries >= 8) {
            d->dead = true;
            d->active = false;
            kprintf("door: a client timed out; session closed\n");
            continue;
        }
        u32 n = d->snd_nxt - d->snd_una;
        if (n > 1200) n = 1200;
        u8 buf[1200];
        for (u32 k = 0; k < n; k++) buf[k] = d->tx[(d->snd_una + k) % DOOR_TX];
        door_emit(d, 0x18, d->snd_una, buf, n);
        d->sent_ns = time_ns();
        d->tries++;
    }
}

bool door_alive(u32 c)    { return c < DOORS && doors[c].active && !doors[c].dead; }
bool door_finished(u32 c) { return c < DOORS && doors[c].peer_done; }
u64  door_visit(u32 c)    { return c < DOORS ? doors[c].visits : 0; }

void door_peer(u32 c, u8 ip[4])
{
    if (c >= DOORS) { for (u32 i = 0; i < 4; i++) ip[i] = 0; return; }
    for (u32 i = 0; i < 4; i++) ip[i] = doors[c].ip[i];
}

u32 door_room(u32 c)
{
    if (c >= DOORS) return 0;
    doorconn *d = &doors[c];
    if (!d->active || d->dead || !d->established) return 0;
    return DOOR_TX - (d->snd_nxt - d->snd_una);
}

u32 door_read(u32 c, u8 *buf, u32 max)
{
    if (c >= DOORS) return 0;
    doorconn *d = &doors[c];
    u32 n = door_waiting(d);
    if (n > max) n = max;
    for (u32 i = 0; i < n; i++) buf[i] = d->rx[(d->rx_head + i) % DOOR_RX];
    d->rx_head += n;
    return n;
}

bool door_write(u32 c, const u8 *buf, u32 len)
{
    if (c >= DOORS || len > door_room(c)) return false;
    doorconn *d = &doors[c];
    for (u32 i = 0; i < len; i++) d->tx[(d->snd_nxt + i) % DOOR_TX] = buf[i];
    u32 at = 0;
    while (at < len) {
        u32 seg = len - at;
        if (seg > 1200) seg = 1200;
        door_emit(d, 0x18, d->snd_nxt + at, buf + at, seg);
        at += seg;
    }
    if (d->snd_nxt == d->snd_una) { d->sent_ns = time_ns(); d->tries = 0; }
    d->snd_nxt += len;
    return true;
}

void door_close(u32 c)
{
    if (c >= DOORS) return;
    doorconn *d = &doors[c];
    if (!d->active) return;
    if (d->established && !d->dead)
        door_emit(d, 0x11, d->snd_nxt, NULL, 0);  /* fin+ack */
    else if (!d->dead)
        door_emit(d, 0x14, d->snd_nxt, NULL, 0);  /* rst+ack */
    d->active = false;
}

static u16 tcp_port_next = 49200;

/* Sends what is unsent and waits for the acknowledgement, resending a
 * few times. The whole client fits in "say it until they say so". */
static bool tcp_say(u8 flags, const void *payload, u32 len, u32 grows)
{
    u32 want = tcb.snd_nxt + grows;
    for (u32 try = 0; try < 6; try++) {
        tcp_emit(flags, payload, len);
        u64 from = time_ns();
        while (time_ns() - from < SECOND / 2) {
            net_breathe();
            if (tcb.reset) return false;
            if ((i32)(tcb.snd_una - want) >= 0) {
                tcb.snd_nxt = want;
                return true;
            }
            if ((flags & 0x02) && tcb.established) {
                tcb.snd_nxt = want;      /* syn counted by their ack */
                return true;
            }
        }
    }
    return false;
}

/* --- the tcp stream, for http and tls alike ----------------------- */

bool tcp_open(const u8 addr[4], u16 port)
{
    memset(&tcb, 0, sizeof(tcb));
    tcb.active = true;
    for (u32 i = 0; i < 4; i++) tcb.remote_ip[i] = addr[i];
    tcb.local_port = tcp_port_next++;
    tcb.remote_port = port;
    tcb.snd_nxt = (u32)(time_ns() & 0x7FFFFFFF);
    tcb.snd_una = tcb.snd_nxt;

    if (!tcp_say(0x02, NULL, 0, 1)) { tcb.active = false; return false; }
    return true;
}

bool tcp_write(const u8 *buf, u32 len)
{
    u32 off = 0;
    while (off < len) {
        u32 seg = len - off;
        if (seg > 1200) seg = 1200;      /* under the smallest sane mtu */
        if (!tcp_say(0x18, buf + off, seg, seg)) return false;
        off += seg;
    }
    return true;
}

/* Drains up to max application bytes, pumping the wire until some
 * arrive, the peer finishes, or the well runs dry. Returns the count,
 * 0 at a clean end, and -1 on reset or a stall with nothing waiting. */
i32 tcp_read(u8 *buf, u32 max)
{
    u64 last = time_ns();
    while (ring_used() == 0) {
        if (tcb.reset) return -1;
        if (tcb.peer_done) return 0;
        net_breathe();
        if (ring_used()) break;
        if (time_ns() - last > 8 * SECOND) return -1;
    }
    u32 n = ring_used();
    if (n > max) n = max;
    for (u32 i = 0; i < n; i++) buf[i] = tcb.ring[(tcb.head + i) % TCP_RING];
    tcb.head += n;
    return (i32)n;
}

bool tcp_eof(void)  { return tcb.peer_done || tcb.reset; }

void tcp_close(void)
{
    if (tcb.active && !tcb.peer_done && !tcb.reset)
        tcp_emit(0x14, NULL, 0);         /* rst+ack: we are done here */
    tcb.active = false;
}

static bool http_fetch(const u8 *addr, const char *host, u32 hlen,
                       const char *path, u32 plen,
                       u8 *body, u32 body_max, u32 *body_len)
{
    if (!tcp_open(addr, 80)) return false;

    char req[420];
    u32 at = 0;
    const char *a = "GET ";
    while (*a) req[at++] = *a++;
    if (plen == 0) req[at++] = '/';
    for (u32 i = 0; i < plen && at < 300; i++) req[at++] = path[i];
    a = " HTTP/1.0\r\nHost: ";
    while (*a) req[at++] = *a++;
    for (u32 i = 0; i < hlen && at < 360; i++) req[at++] = host[i];
    a = "\r\nUser-Agent: erebus/0.1\r\nConnection: close\r\n\r\n";
    while (*a) req[at++] = *a++;

    if (!tcp_write((const u8 *)req, at)) { tcp_close(); return false; }

    /* Their answer, until they finish or the well runs dry. */
    u32 len = 0;
    while (len < body_max) {
        i32 got = tcp_read(body + len, body_max - len);
        if (got <= 0) break;
        len += (u32)got;
    }

    *body_len = len;
    tcp_close();
    return len > 0;
}

/* ------------------------------------------------------------------ */
/* The service: a text goes out, the same text comes back fuller       */
/* ------------------------------------------------------------------ */

static u8 page[65536];

static u32 write_words(u8 *d, u32 at, u32 size, const char *s)
{
    while (*s && at < size - 1) d[at++] = (u8)*s++;
    return at;
}

/* Splits an ask -- "host", "host/path", with or without a scheme in
 * front -- into its parts, and says whether the scheme was https, so
 * the fetch knows to seal the channel. A bare host is taken as plain
 * http, which keeps the http-only corners of the web reachable; a
 * page that wants a seal says https, and now it gets one. */
static void split_ask(const char *ask, u32 alen,
                      char *host, u32 hmax, u32 *hlen,
                      char *path, u32 pmax, u32 *plen, bool *secure)
{
    u32 at = 0;
    if (secure) *secure = false;
    if (alen >= 8 && ask[0]=='h' && ask[1]=='t' && ask[2]=='t' &&
        ask[3]=='p' && ask[4]=='s' && ask[5]==':' &&
        ask[6]=='/' && ask[7]=='/') {
        at = 8;
        if (secure) *secure = true;
    } else if (alen >= 7 && ask[0]=='h' && ask[1]=='t' && ask[2]=='t' &&
               ask[3]=='p' && ask[4]==':' && ask[5]=='/' && ask[6]=='/') {
        at = 7;
    }

    *hlen = 0;
    *plen = 0;
    while (at < alen && ask[at] != '/' && ask[at] != ' ' &&
           *hlen < hmax - 1)
        host[(*hlen)++] = ask[at++];
    while (at < alen && ask[at] != ' ' && *plen < pmax - 1)
        path[(*plen)++] = ask[at++];
}

static void fetch_into(object *o)
{
    u8 *d = (u8 *)obj_data(o);
    u64 size = obj_size(o);
    if (!d || size < 64) return;

    /* The first line is the ask; everything after it will be ours. */
    u32 ask_end = 0;
    while (ask_end < size && d[ask_end] && d[ask_end] != '\n') ask_end++;

    char host[128];
    char path[256];
    u32 hlen = 0, plen = 0;
    bool secure = false;
    split_ask((const char *)d, ask_end, host, sizeof(host), &hlen,
              path, sizeof(path), &plen, &secure);

    u32 out = ask_end;
    if (out < size) d[out] = '\n';
    out++;

    if (hlen == 0) return;

    const char *said = NULL;
    u8 addr[4];
    u32 got = 0;
    u32 body = 0;

    if (!nic_up() || !gateway_find()) {
        said = "no network card.\n";
        goto answer;
    }

    /* Fetch, and follow where it points: a page that moved says so
     * with a number and a Location line, and stopping there would
     * show the reader furniture tags instead of the room. A secure ask
     * goes through tls; the others go plainly. */
    for (u32 hop = 0; hop < 5; hop++) {
        if (!dns_resolve(host, hlen, addr)) {
            said = "dns: no such name.\n";
            break;
        }
        bool ok = secure
                ? tls_get(addr, host, hlen, path, plen, page, sizeof(page), &got)
                : http_fetch(addr, host, hlen, path, plen, page, sizeof(page), &got);
        if (!ok) {
            said = secure ? "tls: the connection did not open.\n"
                          : "no answer.\n";
            break;
        }

        /* The status line: HTTP/1.x NNN */
        u32 code = 0;
        if (got > 12 && page[0] == 'H')
            code = (u32)(page[9] - '0') * 100 +
                   (u32)(page[10] - '0') * 10 + (u32)(page[11] - '0');

        body = 0;
        for (u32 i = 0; i + 3 < got; i++) {
            if (page[i] == '\r' && page[i+1] == '\n' &&
                page[i+2] == '\r' && page[i+3] == '\n') {
                body = i + 4;
                break;
            }
        }

        bool moved = (code == 301 || code == 302 || code == 303 ||
                      code == 307 || code == 308);
        if (!moved) break;

        /* Location: ... somewhere in the headers. */
        char where[256];
        u32 wlen = 0;
        for (u32 i = 0; i + 9 < body; i++) {
            if ((page[i]=='l' || page[i]=='L') &&
                (page[i+1]=='o' || page[i+1]=='O') &&
                page[i+2]=='c' && page[i+3]=='a' && page[i+4]=='t' &&
                page[i+5]=='i' && page[i+6]=='o' && page[i+7]=='n' &&
                page[i+8]==':') {
                u32 j = i + 9;
                while (j < body && page[j] == ' ') j++;
                while (j < body && page[j] != '\r' && page[j] != '\n' &&
                       wlen < sizeof(where) - 1)
                    where[wlen++] = (char)page[j++];
                break;
            }
        }
        if (wlen == 0) break;                    /* moved, but mute */

        if (where[0] == '/') {                   /* same house, new room */
            plen = 0;
            for (u32 i = 0; i < wlen && plen < sizeof(path) - 1; i++)
                path[plen++] = where[i];
        } else {
            split_ask(where, wlen, host, sizeof(host), &hlen,
                      path, sizeof(path), &plen, &secure);
            if (hlen == 0) break;
        }
        got = 0;
    }

answer:
    if (said) {
        out = write_words(d, out, (u32)size, said);
        for (u64 i = out; i < size; i++) d[i] = 0;
        obj_touch(o);
        journal_says("net", "the page was not fetched");
        return;
    }

    for (u32 i = body; i < got && out < size - 1; i++) {
        u8 c = page[i];
        if (c == '\r') continue;
        d[out++] = (c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7F))
                 ? c : '.';
    }
    for (u64 i = out; i < size; i++) d[i] = 0;

    last_secure = secure;
    last_verified = secure && tls_last_verified();

    obj_touch(o);
    journal_says("net", secure ? "a page arrived (tls)"
                               : "a page arrived");
}

static void net_thread(void *arg)
{
    (void)arg;
    dhcp_run();

    /* The loop is a walk, not a wait: fetch requests are taken when
     * they queue, but the wire is watched between them -- the pipe
     * receives whenever the other machine cares to send, which is
     * exactly the part a blocking receive would sleep through. */
    for (;;) {
        message m;
        if (port_try_receive(kdom, service_receive, &m)) {
            if (m.ncaps >= 1 && m.tag == 0x4B524F57ULL /* "WORK" */) {
                /* A task for the desk. The capability decides how the
                 * answer can come back: writable, and it is written
                 * into the task; readable only, and it goes to
                 * arrivals. */
                object *o = cap_lookup(kdom, m.caps[0],
                                       CAP_READ | CAP_WRITE);
                bool writable = (o != NULL);
                if (!o) o = cap_lookup(kdom, m.caps[0], CAP_READ);
                if (o && obj_type(o) == TYPE_TEXT)
                    pipe_ask(o, writable);
                cap_revoke(kdom, m.caps[0]);
                if (m.ncaps > 1) cap_revoke(kdom, m.caps[1]);
            } else if (m.ncaps >= 1) {
                /* The capability names the object and carries the
                 * right; the lookup is the only door, and it opens
                 * for read-and-write or not at all. */
                object *o = cap_lookup(kdom, m.caps[0],
                                       CAP_READ | CAP_WRITE);
                if (o && obj_type(o) == TYPE_TEXT) fetch_into(o);
                cap_revoke(kdom, m.caps[0]);
                if (m.ncaps > 1) cap_revoke(kdom, m.caps[1]);
            }
        }

        /* A newly claimed address takes effect as it is typed, like
         * every other setting: the person writes who the machine is,
         * and the machine is it from then on. */
        u8 claimed[4];
        if (settings_address(claimed) && !ip4_eq(claimed, ip_ours)) {
            for (u32 i = 0; i < 4; i++) ip_ours[i] = claimed[i];
            ip_gw[0] = claimed[0]; ip_gw[1] = claimed[1];
            ip_gw[2] = claimed[2]; ip_gw[3] = 2;
            have_gw = false;
            for (u32 i = 0; i < ARP_CACHE; i++) neigh[i].used = false;
            kprintf("net:  %u.%u.%u.%u by claim\n",
                    ip_ours[0], ip_ours[1], ip_ours[2], ip_ours[3]);
        }

        /* The clock from the net: soon after the wires are up, then
         * once an hour. A wire with no time server on it costs a few
         * quiet seconds and is left in peace for the next hour. */
        static u64 sntp_next_ns;
        if (configured && time_ns() >= sntp_next_ns) {
            sntp_next_ns = time_ns() + 3600ULL * SECOND;
            sntp_ask();
        }

        /* The air: the station's clock, and a link that changed
         * underneath -- joined, or left -- asks for its address anew,
         * as a cable plugged in elsewhere would. */
        wifi_poll();
        if (relink_wanted) {
            relink_wanted = false;
            configured = false;
            have_gw = false;
            for (u32 i = 0; i < ARP_CACHE; i++) neigh[i].used = false;
            wait_dhcp.have_offer = false;
            wait_dhcp.have_ack = false;
            dhcp_run();
        }

        pipe_service();
        door_service();
        ssh_service();
        net_breathe();
    }
}

void net_relink(void) { relink_wanted = true; }

void net_prepare(domain *kernel)
{
    kdom = kernel;
    pipe_prepare(kernel);
    service_port = port_create(16);
    if (!service_port) return;
    obj_set_name(service_port, "the wire");
    service_receive = cap_insert(kdom, service_port,
                                 CAP_READ | CAP_CALL | CAP_GRANT);
}

bool net_start(void)
{
    if (!service_port) return false;

    /* The drivers, in the order of how much silicon each one covers
     * in practice, and preferring a card with a cable in it. */
    if (!nic_start())
        return false;

    /* The seal proves its arithmetic before it is offered. A failure
     * here does not stop the network -- plain http still works -- it
     * only means tls and the pipe stay unavailable, which is the
     * honest outcome of primitives that cannot vouch for themselves. */
    extern bool tls_schedule_selftest(void);
    crypto_good = crypto_selftest();
    if (crypto_good && tls_schedule_selftest())
        kprintf("tls:  self test passed -- sha256, x25519, aes-128-gcm, "
                "and the 1.3 key schedule\n");
    else
        kprintf("tls:  self test FAILED -- https disabled\n");
    if (crypto_good)
        kprintf("pipe: ready; the handshake is signed with the door key, "
                "and a known address must answer with its known key\n");

    wifi_init();
    thread_create("net", net_thread, NULL, kdom);
    running = true;

    kprintf("net:  client only; http, and https without certificate "
            "verification; one connection at a time\n");
    return true;
}
