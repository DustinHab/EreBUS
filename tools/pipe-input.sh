#!/bin/sh
# pipe-input.sh -- work with an input object, and the answer names who did it.
# - A (alpha) welcomes work; B (beta) writes a recipe that reads its input's first eight bytes and answers them
# - 'ask task with in' sends the input ahead of the recipe; A's log shows it arrive; B's answer says "(by alpha)"
# - then B asks a COMPILED task 'with' an input: the c program receives the input on its letter box, reads its bytes and echoes them
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
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/input-ready
    waitfile $BUILD/input-done 240
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>$BUILD/input-a.err &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

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
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/input-ready 90
    sleep 1
    say "ask task with in"
    waitlog $BLOG 'pipe: job 1 answers\|pipe: job 1 failed' 90
    say "read task"
    sleep 2
    echo "screendump $BUILD/input-b.ppm"
    sleep 1
    # a COMPILED task, with an input: the c program takes the input
    # capability out of its letter box (handle n), reads the first eight
    # bytes, and sends them back as its answer -- so an input of eight
    # letters comes home as that word.
    say "make text codein"
    say "go codein"
    say "write long main(long c, long n) {"
    say "write long b[16];"
    say "write syscall(3, n, b, 0, 0, 0);"
    say "write long got = b[6];"
    say "write long v = syscall(4, got, 0, 0, 0, 0);"
    say "write syscall(2, c, 0x54584554, v, 0, 0);"
    say "write return 0;"
    say "write }"
    say "back"
    say "make text gift"
    say "go gift"
    say "write gotinput"
    say "back"
    say "ask codein as code with gift"
    waitlog $BLOG 'pipe: job 2 answers\|pipe: job 2 failed' 120
    say "read codein"
    sleep 1
    echo "screendump $BUILD/input-code.ppm"
    sleep 1
    touch $BUILD/input-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>$BUILD/input-b.err

wait $A_JOB 2>/dev/null
grep -v '^$' $BUILD/input-a.err $BUILD/input-b.err 2>/dev/null | grep -v 'monitor -\|(qemu)' | head -5
python3 tools/ppm2png.py $BUILD/input-b.ppm $BUILD/input-b.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/input-code.ppm $BUILD/input-code.png 2>/dev/null

echo "--- B, the asker ---"
grep -a 'pipe:' $BLOG | cut -c1-120
echo "--- A, the worker ---"
grep -a 'pipe:\|user: \|compiled image' $ALOG | cut -c1-120 | tail -14
echo "--- the checks ---"
ok=1
grep -aq 'an input of .* bytes for work came from beta' $ALOG && echo "the input went ahead of the work" || { echo "FAILED: no input arrived at A"; ok=0; }
grep -aq 'pipe: running a job from' $ALOG && echo "A ran the recipe" || { echo "FAILED: A ran nothing"; ok=0; }
# "hello\n" and two zero bytes, read as one little-endian number
grep -aq 'pipe: job 1 answers: 11473676690792 (by alpha)' $BLOG && echo "the recipe read the input's bytes, and the answer names alpha" || { echo "FAILED: the answer is not the input's bytes by alpha"; ok=0; }
grep -aq 'running a compiled image with an input' $ALOG && echo "A ran a compiled image that was given an input" || { echo "FAILED: the compiled job got no input"; ok=0; }
grep -aq 'pipe: job 2 answers: gotinput (by alpha)' $BLOG && echo "the compiled task read its input's bytes and echoed them, by alpha" || { echo "FAILED: the compiled task did not echo its input"; ok=0; }
[ $ok = 1 ] && echo "work travels with its input -- to a recipe and to a compiled image" || echo "work with an input FAILED"
