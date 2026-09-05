#!/bin/sh
# pipe-quorum.sh -- the same task on two machines, answers compared.
# - two workers (alpha, gamma) welcome work; the asker (beta) runs a task "across 2"
# - the desk sends the whole task to each distinct worker, both answer 42, and the
#   verified majority makes the result "42  (agreed by 2 of 2)"
# - three QEMUs share a multicast socket as the cable; each has its own MAC

cd "$(dirname "$0")/.."
. tools/testlib.sh
MCAST=230.0.0.7:${QUORUMPORT:-8022}
A1LOG=$BUILD/q-a1.log
A2LOG=$BUILD/q-a2.log
BLOG=$BUILD/q-b.log

rm -f $BUILD/q-a1.img $BUILD/q-a2.img $BUILD/q-b.img \
      $BUILD/q-a1-vars.fd $BUILD/q-a2-vars.fd $BUILD/q-b-vars.fd \
      $BUILD/q-a1-esp.img $BUILD/q-a2-esp.img \
      $A1LOG $A2LOG $BLOG $BUILD/q-b.ppm \
      $BUILD/q-a1-ready $BUILD/q-a2-ready $BUILD/q-done
fresh_store $BUILD/q-a1.img
fresh_store $BUILD/q-a2.img
fresh_store $BUILD/q-b.img
fresh_vars $BUILD/q-a1-vars.fd
fresh_vars $BUILD/q-a2-vars.fd
fresh_vars $BUILD/q-b-vars.fd
cp $BUILD/esp.img $BUILD/q-a1-esp.img
cp $BUILD/esp.img $BUILD/q-a2-esp.img

# worker <last-octet> <name> <mac> <vars> <esp> <store> <log> <ready>
worker() {
    oct=$1; nm=$2; mac=$3; vars=$4; esp=$5; store=$6; log=$7; ready=$8
    {
        bootwait $log
        keys tab tab tab tab tab pause
        say "go system"
        say "go settings"
        say "write address | 10.9.9.$oct"
        say "write name | $nm"
        say "write work | welcomed"
        say "back"
        say "back"
        waitlog $log "10.9.9.$oct by claim" 40
        touch $ready
        waitfile $BUILD/q-done 180
        sleep 1
        echo quit
    } | qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$vars \
      -drive format=raw,file=$esp \
      -drive id=store,file=$store,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0,mac=$mac \
      -netdev socket,id=n0,mcast=$MCAST \
      -serial file:$log >/dev/null 2>&1 &
}

worker 20 alpha 52:54:00:aa:99:20 $BUILD/q-a1-vars.fd $BUILD/q-a1-esp.img \
       $BUILD/q-a1.img $A1LOG $BUILD/q-a1-ready
A1=$!
sleep 1
worker 22 gamma 52:54:00:aa:99:22 $BUILD/q-a2-vars.fd $BUILD/q-a2-esp.img \
       $BUILD/q-a2.img $A2LOG $BUILD/q-a2-ready
A2=$!
sleep 1

# --- B: beta, asks the task across two machines ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "back"
    say "back"
    say "make text task"
    say "go task"
    say "write wait"
    say "write answer 42"
    say "write stop"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/q-a1-ready 90
    waitfile $BUILD/q-a2-ready 90
    sleep 1
    say "ask task across 2"
    waitlog $BLOG 'pipe: job 1 answers\|pipe: job 1 failed' 90
    say "read task"
    sleep 1
    echo "screendump $BUILD/q-b.ppm"
    sleep 1
    touch $BUILD/q-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/q-b-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/q-b.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,mcast=$MCAST \
  -serial file:$BLOG >/dev/null 2>&1

wait $A1 $A2 2>/dev/null
python3 tools/ppm2png.py $BUILD/q-b.ppm $BUILD/q-b.png 2>/dev/null

echo "--- the asker (beta) ---"
grep -a 'pipe: job\|pipe: the desk deals' $BLOG | cut -c1-120
echo "--- worker alpha ---"
grep -ac 'pipe: running a job' $A1LOG | sed 's/^/    jobs run: /'
echo "--- worker gamma ---"
grep -ac 'pipe: running a job' $A2LOG | sed 's/^/    jobs run: /'
echo "--- the checks ---"
ok=1
a1=$(grep -ac 'pipe: running a job' $A1LOG)
a2=$(grep -ac 'pipe: running a job' $A2LOG)
[ "$a1" -ge 1 ] && [ "$a2" -ge 1 ] && echo "both machines ran the task" || { echo "FAILED: the task did not reach two machines (alpha $a1, gamma $a2)"; ok=0; }
grep -aq 'pipe: job 1 answers: 42  (agreed by 2 of 2)' $BLOG && echo "the two answers agreed: 42 by 2 of 2" || { echo "FAILED: no verified agreement"; ok=0; }
[ $ok = 1 ] && echo "a quorum agrees on a verified result" || echo "the quorum FAILED"
