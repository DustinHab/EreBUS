#!/bin/sh
# oldstore.sh -- a store made by the last released kernel is read by today's.
# - the released tag (OLD, default 0.9.6) is built once (tools/oldbuild.sh) and booted on a fresh
#   store: a note is typed, a generation is written
# - today's kernel boots on that store: the graph must be restored from that generation, no
#   generation skipped for its checksum, nothing "newer than this kernel", and the nodes table
#   -- smaller in that release -- widened to today's size with its rows carried over
cd "$(dirname "$0")/.."
. tools/testlib.sh
OLD=${OLD:-0.9.6}
sh tools/oldbuild.sh $OLD || { echo "FAILED: no $OLD build"; exit 1; }
OLDESP=build/old-$OLD/esp.img
LOG1=$BUILD/oldstore-1.log
LOG2=$BUILD/oldstore-2.log
rm -f $BUILD/teststore.img $LOG1 $LOG2 $BUILD/old-esp.img
cp $OLDESP $BUILD/old-esp.img
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

boot() {   # $1 esp, $2 log
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$1 \
      -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$2 >/dev/null 2>&1
}

{
    bootwait $LOG1
    n=$(count $LOG1 'generation . written')
    keys right o l d spc s t o r e
    waitcount $LOG1 'generation [0-9]* written' $((n + 1)) 40
    sleep 1
    echo quit
} | boot $BUILD/old-esp.img $LOG1

fresh_vars $BUILD/test-vars.fd
{
    waitlog $LOG2 'graph restored\|starting fresh' 60
    waitlog $LOG2 'kern: idle' 60
    sleep 2
    echo quit
} | boot $BUILD/esp.img $LOG2

echo "--- the $OLD kernel ---"
grep -ao 'EreBUS [^ ]*\|snap:.*' $LOG1 | tail -3 | cut -c1-90
echo "--- today's kernel on its store ---"
grep -ao 'EreBUS [^ ]*\|snap:.*\|blk:.*format.*' $LOG2 | tail -5 | cut -c1-100
echo "--- the checks ---"
ok=1
grep -aq "EreBUS $OLD" $LOG1 && echo "the $OLD kernel booted" || { echo "FAILED: the old kernel did not come up as $OLD"; ok=0; }
grep -aq 'generation [0-9]* written' $LOG1 && echo "and wrote a generation" || { echo "FAILED: the old kernel wrote nothing"; ok=0; }
grep -aq 'graph restored from generation' $LOG2 && echo "today's kernel restored the graph from it" || { echo "FAILED: today's kernel did not restore the old store"; ok=0; }
grep -aq 'fails its checksum\|newer than this kernel' $LOG2 && { echo "FAILED: a generation was skipped or the store refused"; ok=0; } || echo "nothing skipped, nothing refused"
grep -aq 'the nodes table was widened' $LOG2 && echo "the nodes table was widened to today's size" || { echo "FAILED: the nodes table was not widened"; ok=0; }
old_objs=$(grep -ao 'written, [0-9]* objects' $LOG1 | tail -1 | awk '{print $2}')
new_objs=$(grep -ao 'restored from generation [0-9]*, [0-9]* objects' $LOG2 | tail -1 | awk '{print $NF-0}' | tr -d ',')
new_objs=$(grep -ao 'restored from generation [0-9]*, [0-9]*' $LOG2 | tail -1 | awk '{print $NF}')
[ -n "$old_objs" ] && [ -n "$new_objs" ] && [ "$new_objs" -ge "$old_objs" ] && echo "every object came along ($new_objs of $old_objs)" || { echo "FAILED: objects went missing ($new_objs of $old_objs)"; ok=0; }
[ $ok = 1 ] && echo "a store made by $OLD is read by today's kernel" || echo "old store FAILED"
