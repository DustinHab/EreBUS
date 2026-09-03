/*
 * wifi.c -- a wireless station.
 *
 * Above it the stack sees the same frames a cable would carry. Below
 * it a radio: three verbs, send a frame into the air, hand up what
 * arrived, name the antenna. In between, here: hearing the networks
 * around, joining one the WPA2 way, and sealing every data frame.
 *
 * The WPA2 way, in full: the passphrase and the network's name become
 * a 32-byte master key by 4096 rounds of HMAC-SHA1; the four-way
 * handshake mixes that with two fresh nonces and both addresses into
 * the session keys -- one to check the handshake itself, one to
 * unwrap the group key the access point sends, one to seal the data;
 * and every frame after that is AES-CCM with a packet number that
 * never repeats, so a frame recorded cannot be played back.
 *
 * The radio the machine has today is the test bench's: 802.11 frames
 * carried inside ethernet frames down the wire to a virtual access
 * point on the host. It is a radio in every way but the antenna. A
 * real chip's driver -- and every chip needs its own, with the
 * maker's firmware -- plugs in under the same three verbs.
 */
#include <eb/wifi.h>
#include <eb/net.h>
#include <eb/crypto.h>
#include <eb/settings.h>
#include <eb/journal.h>
#include <eb/time.h>
#include <eb/fmt.h>
#include <eb/string.h>

#define ETH_RADIO 0x88B5
#define NET_MAX   16
#define SECOND    1000000000ULL

#define FC_ASSOC_REQ  0x0000
#define FC_ASSOC_RESP 0x0010
#define FC_PROBE_REQ  0x0040
#define FC_PROBE_RESP 0x0050
#define FC_BEACON     0x0080
#define FC_DISASSOC   0x00A0
#define FC_AUTH       0x00B0
#define FC_DEAUTH     0x00C0
#define FC_DATA       0x0008
#define FC_TODS       0x0100
#define FC_FROMDS     0x0200
#define FC_PROTECTED  0x4000

typedef struct {
    bool used;
    char ssid[33];
    u8   bssid[6];
    u8   channel;
    i8   rssi;
    u8   security;
    u64  seen_ns;
} wnet;

static wnet nets[NET_MAX];

enum { S_IDLE, S_AUTH, S_ASSOC, S_HANDSHAKE, S_JOINED };

static struct {
    u8      state;
    char    ssid[33];
    u8      bssid[6];
    u8      channel;
    char    pass[64];
    bool    open;
    u8      pmk[32], ptk[48], gtk[16], anonce[32], snonce[32];
    u8      replay[8];
    aes_key tk, gk;
    bool    have_gk;
    u64     pn_out, pn_in, pn_in_group;
    u16     seq;
    u64     step_ns;
    u32     tries;
    char    why[80];
    u64     sealed_out, sealed_in;

    /* asked for from another thread, taken up by the net thread */
    bool    want, want_leave;
    char    want_ssid[33], want_pass[64];
    u64     auto_ns, probe_ns;
} st;

static bool crypto_ok;

/* Frames unsealed and waiting for the stack. */
#define INQ 4
static u8  inq[INQ][1600];
static u32 inq_len[INQ];
static u32 inq_head, inq_tail;

static const u8 own_rsn[22] = { 0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04,
                                0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04,
                                0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00 };
static const u8 everyone[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ------------------------------------------------------------------ */
/* Bytes                                                               */
/* ------------------------------------------------------------------ */

static u16 rd16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }
static void wr16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static bool same6(const u8 *a, const u8 *b) { return memcmp(a, b, 6) == 0; }

