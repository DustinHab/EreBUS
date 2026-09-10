#!/bin/sh
# oldpipe.sh -- a node of the last released kernel and one of today's talk over the pipe.
# - A runs the released tag (OLD, default 0.9.6, built once by tools/oldbuild.sh), B runs today's
# - the same crossing as pipe-two: B scans, points at A, sends its notes; A must take them, sealed
# - the wire stays sealed: nothing of the notes in the clear
cd "$(dirname "$0")/.."
. tools/testlib.sh
OLD=${OLD:-0.9.6}
sh tools/oldbuild.sh $OLD || { echo "FAILED: no $OLD build"; exit 1; }
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8012}

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG \
      $BUILD/peer-esp.img $BUILD/teststore.img $BLOG \
      $BUILD/pipe-wire.dump $BUILD/pipe-a-ready \
      $BUILD/pipe-a.err $BUILD/pipe-b.err
fresh_store $BUILD/peerstore.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd
cp build/old-$OLD/esp.img $BUILD/peer-esp.img

# --- machine A, the released kernel: claim 10.9.9.20, take work, wait ---
{
    bootwait $ALOG
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/pipe-a-ready
    waitlog $ALOG 'bytes arrived' 90
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -object filter-dump,id=fd0,netdev=n0,file=$BUILD/pipe-wire.dump \
  -serial file:$ALOG >/dev/null 2>$BUILD/pipe-a.err &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- machine B, today's kernel: claim 10.9.9.21, find A, send the notes ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "back"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/pipe-a-ready 90
    sleep 1
    say "scan"
    sleep 2
    say "point at 10.9.9.20"
    say "send notes"
    waitlog $BLOG 'carried\|nothing was sent\|did not take\|refused it' 60
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>$BUILD/pipe-b.err

wait $A_JOB 2>/dev/null

echo "--- A, $OLD ---"
grep -a 'EreBUS [^ ]*\|net:.*claim\|pipe' $ALOG | cut -c1-110
echo "--- B, today ---"
grep -a 'EreBUS [^ ]*\|net:.*claim\|pipe' $BLOG | cut -c1-110
echo "--- the checks ---"
ok=1
grep -aq "EreBUS $OLD" $ALOG && echo "A ran $OLD" || { echo "FAILED: A did not come up as $OLD"; ok=0; }
grep -aq 'bytes arrived' $ALOG && echo "the notes crossed from today's kernel to the released one" || { echo "FAILED: nothing arrived at the old node"; ok=0; }
grep -aq 'session with 10.9.9.20, proven' $BLOG && echo "under a proven session" || { echo "FAILED: the session with the old node was not proven"; ok=0; }
grep -aq 'not a file' $BUILD/pipe-wire.dump && { echo "FAILED: the words crossed in the clear"; ok=0; } || echo "and nothing in the clear on the wire"
[ $ok = 1 ] && echo "a node of $OLD and a node of today talk over the pipe" || echo "old pipe FAILED"
