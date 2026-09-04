#!/bin/sh
# pipe-update.sh -- a kernel through the pipe: B updates A, once A's nodes table lets it.
# - A boots a kernel built as "older"; B the current one; both on one cable (QEMU socket netdev)
# - words typed through the screen terminal: names alpha/beta, addresses, B points at A and says hello (the handshake writes both rows)
# - B's first 'update alpha' is refused (A's row for beta grants nothing); A: 'allow beta update'; B's second goes through
# - A installs the kernel, restarts, and its second boot line names the current version
# - OLDESP may name a ready image built as "older"; without it, this script builds one (make, twice)

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8012}

if [ -n "$OLDESP" ] && [ -f "$OLDESP" ]; then
    cp "$OLDESP" $BUILD/peer-esp.img
else
    make -s VERSION=older >/dev/null || exit 1
    cp build/esp.img $BUILD/peer-esp.img
    make -s >/dev/null || exit 1
    [ "$BUILD" = build ] || cp build/esp.img $BUILD/esp.img
fi
NOW=$(sed -n 's/.*"\(.*\)".*/\1/p' build/version.c)

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG \
      $BUILD/teststore.img $BLOG $BUILD/update-a.ppm $BUILD/update-b.ppm \
      $BUILD/upd-ready $BUILD/upd-allowed $BUILD/upd-done
fresh_store $BUILD/peerstore.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd

# --- A: alpha at 10.9.9.20, runs the older kernel, lets beta update it later ---
{
    bootwait $ALOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "back"
    say "back"
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/upd-ready
    waitlog $ALOG 'may not update this machine; declined' 120
    say "allow beta update"
    touch $BUILD/upd-allowed
    say "nodes"
    waitfile $BUILD/upd-done 240
    sleep 1
    echo "screendump $BUILD/update-a.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>$BUILD/upd-a.err &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- B: beta at 10.9.9.21, points at alpha, handshakes, updates it ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "write peer | 10.9.9.20 7800"
    say "back"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/upd-ready 90
    sleep 1
    say "say hello"
    waitlog $BLOG 'session with' 30
    say "update alpha"
    waitlog $BLOG 'declined it' 60
    waitfile $BUILD/upd-allowed 120
    sleep 1
    say "update alpha"
    waitlog $ALOG 'installed for the next start' 120
    waitcount $ALOG 'EreBUS .* (x86_64)' 2 90
    waitcount $ALOG 'shell: ' 2 60
    say "nodes"
    sleep 2
    echo "screendump $BUILD/update-b.ppm"
    sleep 1
    touch $BUILD/upd-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>$BUILD/upd-b.err

wait $A_JOB 2>/dev/null
grep -v '^$' $BUILD/upd-a.err $BUILD/upd-b.err 2>/dev/null | grep -v 'monitor -\|(qemu)' | head -5
python3 tools/ppm2png.py $BUILD/update-a.ppm $BUILD/update-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/update-b.ppm $BUILD/update-b.png 2>/dev/null

echo "--- B, the updater ---"
grep -a 'pipe:' $BLOG | cut -c1-120
echo "--- A, the updated ---"
grep -a 'pipe:\|EreBUS .* (x86_64)\|boot:' $ALOG | cut -c1-120
echo "--- the checks ---"
ok=1
grep -aq "remembered in nodes as 'beta'" $ALOG && echo "A wrote beta's key into its nodes table" || { echo "FAILED: A has no row for beta"; ok=0; }
grep -aq 'may not update this machine; declined' $ALOG && echo "the first kernel was declined: beta had no right" || { echo "FAILED: no refusal"; ok=0; }
grep -aq 'declined it (no update right granted there)' $BLOG && echo "and B heard why" || { echo "FAILED: B did not hear the refusal"; ok=0; }
grep -aq 'came from beta; installed for the next start' $ALOG && echo "after 'allow beta update' the kernel was taken and installed" || { echo "FAILED: the kernel was not installed"; ok=0; }
grep -aq 'carried .* bytes across, sealed' $BLOG && echo "B carried it across" || { echo "FAILED: B did not finish the transfer"; ok=0; }
boots=$(grep -ac 'EreBUS .* (x86_64)' $ALOG)
last=$(grep -a 'EreBUS .* (x86_64)' $ALOG | tail -1)
echo "A booted $boots times; last: $last"
[ "$boots" -ge 2 ] && echo "$last" | grep -q "EreBUS $NOW " && echo "A runs the kernel B sent: $NOW" || { echo "FAILED: A does not run the new kernel"; ok=0; }
[ $ok = 1 ] && echo "a node updates another through the pipe, with the update right" || echo "the update through the pipe FAILED"
