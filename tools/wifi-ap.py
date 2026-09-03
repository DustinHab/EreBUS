#!/usr/bin/env python3
"""wifi-ap.py -- a virtual access point for the test bench.

QEMU's socket netdev delivers the machine's ethernet frames here, each
with a four-byte length in front. Frames of type 0x88B5 carry 802.11
frames the way the machine's test-bench radio wraps them: a mark 'R',
a version, the signal, the channel, then the frame as the air would
have had it. This program is the other end of that air: it beacons,
answers probes, greets, associates, runs the WPA2 four-way handshake
with a passphrase of its own, seals and unseals data frames with CCMP,
and behind the seal plays the small network a home router would: it
answers ARP, leases an address, answers pings -- and pings the station
back, so that a reply through the seal proves the whole path.

  python3 tools/wifi-ap.py --port 8020 --ssid "erebus test" --password "correct horse"
"""
import argparse, hashlib, hmac, os, select, socket, struct, sys, time
from cryptography.hazmat.primitives.ciphers.aead import AESCCM
from cryptography.hazmat.primitives import keywrap

ap = argparse.ArgumentParser()
ap.add_argument("--port", type=int, default=8020)
ap.add_argument("--ssid", default="erebus test")
ap.add_argument("--password", default="correct horse")
ap.add_argument("--channel", type=int, default=6)
ap.add_argument("--open", action="store_true", help="no passphrase, nothing sealed")
args = ap.parse_args()

SSID = args.ssid.encode()
BSSID = bytes.fromhex("021122334455")
BCAST = b"\xff" * 6
AP_IP = bytes([10, 9, 8, 1])
STA_IP = bytes([10, 9, 8, 20])
PMK = hashlib.pbkdf2_hmac("sha1", args.password.encode(), SSID, 4096, 32)
RSN_IE = bytes.fromhex("3014" "0100" "000fac04" "0100" "000fac04" "0100" "000fac02" "0000")
GTK = os.urandom(16)
ETH_RADIO = 0x88B5

def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)

# ---------------------------------------------------------------- frames

seq = 0
def mgmt(fc, a1, a2, a3, body):
    global seq
    seq = (seq + 1) & 0xFFF
    return struct.pack("<HH", fc, 0) + a1 + a2 + a3 + struct.pack("<H", seq << 4) + body

def ie(id_, data):
    return bytes([id_, len(data)]) + data

def beacon_body():
    capab = 0x0401 if args.open else 0x0411
    body = struct.pack("<QHH", int(time.time() * 1e6) & 0xFFFFFFFFFFFFFFFF, 100, capab)
    body += ie(0, SSID) + ie(1, bytes.fromhex("82848b960c121824")) + ie(3, bytes([args.channel]))
    if not args.open:
        body += RSN_IE
    return body

def prf(key, label, data, n):
    out = b""
    i = 0
    while len(out) < n:
        out += hmac.new(key, label + b"\x00" + data + bytes([i]), hashlib.sha1).digest()
        i += 1
    return out[:n]

def eapol_key(info, replay, nonce, keylen, keydata, kck=None):
    body = struct.pack(">BHHQ", 2, info, keylen, replay) + nonce + b"\x00" * 16 + b"\x00" * 8 + b"\x00" * 8
    body += b"\x00" * 16 + struct.pack(">H", len(keydata)) + keydata
    frame = struct.pack(">BBH", 2, 3, len(body)) + body
    if kck is not None:
        mic = hmac.new(kck, frame, hashlib.sha1).digest()[:16]
        frame = frame[:4 + 77] + mic + frame[4 + 93:]
    return frame

LLC_EAPOL = bytes.fromhex("aaaa030000008 88e".replace(" ", ""))

