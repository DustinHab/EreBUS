#!/usr/bin/env python3
"""pcap-frames.py <file.pcap> [max] -- the frames in a QEMU filter-dump, one line each:
time, source and destination, type, length, and for arp and udp what they carry.
For reading a test's wire dump without tcpdump."""
import struct, sys

def mac(b): return ':'.join('%02x' % x for x in b)
def ip(b):  return '.'.join(str(x) for x in b)

def main():
    data = open(sys.argv[1], 'rb').read()
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    magic = struct.unpack('<I', data[:4])[0]
    endian = '<' if magic in (0xa1b2c3d4, 0xa1b23c4d) else '>'
    at = 24
    n = 0
    t0 = None
    while at + 16 <= len(data) and n < limit:
        ts, tu, caplen, wirelen = struct.unpack(endian + 'IIII', data[at:at + 16])
        at += 16
        f = data[at:at + caplen]
        at += caplen
        t = ts + tu / 1e6
        if t0 is None: t0 = t
        if len(f) < 14: continue
        etype = struct.unpack('!H', f[12:14])[0]
        line = '%7.3f %s > %s' % (t - t0, mac(f[6:12]), mac(f[0:6]))
        if etype == 0x0806 and len(f) >= 42:
            op = struct.unpack('!H', f[20:22])[0]
            line += ' arp %s %s(%s) -> %s(%s)' % ('request' if op == 1 else 'reply' if op == 2 else str(op),
                                                  ip(f[28:32]), mac(f[22:28]), ip(f[38:42]), mac(f[32:38]))
        elif etype == 0x0800 and len(f) >= 34:
            proto = f[23]
            ihl = (f[14] & 15) * 4
            line += ' ip %s -> %s' % (ip(f[26:30]), ip(f[30:34]))
            if proto == 17 and len(f) >= 14 + ihl + 8:
                sp, dp = struct.unpack('!HH', f[14 + ihl:14 + ihl + 4])
                pl = f[14 + ihl + 8:]
                line += ' udp %d -> %d' % (sp, dp)
                if pl[:4] == b'EBPX': line += ' EBPX kind %d' % pl[4]
            elif proto == 6: line += ' tcp'
            elif proto == 1: line += ' icmp'
        else:
            line += ' type %04x' % etype
        print('%s (%d bytes)' % (line, wirelen))
        n += 1

main()
