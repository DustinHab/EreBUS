#!/bin/sh
# run.sh -- fuzz the parsers for a while under the address and undefined-behaviour sanitizers.
# - tools: lang (compiler, assembler, linker), pki (certificates, keys, signatures), html (the page renderer),
#   net (frames, the tcp client, the http client, the door with ssh, the air), pipe (the object pipe's
#   datagrams, sealed and plain), tls (the tls client fed a server's bytes)
# - seeds: the kernel's own sources and objects for lang, the certificate fixtures for pki, the manual for html,
#   hand-made frames, datagrams and records for the rest
# - the net, pipe and tls tools link the kernel's own files with the ciphers and signatures stubbed to pass, so
#   the parsers behind authentication see the input; they keep state between inputs, so a crash may need its
#   corpus replayed in order to repeat
# - corpora and crashes under build/fuzz/<tool>/; a crash file replays with build/fuzz/<tool>/<tool>_fuzz <file>
#
#   sh tools/fuzz/run.sh [seconds per tool] [tool ...]
cd "$(dirname "$0")/../.."
SECS=${1:-60}
shift 2>/dev/null
TOOLS=${*:-"lang pki html net pipe tls"}
CC="clang -O1 -g -std=c11 -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer \
    -Wno-unused-function -Wno-incompatible-library-redeclaration -I. -Ikernel/include"

build() {   # build <tool> <sources...>
    t=$1; shift
    mkdir -p build/fuzz/$t/corpus build/fuzz/$t/crashes
    $CC -o build/fuzz/$t/${t}_fuzz tools/fuzz/${t}_fuzz.c "$@" 2>&1 | grep -E 'error' | head
    [ -x build/fuzz/$t/${t}_fuzz ] || { echo "the $t fuzzer does not build"; return 1; }
}

seed_lang() {
    C=build/fuzz/lang/corpus
    i=0
    for f in tools/cc/proof.c kernel/lib/string.c kernel/net/sha256.c kernel/lang/big.c; do
        { printf '\000'; cat "$f"; } > $C/c$i; i=$((i + 1))
    done
    [ -f tools/cc/proof.c.asm ] && { printf '\001'; cat tools/cc/proof.c.asm; } > $C/a$i; i=$((i + 1))
    for f in kernel/arch/x86_64/start.S kernel/user/agent.S kernel/arch/x86_64/isr.S; do
        { printf '\002'; cat "$f"; } > $C/g$i; i=$((i + 1))
    done
    [ -f build/self/string.obj ] && { printf '\003'; cat build/self/string.obj; } > $C/o$i
}

