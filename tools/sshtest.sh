#!/bin/sh
# sshtest.sh -- the ssh door with the host's own ssh client.
# - a fresh key is typed into the settings as a "door |" line through the screen
# - exec, piped shell, forced pty; a second unknown key must be refused
# - the host key fingerprint in the serial log must match the client's first-visit line

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log
PORT=${SSHPORT:-2222}

# The client's keys live on a filesystem that knows what 0600 means;
# a key on the shared drive is world-readable there, and ssh rightly
# refuses to touch one.
KEYS=/tmp/erebus-sshtest
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/sshkey ]  || ssh-keygen -q -t ed25519 -N '' -f $KEYS/sshkey
[ -f $KEYS/sshkey2 ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/sshkey2
PUB=$(cut -d' ' -f1,2 $KEYS/sshkey.pub)

rm -f $BUILD/teststore.img $LOG $BUILD/known_hosts \
      $BUILD/ssh-exec.txt $BUILD/ssh-pipe.txt $BUILD/ssh-pty.txt \
      $BUILD/ssh-refused.txt $BUILD/ssh-ready $BUILD/ssh-done
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    touch $BUILD/ssh-ready
    waitfile $BUILD/ssh-done 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -serial file:$LOG >/dev/null 2>&1 &
Q_JOB=$!

waitfile $BUILD/ssh-ready 150
sleep 1

rm -f $KEYS/known_hosts
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p $PORT"

echo "--- exec: look ---"
$SSH -i $KEYS/sshkey someone@127.0.0.1 look > $BUILD/ssh-exec.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-exec.txt

echo "--- pipe: go system, where ---"
printf 'go system\nwhere\n' | $SSH -T -i $KEYS/sshkey someone@127.0.0.1 \
    > $BUILD/ssh-pipe.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-pipe.txt

echo "--- pty: time ---"
printf 'time\n' | $SSH -tt -i $KEYS/sshkey someone@127.0.0.1 \
    > $BUILD/ssh-pty.txt 2>&1
echo "exit $?"
tr -d '\r' < $BUILD/ssh-pty.txt

echo "--- a key the door was never told about ---"
$SSH -i $KEYS/sshkey2 someone@127.0.0.1 look > $BUILD/ssh-refused.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-refused.txt

touch $BUILD/ssh-done
wait $Q_JOB 2>/dev/null

echo "--- the machine's side ---"
grep -a 'ssh:\|door:\|crypto:' $LOG
echo "--- the host key the client saw ---"
ssh-keygen -lf $KEYS/known_hosts 2>/dev/null | head -1
