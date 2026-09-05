#!/bin/sh
# sshmulti.sh -- several ssh visitors at once through the one door.
# - the door key is typed in; three clients connect concurrently and each runs a command
# - all three must succeed, and the machine must report more than one session open at a time

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/sshm-serial.log
PORT=${SSHMPORT:-2233}

KEYS=/tmp/erebus-sshmulti
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/k ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/k
PUB=$(cut -d' ' -f1,2 $KEYS/k.pub)

rm -f $BUILD/sshmstore.img $LOG $BUILD/sshm-ready $BUILD/sshm-done \
      $BUILD/sshm-1.txt $BUILD/sshm-2.txt $BUILD/sshm-3.txt
fresh_store $BUILD/sshmstore.img
fresh_vars $BUILD/sshm-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    touch $BUILD/sshm-ready
    waitfile $BUILD/sshm-done 150
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/sshm-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/sshmstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -serial file:$LOG >/dev/null 2>&1 &
Q_JOB=$!

waitfile $BUILD/sshm-ready 150
sleep 1
rm -f $KEYS/known_hosts
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
     -o IdentitiesOnly=yes -o ConnectTimeout=15 -o LogLevel=ERROR -p $PORT"

# Three at once: each opens a pty and holds it a few seconds, so their
# sessions overlap on the machine, then asks the time and leaves.
one() {
    printf 'time\ntime\n' | $SSH -tt -i $KEYS/k v$1@127.0.0.1 > $BUILD/sshm-$1.txt 2>&1
    echo "client $1 exit $?"
}
one 1 &
P1=$!
one 2 &
P2=$!
one 3 &
P3=$!
wait $P1 $P2 $P3

touch $BUILD/sshm-done
wait $Q_JOB 2>/dev/null

echo "--- what each client got ---"
for i in 1 2 3; do
    echo "client $i:"
    tr -d '\r' < $BUILD/sshm-$i.txt | grep -a 'time\|:' | head -2 | sed 's/^/    /'
done
echo "--- the machine's sessions ---"
grep -a 'ssh:\|door:' $LOG | cut -c1-100 | tail -16

echo "--- the checks ---"
ok=1
n=0
for i in 1 2 3; do
    if grep -aq ':' $BUILD/sshm-$i.txt && grep -aiq '[0-9][0-9]:[0-9][0-9]' $BUILD/sshm-$i.txt; then
        n=$((n + 1))
    fi
done
# All three succeeding IS the proof of concurrency: a single-visitor door
# would let each new knock take the slot over, breaking the earlier
# client, so three overlapping clients could not all get a reply.
[ $n -eq 3 ] && echo "all three visitors got a reply at once" || { echo "FAILED: only $n of 3 visitors got a reply"; ok=0; }
logins=$(grep -ac 'logged in from' $LOG)
[ "$logins" -ge 3 ] && echo "the machine logged in $logins visitors" || { echo "FAILED: the machine logged in only $logins"; ok=0; }
[ $ok = 1 ] && echo "several ssh visitors are served at once" || echo "multiple ssh visitors FAILED"
