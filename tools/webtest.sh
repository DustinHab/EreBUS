#!/bin/sh
# webtest.sh -- the browser against a server of its own on the host.
# - the page: a title, utf-8 prose, links, two pictures (png, jpeg), a form that posts
# - boot 1: the page fetched and rendered (pictures decoded), a link followed by keyboard and back again,
#   the form filled and posted -> a cookie set and a redirect followed with the cookie sent, the page kept
#   as a bookmark, a gzip page and a chunked page fetched
# - boot 2: the bookmark and the cookie are back; the cookie rides on the next request
# - the server reaches the guest through qemu's guestfwd, a netcat per connection (like update-test)
cd "$(dirname "$0")/.."
. tools/testlib.sh
W=$BUILD/web
PORT=${WEBPORT:-8480}
rm -rf $W; mkdir -p $W

cat > $W/serve.py <<'PY'
import sys, zlib, struct, gzip, io
from http.server import BaseHTTPRequestHandler, HTTPServer
port = int(sys.argv[1])

def png16():
    w = h = 16
    rows = b""
    for y in range(h):
        rows += b"\x00" + b"".join(bytes((x * 16, y * 16, 128)) for x in range(w))
    def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")

def jpeg24():
    try:
        from PIL import Image
        im = Image.new("RGB", (24, 16))
        for x in range(24):
            for y in range(16): im.putpixel((x, y), (x * 10, 200 - y * 10, 90))
        b = io.BytesIO(); im.save(b, "JPEG", quality=90); return b.getvalue()
    except Exception as e:
        sys.stderr.write("server: no pil: %s\n" % e); return b""

PAGE = """<html><head><title>Erebus test page</title></head><body>
<h1>Hello from the host</h1>
<p>Some prose with umlauts: &auml;&ouml;&uuml; and Ω and &#x2014; and a link to <a href="/second">the second page</a>.</p>
<p><a href="gz">a packed page</a> and <a href="/chunk">a page in chunks</a></p>
<img src="/i.png" alt="a png"> <img src="p.jpg" alt="a jpeg">
<form action="/login" method="post">name <input name="user" value=""> word <input type="password" name="pass" value="x"> <input type="submit" value="sign in"></form>
</body></html>""".encode("utf-8")

