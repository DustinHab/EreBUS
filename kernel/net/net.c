/*
 * net.c -- enough of the internet to fetch a page.
 *
 * This is a client, not a stack. One thread, one conversation at a
 * time, and only the protocols the errand needs: ARP to find the
 * gateway, DNS over UDP to turn a name into an address, TCP carrying
 * one HTTP/1.0 request and its answer. Nothing listens. Nothing runs
 * concurrently. A lost packet is retransmitted a few times and then
 * the errand fails with its reason written where the answer would
 * have gone.
 *
 * The machine's own address is asked for, not assumed: DHCP runs
 * once when the service comes up, and whatever network answers --
 * the emulator's built-in landlord or a real router -- decides who
 * this machine is, where the way out stands, and who answers names.
 * A network that stays silent gets the emulator's well-known
 * defaults, so the tests neither wait nor lie.
 *
 * Security is the shape of the thing, not a check inside it: the one
 * way to reach this code is a capability to its port, requests name
 * their object by capability, and the service writes only where that
 * capability may write. TLS is honestly absent -- what travels here
 * travels readable, and the page says "http" so nobody mistakes it.
 */
#include <eb/net.h>
#include <eb/msg.h>
#include <eb/thread.h>
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

static domain    *kdom;
static object    *service_port;
static cap_handle service_receive;
static bool       running;

object *net_port(void) { return service_port; }
bool    net_up(void)   { return running; }

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

/* The one tcp conversation. */
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

    u8  *body;                      /* where arriving bytes go */
    u32  body_max, body_len;
} tcb;

