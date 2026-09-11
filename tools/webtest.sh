#!/bin/sh
# webtest.sh -- the browser against a server of its own on the host.
# - the page: a title, utf-8 prose, links, three pictures (png, jpeg, webp), a slow fourth one, a form that posts,
#   a stylesheet of its own and an inline one, a link hidden by a rule, a navigation that folds
# - boot 1: the page fetched and rendered (pictures decoded, the sheet read), the first link followed by keyboard
#   while the slow picture is still coming (it is not the hidden one, nor one of the folded navigation's) and back
#   again, the navigation opened and its first link followed, the styles turned off (the hidden link is back)
#   and on, the form filled and posted -> a cookie set and a redirect followed with the cookie sent, the page
#   kept as a bookmark, a gzip page and a chunked page fetched
# - boot 2: the bookmark and the cookie are back; the cookie rides on the next request
# - the server reaches the guest through qemu's guestfwd, a netcat per connection (like update-test)
cd "$(dirname "$0")/.."
. tools/testlib.sh
W=$BUILD/web
PORT=${WEBPORT:-8480}
rm -rf $W; mkdir -p $W

cat > $W/serve.py <<'PY'
import sys, zlib, struct, gzip, io, time
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

def webp16():
    try:
        from PIL import Image
        im = Image.new("RGB", (16, 16))
        for x in range(16):
            for y in range(16): im.putpixel((x, y), ((x * 15) & 255, (y * 15) & 255, 140))
        b = io.BytesIO(); im.save(b, "WEBP", quality=80, method=4); return b.getvalue()
    except Exception as e:
        sys.stderr.write("server: no pil webp: %s\n" % e); return b""

SHEET = b""".gone { display: none }
.warm { color: #a00000; font-weight: bold }
.mid { text-align: center }
@media (max-width: 600px) { h1 { display: none } }
"""