static void cpy_ssid(char *dst, const char *src)
{
    u32 i = 0;
    while (src[i] && i < 32) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* ------------------------------------------------------------------ */
/* The radio                                                           */
/* ------------------------------------------------------------------ */

static bool radio_send(const u8 *frame, u32 len)
{
    static u8 out[1600];
    if (len + 18 > sizeof(out)) return false;
    memcpy(out, everyone, 6);
    memcpy(out + 6, nic_mac(), 6);
    out[12] = (u8)(ETH_RADIO >> 8);
    out[13] = (u8)ETH_RADIO;
    out[14] = 'R';
    out[15] = 1;
    out[16] = 0;
    out[17] = st.channel;
    memcpy(out + 18, frame, len);
    return nic_send_raw(out, 18 + len);
}

bool wifi_radio_present(void)   { return nic_up(); }
const char *wifi_radio_name(void)
{
    return nic_up() ? "no radio chip; the wire carries the test bench's air"
                    : "no radio, and no wire";
}

/* ------------------------------------------------------------------ */
/* Management frames                                                   */
/* ------------------------------------------------------------------ */

static u32 mgmt_head(u8 *f, u16 fc, const u8 *a1, const u8 *a2, const u8 *a3)
{
    wr16(f, fc);
    wr16(f + 2, 0);
    memcpy(f + 4, a1, 6);
    memcpy(f + 10, a2, 6);
    memcpy(f + 16, a3, 6);
    wr16(f + 22, (u16)(st.seq++ << 4));
    return 24;
}

static const u8 rates_ie[6] = { 0x01, 0x04, 0x82, 0x84, 0x8B, 0x96 };

static void send_probe(void)
{
    u8 f[64];
    u32 n = mgmt_head(f, FC_PROBE_REQ, everyone, nic_mac(), everyone);
    f[n++] = 0; f[n++] = 0;                          /* any name */
    memcpy(f + n, rates_ie, 6); n += 6;
    radio_send(f, n);
}

static void send_auth(void)
{
    u8 f[64];
    u32 n = mgmt_head(f, FC_AUTH, st.bssid, nic_mac(), st.bssid);
    wr16(f + n, 0); wr16(f + n + 2, 1); wr16(f + n + 4, 0);   /* open, first, fine */
    radio_send(f, n + 6);
}

static void send_assoc(void)
{
    u8 f[128];
    u32 n = mgmt_head(f, FC_ASSOC_REQ, st.bssid, nic_mac(), st.bssid);
    wr16(f + n, 0x0411); n += 2;                     /* ess, short preamble, short slot */
    wr16(f + n, 10); n += 2;                         /* listen interval */
    u32 sl = 0;
    while (st.ssid[sl]) sl++;
    f[n++] = 0; f[n++] = (u8)sl;
    memcpy(f + n, st.ssid, sl); n += sl;
    memcpy(f + n, rates_ie, 6); n += 6;
    if (!st.open) { memcpy(f + n, own_rsn, 22); n += 22; }
    radio_send(f, n);
}

static void send_deauth(u16 reason)
{
    u8 f[32];
    u32 n = mgmt_head(f, FC_DEAUTH, st.bssid, nic_mac(), st.bssid);
    wr16(f + n, reason);
    radio_send(f, n + 2);
}

/* ------------------------------------------------------------------ */
/* Keys                                                                */
/* ------------------------------------------------------------------ */

static void derive_ptk(void)
{
    const u8 *us = nic_mac();
    u8 data[76];
    bool us_first = memcmp(us, st.bssid, 6) < 0;
    memcpy(data, us_first ? us : st.bssid, 6);
    memcpy(data + 6, us_first ? st.bssid : us, 6);
    bool a_first = memcmp(st.anonce, st.snonce, 32) < 0;
    memcpy(data + 12, a_first ? st.anonce : st.snonce, 32);
    memcpy(data + 44, a_first ? st.snonce : st.anonce, 32);
    prf_sha1(st.pmk, 32, "Pairwise key expansion", data, 76, st.ptk, 48);
}

/* An EAPOL key frame in the clear: the LLC head, the EAPOL head, the
 * descriptor, the key data; the MIC over all of it from the EAPOL
 * head on, with the MIC's own place zeroed. */
static void send_eapol(u16 info, const u8 *nonce, const u8 *kd, u32 kdlen)
{
    u8 f[256];
    u32 n = mgmt_head(f, FC_DATA | FC_TODS, st.bssid, nic_mac(), st.bssid);
    static const u8 llc[8] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E };
    memcpy(f + n, llc, 8); n += 8;
    u8 *e = f + n;                                   /* the EAPOL frame */
    u32 body = 95 + kdlen;
    e[0] = 2; e[1] = 3;                              /* version, key */
    e[2] = (u8)(body >> 8); e[3] = (u8)body;
    u8 *k = e + 4;
    memset(k, 0, 95);
    k[0] = 2;                                        /* rsn descriptor */
    k[1] = (u8)(info >> 8); k[2] = (u8)info;
    memcpy(k + 5, st.replay, 8);
    if (nonce) memcpy(k + 13, nonce, 32);
    k[93] = (u8)(kdlen >> 8); k[94] = (u8)kdlen;
    if (kdlen) memcpy(k + 95, kd, kdlen);
    u8 mic[20];
    hmac_sha1(st.ptk, 16, e, 4 + body, mic);
    memcpy(k + 77, mic, 16);
    radio_send(f, n + 4 + body);
}