def csum(data):
    if len(data) % 2:
        data += b"\x00"
    s = sum(struct.unpack(">%dH" % (len(data) // 2), data))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF

def ipv4(src, dst, proto, payload):
    head = struct.pack(">BBHHHBBH4s4s", 0x45, 0, 20 + len(payload), 0x1234, 0, 64, proto, 0, src, dst)
    head = head[:10] + struct.pack(">H", csum(head)) + head[12:]
    return head + payload

def udp(src, dst, sport, dport, payload):
    return ipv4(src, dst, 17, struct.pack(">HHHH", sport, dport, 8 + len(payload), 0) + payload)

def icmp_echo(src, dst, kind, ident, seqn, data):
    body = struct.pack(">BBHHH", kind, 0, 0, ident, seqn) + data
    body = body[:2] + struct.pack(">H", csum(body)) + body[4:]
    return ipv4(src, dst, 1, body)

# ---------------------------------------------------------------- station

class Station:
    def __init__(self, mac):
        self.mac = mac
        self.state = "new"
        self.anonce = os.urandom(32)
        self.ptk = None
        self.pn_out = 0
        self.pn_in = 0
        self.ip = None
        self.pings = 0
        self.ping_seq = 0
        self.last_ping = 0.0

stations = {}
gtk_pn = 0

def station(mac):
    if mac not in stations:
        stations[mac] = Station(mac)
    return stations[mac]

# ---------------------------------------------------------------- the seal

def aad_of(frame):
    fc = struct.unpack("<H", frame[:2])[0]
    fc = (fc & ~0x3800) | 0x4000
    sc = struct.unpack("<H", frame[22:24])[0] & 0x000F
    return struct.pack("<H", fc) + frame[4:22] + struct.pack("<H", sc)

def seal_data(st, dst, src, typed, group=False):
    """An ethernet-shaped payload into a sealed data frame from the AP."""
    global gtk_pn
    fc = 0x0208 | (0 if args.open else 0x4000)
    head = mgmt(fc, dst, BSSID, src, b"")
    plain = bytes.fromhex("aaaa03000000") + typed
    if args.open:
        return head + plain
    if group:
        gtk_pn += 1
        pn, key, keyid = gtk_pn, GTK, 1
    else:
        st.pn_out += 1
        pn, key, keyid = st.pn_out, st.ptk[32:48], 0
    pnb = pn.to_bytes(6, "big")
    ccmp = bytes([pnb[5], pnb[4], 0, 0x20 | (keyid << 6), pnb[3], pnb[2], pnb[1], pnb[0]])
    nonce = b"\x00" + BSSID + pnb
    sealed = AESCCM(key, tag_length=8).encrypt(nonce, plain, aad_of(head))
    return head + ccmp + sealed

def open_data(st, frame):
    """A data frame from the station: the ethernet-shaped payload, or None."""
    fc = struct.unpack("<H", frame[:2])[0]
    if fc & 0x4000:
        if st.ptk is None or len(frame) < 24 + 8 + 8:
            return None
        h = frame[24:32]
        pn = int.from_bytes(bytes([h[7], h[6], h[5], h[4], h[1], h[0]]), "big")
        if pn <= st.pn_in:
            log("replayed frame from the station, dropped")
            return None
        nonce = b"\x00" + frame[10:16] + pn.to_bytes(6, "big")
        try:
            plain = AESCCM(st.ptk[32:48], tag_length=8).decrypt(nonce, frame[32:], aad_of(frame))
        except Exception:
            log("a sealed frame from the station did not open")
            return None
        st.pn_in = pn
        return plain
    return frame[24:]

# ---------------------------------------------------------------- the wire

def send(conn, frame, rssi=-40):
    eth = BCAST + BSSID + struct.pack(">H", ETH_RADIO) + b"R\x01" + struct.pack("bB", rssi, args.channel) + frame
    conn.sendall(struct.pack(">I", len(eth)) + eth)

def on_mgmt(conn, frame):
    fc = struct.unpack("<H", frame[:2])[0]
    kind = fc & 0x00FC
    a2 = frame[10:16]
    body = frame[24:]
    if kind == 0x0040:                                   # probe request
        send(conn, mgmt(0x0050, a2, BSSID, BSSID, beacon_body()))
    elif kind == 0x00B0:                                 # authentication
        st = station(a2)
        st.state = "authenticated"
        send(conn, mgmt(0x00B0, a2, BSSID, BSSID, struct.pack("<HHH", 0, 2, 0)))
        log("greeting from", a2.hex(":"))
    elif kind == 0x0000:                                 # association request
        st = station(a2)
        st.state = "associated"
        send(conn, mgmt(0x0010, a2, BSSID, BSSID, struct.pack("<HHH", 0x0411, 0, 0xC001) + ie(1, bytes.fromhex("82848b96"))))
        log("association from", a2.hex(":"))
        if args.open:
            st.state = "joined"
            log("joined, open")
        else:
            st.anonce = os.urandom(32)
            msg1 = eapol_key(0x008A, 1, st.anonce, 16, b"")
            send(conn, mgmt(0x0208, a2, BSSID, BSSID, LLC_EAPOL + msg1))
            st.state = "handshake"
    elif kind in (0x00C0, 0x00A0):
        if a2 in stations:
            log("the station left")
            del stations[a2]

def on_eapol(conn, st, e):
    if len(e) < 99 or e[1] != 3:
        return
    k = e[4:]
    info = struct.unpack(">H", k[1:3])[0]
    replay = struct.unpack(">Q", k[5:13])[0]
    nonce = k[13:45]
    mic = k[77:93]
    kdlen = struct.unpack(">H", k[93:95])[0]
    body_len = struct.unpack(">H", e[2:4])[0]
    whole = e[:4 + body_len]
    if info & 0x0100 and not (info & 0x0080) and st.ptk is None:
        # the second message: their nonce; the keys follow from it
        a, b = sorted([st.mac, BSSID])
        n1, n2 = sorted([st.anonce, nonce])
        ptk = prf(PMK, b"Pairwise key expansion", a + b + n1 + n2, 48)
        zeroed = whole[:4 + 77] + b"\x00" * 16 + whole[4 + 93:]
        calc = hmac.new(ptk[:16], zeroed, hashlib.sha1).digest()[:16]
        if calc != mic:
            log("wrong passphrase from", st.mac.hex(":"), "-- turned away")
            send(conn, mgmt(0x00C0, st.mac, BSSID, BSSID, struct.pack("<H", 23)))
            del stations[st.mac]
            return
        st.ptk = ptk
        kde = b"\xdd\x16\x00\x0f\xac\x01\x05\x00" + GTK
        keydata = RSN_IE + kde
        while len(keydata) % 8:
            keydata += b"\xdd" if len(keydata) % 8 == 7 or keydata[-1:] != b"\xdd" else b"\x00"
        wrapped = keywrap.aes_key_wrap(ptk[16:32], keydata)
        msg3 = eapol_key(0x13CA, 2, st.anonce, 16, wrapped, kck=ptk[:16])
        send(conn, mgmt(0x0208, st.mac, BSSID, BSSID, LLC_EAPOL + msg3))
        log("the second message checked out; the third is on its way with the group key")
    elif info & 0x0100 and st.ptk is not None and st.state == "handshake":
        zeroed = whole[:4 + 77] + b"\x00" * 16 + whole[4 + 93:]
        calc = hmac.new(st.ptk[:16], zeroed, hashlib.sha1).digest()[:16]
        if calc == mic:
            st.state = "joined"
            log("the four-way handshake is done; the station holds the keys")
        else:
            log("the fourth message did not check out")

def on_ether(conn, st, dst, src, typed):
    etype = struct.unpack(">H", typed[:2])[0]
    p = typed[2:]
    if etype == 0x0806 and len(p) >= 28:
        if p[6:8] == b"\x00\x01" and p[24:28] == AP_IP:
            reply = p[:6] + b"\x00\x02" + BSSID + AP_IP + p[8:14] + p[14:18]
            send(conn, seal_data(st, src, BSSID, struct.pack(">H", 0x0806) + reply))
            log("arp: who has 10.9.8.1 -- me")
        return
    if etype != 0x0800 or len(p) < 20:
        return
    proto = p[9]
    ihl = (p[0] & 15) * 4
    body = p[ihl:]
    if proto == 17 and len(body) >= 8:
        sport, dport = struct.unpack(">HH", body[:4])
        if dport == 67 and len(body) >= 8 + 240:
            d = body[8:]
            xid = d[4:8]
            chaddr = d[28:34]
            kind = None
            o = d[240:]
            while o and o[0] != 255:
                if o[0] == 53:
                    kind = o[2]
                o = o[2 + o[1]:] if o[0] != 0 else o[1:]
            reply_kind = 2 if kind == 1 else 5 if kind == 3 else None
            if reply_kind is None:
                return
            rep = b"\x02\x01\x06\x00" + xid + b"\x00" * 8 + STA_IP + AP_IP + b"\x00" * 4 + chaddr + b"\x00" * 10 + b"\x00" * 192
            rep += b"\x63\x82\x53\x63"
            rep += bytes([53, 1, reply_kind, 1, 4, 255, 255, 255, 0, 3, 4]) + AP_IP + bytes([6, 4]) + AP_IP
            rep += bytes([51, 4, 0, 0, 14, 16, 54, 4]) + AP_IP + b"\xff"
            pkt = udp(AP_IP, b"\xff\xff\xff\xff", 67, 68, rep)
            # the offer goes to everyone, under the group key; the ack to the one
            if reply_kind == 2:
                send(conn, seal_data(st, BCAST, BSSID, struct.pack(">H", 0x0800) + pkt, group=True))
                log("dhcp: offer of 10.9.8.20, sent to everyone under the group key")
            else:
                send(conn, seal_data(st, src, BSSID, struct.pack(">H", 0x0800) + pkt))
                st.ip = STA_IP
                log("dhcp: ack; the station is 10.9.8.20")
    elif proto == 1 and len(body) >= 8:
        if body[0] == 8 and p[16:20] == AP_IP:
            ident, seqn = struct.unpack(">HH", body[4:8])
            send(conn, seal_data(st, src, BSSID, struct.pack(">H", 0x0800) + icmp_echo(AP_IP, p[12:16], 0, ident, seqn, body[8:])))
            log("ping from the station, answered")
        elif body[0] == 0:
            st.pings += 1
            log("the station answered ping %d through the seal" % st.pings)

def on_frame(conn, frame):
    if len(frame) < 24:
        return
    fc = struct.unpack("<H", frame[:2])[0]
    if (fc & 0x000C) == 0x0000:
        on_mgmt(conn, frame)
        return
    if (fc & 0x000C) != 0x0008:
        return
    a2 = frame[10:16]
    if a2 not in stations:
        return
    st = stations[a2]
    plain = open_data(st, frame)
    if plain is None or len(plain) < 8:
        return
    if plain[:3] != b"\xaa\xaa\x03":
        return
    if plain[6:8] == b"\x88\x8e":
        on_eapol(conn, st, plain[8:])
        return
    if st.state != "joined":
        return
    on_ether(conn, st, frame[16:22], a2, plain[6:])

def pings(conn):
    now = time.time()
    for st in list(stations.values()):
        if st.state == "joined" and st.ip and now - st.last_ping > 2.0:
            st.last_ping = now
            st.ping_seq += 1
            send(conn, seal_data(st, st.mac, BSSID, struct.pack(">H", 0x0800) + icmp_echo(AP_IP, st.ip, 8, 0x4242, st.ping_seq, b"erebus")))

# ---------------------------------------------------------------- main

srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", args.port))
srv.listen(1)
log("the access point '%s' on channel %d waits on port %d%s" % (args.ssid, args.channel, args.port, ", open" if args.open else ", wpa2"))
conn, _ = srv.accept()
log("the machine is on the wire")
buf = b""
last_beacon = 0.0
while True:
    r, _, _ = select.select([conn], [], [], 0.05)
    if r:
        data = conn.recv(65536)
        if not data:
            log("the machine left the wire")
            break
        buf += data
        while len(buf) >= 4:
            n = struct.unpack(">I", buf[:4])[0]
            if len(buf) < 4 + n:
                break
            eth = buf[4:4 + n]
            buf = buf[4 + n:]
            if len(eth) >= 18 and struct.unpack(">H", eth[12:14])[0] == ETH_RADIO and eth[14:15] == b"R":
                on_frame(conn, eth[18:])
    now = time.time()
    if now - last_beacon > 0.1:
        last_beacon = now
        send(conn, mgmt(0x0080, BCAST, BSSID, BSSID, beacon_body()))
    pings(conn)
