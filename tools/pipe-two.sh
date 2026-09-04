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
      $BUILD/pipe-choose.ppm $BUILD/pipe-wire.dump $BUILD/pipe-a-ready
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
    # The settings text is applied once typing pauses; the claim in the
    # log is the sign that the address is in force.
    waitlog $ALOG 'by claim' 30
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
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -object filter-dump,id=fd0,netdev=n0,file=$BUILD/pipe-wire.dump \
  -serial file:$ALOG >/dev/null 2>&1 &
A_JOB=$!

sleep 2

# --- machine B: claim 10.9.9.21, then send the shortest way there is:
# stand on the notes, press send. No peer is set, so the chooser opens
# under the word and scans by itself; one click on the machine that
# answers points the pipe and lets the notes go, in the same breath.
{
    bootwait $BLOG
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 1 \
         left left up up up up up right
    waitlog $BLOG 'by claim' 30
    waitfile $BUILD/pipe-a-ready 90
    sleep 1
    keys corner m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m95,0 click \
         m0,1 m0,1 m0,1 m0,1 m0,1 m0,1
    sleep 3
    echo "screendump $BUILD/pipe-choose.ppm"
    sleep 1
    keys m105,49 click \
         m0,1 m0,1 m0,1 m0,1 m0,1 m0,1
    waitlog $BLOG 'carried\|nothing was sent\|did not take' 60
    sleep 1
    echo "screendump $BUILD/pipe-b.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>&1

wait $A_JOB 2>/dev/null

python3 tools/ppm2png.py $BUILD/pipe-a.ppm $BUILD/pipe-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-b.ppm $BUILD/pipe-b.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-choose.ppm $BUILD/pipe-choose.png 2>/dev/null

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
