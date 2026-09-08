#!/bin/sh
# ssh-task.sh -- feed a task package into a node over ssh; the desk takes it.
# - a node is given the host's ssh public key as a "door |" line
# - erebus-task packages a task and emits the door commands (receive + submit)
# - piped over ssh, the node reads the package into an object and hands it to the desk
# - proven by the node's log: the desk queued the job (self-distribution to workers is pipe-task.sh)
cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/st-serial.log
PORT=${SSHTASKPORT:-2229}

make -s task-tool >/dev/null || { echo "FAILED: task-tool did not build"; exit 1; }

KEYS=/tmp/erebus-sshtask
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/k ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/k
PUB=$(cut -d' ' -f1,2 $KEYS/k.pub)

# a small task: a recipe, so no compiler is needed on the path
cat > $KEYS/job.recipe <<'EOF'
answer 42
stop
EOF
build/erebus-task $KEYS/job.recipe --recipe --name demo --combine first --ssh > $KEYS/feed.txt 2>$KEYS/gen.err || { echo "FAILED: erebus-task"; cat $KEYS/gen.err; exit 1; }

rm -f $BUILD/st-store.img $LOG $BUILD/st-ready $BUILD/st-done $KEYS/known_hosts
fresh_store $BUILD/st-store.img
fresh_vars $BUILD/st-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    touch $BUILD/st-ready
    waitfile $BUILD/st-done 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/st-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/st-store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -serial file:$LOG >/dev/null 2>&1 &
Q=$!

waitfile $BUILD/st-ready 150
sleep 1
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p $PORT"

echo "--- the package fed over ssh ---"
cat $KEYS/feed.txt | $SSH -T -i $KEYS/k someone@127.0.0.1 > $KEYS/out.txt 2>&1
echo "exit $?"
tr -d '\r' < $KEYS/out.txt

waitlog $LOG 'pipe: job [0-9]* queued\|created here' 20
touch $BUILD/st-done
wait $Q 2>/dev/null

echo "--- the node's side ---"
grep -a 'pipe:\|created here' $LOG | cut -c1-120
echo "--- the checks ---"
ok=1
grep -aq 'created here' $KEYS/out.txt || grep -aq 'submitted to the desk' $KEYS/out.txt || { echo "FAILED: the package was not received and submitted over ssh"; ok=0; }
grep -aq 'pipe: job [0-9]* queued' $LOG && echo "the package fed over ssh reached the desk" || { echo "FAILED: the desk did not queue the job"; ok=0; }
[ $ok = 1 ] && echo "a task package feeds into a node over ssh and the desk takes it" || echo "ssh task ingestion FAILED"
