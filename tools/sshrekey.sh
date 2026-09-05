#!/bin/sh
# sshrekey.sh -- the door survives a mid-session key exchange.
# - one client stays on with RekeyLimit set low in time, so it renegotiates keys while connected
# - it asks the time before and after; both must answer, and the machine must log a rekey

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/sshr-serial.log
PORT=${SSHRPORT:-2244}

KEYS=/tmp/erebus-sshrekey
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/k ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/k
PUB=$(cut -d' ' -f1,2 $KEYS/k.pub)

rm -f $BUILD/sshrstore.img $LOG $BUILD/sshr-ready $BUILD/sshr-done $BUILD/sshr.txt
fresh_store $BUILD/sshrstore.img
fresh_vars $BUILD/sshr-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    touch $BUILD/sshr-ready
    waitfile $BUILD/sshr-done 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/sshr-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/sshrstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -serial file:$LOG >/dev/null 2>&1 &
Q_JOB=$!

waitfile $BUILD/sshr-ready 150
sleep 1
rm -f $KEYS/known_hosts

# A pty session that stays open across a rekey: RekeyLimit "default 5"
# renegotiates keys about every five seconds. The lines are fed with a
# gap, so at least one "time" is answered after the rekey.
{
    printf 'time\n'
    sleep 9
    printf 'time\n'
    sleep 2
} | ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
        -o IdentitiesOnly=yes -o ConnectTimeout=15 -o LogLevel=ERROR \
        -o "RekeyLimit=default 5" -tt -p $PORT -i $KEYS/k v@127.0.0.1 \
        > $BUILD/sshr.txt 2>&1
echo "client exit $?"

touch $BUILD/sshr-done
wait $Q_JOB 2>/dev/null

echo "--- what the client got ---"
tr -d '\r' < $BUILD/sshr.txt | grep -a '[0-9][0-9]:[0-9][0-9]' | sed 's/^/    /'
echo "--- the machine ---"
grep -a 'ssh:' $LOG | cut -c1-100

echo "--- the checks ---"
ok=1
answers=$(tr -d '\r' < $BUILD/sshr.txt | grep -ac '[0-9][0-9]:[0-9][0-9]')
[ "$answers" -ge 2 ] && echo "the session answered before and after ($answers times)" || { echo "FAILED: only $answers answers"; ok=0; }
grep -aq 'ssh:  rekeyed with' $LOG && echo "the machine rekeyed mid-session" || { echo "FAILED: no rekey logged"; ok=0; }
grep -aq 'logged in from' $LOG && echo "and stayed logged in" || { echo "FAILED: no login"; ok=0; }
[ $ok = 1 ] && echo "the door survives a rekey" || echo "ssh rekey FAILED"