PAGE = """<html><head><title>Erebus test page</title>
<link rel="stylesheet" href="/s.css"><style>#trap { display: none }</style></head><body>
<nav><a href="/n1">one</a> <a href="/n2">two</a> <a href="/n3">three</a></nav>
<div id="trap"><a href="/trap">a link a rule hides</a></div>
<h1 class="warm">Hello from the host</h1>
<p class="mid">Some prose with umlauts: &auml;&ouml;&uuml; and Î© and &#x2014; and a link to <a href="/second">the second page</a>.</p>
<p><a href="gz">a packed page</a> and <a href="/chunk">a page in chunks</a> <span class="gone">and words a class hides</span></p>
<img src="/i.png" alt="a png"> <img src="p.jpg" alt="a jpeg"> <img src="/i.webp" alt="a webp"> <img src="/slow.png" alt="a slow one">
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
        elif p == "/s.css": self.send(200, "text/css", SHEET)
        elif p == "/second": self.send(200, "text/html", b"<html><head><title>Second</title></head><body><h2>the second page</h2><p><a href='/'>home</a></p></body></html>")
        elif p == "/n1": self.send(200, "text/html", b"<html><head><title>One</title></head><body><p>the navigation's first</p></body></html>")
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
        elif p == "/i.webp": self.send(200, "image/webp", webp16())
        elif p == "/slow.png":
            time.sleep(3)
            self.send(200, "image/png", png16())
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

# The main page again, with its sheet and pictures: the count of loads so
# far. A fresh load leaves the keyboard spot on nothing, so what follows
# can step from a known place. The navigation folds, so the visible links
# are the three in the body: the second page, the packed page, the chunked
# page; then the form's fields; then the fold's own line.
home_again() {
    waitcount $LOG 'get http://10.0.2.100/ -> 200' $1 20
    waitcount $LOG 'styles: .* 1 of 1 sheets' $1 20
    waitcount $LOG 'picture p.jpg' $1 20
}

first() {
    waitlog $LOG 'by lease' 30
    keys tab tab tab tab tab tab pause
    say "10.0.2.100/"
    home_again 1
    sleep 1
    echo "screendump $W/page.ppm"
    # the first link in hand while the slow picture is still coming; it is the
    # body's second-page link, not the hidden one nor one of the folded navigation's
    keys n ret pause
    waitlog $LOG 'get http://10.0.2.100/second -> 200' 20
    keys backspace pause
    home_again 2
    # the form, on the known layout: three body links, then the name field
    keys n n n n ret pause
    say "erebus"
    waitlog $LOG 'at http://10.0.2.100/home' 20
    keys b pause
    waitlog $LOG 'kept http://10.0.2.100/home' 10
    # back to the main page for the fold and the styles
    say "10.0.2.100/"
    home_again 3
    # the fold is the last spot: open it
    keys p ret pause
    waitlog $LOG 'fold 0 opened' 10
    sleep 1
    echo "screendump $W/open.ppm"
    # the styles off: the whole page as it came, the hidden link among it
    keys s pause
    waitlog $LOG 'styles off' 10
    sleep 1
    echo "screendump $W/plain.ppm"
    # plain document order: the navigation's three, then the link a rule hid
    keys n n n n ret pause
    waitlog $LOG 'trap -> 404' 20
    keys s pause
    waitlog $LOG 'styles on' 10
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
for p in page open plain chunk marks; do python3 tools/ppm2png.py $W/$p.ppm $W/$p.png 2>/dev/null; done

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
grep -aq 'picture /i.webp: webp 16x16' $W/boot1.log && echo "the webp was decoded" || { echo "FAILED: webp"; ok=0; }
[ "$(count $W/boot1.log '(decoder) ended; all capabilities released')" -ge 3 ] && echo "each by a program in ring 3 that was ended after" || { echo "FAILED: no decoder programs"; ok=0; }
grep -aq 'get http://10.0.2.100/s.css -> 200.*text/css' $W/boot1.log && echo "the stylesheet was fetched" || { echo "FAILED: sheet"; ok=0; }
grep -aq 'styles: [0-9]* rules, 1 of 1 sheets, 2 parts hidden, 1 folds' $W/boot1.log && echo "and read: two parts hidden, the navigation folded" || { echo "FAILED: styles"; ok=0; }
grep -aq 'get http://10.0.2.100/second -> 200' $W/boot1.log && echo "a link was followed by keyboard" || { echo "FAILED: link"; ok=0; }
awk '/GET \/slow.png/ { s = 1 } /GET \/second/ { if (s) ok = 1 } END { exit !ok }' $W/server.log && echo "while the slow picture was still coming" || { echo "FAILED: the slow picture did not precede the link"; ok=0; }
[ "$(count $W/boot1.log 'get http://10.0.2.100/ -> 200')" -ge 2 ] && echo "and backspace went back" || { echo "FAILED: back"; ok=0; }
grep -aq 'fold 0 opened' $W/boot1.log && echo "the navigation folded, and opened on a press" || { echo "FAILED: fold"; ok=0; }
grep -aq 'styles off' $W/boot1.log && grep -aq 'trap -> 404' $W/boot1.log && echo "with the styles off the hidden link was there" || { echo "FAILED: plain"; ok=0; }
[ "$(grep -c 'GET /trap' $W/server.log)" -eq 1 ] && echo "and only then" || { echo "FAILED: the hidden link was followed while styled"; ok=0; }
grep -aq 'post http://10.0.2.100/login -> 200.* at http://10.0.2.100/home' $W/boot1.log && echo "the post was answered with a move, followed" || { echo "FAILED: redirect after post"; ok=0; }
grep -q 'POST /login .*body=user=erebus&pass=x' $W/server.log && echo "the form was posted with what was typed" || { echo "FAILED: post"; ok=0; }
grep -aq 'cookie from 10.0.2.100: session (kept)' $W/boot1.log && echo "the cookie was taken" || { echo "FAILED: cookie"; ok=0; }
grep -q 'GET /home | cookie=session=abc123' $W/server.log && echo "and sent on the redirect that followed" || { echo "FAILED: cookie not sent"; ok=0; }
grep -aq 'kept http://10.0.2.100/home as a bookmark' $W/boot1.log && echo "the page was kept" || { echo "FAILED: bookmark"; ok=0; }
grep -aq 'gz -> 200.*gzip' $W/boot1.log && echo "a gzip page was unpacked" || { echo "FAILED: gzip"; ok=0; }
grep -aq 'chunk -> 200.*chunked' $W/boot1.log && echo "a chunked page was joined" || { echo "FAILED: chunked"; ok=0; }
grep -aq '1 bookmark kept' $W/boot2.log && echo "the bookmark was there at the next start" || { echo "FAILED: bookmark gone"; ok=0; }
grep -aq '1 cookie kept from before' $W/boot2.log && echo "so was the cookie" || { echo "FAILED: cookie gone"; ok=0; }
[ "$(grep -c 'GET /home | cookie=session=abc123' $W/server.log)" -ge 2 ] && echo "and it rode on the first request after" || { echo "FAILED: kept cookie not sent"; ok=0; }
[ $ok = 1 ] && echo "the browser fetches, styles, folds, follows, posts, keeps cookies and bookmarks" || echo "the browser FAILED"
