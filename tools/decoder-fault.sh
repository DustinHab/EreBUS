#!/bin/sh
# decoder-fault.sh -- the picture decoder runs in ring 3: a decoder that faults or never answers costs one picture,
# not the kernel.
# - the decoder program is rebuilt with its deliberate faults (DECODER_EXTRA=-DEREBUS_TEST_FAULT=4, decoder.c): a
#   jpeg takes it down on a page fault, a webp makes it never answer; the kernel is built as it is
# - the page: a png (decoded), a jpeg (the decoder faults), a webp (the decoder hangs, ended after the limit), and
#   bytes that are no picture (refused)
# - then another page is opened, and the first again with the browser leaving while the webp's decoder still hangs:
#   the shell answers meanwhile, and the decoder is ended with the page
# - the ordinary decoder is built back at the end
cd "$(dirname "$0")/.."
. tools/testlib.sh
W=$BUILD/decfault
PORT=${DECPORT:-8481}
rm -rf $W; mkdir -p $W

# The decoder with its faults, into a boot disk of this test's own; the
# kernel's own objects are up to date and stay as they are.
rm -rf build/decoder
make -s DECODER_EXTRA=-DEREBUS_TEST_FAULT=4 >/dev/null || { echo "FAILED: the faulting decoder did not build"; exit 1; }
cp build/esp.img $W/esp.img
rm -rf build/decoder
make -s >/dev/null || { echo "FAILED: the ordinary decoder did not build back"; exit 1; }

cat > $W/serve.py <<'PY'
import sys, zlib, struct, io, os
from http.server import BaseHTTPRequestHandler, HTTPServer
port = int(sys.argv[1])

def png16():
    w = h = 16
    rows = b""
    for y in range(h):
        rows += b"\x00" + b"".join(bytes((x * 16, y * 16, 128)) for x in range(w))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")

def pil(kind):
    try:
        from PIL import Image
        im = Image.new("RGB", (24, 16))
        for x in range(24):
            for y in range(16): im.putpixel((x, y), (x * 10, 200 - y * 10, 90))
        b = io.BytesIO(); im.save(b, kind); return b.getvalue()
    except Exception as e:
        sys.stderr.write("server: no pil: %s\n" % e); return b""

PAGE = b"""<html><head><title>Decoder faults</title></head><body>
<p>four pictures, three of them trouble</p>
<img src="/i.png" alt="a png"> <img src="/p.jpg" alt="a jpeg"> <img src="/i.webp" alt="a webp"> <img src="/bad.png" alt="no picture">
<p><a href="/second">the second page</a></p>
</body></html>"""

class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    def log_message(self, *a): pass
    def send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        sys.stderr.write("server: GET %s\n" % self.path); sys.stderr.flush()
        p = self.path
        if p == "/": self.send(200, "text/html", PAGE)
        elif p == "/second": self.send(200, "text/html", b"<html><head><title>Second</title></head><body><p>the second page</p><p><a href='/'>home</a></p></body></html>")
        elif p == "/i.png": self.send(200, "image/png", png16())
        elif p == "/p.jpg": self.send(200, "image/jpeg", pil("JPEG"))
        elif p == "/i.webp": self.send(200, "image/webp", pil("WEBP"))
        elif p == "/bad.png": self.send(200, "image/png", b"\x89PNG\r\n\x1a\n" + os.urandom(700))
        else: self.send(404, "text/html", b"<html><body><p>no such page</p></body></html>")

HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
python3 $W/serve.py $PORT > $W/server.log 2>&1 &
SRV=$!
waitport $PORT 20

fresh_store $W/store.img
fresh_vars $W/vars.fd
LOG=$W/boot.log
rm -f $LOG
{
    bootwait $LOG
    waitlog $LOG 'by lease' 30
    keys tab tab tab tab tab tab pause
    say "10.0.2.100/"
    waitlog $LOG 'get http://10.0.2.100/ -> 200' 20
    # the png decodes; the jpeg's decoder faults; the webp's decoder hangs and is ended after the limit;
    # the bytes that are no picture are refused
    waitlog $LOG 'picture /i.png: png 16x16' 30
    waitlog $LOG 'picture /p.jpg: the decoder ended without an answer' 30
    waitlog $LOG 'picture /i.webp: the decoder was ended after the time limit' 60
    waitlog $LOG 'picture /bad.png: not a picture the decoder reads' 30
    sleep 1
    echo "screendump $W/page.ppm"
    say "10.0.2.100/second"
    waitlog $LOG 'get http://10.0.2.100/second -> 200' 20
    # the first page again; leave while the webp's decoder still hangs
    say "10.0.2.100/"
    waitcount $LOG 'picture /p.jpg: the decoder ended without an answer' 2 30
    sleep 2
    say "10.0.2.100/second"
    waitcount $LOG 'get http://10.0.2.100/second -> 200' 2 20
    waitcount $LOG '(decoder) ended; all capabilities released' 7 20
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$W/vars.fd \
  -drive format=raw,file=$W/esp.img \
  -drive id=store,file=$W/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev "user,id=n0,guestfwd=tcp:10.0.2.100:80-cmd:nc -N 127.0.0.1 $PORT" \
  -serial file:$LOG >/dev/null 2>&1
kill $SRV 2>/dev/null
wait 2>/dev/null
python3 tools/ppm2png.py $W/page.ppm $W/page.png 2>/dev/null

echo "--- what the browser and the kernel said ---"
grep -a -E 'web:  (get|picture)|proc: .*decoder|the decoder gave|faulted in ring 3' $LOG | cut -c1-120
echo "--- the checks ---"
ok=1
grep -aq 'picture /i.png: png 16x16' $LOG && echo "the png was decoded by a program in ring 3" || { echo "FAILED: png"; ok=0; }
grep -aq 'proc: thread [0-9]* (decoder) faulted in ring 3: page fault' $LOG && echo "the jpeg took its decoder down on a page fault, in ring 3" || { echo "FAILED: no ring-3 fault"; ok=0; }
grep -aq 'picture /p.jpg: the decoder ended without an answer' $LOG && echo "and the page went on without the jpeg" || { echo "FAILED: jpeg outcome"; ok=0; }
grep -aq 'the decoder gave no answer in 30 s' $LOG && grep -aq 'picture /i.webp: the decoder was ended after the time limit' $LOG && echo "the webp's decoder never answered and was ended after the limit" || { echo "FAILED: hang"; ok=0; }
grep -aq 'picture /bad.png: not a picture the decoder reads' $LOG && echo "bytes that are no picture were refused" || { echo "FAILED: refusal"; ok=0; }
[ "$(count $LOG 'get http://10.0.2.100/second -> 200')" -ge 2 ] && echo "the browser went on to another page twice, the second time while a decoder hung" || { echo "FAILED: the browser did not go on"; ok=0; }
[ "$(count $LOG 'the decoder gave no answer in 30 s')" -eq 1 ] && echo "the second time the hanging decoder was ended with the page, before the limit" || { echo "FAILED: the hanging decoder ran to the limit again"; ok=0; }
[ "$(count $LOG '(decoder) ended; all capabilities released')" -ge 7 ] && echo "every decoder process was reaped" || { echo "FAILED: decoders not reaped ($(count $LOG '(decoder) ended; all capabilities released'))"; ok=0; }
grep -aq 'kern: panic' $LOG && { echo "FAILED: the kernel panicked"; ok=0; }
[ $ok = 1 ] && echo "a decoder that faults, hangs or refuses costs its picture and nothing else" || echo "the decoder faults FAILED"