class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    def log_message(self, *a): pass
    def note(self, body=b""):
        sys.stderr.write("server: %s %s | cookie=%s | body=%s | enc=%s\n" % (
            self.command, self.path, self.headers.get("Cookie", "-"), body.decode("latin-1"),
            self.headers.get("Accept-Encoding", "-")))
        sys.stderr.flush()
    def send(self, code, ctype, body, extra=()):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in extra: self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        self.note()
        p = self.path
        if p == "/": self.send(200, "text/html; charset=utf-8", PAGE)
        elif p == "/second": self.send(200, "text/html", b"<html><head><title>Second</title></head><body><h2>the second page</h2><p><a href='/'>home</a></p></body></html>")
        elif p == "/home":
            c = self.headers.get("Cookie", "")
            self.send(200, "text/html", ("<html><head><title>Home</title></head><body><p>cookie: %s</p></body></html>" % (c or "none")).encode())
        elif p == "/gz":
            body = gzip.compress(b"<html><head><title>Packed</title></head><body><p>" + b"packed prose " * 300 + b"</p></body></html>")
            self.send(200, "text/html", body, [("Content-Encoding", "gzip")])
        elif p == "/chunk":
            self.wfile.write(b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n")
            for piece in [b"<html><head><title>Chunks</title></head><body>", b"<p>one chunk</p>", b"<p>another chunk</p></body></html>"]:
                self.wfile.write(b"%x\r\n" % len(piece) + piece + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        elif p == "/i.png": self.send(200, "image/png", png16())
        elif p == "/p.jpg": self.send(200, "image/jpeg", jpeg24())
        else: self.send(404, "text/html", b"<html><body><p>no such page</p></body></html>")
    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(n)
        self.note(body)
        if self.path == "/login":
            self.send_response(303)
            self.send_header("Set-Cookie", "session=abc123; Path=/; Max-Age=3600")
            self.send_header("Location", "/home")
            self.send_header("Content-Length", "0")
            self.end_headers()
        else:
            self.send(404, "text/html", b"<p>no</p>")

HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
python3 $W/serve.py $PORT > $W/server.log 2>&1 &
SRV=$!
waitport $PORT 20

fresh_store $W/store.img
fresh_vars $W/vars.fd
FWD="user,id=n0,guestfwd=tcp:10.0.2.100:80-cmd:nc -N 127.0.0.1 $PORT"
boot() {   # boot <log> <keys function>
    LOG=$1
    rm -f $LOG
    {
        bootwait $LOG
        $2
        echo quit
    } | qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$W/vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -drive id=store,file=$W/store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev "$FWD" \
      -serial file:$LOG >/dev/null 2>&1
}

first() {
    waitlog $LOG 'by lease' 30
    keys tab tab tab tab tab tab pause
    say "10.0.2.100/"
    waitlog $LOG 'get http://10.0.2.100/ -> 200' 30
    waitlog $LOG 'picture p.jpg' 30
    sleep 1
    echo "screendump $W/page.ppm"
    keys n ret pause
    waitlog $LOG 'get http://10.0.2.100/second -> 200' 20
    keys backspace pause
    waitcount $LOG 'get http://10.0.2.100/ -> 200' 2 20
    waitlog $LOG 'picture p.jpg' 5
    keys n n n n ret pause
    say "erebus"
    waitlog $LOG 'at http://10.0.2.100/home' 20
    keys b pause
    waitlog $LOG 'kept http://10.0.2.100/home' 10
    say "10.0.2.100/gz"
    waitlog $LOG 'gz -> 200' 20
    say "10.0.2.100/chunk"
    waitlog $LOG 'chunk -> 200' 20
    sleep 1
    echo "screendump $W/chunk.ppm"
    waitlog $LOG 'generation [0-9]* written' 30
    sleep 1
}

# The session comes back where it was left: in the browser, so no tabs.
second() {
    waitlog $LOG 'by lease' 30
    sleep 1
    echo "screendump $W/marks.ppm"
    say "10.0.2.100/home"
    waitlog $LOG 'get http://10.0.2.100/home -> 200' 20
    sleep 1
}

boot $W/boot1.log first
boot $W/boot2.log second
kill $SRV 2>/dev/null
wait 2>/dev/null
python3 tools/ppm2png.py $W/page.ppm $W/page.png 2>/dev/null
python3 tools/ppm2png.py $W/chunk.ppm $W/chunk.png 2>/dev/null
python3 tools/ppm2png.py $W/marks.ppm $W/marks.png 2>/dev/null

echo "--- boot 1: what the browser said ---"
grep -a 'web:' $W/boot1.log | cut -c1-130
echo "--- boot 2 ---"
grep -a 'web:' $W/boot2.log | cut -c1-130
echo "--- the server ---"
grep 'server:' $W/server.log | cut -c1-130
echo "--- the checks ---"
ok=1
grep -aq 'get http://10.0.2.100/ -> 200' $W/boot1.log && echo "the page was fetched" || { echo "FAILED: no page"; ok=0; }
grep -aq 'picture /i.png: png 16x16' $W/boot1.log && echo "the png was decoded" || { echo "FAILED: png"; ok=0; }
grep -aq 'picture p.jpg: jpeg 24x16' $W/boot1.log && echo "the jpeg was decoded" || { echo "FAILED: jpeg"; ok=0; }
grep -aq 'get http://10.0.2.100/second -> 200' $W/boot1.log && echo "a link was followed by keyboard" || { echo "FAILED: link"; ok=0; }
grep -aq 'post http://10.0.2.100/login -> 200.* at http://10.0.2.100/home' $W/boot1.log && echo "the post was answered with a move, followed" || { echo "FAILED: redirect after post"; ok=0; }
[ "$(count $W/boot1.log 'get http://10.0.2.100/ -> 200')" -ge 2 ] && echo "and backspace went back" || { echo "FAILED: back"; ok=0; }
grep -q 'POST /login .*body=user=erebus&pass=x' $W/server.log && echo "the form was posted with what was typed" || { echo "FAILED: post"; ok=0; }
grep -aq 'cookie from 10.0.2.100: session (kept)' $W/boot1.log && echo "the cookie was taken" || { echo "FAILED: cookie"; ok=0; }
grep -q 'GET /home | cookie=session=abc123' $W/server.log && echo "and sent on the redirect that followed" || { echo "FAILED: cookie not sent"; ok=0; }
grep -aq 'kept http://10.0.2.100/home as a bookmark' $W/boot1.log && echo "the page was kept" || { echo "FAILED: bookmark"; ok=0; }
grep -aq 'gz -> 200.*gzip' $W/boot1.log && echo "a gzip page was unpacked" || { echo "FAILED: gzip"; ok=0; }
grep -aq 'chunk -> 200.*chunked' $W/boot1.log && echo "a chunked page was joined" || { echo "FAILED: chunked"; ok=0; }
grep -aq '1 bookmark kept' $W/boot2.log && echo "the bookmark was there at the next start" || { echo "FAILED: bookmark gone"; ok=0; }
grep -aq '1 cookie kept from before' $W/boot2.log && echo "so was the cookie" || { echo "FAILED: cookie gone"; ok=0; }
[ "$(grep -c 'GET /home | cookie=session=abc123' $W/server.log)" -ge 2 ] && echo "and it rode on the first request after" || { echo "FAILED: kept cookie not sent"; ok=0; }
[ $ok = 1 ] && echo "the browser fetches, renders, follows, posts, keeps cookies and bookmarks" || echo "the browser FAILED"
