#!/bin/sh
# limits.sh -- the limits at their edge: 64 processors, and 64 nodes in the table.
# - the machine boots with -smp 64: every processor must come up and kernel work must run on the others
# - through the door, 64 nodes are trusted by hand, each with its own key; the 65th is refused as
#   the table being full; `nodes` then lists all 64
cd "$(dirname "$0")/.."
SMP=64
. tools/testlib.sh
LOG=$BUILD/serial.log
KEY=$BUILD/limits_key
PORT=${LIMPORT:-2231}

rm -f $BUILD/teststore.img $LOG $KEY $KEY.pub $BUILD/limits-*.txt $BUILD/limits-ready $BUILD/limits-done $BUILD/limits-known
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd
ssh-keygen -q -t ed25519 -N '' -f $KEY
PUB=$(cut -d' ' -f1,2 $KEY.pub)

# 65 distinct ed25519 public keys, each as the line of an id_ed25519.pub,
# each under its own node name.
python3 - $BUILD/limits-trust.txt <<'PY'
import base64, os, struct, sys
with open(sys.argv[1], 'w') as f:
    for i in range(65):
        blob = struct.pack('!I', 11) + b'ssh-ed25519' + struct.pack('!I', 32) + os.urandom(32)
        f.write('trust node%02d ssh-ed25519 %s\n' % (i + 1, base64.b64encode(blob).decode()))
PY

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    say "back"
    touch $BUILD/limits-ready
    waitfile $BUILD/limits-done 180
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

waitfile $BUILD/limits-ready 180 || echo "(the door was never opened)"
sleep 2
SSH="ssh -T -o StrictHostKeyChecking=no -o UserKnownHostsFile=$BUILD/limits-known \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p $PORT -i $KEY someone@127.0.0.1"
head -64 $BUILD/limits-trust.txt | $SSH > $BUILD/limits-out64.txt 2>&1
tail -1  $BUILD/limits-trust.txt | $SSH > $BUILD/limits-out65.txt 2>&1
echo nodes | $SSH > $BUILD/limits-nodes.txt 2>&1
touch $BUILD/limits-done
wait $Q_JOB 2>/dev/null

echo "--- processors ---"
grep -a 'smp:' $LOG | cut -c1-110
echo "--- the 65th node ---"
grep -a 'full\|trusted' $BUILD/limits-out65.txt | head -2
echo "--- the checks ---"
ok=1
grep -aq 'smp:  64 of 64 processors up' $LOG && echo "all 64 processors came up" || { echo "FAILED: not every processor came up"; ok=0; }
grep -aq 'smp:  kernel work ran on [1-9][0-9]* application' $LOG && echo "and kernel work ran on the others" || { echo "FAILED: no kernel work on the application processors"; ok=0; }
n=$(grep -ac 'is trusted' $BUILD/limits-out64.txt)
[ "$n" -eq 64 ] && echo "64 nodes trusted by hand" || { echo "FAILED: $n of 64 nodes were trusted"; ok=0; }
grep -aq 'the nodes table is full' $BUILD/limits-out65.txt && echo "the 65th was refused: the table is full" || { echo "FAILED: the 65th node was not refused"; ok=0; }
rows=$(grep -ac '^ *node[0-9][0-9] ' $BUILD/limits-nodes.txt)
[ "$rows" -eq 64 ] && echo "and the table lists all 64" || { echo "FAILED: the table lists $rows rows"; ok=0; }
[ $ok = 1 ] && echo "the limits hold at their edge: 64 processors, 64 nodes" || echo "limits FAILED"
