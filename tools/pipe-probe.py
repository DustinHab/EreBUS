#!/usr/bin/env python3
"""pipe-probe.py -- a machine that is not EreBUS, on the same wire, sending
one SEEK datagram to an EreBUS node and reporting whether a HERE came back.

The wire is a QEMU multicast socket netdev: every Ethernet frame is one
UDP datagram to the group, no framing around it. This joins the group,
answers ARP for the address it claims, sends the SEEK, and listens for a
pipe datagram addressed back to it.

  pipe-probe.py <group> <port> <own ip> <own mac> <target ip> <target mac> [seconds]

Exit 0 and "HERE from ..." when the node answered, exit 1 and "no answer"
when it did not within the time. Nothing else is asserted; the test that
runs it decides which outcome is the right one.
"""
import socket, struct, sys, time

def mac_bytes(s):  return bytes(int(x, 16) for x in s.split(':'))
def ip_bytes(s):   return bytes(int(x) for x in s.split('.'))

def csum(b):
    if len(b) % 2: b += b'\0'
    s = sum(struct.unpack('!%dH' % (len(b) // 2), b))
    while s >> 16: s = (s & 0xffff) + (s >> 16)
    return (~s) & 0xffff

def ip_udp(src_ip, dst_ip, sport, dport, payload):
    udp = struct.pack('!HHHH', sport, dport, 8 + len(payload), 0) + payload
    total = 20 + len(udp)
    hdr = struct.pack('!BBHHHBBH4s4s', 0x45, 0, total, 0x1234, 0, 64, 17, 0, src_ip, dst_ip)
    hdr = hdr[:10] + struct.pack('!H', csum(hdr)) + hdr[12:]
    return hdr + udp

def seek(name):
    pkt = bytearray(40)
    pkt[0:4] = b'EBPX'              # 0x58504245 little-endian
    pkt[4] = 4                       # K_SEEK
    pkt[8:8 + len(name)] = name.encode()[:24]
    return bytes(pkt)

def main():
    group, port = sys.argv[1], int(sys.argv[2])
    own_ip, own_mac = ip_bytes(sys.argv[3]), mac_bytes(sys.argv[4])
    dst_ip, dst_mac = ip_bytes(sys.argv[5]), mac_bytes(sys.argv[6])
    limit = float(sys.argv[7]) if len(sys.argv) > 7 else 4.0

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', port))
    mreq = struct.pack('4s4s', socket.inet_aton(group), socket.inet_aton('0.0.0.0'))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    s.settimeout(0.2)

    # A frame under 60 bytes is a runt: a card pads on the way out and
    # drops one that arrives short (QEMU's e1000 does), so pad here.
    def send(frame): s.sendto(frame.ljust(60, b'\0'), (group, port))

    eth = dst_mac + own_mac + b'\x08\x00'
    send(eth + ip_udp(own_ip, dst_ip, 7800, 7800, seek('probe')))

    import os
    debug = os.environ.get('PROBE_DEBUG')
    end = time.time() + limit
    while time.time() < end:
        try:
            f, _ = s.recvfrom(2000)
        except socket.timeout:
            continue
        if len(f) < 14: continue
        etype = struct.unpack('!H', f[12:14])[0]
        if debug:
            sys.stderr.write('frame: type %04x from %s to %s, %d bytes\n' % (
                etype, ':'.join('%02x' % b for b in f[6:12]), ':'.join('%02x' % b for b in f[0:6]), len(f)))
        if etype == 0x0806 and len(f) >= 42:          # ARP request for us: answer
            op = struct.unpack('!H', f[20:22])[0]
            tip = f[38:42]
            if op == 1 and tip == own_ip:
                sha, spa = f[22:28], f[28:32]
                arp = (b'\x00\x01\x08\x00\x06\x04\x00\x02' + own_mac + own_ip + sha + spa)
                send(sha + own_mac + b'\x08\x06' + arp)
        elif etype == 0x0800 and len(f) >= 42:
            ihl = (f[14] & 0x0f) * 4
            if f[23] != 17: continue
            if f[30:34] != own_ip: continue
            udp = 14 + ihl
            payload = f[udp + 8:]
            if payload[:4] == b'EBPX':
                kind = payload[4]
                print('HERE from %s (kind %d, %d bytes)' % (sys.argv[5], kind, len(payload)))
                sys.exit(0)
    print('no answer')
    sys.exit(1)

main()
