#!/bin/sh
# renderer-fault.sh -- the page renderer runs in ring 3: a page that breaks its renderer costs that page, not the
# kernel, and not the next page.
# - the renderer program is rebuilt with its deliberate faults (RENDERER_EXTRA=-DEREBUS_TEST_FAULT=5, renderer.c):
#   a page beginning "<!--fault-->" takes it down on a page fault, one beginning "<!--hang-->" makes it never
#   answer; the kernel is built as it is
# - the pages: an ordinary one (laid out), the faulting one (the browser says so and goes on), the hanging one
#   (its renderer ended after the limit), then the ordinary one again; the hanging one once more, left at once:
#   the renderer is ended with the page and the next page is laid out without waiting for the limit
# - the ordinary renderer is built back at the end
cd "$(dirname "$0")/.."
. tools/testlib.sh
W=$BUILD/renfault
PORT=${RENPORT:-8482}
rm -rf $W; mkdir -p $W

rm -rf build/renderer
make -s RENDERER_EXTRA=-DEREBUS_TEST_FAULT=5 >/dev/null || { echo "FAILED: the faulting renderer did not build"; exit 1; }
cp build/esp.img $W/esp.img
rm -rf build/renderer
make -s >/dev/null || { echo "FAILED: the ordinary renderer did not build back"; exit 1; }

cat > $W/serve.py <<'PY'
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
port = int(sys.argv[1])
PAGE = b"<html><head><title>Renderer faults</title></head><body><h1>an ordinary page</h1><p>with <a href='/fault'>a page that faults its renderer</a> and <a href='/hang'>one that hangs it</a>.</p></body></html>"
FAULT = b"<!--fault--><html><head><title>Fault</title></head><body><p>this page takes the renderer down</p></body></html>"
HANG = b"<!--hang--><html><head><title>Hang</title></head><body><p>this page never lets the renderer answer</p></body></html>"
class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    def log_message(self, *a): pass
    def send(self, code, body):
        self.send_response(code)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        sys.stderr.write("server: GET %s\n" % self.path); sys.stderr.flush()
        if self.path == "/": self.send(200, PAGE)
        elif self.path == "/fault": self.send(200, FAULT)
        elif self.path == "/hang": self.send(200, HANG)
        else: self.send(404, b"<html><body><p>no such page</p></body></html>")
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
    waitlog $LOG 'styles: ' 30
    say "10.0.2.100/fault"
    waitlog $LOG 'page not laid out: the renderer ended without a page' 30
    say "10.0.2.100/"
    waitcount $LOG 'styles: ' 2 30
    say "10.0.2.100/hang"
    waitlog $LOG 'page not laid out: the renderer was ended after the time limit' 60
    say "10.0.2.100/"
    waitcount $LOG 'styles: ' 3 30
    # the hanging page once more, left at once
    say "10.0.2.100/hang"
    waitlog $LOG 'get http://10.0.2.100/hang -> 200' 20
    sleep 3
    say "10.0.2.100/"
    waitcount $LOG 'styles: ' 4 30
    sleep 1
    echo "screendump $W/page.ppm"
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
grep -a -E 'web:  (get|styles|page)|proc: .*renderer|the renderer gave|faulted in ring 3' $LOG | cut -c1-120
echo "--- the checks ---"
ok=1
[ "$(count $LOG 'styles: ')" -ge 4 ] && echo "the ordinary page was laid out by a program in ring 3, four times" || { echo "FAILED: the ordinary page ($(count $LOG 'styles: ') times)"; ok=0; }
grep -aq 'proc: thread [0-9]* (renderer) faulted in ring 3: page fault' $LOG && echo "the faulting page took its renderer down on a page fault, in ring 3" || { echo "FAILED: no ring-3 fault"; ok=0; }
grep -aq 'page not laid out: the renderer ended without a page' $LOG && echo "and the browser said so and went on" || { echo "FAILED: fault outcome"; ok=0; }
grep -aq 'the renderer gave no answer in 30 s' $LOG && grep -aq 'page not laid out: the renderer was ended after the time limit' $LOG && echo "the hanging page's renderer never answered and was ended after the limit" || { echo "FAILED: hang"; ok=0; }
[ "$(count $LOG 'the renderer gave no answer in 30 s')" -eq 1 ] && echo "the second time it was ended with the page, and the next page came without waiting" || { echo "FAILED: the hanging renderer ran to the limit again"; ok=0; }
[ "$(count $LOG '(renderer) ended; all capabilities released')" -ge 6 ] && echo "every renderer process was reaped" || { echo "FAILED: renderers not reaped ($(count $LOG '(renderer) ended; all capabilities released'))"; ok=0; }
grep -aq 'kern: panic' $LOG && { echo "FAILED: the kernel panicked"; ok=0; }
[ $ok = 1 ] && echo "a page that faults or hangs its renderer costs that page and nothing else" || echo "the renderer faults FAILED"
