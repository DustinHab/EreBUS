#!/bin/sh
# pipe-input.sh -- work with an input object, and the answer names who did it.
# - A (alpha) welcomes work; B (beta) writes a recipe that reads its input's first eight bytes and answers them
# - 'ask task with in' sends the input ahead of the recipe; A's log shows it arrive; B's answer says "(by alpha)"
# - words typed through the screen terminal on both machines

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8013}

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG \
      $BUILD/peer-esp.img $BUILD/teststore.img $BLOG $BUILD/input-b.ppm \
      $BUILD/input-ready $BUILD/input-done
fresh_store $BUILD/peerstore.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

# --- A: alpha at 10.9.9.20, welcomes work from everyone ---
{
    bootwait $ALOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "write work | welcomed"
    say "back"
    say "back"
    waitlog $ALOG 'by claim' 30
    touch $BUILD/input-ready
    waitfile $BUILD/input-done 180
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>&1 &
A_JOB=$!

sleep 2

# --- B: beta at 10.9.9.21, points at alpha, asks with an input ---
# The recipe waits three times: the reply port, the range's high end,
# then the input; 'get a 0' reads the input's first eight bytes as a
# number, which is what it answers.
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
    say "make text task"
    say "go task"
    say "write wait"
    say "write wait"
    say "write wait"
    say "write get a 0"
    say "write answer a"
    say "write stop"
    say "back"
    say "make text in"
    say "go in"
    say "write hello"
    say "back"
    waitfile $BUILD/input-ready 90
    sleep 1
    say "ask task with in"
    waitlog $BLOG 'pipe: job 1 answers\|pipe: job 1 failed' 90
    say "read task"
    sleep 2
    echo "screendump $BUILD/input-b.ppm"
    sleep 1
    touch $BUILD/input-done
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
python3 tools/ppm2png.py $BUILD/input-b.ppm $BUILD/input-b.png 2>/dev/null

echo "--- B, the asker ---"
grep -a 'pipe:' $BLOG | cut -c1-120
echo "--- A, the worker ---"
grep -a 'pipe:\|user: ' $ALOG | cut -c1-120 | tail -12
echo "--- the checks ---"
ok=1
grep -aq 'an input of .* bytes for work came from beta' $ALOG && echo "the input went ahead of the work" || { echo "FAILED: no input arrived at A"; ok=0; }
grep -aq 'pipe: running a job from' $ALOG && echo "A ran the recipe" || { echo "FAILED: A ran nothing"; ok=0; }
# "hello\n" and two zero bytes, read as one little-endian number
grep -aq 'pipe: job 1 answers: 11473676690792 (by alpha)' $BLOG && echo "the recipe read the input's bytes, and the answer names alpha" || { echo "FAILED: the answer is not the input's bytes by alpha"; ok=0; }
[ $ok = 1 ] && echo "work travels with its input, and answers say who did it" || echo "work with an input FAILED"