/* ------------------------------------------------------------------ */
/* Failing, leaving                                                    */
/* ------------------------------------------------------------------ */

static void keys_gone(void)
{
    memset(st.ptk, 0, 48);
    memset(st.gtk, 0, 16);
    st.have_gk = false;
    st.pn_out = st.pn_in = st.pn_in_group = 0;
}

static void fail(const char *why)
{
    bool was = st.state == S_JOINED;
    st.state = S_IDLE;
    keys_gone();
    u32 i = 0;
    while (why[i] && i < sizeof(st.why) - 1) { st.why[i] = why[i]; i++; }
    st.why[i] = 0;
    kprintf("wifi: %s\n", why);
    journal_says("wifi", why);
    if (was) net_relink();
}

void wifi_leave(void)  { st.want_leave = true; }

/* ------------------------------------------------------------------ */
/* Hearing                                                             */
/* ------------------------------------------------------------------ */

static void heard(const u8 *f, u32 len, i8 rssi, u8 channel)
{
    if (len < 36) return;
    const u8 *bssid = f + 16;
    u16 capab = rd16(f + 34);
    const u8 *ie = f + 36;
    u32 left = len - 36;

    char ssid[33];
    ssid[0] = 0;
    u8 security = (capab & 0x0010) ? WIFI_OTHER : WIFI_OPEN;
    while (left >= 2) {
        u8 id = ie[0], l = ie[1];
        if (2u + l > left) break;
        const u8 *v = ie + 2;
        if (id == 0 && l <= 32) { memcpy(ssid, v, l); ssid[l] = 0; }
        else if (id == 3 && l >= 1) channel = v[0];
        else if (id == 48 && l >= 8 && rd16(v) == 1) {
            /* the security element: group cipher, the pairwise ones,
             * the key managements; a passphrase and ccmp among them
             * is what this station speaks */
            u32 at = 6;
            bool ccmp = false, psk = false;
            u16 np = at + 2 <= l ? rd16(v + at) : 0;
            at += 2;
            for (u16 i = 0; i < np && at + 4 <= l; i++, at += 4)
                if (v[at] == 0x00 && v[at + 1] == 0x0F && v[at + 2] == 0xAC && v[at + 3] == 4) ccmp = true;
            u16 na = at + 2 <= l ? rd16(v + at) : 0;
            at += 2;
            for (u16 i = 0; i < na && at + 4 <= l; i++, at += 4)
                if (v[at] == 0x00 && v[at + 1] == 0x0F && v[at + 2] == 0xAC && v[at + 3] == 2) psk = true;
            security = (ccmp && psk) ? WIFI_WPA2 : WIFI_OTHER;
        }
        ie += 2 + l;
        left -= 2 + l;
    }
    if (!ssid[0]) return;                            /* a hidden name: not listed */

    wnet *slot = NULL, *oldest = &nets[0];
    for (u32 i = 0; i < NET_MAX; i++) {
        if (nets[i].used && same6(nets[i].bssid, bssid)) { slot = &nets[i]; break; }
        if (!nets[i].used && !slot) slot = &nets[i];
        if (nets[i].seen_ns < oldest->seen_ns) oldest = &nets[i];
    }
    if (!slot) slot = oldest;
    slot->used = true;
    cpy_ssid(slot->ssid, ssid);
    memcpy(slot->bssid, bssid, 6);
    slot->channel = channel;
    slot->rssi = rssi;
    slot->security = security;
    slot->seen_ns = time_ns();
}

void wifi_scan(void)
{
    st.probe_ns = 0;                                 /* the next poll asks */
}