static void tcp_emit(u8 flags, const void *payload, u32 len);

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
            u32 room = tcb.body_max - tcb.body_len;
            u32 take = dlen < room ? dlen : room;
            for (u32 i = 0; i < take; i++)
                tcb.body[tcb.body_len + i] = data[i];
            tcb.body_len += take;
            tcb.rcv_nxt += dlen;                        /* count it all */
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
            } else if (p[7] == 2 && ip4_eq(p + 14, ip_gw)) {
                for (u32 i = 0; i < 6; i++) mac_gw[i] = p[8 + i];
                have_gw = true;
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
            tcp_input(inner, ilen);
        } else if (proto == 17 && ilen >= 8) {
            u16 sport = ((u16)inner[0] << 8) | inner[1];
            u16 dport = ((u16)inner[2] << 8) | inner[3];
            if (sport == 53) dns_input(inner + 8, ilen - 8);
            if (dport == 68) dhcp_input(inner + 8, ilen - 8);
        } else if (proto == 1 && ilen >= 8 && inner[0] == 8) {
            /* An echo request: answer it. Being pingable costs one
             * buffer and makes the whole path checkable from outside. */
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
static void net_breathe(void)
{
    net_pump();
    sched_yield();
}

/* ------------------------------------------------------------------ */
/* Sending upward: ip, udp, tcp                                        */
/* ------------------------------------------------------------------ */

static u16 ip_ident = 1;

/* Builds and sends one ip packet to `dst` via the gateway. */
static bool ip_send(u8 proto, const u8 *dst, const u8 *payload, u32 len)
{
    if (!have_gw) return false;

    u8 *f = frame_out;
    eth_head(f, mac_gw, ETH_IP);
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
    kprintf("net:  nobody leases here; assuming the emulator's "
            "10.0.2.15\n");
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

static bool http_fetch(const u8 *addr, const char *host, u32 hlen,
                       const char *path, u32 plen,
                       u8 *body, u32 body_max, u32 *body_len)
{
    memset(&tcb, 0, sizeof(tcb));
    tcb.active = true;
    for (u32 i = 0; i < 4; i++) tcb.remote_ip[i] = addr[i];
    tcb.local_port = tcp_port_next++;
    tcb.remote_port = 80;
    tcb.snd_nxt = (u32)(time_ns() & 0x7FFFFFFF);
    tcb.snd_una = tcb.snd_nxt;
    tcb.body = body;
    tcb.body_max = body_max;

    if (!tcp_say(0x02, NULL, 0, 1)) { tcb.active = false; return false; }

    char req[420];
    u32 at = 0;
    const char *a = "GET ";
    while (*a) req[at++] = *a++;
    if (plen == 0) req[at++] = '/';
    for (u32 i = 0; i < plen && at < 300; i++) req[at++] = path[i];
    a = " HTTP/1.0\r\nHost: ";
    while (*a) req[at++] = *a++;
    for (u32 i = 0; i < hlen && at < 380; i++) req[at++] = host[i];
    a = "\r\nConnection: close\r\n\r\n";
    while (*a) req[at++] = *a++;

    if (!tcp_say(0x18, req, at, at)) { tcb.active = false; return false; }

    /* Their answer, until they finish or the well runs dry. */
    u64 last_growth = time_ns();
    u32 seen = 0;
    while (!tcb.peer_done && !tcb.reset && tcb.body_len < body_max) {
        net_breathe();
        if (tcb.body_len != seen) { seen = tcb.body_len; last_growth = time_ns(); }
        if (time_ns() - last_growth > 8 * SECOND) break;
    }

    bool ok = tcb.body_len > 0;
    *body_len = tcb.body_len;
    if (!tcb.peer_done && !tcb.reset) tcp_emit(0x14, NULL, 0);  /* rst+ack */
    tcb.active = false;
    return ok;
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
    u32 at = 0;

    /* "http://" may be said or left off; nothing else may. */
    if (ask_end >= 7 && d[0]=='h' && d[1]=='t' && d[2]=='t' && d[3]=='p' &&
        d[4] == ':' && d[5] == '/' && d[6] == '/')
        at = 7;
    if (ask_end >= 8 && at == 0 && d[0]=='h' && d[1]=='t' && d[2]=='t' &&
        d[3]=='p' && d[4]=='s') {
        u32 w = ask_end + 1;
        if (w > size) w = (u32)size;
        w = write_words(d, w, (u32)size,
                        "this door speaks plain http only.\n");
        for (u64 i = w; i < size; i++) d[i] = 0;
        return;
    }

    while (at < ask_end && d[at] != '/' && d[at] != ' ' &&
           hlen < sizeof(host) - 1)
        host[hlen++] = (char)d[at++];
    while (at < ask_end && d[at] != ' ' && plen < sizeof(path) - 1)
        path[plen++] = (char)d[at++];

    u32 out = ask_end;
    if (out < size) d[out] = '\n';
    out++;

    if (hlen == 0) return;

    const char *said = NULL;
    u8 addr[4];
    u32 got = 0;

    if (!nic_up() || !gateway_find()) {
        said = "the wire goes nowhere.\n";
    } else if (!dns_resolve(host, hlen, addr)) {
        said = "the name service does not know it.\n";
    } else if (!http_fetch(addr, host, hlen, path, plen,
                           page, sizeof(page), &got)) {
        said = "no answer from there.\n";
    }

    if (said) {
        out = write_words(d, out, (u32)size, said);
        for (u64 i = out; i < size; i++) d[i] = 0;
        journal_says("net", "a page did not come");
        return;
    }

    /* Step over the http headers; the page is what follows them. */
    u32 body = 0;
    for (u32 i = 0; i + 3 < got; i++) {
        if (page[i] == '\r' && page[i+1] == '\n' &&
            page[i+2] == '\r' && page[i+3] == '\n') {
            body = i + 4;
            break;
        }
    }

    for (u32 i = body; i < got && out < size - 1; i++) {
        u8 c = page[i];
        if (c == '\r') continue;
        d[out++] = (c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7F))
                 ? c : '.';
    }
    for (u64 i = out; i < size; i++) d[i] = 0;

    journal_says("net", got + 64 > (u32)size
                 ? "a page came; the text holds what fit"
                 : "a page came");
}

static void net_thread(void *arg)
{
    (void)arg;
    dhcp_run();

    for (;;) {
        message m;
        const char *from = NULL;
        if (!port_receive_labelled(kdom, service_receive, &m, &from)) {
            sched_yield();
            continue;
        }
        if (m.ncaps < 1) continue;

        /* The capability names the object and carries the right; the
         * lookup is the only door, and it opens for read-and-write or
         * not at all. */
        object *o = cap_lookup(kdom, m.caps[0], CAP_READ | CAP_WRITE);
        if (o && obj_type(o) == TYPE_TEXT) fetch_into(o);
        cap_revoke(kdom, m.caps[0]);
        if (m.ncaps > 1) cap_revoke(kdom, m.caps[1]);
    }
}

void net_prepare(domain *kernel)
{
    kdom = kernel;
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
     * in practice. The first to find its chip carries the traffic. */
    if (!e1000_init() && !rtl8169_init() && !rtl8139_init())
        return false;

    thread_create("net", net_thread, NULL, kdom);
    running = true;

    kprintf("net:  outbound only, plain http, one errand at a time\n");
    return true;
}
