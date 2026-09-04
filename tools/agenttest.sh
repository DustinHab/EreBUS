#!/bin/sh
# agenttest.sh -- the agent program under changing rights.
# - the notes are handed to the running agent: it reads them and writes into them
# - the write right is taken away: it reads the changed text and its write is refused
# - the reference is taken away: its read is refused
# - the program checks nothing itself; every outcome is the kernel's answer to a call
# - mouse counts: a PS/2 mouse reports movement only and the monitor clamps one report to about 100 steps

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/agent-serial.log
rm -f $BUILD/teststore.img $LOG $BUILD/agent.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    # into the programs list, onto the agent; the pointer to the corner,
    # then onto the notes carry
    keys down down down down right right corner
    i=0; while [ $i -lt 17 ]; do keys m100,7; i=$((i + 1)); done
    keys click
    waitlog $LOG 'user: write ok' 20
    # the w of the reference's rights: the write right goes
    keys m0,100 m0,100 m0,100 m0,100 m0,100 m0,46 click esc
    waitlog $LOG 'user: write refused' 20
    # back up, then the x: the reference goes
    keys m-94,-100 m0,-100 m0,-100 m0,-100 m0,-100 m0,-47 click
    keys m100,0 m100,0 m78,0 click
    waitlog $LOG 'user: read refused' 20
    sleep 1
    echo "screendump $BUILD/agent.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/agent.ppm $BUILD/agent.png 2>/dev/null

echo "--- what the program said, in order ---"
grep -ao 'user: .*' $LOG | sed 's/^user: //; s/[[:space:]]*$//' | grep -v '^$' | sed 's/^/    /'
echo "--- the checks ---"
ok=1
got=$(grep -ao 'user: .*' $LOG | grep -o 'reads:  type here\|write ok\|reads:   MARK!\|write refused\|read refused' | tr '\n' '|')
want='reads:  type here|write ok|reads:   MARK!|write refused|read refused|'
if [ "$got" = "$want" ]; then
    echo "read and write, then read only, then nothing: in that order"
else
    echo "FAILED: the sequence was: $got"
    ok=0
fi
[ $ok = 1 ] && echo "the agent's rights follow the reference" || echo "the agent test FAILED"
