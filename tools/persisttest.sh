#!/bin/sh
# persisttest.sh -- the graph survives a restart without anyone saving.
# - boot 1 with an empty store: type into the notes, leave the machine alone until a generation is written
# - boot 2: nothing typed; the graph must be restored from that generation

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG1=$BUILD/persist-1.log
LOG2=$BUILD/persist-2.log
rm -f $BUILD/teststore.img $LOG1 $LOG2 $BUILD/persist.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

boot() {
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 >/dev/null 2>&1
}

{
    bootwait $LOG1
    # a generation may already be on the disk before anything is typed;
    # the one that matters is the next one
    n=$(count $LOG1 'generation . written')
    keys right i t spc j u s t spc s t a y s
    waitcount $LOG1 'generation . written' $((n + 1)) 40
    sleep 1
    echo quit
} | boot $LOG1

{
    waitlog $LOG2 'graph restored\|starting fresh' 60
    sleep 2
    echo "screendump $BUILD/persist.ppm"
    sleep 1
    echo quit
} | boot $LOG2

python3 tools/ppm2png.py $BUILD/persist.ppm $BUILD/persist.png 2>/dev/null

echo "--- first boot ---"
grep -ao 'snap:.*' $LOG1 | tail -3
echo "--- second boot ---"
grep -ao 'snap:.*' $LOG2 | tail -3
echo "--- the checks ---"
ok=1
grep -aq 'generation . written' $LOG1 && echo "a generation was written without anyone saving" || { echo "FAILED: nothing was written"; ok=0; }
grep -aq 'graph restored from generation' $LOG2 && echo "the second boot restored it" || { echo "FAILED: the graph was not restored"; ok=0; }
[ $ok = 1 ] && echo "the graph persists across a restart" || echo "persistence FAILED"
