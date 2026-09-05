#!/bin/sh
# pipe-code.sh -- compiled far work: a c task is compiled and run on the worker, under a kernel-enforced deadline.
# - A (alpha) welcomes work; B (beta) writes a small c program as a task and asks it "as code"
# - A compiles it with the in-kernel compiler, runs the image, and it answers "7"; the answer is signed, so B shows "(by alpha)"
# - then B asks a task that never returns (for(;;){}); A's deadline ends it and B hears "it ran out of time"
# - both machines are driven through the screen terminal; each has its own MAC

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8014}

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG \
      $BUILD/peer-esp.img $BUILD/teststore.img $BLOG $BUILD/code-b.ppm \
      $BUILD/code-ready $BUILD/code-done
fresh_store $BUILD/peerstore.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

# --- A: alpha at 10.9.9.20, welcomes work ---
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
    touch $BUILD/code-ready
    waitfile $BUILD/code-done 240
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>$BUILD/code-a.err &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- B: beta at 10.9.9.21, points at alpha, asks a c task "as code" ---
# The task is c: it adds 3 and 4 and sends the digit as one "TEXT"
# message to its console, which the worker has pointed home.
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
    say "write long main(long c, long n) {"
    say "write long x = 3 + 4;"
    say "write syscall(2, c, 0x54584554, 48 + x, 0, 0);"
    say "write return 0;"
    say "write }"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/code-ready 90
    sleep 1
    say "ask task as code"
    waitlog $BLOG 'pipe: job 1 answers\|pipe: job 1 failed' 90
    say "read task"
    sleep 1
    echo "screendump $BUILD/code-b.ppm"
    sleep 1
    # a task that never returns: the worker's deadline must end it
    say "make text spin"
    say "go spin"
    say "write long main(long c, long n) {"
    say "write for (;;) {"
    say "write }"
    say "write return 0;"
    say "write }"
    say "back"
    say "ask spin as code"
    waitlog $BLOG 'pipe: job 2 answers\|pipe: job 2 failed' 60
    sleep 1
    say "go system"
    say "go the ledger"
    sleep 1
    echo "screendump $BUILD/code-ledger.ppm"
    sleep 1
    # the failed job is a notable event: it waits on the attention page
    say "back"
    say "go attention"
    sleep 1
    echo "screendump $BUILD/code-attn.ppm"
    sleep 1
    touch $BUILD/code-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>$BUILD/code-b.err

wait $A_JOB 2>/dev/null
grep -v '^$' $BUILD/code-a.err $BUILD/code-b.err 2>/dev/null | grep -v 'monitor -\|(qemu)' | head -5
python3 tools/ppm2png.py $BUILD/code-b.ppm $BUILD/code-b.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/code-ledger.ppm $BUILD/code-ledger.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/code-attn.ppm $BUILD/code-attn.png 2>/dev/null

echo "--- B, the asker ---"
grep -a 'pipe:' $BLOG | cut -c1-120
echo "--- A, the worker ---"
grep -a 'pipe:\|proc: .*compiled\|proc: .*being ended' $ALOG | cut -c1-120 | tail -14
echo "--- the checks ---"
ok=1
grep -aq 'running a job from' $ALOG && echo "A took the job" || { echo "FAILED: A took no job"; ok=0; }
grep -aq 'running a compiled image' $ALOG && echo "A compiled the c source and ran the image" || { echo "FAILED: A did not run a compiled image"; ok=0; }
grep -aq 'pipe: job 1 answers: 7 (by alpha)' $BLOG && echo "the compiled task answered 7, signed by alpha" || { echo "FAILED: the compiled answer is wrong or unverified"; ok=0; }
grep -aq 'being ended' $ALOG && echo "the runaway task was ended by the deadline" || { echo "FAILED: the runaway task was not ended"; ok=0; }
grep -aq 'pipe: job 2.*ran out of time\|job 2 failed' $BLOG && echo "and B heard it ran out of time" || { echo "FAILED: B did not hear the deadline"; ok=0; }
grep -aq 'attention: pipe: job 2 failed' $BLOG && echo "the failed job was raised on the attention page" || { echo "FAILED: the failure did not reach attention"; ok=0; }
[ $ok = 1 ] && echo "compiled far work runs under a kernel-enforced deadline, and its failure is noticed" || echo "compiled far work FAILED"
