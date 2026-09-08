#!/bin/sh
# pipe-task.sh -- a task package with a manifest, submitted and distributed by combine rule.
# - two workers (alpha, gamma) welcome work; the asker (beta) writes a task package whose
#   "key | value" manifest says: split 1..100 into 2 pieces, combine the pieces by max
# - the payload answers each piece's high end (50 and 100); "combine | max" folds them to 100
# - proven by 'submit', not 'ask': the manifest carries the whole policy
# - three QEMUs share a multicast socket as the cable; each has its own MAC
cd "$(dirname "$0")/.."
. tools/testlib.sh
MCAST=230.0.0.9:${TASKPORT:-8029}
A1LOG=$BUILD/t-a1.log
A2LOG=$BUILD/t-a2.log
BLOG=$BUILD/t-b.log

rm -f $BUILD/t-a1.img $BUILD/t-a2.img $BUILD/t-b.img \
      $BUILD/t-a1-vars.fd $BUILD/t-a2-vars.fd $BUILD/t-b-vars.fd \
      $BUILD/t-a1-esp.img $BUILD/t-a2-esp.img \
      $A1LOG $A2LOG $BLOG $BUILD/t-b.ppm \
      $BUILD/t-a1-ready $BUILD/t-a2-ready $BUILD/t-done
fresh_store $BUILD/t-a1.img
fresh_store $BUILD/t-a2.img
fresh_store $BUILD/t-b.img
fresh_vars $BUILD/t-a1-vars.fd
fresh_vars $BUILD/t-a2-vars.fd
fresh_vars $BUILD/t-b-vars.fd
cp $BUILD/esp.img $BUILD/t-a1-esp.img
cp $BUILD/esp.img $BUILD/t-a2-esp.img

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
        waitfile $BUILD/t-done 180
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

worker 20 alpha 52:54:00:aa:99:40 $BUILD/t-a1-vars.fd $BUILD/t-a1-esp.img \
       $BUILD/t-a1.img $A1LOG $BUILD/t-a1-ready
A1=$!
sleep 1
worker 22 gamma 52:54:00:aa:99:42 $BUILD/t-a2-vars.fd $BUILD/t-a2-esp.img \
       $BUILD/t-a2.img $A2LOG $BUILD/t-a2-ready
A2=$!
sleep 1

# --- B: beta, writes the package and submits it ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "back"
    say "back"
    say "make text maxjob"
    say "go maxjob"
    say "write task | maxdemo"
    say "write split | 1 100"
    say "write pieces | 2"
    say "write combine | max"
    say "write --"
    say "write wait"
    say "write set a m"
    say "write wait"
    say "write set b m"
    say "write answer b"
    say "write stop"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/t-a1-ready 90
    waitfile $BUILD/t-a2-ready 90
    sleep 1
    say "submit maxjob"
    waitlog $BLOG 'pipe: job 1 answers\|pipe: job 1 failed' 120
    say "read maxjob"
    sleep 1
    echo "screendump $BUILD/t-b.ppm"
    sleep 1
    touch $BUILD/t-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/t-b-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/t-b.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:41 \
  -netdev socket,id=n0,mcast=$MCAST \
  -serial file:$BLOG >/dev/null 2>&1

wait $A1 $A2 2>/dev/null
python3 tools/ppm2png.py $BUILD/t-b.ppm $BUILD/t-b.png 2>/dev/null

echo "--- the asker (beta) ---"
grep -a 'pipe: job' $BLOG | cut -c1-120
echo "--- worker alpha ---"
grep -ac 'pipe: running a job' $A1LOG | sed 's/^/    jobs run: /'
echo "--- worker gamma ---"
grep -ac 'pipe: running a job' $A2LOG | sed 's/^/    jobs run: /'
echo "--- the checks ---"
ok=1
a1=$(grep -ac 'pipe: running a job' $A1LOG)
a2=$(grep -ac 'pipe: running a job' $A2LOG)
[ "$a1" -ge 1 ] && [ "$a2" -ge 1 ] && echo "the package reached both machines" || { echo "FAILED: a piece did not reach two machines (alpha $a1, gamma $a2)"; ok=0; }
grep -aq 'pipe: job 1 answers: 100 ' $BLOG && echo "the two pieces (50, 100) combined by max to 100" || { echo "FAILED: the max combine did not answer 100"; ok=0; }
[ $ok = 1 ] && echo "a submitted task package distributes by its manifest and folds by its combine rule" || echo "the task package FAILED"