seed_pki() {
    C=build/fuzz/pki/corpus
    i=0
    for f in tools/pki/fixtures/*.der build/pki/*.der; do
        [ -f "$f" ] || continue
        { printf '\000'; cat "$f"; } > $C/c$i
        { printf '\002'; openssl x509 -in "$f" -inform DER -pubkey -noout 2>/dev/null | openssl pkey -pubin -outform DER 2>/dev/null; } > $C/k$i
        { printf '\005'; cat "$f"; } > $C/d$i
        i=$((i + 1))
    done
    python3 - $C/chain0 tools/pki/fixtures/github-leaf.der tools/pki/fixtures/github-e36.der <<'PY'
import sys, struct
out = bytearray(b"\x01")
for f in sys.argv[2:]:
    b = open(f, "rb").read(); out += struct.pack(">H", len(b)) + b
open(sys.argv[1], "wb").write(out)
PY
}

seed_html() {
    C=build/fuzz/html/corpus
    printf '<html><body><h1>Title</h1><p>Some <b>bold</b> and <a href="/x?y=1">a link</a>.</p><ul><li>one</li><li>two</li></ul><table><tr><td>a</td><td>b</td></tr></table><form action="/go" method="get"><input name="q" value="hi"><input type="submit" value="Go"></form><pre>  kept\n spaces</pre><blockquote>quoted</blockquote>&amp;&#65;&lt;</body></html>' > $C/page0
    head -c 20000 MANUAL.md > $C/manual0
}

seed_net() {
    python3 - build/fuzz/net/corpus <<'PY'
import sys, struct, os
C = sys.argv[1]
OUR = bytes.fromhex("525400123456"); PEER = bytes.fromhex("525400000002")
def frame(t, payload): return OUR + PEER + struct.pack(">H", t) + payload
def csum(b):
    s = sum(struct.unpack(">%dH" % (len(b)//2), b[:len(b)&~1])) + (b[-1] << 8 if len(b) & 1 else 0)
    while s >> 16: s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF
def ip(proto, payload, src=b"\x0a\x00\x02\x02", dst=b"\x0a\x00\x02\x0f"):
    h = struct.pack(">BBHHHBBH4s4s", 0x45, 0, 20 + len(payload), 1, 0, 64, proto, 0, src, dst)
    h = h[:10] + struct.pack(">H", csum(h)) + h[12:]
    return frame(0x0800, h + payload)
def udp(sp, dp, data): return ip(17, struct.pack(">HHHH", sp, dp, 8 + len(data), 0) + data)
def tcp(sp, dp, seq, ack, fl, data=b""): return ip(6, struct.pack(">HHIIBBHHH", sp, dp, seq, ack, 0x50, fl, 65535, 0, 0) + data)
def frames(*fs): return b"".join(struct.pack("<H", len(f)) + f for f in fs)
arp_req = frame(0x0806, struct.pack(">HHBBH", 1, 0x0800, 6, 4, 1) + PEER + b"\x0a\x00\x02\x02" + b"\0"*6 + b"\x0a\x00\x02\x0f")
arp_rep = frame(0x0806, struct.pack(">HHBBH", 1, 0x0800, 6, 4, 2) + PEER + b"\x0a\x00\x02\x02" + OUR + b"\x0a\x00\x02\x0f")
icmp = ip(1, b"\x08\x00\x00\x00\x00\x01\x00\x01" + b"ping!" * 8)
dns = udp(53, 40000, b"\x00\x01\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00" + b"\x07example\x03com\x00\x00\x01\x00\x01" + b"\xc0\x0c\x00\x01\x00\x01\x00\x00\x0e\x10\x00\x04\x5d\xb8\xd8\x22")
dhcp = udp(67, 68, b"\x02\x01\x06\x00" + b"\x12\x34\x56\x78" + b"\0"*8 + b"\x0a\x00\x02\x0f" + b"\x0a\x00\x02\x02" + b"\0"*4 + OUR + b"\0"*10 + b"\0"*192 + b"\x63\x82\x53\x63" + b"\x35\x01\x02\x36\x04\x0a\x00\x02\x02\x01\x04\xff\xff\xff\x00\x03\x04\x0a\x00\x02\x02\x06\x04\x0a\x00\x02\x03\xff")
sntp = udp(123, 123, b"\x24\x01\x00\x00" + b"\0"*36 + b"\xea\x00\x00\x00\x00\x00\x00\x00")
syn22 = tcp(40000, 22, 0x20000, 0, 0x02)
syn80 = tcp(40001, 80, 0x30000, 0, 0x02)
get80 = tcp(40001, 80, 0x30001, 1, 0x18, b"GET /hello HTTP/1.0\r\nHost: x\r\n\r\n")
pipe = udp(7800, 7800, b"EBPX\x04\0\0\0" + b"alpha".ljust(24, b"\0") + b"\x01\0\0\0\x00\x01\x00\x00")
beacon = struct.pack("<HH", 0x0080, 0) + b"\xff"*6 + PEER + PEER + b"\0\0" + b"\0"*8 + b"\x64\x00" + b"\x11\x04" + b"\x00\x07fuzznet" + b"\x01\x08\x82\x84\x8b\x96\x0c\x12\x18\x24" + b"\x03\x01\x06" + b"\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x02\x00\x00"
auth = struct.pack("<HH", 0x00b0, 0) + OUR + PEER + PEER + b"\0\0" + b"\x00\x00\x02\x00\x00\x00"
assoc = struct.pack("<HH", 0x0010, 0) + OUR + PEER + PEER + b"\0\0" + b"\x11\x04\x00\x00\x01\xc0" + b"\x01\x08\x82\x84\x8b\x96\x0c\x12\x18\x24"
eapol1 = struct.pack("<HH", 0x0008, 0) + OUR + PEER + PEER + b"\0\0" + b"\xaa\xaa\x03\x00\x00\x00\x88\x8e" + b"\x02\x03\x00\x5f\x02\x00\x8a\x00\x10" + b"\0"*8 + b"\x11"*32 + b"\0"*16 + b"\0"*8 + b"\0"*8 + b"\0"*16 + b"\x00\x00"
open(f"{C}/raw0", "wb").write(b"\x00" + frames(arp_req, arp_rep, icmp, dns, dhcp, sntp, syn22, syn80, get80, pipe))
open(f"{C}/http0", "wb").write(b"\x01" + b"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: 12\r\n\r\n<p>hello</p>")
open(f"{C}/http1", "wb").write(b"\x01" + b"HTTP/1.1 302 Found\r\nLocation: http://10.0.2.2/y\r\nContent-Length: 0\r\n\r\n")
open(f"{C}/http2", "wb").write(b"\x01" + b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n")
kexinit = b"\x00\x00\x01\x2c\x04\x14" + b"\x77"*16 + b"".join(struct.pack(">I", len(s)) + s for s in [b"curve25519-sha256", b"ssh-ed25519", b"aes128-gcm@openssh.com", b"aes128-gcm@openssh.com", b"hmac-sha2-256", b"hmac-sha2-256", b"none", b"none", b"", b""]) + b"\x00\x00\x00\x00\x00"
open(f"{C}/ssh0", "wb").write(b"\x02" + b"SSH-2.0-fuzz_1.0\r\n" + kexinit)
open(f"{C}/air0", "wb").write(b"\x03" + frames(beacon, auth, assoc, eapol1))
open(f"{C}/dns0", "wb").write(b"\x04" + struct.pack("<H", 45) + b"\x00\x01\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00" + b"\x07example\x03com\x00\x00\x01\x00\x01" + b"\xc0\x0c\x00\x01\x00\x01\x00\x00\x0e\x10\x00\x04\x0a\x00\x02\x02" + b"HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nok")
PY
}

seed_pipe() {
    python3 - build/fuzz/pipe/corpus <<'PY'
import sys, struct
C = sys.argv[1]
M = b"EBPX"
def dg(*ds): return b"".join(struct.pack("<H", len(d)) + d for d in ds)
seek = M + b"\x04\0\0\0" + b"alpha".ljust(24, b"\0") + b"\x01\0\x05\x00" + struct.pack("<I", 200) + b"\x99"*32 + b"0.8.9".ljust(24, b"\0") + b"\x01" + b"\x0a\x00\x02\x17\x78\x1e"
here = M + b"\x05\0\0\0" + b"beta".ljust(24, b"\0") + b"\x01\0\x05\x00" + struct.pack("<I", 200) + b"\x99"*32 + b"0.8.9".ljust(24, b"\0")
rotate = M + b"\x0d\0\0\0" + b"\xa0"*32 + b"\xc1"*32 + b"\x01"*64 + b"\x02"*64
vouch = M + b"\x0e\0\0\0" + b"\xa0"*32 + b"\xd1"*32 + b"\x0a\x00\x02\x18" + b"\x78\x1e" + b"gamma".ljust(24, b"\0") + b"\x03"*64
unvouch = M + b"\x0f\0\0\0" + b"\xa0"*32 + b"\xd1"*32 + b"\x04"*64
hello = M + b"\x06\0\0\0" + struct.pack("<I", 0x4242) + b"\x50"*32 + b"\xa0"*32 + b"\x33"*64 + b"far".ljust(24, b"\0") + b"0.9".ljust(24, b"\0")
offer = M + b"\x01\0\0\0" + struct.pack("<Q", 7) + struct.pack("<IIQ", 3, 0, 40) + b"a gift".ljust(32, b"\0")
chunk = M + b"\x02\0\0\0" + struct.pack("<Q", 7) + struct.pack("<II", 0, 40) + b"x"*40
taken = M + b"\x03\0\0\0" + struct.pack("<Q", 7)
ask = M + b"\x09\0\0\0" + struct.pack("<Q", 9) + struct.pack("<IIqq", 20, 0, 1, 100) + b"say hello".ljust(64, b"\0")
answer = M + b"\x0a\0\0\0" + struct.pack("<Q", 9) + b"\x00" + b"42".ljust(24, b"\0")
say = M + b"\x0b\0\0\0" + struct.pack("<Q", 0) + b"hello there"
have = M + b"\x0c\0\0\0" + struct.pack("<Q", 7) + struct.pack("<I", 40)
open(f"{C}/plain0", "wb").write(b"\x06" + dg(seek, here, rotate, vouch, unvouch, hello))
open(f"{C}/sealed0", "wb").write(b"\x07" + dg(offer, chunk, taken, ask, answer, say, have))
open(f"{C}/sealed1", "wb").write(b"\x05" + dg(offer, chunk, have, say))
PY
}

seed_tls() {
    python3 - build/fuzz/tls/corpus <<'PY'
import sys, struct
C = sys.argv[1]
def rec(t, body): return bytes([t, 3, 3]) + struct.pack(">H", len(body)) + body
def hs(t, body): return bytes([t]) + len(body).to_bytes(3, "big") + body
def ext(t, body): return struct.pack(">HH", t, len(body)) + body
sh = hs(2, b"\x03\x03" + b"\x11"*32 + b"\x00" + b"\x13\x01" + b"\x00" + struct.pack(">H", 0) )
exts = ext(0x2b, b"\x03\x04") + ext(0x33, b"\x00\x1d\x00\x20" + b"\x22"*32)
sh = hs(2, b"\x03\x03" + b"\x11"*32 + b"\x00" + b"\x13\x01" + b"\x00" + struct.pack(">H", len(exts)) + exts)
cert = open("tools/pki/fixtures/github-leaf.der", "rb").read()
certmsg = hs(11, b"\x00" + (len(cert) + 5).to_bytes(3, "big") + len(cert).to_bytes(3, "big") + cert + b"\x00\x00")
ee = hs(8, b"\x00\x00")
cv = hs(15, b"\x04\x03" + struct.pack(">H", 70) + b"\x30\x44\x02\x20" + b"\x01"*32 + b"\x02\x20" + b"\x02"*32)
fin = hs(20, b"\x00"*32)
def app(body): return rec(23, body + b"\x16")
def appdata(body): return rec(23, body + b"\x17")
stream = b"\x00" + rec(22, sh) + rec(20, b"\x01") + app(ee) + app(certmsg) + app(cv) + app(fin) + appdata(b"HTTP/1.0 200 OK\r\nContent-Length: 5\r\n\r\nhello") + rec(21, b"\x01\x00\x17")
open(f"{C}/hs0", "wb").write(stream)
open(f"{C}/hs1", "wb").write(b"\x05" + rec(22, sh) + rec(21, b"\x02\x28"))
PY
}

run() {   # run <tool> <max_len>
    t=$1
    echo "=== $t: fuzzing for $SECS s"
    ASAN_OPTIONS=detect_leaks=0 build/fuzz/$t/${t}_fuzz -max_total_time=$SECS -max_len=$2 -timeout=20 \
        -artifact_prefix=build/fuzz/$t/crashes/ -print_final_stats=1 build/fuzz/$t/corpus 2>&1 \
        | grep -E 'ERROR|SUMMARY|runtime error|stat::number|stat::new|crash|timeout|artifact' | tail -12
    ls build/fuzz/$t/crashes 2>/dev/null | head
}

for t in $TOOLS; do
    case $t in
        lang) build lang kernel/lang/cc.c kernel/lang/asm.c kernel/lang/ld.c kernel/lang/gnu.c && seed_lang && run lang 65536 ;;
        pki)  build pki kernel/net/asn1.c kernel/net/bn.c kernel/net/ec.c kernel/net/rsa.c kernel/net/x509.c kernel/net/sha256.c kernel/net/sha512.c && seed_pki && run pki 8192 ;;
        html) build html kernel/gfx/html.c && seed_html && run html 32768 ;;
        net)  build net kernel/net/net.c kernel/net/ssh.c kernel/net/wifi.c kernel/net/nodes.c kernel/net/sha256.c kernel/net/x25519.c kernel/lib/base64.c && seed_net && run net 65536 ;;
        pipe) build pipe kernel/net/pipe.c kernel/net/nodes.c kernel/net/sha256.c kernel/lib/base64.c && seed_pipe && run pipe 65536 ;;
        tls)  build tls kernel/net/tls.c kernel/net/asn1.c kernel/net/bn.c kernel/net/ec.c kernel/net/rsa.c kernel/net/x509.c kernel/net/pki_selftest.c kernel/net/sha512.c kernel/net/x25519.c && seed_tls && run tls 65536 ;;
        *) echo "no such tool: $t" ;;
    esac
done