u32 wifi_networks(wifi_net *out, u32 max)
{
    u32 n = 0;
    u64 now = time_ns();
    for (u32 i = 0; i < NET_MAX && n < max; i++) {
        if (!nets[i].used || now - nets[i].seen_ns > 60 * SECOND) continue;
        wifi_net *o = &out[n++];
        cpy_ssid(o->ssid, nets[i].ssid);
        memcpy(o->bssid, nets[i].bssid, 6);
        o->channel = nets[i].channel;
        o->rssi = nets[i].rssi;
        o->security = nets[i].security;
        o->joined = st.state == S_JOINED && same6(st.bssid, nets[i].bssid);
        char pass[64];
        o->remembered = settings_wlan(nets[i].ssid, pass, sizeof(pass));
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* The handshake                                                       */
/* ------------------------------------------------------------------ */

static void eapol_in(const u8 *e, u32 len)
{
    if (len < 4 + 95 || e[1] != 3) return;
    const u8 *k = e + 4;
    u32 body = ((u32)e[2] << 8) | e[3];
    if (body < 95 || 4 + body > len || k[0] != 2) return;
    u16 info = (u16)((k[1] << 8) | k[2]);
    u32 kdlen = ((u32)k[93] << 8) | k[94];
    if (95 + kdlen > body) return;
    bool pairwise = (info & 0x0008) != 0, ack = (info & 0x0080) != 0;
    bool has_mic = (info & 0x0100) != 0, encrypted = (info & 0x1000) != 0;

    if (st.state != S_HANDSHAKE && st.state != S_JOINED) return;

    if (pairwise && ack && !has_mic) {
        /* the first message: their nonce; ours goes back with the
         * session keys already derived on both sides */
        memcpy(st.anonce, k + 13, 32);
        memcpy(st.replay, k + 5, 8);
        rand_bytes(st.snonce, 32);
        derive_ptk();
        send_eapol(0x010A, st.snonce, own_rsn, 22);
        st.step_ns = time_ns();
        return;
    }

    if (pairwise && ack && has_mic && encrypted) {
        /* the third: the proof that they hold the same key, and the
         * group key wrapped in ours */
        u8 mic[16], calc[20];
        static u8 copy[512];
        if (4 + body > sizeof(copy)) return;
        memcpy(copy, e, 4 + body);
        memcpy(mic, copy + 4 + 77, 16);
        memset(copy + 4 + 77, 0, 16);
        hmac_sha1(st.ptk, 16, copy, 4 + body, calc);
        if (memcmp(mic, calc, 16) != 0) { fail("the password did not open it"); return; }

        static u8 plain[256];
        if (kdlen > sizeof(plain) + 8 || kdlen < 24) { fail("the network's key data made no sense"); return; }
        if (!aes_unwrap(st.ptk + 16, k + 95, kdlen, plain)) { fail("the network's group key would not unwrap"); return; }
        u32 plen = kdlen - 8;
        st.have_gk = false;
        for (u32 at = 0; at + 2 <= plen;) {
            u8 id = plain[at], l = plain[at + 1];
            if (id == 0xDD && l == 0) break;
            if (at + 2 + l > plen) break;
            if (id == 0xDD && l >= 6 + 16 && plain[at + 2] == 0x00 && plain[at + 3] == 0x0F &&
                plain[at + 4] == 0xAC && plain[at + 5] == 0x01) {
                memcpy(st.gtk, plain + at + 8, 16);
                aes128_setkey(&st.gk, st.gtk);
                st.have_gk = true;
            }
            at += 2 + l;
        }
        memcpy(st.replay, k + 5, 8);
        aes128_setkey(&st.tk, st.ptk + 32);
        st.pn_out = st.pn_in = st.pn_in_group = 0;
        send_eapol(0x030A, NULL, NULL, 0);

        bool fresh = st.state != S_JOINED;
        st.state = S_JOINED;
        st.why[0] = 0;
        if (fresh) {
            kprintf("wifi: joined '%s' on channel %u, sealed with wpa2\n", st.ssid, st.channel);
            journal_says("wifi", "joined the network; the frames are sealed");
            char had[64];
            if (!settings_wlan(st.ssid, had, sizeof(had)) && st.pass[0])
                settings_remember_wlan(st.ssid, st.pass);
            net_relink();
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Data                                                                */
/* ------------------------------------------------------------------ */

static void aad_of(const u8 *f, u8 aad[22])
{
    u16 fc = (u16)((rd16(f) & (u16)~0x3800u) | FC_PROTECTED);
    wr16(aad, fc);
    memcpy(aad + 2, f + 4, 18);
    wr16(aad + 20, (u16)(rd16(f + 22) & 0x000F));
}

static void queue_ether(const u8 *dst, const u8 *src, const u8 *typed, u32 len)
{
    u32 next = (inq_tail + 1) % INQ;
    if (next == inq_head) return;                    /* full: the air drops frames too */
    if (14 + len > sizeof(inq[0])) return;
    u8 *q = inq[inq_tail];
    memcpy(q, dst, 6);
    memcpy(q + 6, src, 6);
    memcpy(q + 12, typed, len);
    inq_len[inq_tail] = 14 + len;
    inq_tail = next;
}

i32 wifi_take(void *out, u32 max)
{
    if (inq_head == inq_tail) return 0;
    u32 n = inq_len[inq_head];
    if (n > max) n = max;
    memcpy(out, inq[inq_head], n);
    inq_head = (inq_head + 1) % INQ;
    return (i32)n;
}

static void data_in(const u8 *f, u32 len)
{
    if (len < 24) return;
    u16 fc = rd16(f);
    if (fc & 0x0080) return;                         /* qos data: not negotiated, not read */
    if (!(fc & FC_FROMDS)) return;
    const u8 *a1 = f + 4, *a2 = f + 10, *a3 = f + 16;
    if (!same6(a2, st.bssid)) return;
    bool to_us = same6(a1, nic_mac()), group = (a1[0] & 1) != 0;
    if (!to_us && !group) return;

    static u8 plain[1600];
    const u8 *payload;
    u32 plen;
    if (fc & FC_PROTECTED) {
        if (len < 24 + 8 + 8 || st.state != S_JOINED) return;
        const u8 *h = f + 24;
        u32 keyid = h[3] >> 6;
        u64 pn = (u64)h[0] | ((u64)h[1] << 8) | ((u64)h[4] << 16) | ((u64)h[5] << 24) |
                 ((u64)h[6] << 32) | ((u64)h[7] << 40);
        const aes_key *key;
        u64 *last;
        if (keyid == 0) { key = &st.tk; last = &st.pn_in; }
        else { if (!st.have_gk) return; key = &st.gk; last = &st.pn_in_group; }
        if (pn <= *last) return;                     /* replayed, or old: not twice */
        u8 nonce[13];
        nonce[0] = 0;
        memcpy(nonce + 1, a2, 6);
        for (u32 i = 0; i < 6; i++) nonce[7 + i] = (u8)(pn >> (40 - 8 * i));
        u8 aad[22];
        aad_of(f, aad);
        plen = len - 32 - 8;
        if (!aes_ccm_open(key, nonce, aad, 22, f + 32, plen, f + 32 + plen, plain)) return;
        *last = pn;
        st.sealed_in++;
        payload = plain;
    } else {
        if (!st.open || st.state != S_JOINED) {
            /* the handshake travels in the clear, before any seal */
            if (len >= 24 + 8 + 4 && f[24] == 0xAA && f[25] == 0xAA && f[30] == 0x88 && f[31] == 0x8E)
                eapol_in(f + 32, len - 32);
            return;
        }
        payload = f + 24;
        plen = len - 24;
    }
    /* llc/snap, then the ethernet type and the bytes */
    if (plen < 8 || payload[0] != 0xAA || payload[1] != 0xAA || payload[2] != 0x03) return;
    if (payload[6] == 0x88 && payload[7] == 0x8E) { eapol_in(payload + 8, plen - 8); return; }
    queue_ether(a1, a3, payload + 6, plen - 6);
}

bool wifi_send(const void *frame, u32 len)
{
    if (st.state != S_JOINED || len < 14) return false;
    const u8 *e = (const u8 *)frame;
    static u8 f[1600];
    static u8 plain[1600];
    u32 plen = 8 + (len - 14);
    if (plen + 40 > sizeof(f)) return false;

    u16 fc = FC_DATA | FC_TODS | (st.open ? 0 : FC_PROTECTED);
    u32 n = mgmt_head(f, fc, st.bssid, nic_mac(), e);
    plain[0] = 0xAA; plain[1] = 0xAA; plain[2] = 0x03; plain[3] = plain[4] = plain[5] = 0;
    memcpy(plain + 6, e + 12, len - 12);            /* the type and the bytes */

    if (st.open) {
        memcpy(f + n, plain, plen);
        return radio_send(f, n + plen);
    }
    u64 pn = ++st.pn_out;
    u8 *h = f + n;
    h[0] = (u8)pn; h[1] = (u8)(pn >> 8); h[2] = 0; h[3] = 0x20;
    h[4] = (u8)(pn >> 16); h[5] = (u8)(pn >> 24); h[6] = (u8)(pn >> 32); h[7] = (u8)(pn >> 40);
    u8 nonce[13];
    nonce[0] = 0;
    memcpy(nonce + 1, nic_mac(), 6);
    for (u32 i = 0; i < 6; i++) nonce[7 + i] = (u8)(pn >> (40 - 8 * i));
    u8 aad[22];
    aad_of(f, aad);
    aes_ccm_seal(&st.tk, nonce, aad, 22, plain, plen, f + n + 8, f + n + 8 + plen);
    st.sealed_out++;
    return radio_send(f, n + 8 + plen + 8);
}

/* ------------------------------------------------------------------ */
/* Frames from the radio                                               */
/* ------------------------------------------------------------------ */

void wifi_radio_input(const u8 *f, u32 len, i8 rssi, u8 channel)
{
    if (len < 24) return;
    u16 fc = rd16(f);
    u16 kind = (u16)(fc & 0x00FC);
    const u8 *a2 = f + 10;

    if (kind == FC_BEACON || kind == FC_PROBE_RESP) { heard(f, len, rssi, channel); return; }
    if ((fc & 0x000C) == FC_DATA) { data_in(f, len); return; }
    if (st.state == S_IDLE || !same6(a2, st.bssid)) return;

    if (kind == FC_AUTH && st.state == S_AUTH && len >= 30) {
        if (rd16(f + 26) == 2 && rd16(f + 28) == 0) {
            st.state = S_ASSOC;
            st.tries = 0;
            st.step_ns = time_ns();
            send_assoc();
        } else fail("the network refused the greeting");
        return;
    }
    if (kind == FC_ASSOC_RESP && st.state == S_ASSOC && len >= 30) {
        if (rd16(f + 26) == 0) {
            if (st.open) {
                st.state = S_JOINED;
                st.why[0] = 0;
                kprintf("wifi: joined '%s' on channel %u, open\n", st.ssid, st.channel);
                journal_says("wifi", "joined an open network; nothing on it is sealed");
                net_relink();
            } else {
                st.state = S_HANDSHAKE;
                st.step_ns = time_ns();
            }
        } else fail("the network would not have us");
        return;
    }
    if (kind == FC_DEAUTH || kind == FC_DISASSOC) {
        if (st.state == S_HANDSHAKE) fail("the password did not open it");
        else if (st.state == S_JOINED) fail("the network let go of us");
        else fail("the network turned us away");
    }
}

/* ------------------------------------------------------------------ */
/* Joining                                                             */
/* ------------------------------------------------------------------ */

void wifi_join(const char *ssid, const char *pass)
{
    cpy_ssid(st.want_ssid, ssid);
    u32 i = 0;
    while (pass && pass[i] && i < sizeof(st.want_pass) - 1) { st.want_pass[i] = pass[i]; i++; }
    st.want_pass[i] = 0;
    st.want = true;
}

static void begin_join(const char *ssid, const char *pass)
{
    wnet *best = NULL;
    u64 now = time_ns();
    for (u32 i = 0; i < NET_MAX; i++) {
        if (!nets[i].used || now - nets[i].seen_ns > 60 * SECOND) continue;
        if (strcmp(nets[i].ssid, ssid) != 0) continue;
        if (!best || nets[i].rssi > best->rssi) best = &nets[i];
    }
    if (!best) { fail("no network of that name has been heard"); return; }
    if (best->security == WIFI_OTHER) { fail("that network's protection is not one this station speaks"); return; }
    if (best->security == WIFI_WPA2 && !crypto_ok) { fail("the station's own arithmetic failed its test; it will not seal"); return; }
    u32 pl = 0;
    while (pass[pl]) pl++;
    if (best->security == WIFI_WPA2 && (pl < 8 || pl > 63)) { fail("a passphrase is eight to sixty-three letters"); return; }

    if (st.state == S_JOINED) send_deauth(3);
    keys_gone();
    cpy_ssid(st.ssid, best->ssid);
    memcpy(st.bssid, best->bssid, 6);
    st.channel = best->channel;
    st.open = best->security == WIFI_OPEN;
    memcpy(st.pass, pass, pl + 1);
    if (!st.open) {
        u32 sl = 0;
        while (st.ssid[sl]) sl++;
        pbkdf2_hmac_sha1((const u8 *)pass, pl, (const u8 *)st.ssid, sl, 4096, st.pmk, 32);
    }
    st.state = S_AUTH;
    st.tries = 0;
    st.step_ns = time_ns();
    kprintf("wifi: joining '%s'\n", st.ssid);
    send_auth();
}

bool wifi_up(void) { return st.state == S_JOINED; }

bool wifi_joined_name(char *out, u32 max)
{
    if (st.state != S_JOINED) { out[0] = 0; return false; }
    u32 i = 0;
    while (st.ssid[i] && i + 1 < max) { out[i] = st.ssid[i]; i++; }
    out[i] = 0;
    return true;
}

const char *wifi_state(char *out, u32 max)
{
    u32 at = 0;
    #define PUT(s) do { const char *p_ = (s); while (*p_ && at + 1 < max) out[at++] = *p_++; } while (0)
    #define NUM(v) do { char d_[24]; u32 n_ = 0; u64 x_ = (v); if (!x_) d_[n_++] = '0'; while (x_) { d_[n_++] = (char)('0' + x_ % 10); x_ /= 10; } while (n_ && at + 1 < max) out[at++] = d_[--n_]; } while (0)
    if (st.state == S_JOINED) {
        PUT("joined '"); PUT(st.ssid); PUT("' on channel "); NUM(st.channel);
        PUT(st.open ? ", open; " : ", sealed with wpa2; ");
        NUM(st.sealed_out); PUT(" frames sealed out, "); NUM(st.sealed_in); PUT(" unsealed in");
    } else if (st.state != S_IDLE) {
        PUT("joining '"); PUT(st.ssid); PUT("'");
        PUT(st.state == S_AUTH ? ": greeting" : st.state == S_ASSOC ? ": asking in" : ": the handshake");
    } else {
        PUT("not joined");
        if (st.why[0]) { PUT("; last time: "); PUT(st.why); }
    }
    out[at] = 0;
    #undef PUT
    #undef NUM
    return out;
}

/* ------------------------------------------------------------------ */
/* The clock                                                           */
/* ------------------------------------------------------------------ */

void wifi_poll(void)
{
    u64 now = time_ns();

    if (st.want_leave) {
        st.want_leave = false;
        st.want = false;
        if (st.state != S_IDLE) {
            if (st.state == S_JOINED) send_deauth(3);
            bool was = st.state == S_JOINED;
            st.state = S_IDLE;
            keys_gone();
            st.why[0] = 0;
            kprintf("wifi: left '%s'\n", st.ssid);
            journal_says("wifi", "left the network");
            if (was) net_relink();
        }
        st.auto_ns = now + 60 * SECOND;             /* not straight back in */
    }
    if (st.want) {
        st.want = false;
        begin_join(st.want_ssid, st.want_pass);
        memset(st.want_pass, 0, sizeof(st.want_pass));
        st.auto_ns = now + 30 * SECOND;
    }

    /* asking the air who is there: at the start, and when told to */
    if (nic_up() && now >= st.probe_ns) {
        st.probe_ns = now + 30 * SECOND;
        send_probe();
    }

    switch (st.state) {
    case S_AUTH:
    case S_ASSOC:
        if (now - st.step_ns > SECOND) {
            if (++st.tries >= 3) { fail("no answer from the network"); break; }
            st.step_ns = now;
            if (st.state == S_AUTH) send_auth(); else send_assoc();
        }
        break;
    case S_HANDSHAKE:
        if (now - st.step_ns > 4 * SECOND) fail("the handshake did not finish; is the password right?");
        break;
    default:
        break;
    }

    /* a remembered network in the air, and nowhere joined: in */
    if (st.state == S_IDLE && now >= st.auto_ns) {
        st.auto_ns = now + 30 * SECOND;
        for (u32 i = 0; i < NET_MAX; i++) {
            if (!nets[i].used || now - nets[i].seen_ns > 10 * SECOND) continue;
            char pass[64];
            if (!settings_wlan(nets[i].ssid, pass, sizeof(pass))) continue;
            if (nets[i].security == WIFI_OTHER) continue;
            kprintf("wifi: '%s' is in the air and remembered\n", nets[i].ssid);
            begin_join(nets[i].ssid, pass);
            memset(pass, 0, sizeof(pass));
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Start                                                               */
/* ------------------------------------------------------------------ */

static bool selftest(void)
{
    u8 out[32];
    static const u8 sha1_abc[20] = { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
                                     0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D };
    sha1("abc", 3, out);
    if (memcmp(out, sha1_abc, 20) != 0) return false;

    u8 key[20];
    memset(key, 0x0B, 20);
    static const u8 hmac1[20] = { 0xB6, 0x17, 0x31, 0x86, 0x55, 0x05, 0x72, 0x64, 0xE2, 0x8B,
                                  0xC0, 0xB6, 0xFB, 0x37, 0x8C, 0x8E, 0xF1, 0x46, 0xBE, 0x00 };
    hmac_sha1(key, 20, "Hi There", 8, out);
    if (memcmp(out, hmac1, 20) != 0) return false;

    static const u8 psk[32] = { 0xF4, 0x2C, 0x6F, 0xC5, 0x2D, 0xF0, 0xEB, 0xEF, 0x9E, 0xBB, 0x4B,
                                0x90, 0xB3, 0x8A, 0x5F, 0x90, 0x2E, 0x83, 0xFE, 0x1B, 0x13, 0x5A,
                                0x70, 0xE2, 0x3A, 0xED, 0x76, 0x2E, 0x97, 0x10, 0xA1, 0x2E };
    pbkdf2_hmac_sha1((const u8 *)"password", 8, (const u8 *)"IEEE", 4, 4096, out, 32);
    if (memcmp(out, psk, 32) != 0) return false;

    /* RFC 3610, packet vector 1 */
    u8 ck[16], nonce[13], aad[8], pt[23], ct[23], tag[8], back[23];
    for (u32 i = 0; i < 16; i++) ck[i] = (u8)(0xC0 + i);
    static const u8 n1[13] = { 0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5 };
    memcpy(nonce, n1, 13);
    for (u32 i = 0; i < 8; i++) aad[i] = (u8)i;
    for (u32 i = 0; i < 23; i++) pt[i] = (u8)(8 + i);
    static const u8 want_ct[23] = { 0x58, 0x8C, 0x97, 0x9A, 0x61, 0xC6, 0x63, 0xD2, 0xF0, 0x66, 0xD0, 0xC2,
                                    0xC0, 0xF9, 0x89, 0x80, 0x6D, 0x5F, 0x6B, 0x61, 0xDA, 0xC3, 0x84 };
    static const u8 want_tag[8] = { 0x17, 0xE8, 0xD1, 0x2C, 0xFD, 0xF9, 0x26, 0xE0 };
    aes_key k;
    aes128_setkey(&k, ck);
    aes_ccm_seal(&k, nonce, aad, 8, pt, 23, ct, tag);
    if (memcmp(ct, want_ct, 23) != 0 || memcmp(tag, want_tag, 8) != 0) return false;
    if (!aes_ccm_open(&k, nonce, aad, 8, ct, 23, tag, back) || memcmp(back, pt, 23) != 0) return false;
    tag[0] ^= 1;
    if (aes_ccm_open(&k, nonce, aad, 8, ct, 23, tag, back)) return false;

    /* RFC 3394, 4.1 */
    u8 kek[16];
    for (u32 i = 0; i < 16; i++) kek[i] = (u8)i;
    static const u8 wrapped[24] = { 0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47, 0xAE, 0xF3, 0x4B, 0xD8,
                                    0xFB, 0x5A, 0x7B, 0x82, 0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5 };
    static const u8 plain[16] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    if (!aes_unwrap(kek, wrapped, 24, out) || memcmp(out, plain, 16) != 0) return false;
    return true;
}

void wifi_init(void)
{
    crypto_ok = selftest();
    kprintf("wifi: %s; %s\n",
            crypto_ok ? "self test passed -- sha1, pbkdf2, aes-ccm, key unwrap"
                      : "self test FAILED -- no network will be sealed",
            wifi_radio_name());
    st.auto_ns = time_ns() + 3 * SECOND;             /* let the air be heard first */
}
