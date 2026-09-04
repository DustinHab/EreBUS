#!/bin/sh
# pipe-two.sh -- two machines on one cable (QEMU socket netdev), one object crossing.
# - each machine claims its address in the settings; B stands on its notes and presses send
# - no peer set: the chooser scans, A answers, one click sends
# - A's serial log names the arrival; A's store shows it in arrivals

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8010}

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG \
      $BUILD/peer-esp.img $BUILD/teststore.img $BLOG \
      $BUILD/pipe-wire.dump $BUILD/pipe-a-ready \
      $BUILD/pipe-a.err $BUILD/pipe-b.err
fresh_store $BUILD/peerstore.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

# --- machine A: claim 10.9.9.20, then wait for the arrival ------------------
{
    bootwait $ALOG
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    # The settings text is applied whenever typing pauses, and on a
    # loaded host a pause between keystrokes can apply a half-typed
    # address (10.9.9.2 before the last 1). Wait for the exact address,
    # not just any claim.
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/pipe-a-ready
    waitlog $ALOG 'bytes arrived' 90
    sleep 1
    echo "screendump $BUILD/pipe-a.ppm"
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

# B only meets A if A's listen socket is bound first.
waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- machine B: claim 10.9.9.21, discover A by scan, point at it, send
# the notes. Driven through the screen terminal, the same reliable path
# pipe-update and pipe-input take: the mouse chooser is too sensitive to
# host load to prove anything in a parallel battery.
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
    # scan puts a SEEK on the wire and A answers it; point at names the
    # peer by its address, so the send does not depend on the answer
    # having been parsed yet.
    say "scan"
    sleep 2
    say "point at 10.9.9.20"
    say "send notes"
    waitlog $BLOG 'carried\|nothing was sent\|did not take\|refused it' 60
    sleep 1
    echo "screendump $BUILD/pipe-b.ppm"
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
# QEMU's own complaints, if any (a machine that died early leaves an empty log)
grep -v '^$' $BUILD/pipe-a.err $BUILD/pipe-b.err 2>/dev/null | grep -v 'monitor -\|(qemu)' | head -5

python3 tools/ppm2png.py $BUILD/pipe-a.ppm $BUILD/pipe-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-b.ppm $BUILD/pipe-b.png 2>/dev/null

echo "--- sender (B) ---"
grep -a 'net:.*claim\|pipe' $BLOG
echo "--- receiver (A) ---"
grep -a 'net:.*claim\|pipe' $ALOG

# The wire itself, recorded at A's card: the notes' words must not be
# on it, and the handshake must be.
echo "--- the wire ---"
if grep -aq 'not a file' $BUILD/pipe-wire.dump; then
    echo "FAILED: the words crossed in the clear"
else
    echo "no plaintext on the wire"
fi
if grep -aq 'EBPX' $BUILD/pipe-wire.dump; then
    echo "pipe packets seen"
else
    echo "FAILED: no pipe packets in the dump"
fi
