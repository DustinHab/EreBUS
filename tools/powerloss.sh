#!/bin/sh
# powerloss.sh -- the store survives the power going out while a generation is being written.
# - RUNS boots on one store. Every boot must restore a whole generation and types into the notes;
#   every other boot lets the write that follows finish, so the generations advance and the ring
#   of slots is reused, and every other boot is killed outright (kill -9 of QEMU) a moment after
#   the kernel says it is writing the next one -- the store's writes are throttled so that moment
#   is wide and the cut lands inside the write.
# - what must hold: every boot restores a generation no older than the last one seen finished and
#   never the one that was being written when the power went (a generation is claimed by its
#   header, written last, so a cut one is not there to be found), never "starting fresh" once a
#   generation is there, and the machine comes up every time.
# - the log says how many cuts actually landed inside a write; with RUNS of 8 or more at least one
#   must have, else nothing was proven. Once the ring of sixteen slots has gone round, a cut leaves
#   an old header over new data in some slot; that slot is never the newest, so it is not read,
#   and the count of generations skipped for their checksum stays informational.
#   RUNS=100 sh tools/powerloss.sh is the pre-release run (tools/kvm-battery.sh).
cd "$(dirname "$0")/.."
. tools/testlib.sh
RUNS=${RUNS:-12}
RATE=${RATE:-8192}                  # bytes per second the store takes: a generation of ~40 KiB takes ~5 s
rm -f $BUILD/teststore.img $BUILD/pl-*.log $BUILD/pl-mon
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

start_machine() {                   # $1: its serial log; monitor on fd 3
    rm -f $BUILD/pl-mon
    mkfifo $BUILD/pl-mon
    # The firmware's variable store fresh for every boot: a cut can land
    # while the firmware itself is writing it, and a firmware that then
    # boots into nothing (an empty log, boot 62 of a hundred) is the
    # firmware's failure, not the store's, which is what is under test.
    fresh_vars $BUILD/test-vars.fd
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -drive id=store,file=$BUILD/teststore.img,format=raw,if=none,throttling.bps-write=$RATE \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 < $BUILD/pl-mon >/dev/null 2>&1 &
    QPID=$!
    exec 3> $BUILD/pl-mon
}

stop_machine() {                    # $1: quit (let it finish) or cut (the power goes out)
    if [ "$1" = quit ]; then echo quit >&3; wait $QPID 2>/dev/null
    else kill -9 $QPID 2>/dev/null; wait $QPID 2>/dev/null; fi
    exec 3>&-
    rm -f $BUILD/pl-mon
}

floor=0          # the newest generation seen finished; a boot may not fall below it
cutgen=0         # the generation being written when the power went; it must never be restored
inside=0         # cuts that landed inside a write: a write begun and not finished
torn=0           # generations the next boot skipped for their checksum (informational)
ok=1
i=0
while [ $i -le $RUNS ]; do
    LOG=$BUILD/pl-$i.log
    rm -f $LOG
    start_machine $LOG
    if ! waitlog $LOG 'graph restored from generation\|starting fresh' 90; then
        echo "FAILED: boot $i did not come up"; ok=0; stop_machine cut; break
    fi
    got=$(grep -ao 'graph restored from generation [0-9]*' $LOG | tail -1 | awk '{print $NF}')
    [ -z "$got" ] && got=0
    t=$(grep -ac 'fails its checksum\|no longer has' $LOG)
    torn=$((torn + t))
    if [ $i -gt 0 ]; then
        if [ "$got" -lt "$floor" ]; then echo "FAILED: boot $i restored generation $got, below $floor seen finished"; ok=0; fi
        if [ "$cutgen" -gt 0 ] && [ "$got" -eq "$cutgen" ]; then echo "FAILED: boot $i restored generation $got, the one the power cut"; ok=0; fi
        if [ "$floor" -gt 0 ] && grep -aq 'starting fresh' $LOG; then echo "FAILED: boot $i started fresh with generation $floor on the disk"; ok=0; fi
    fi
    bootwait $LOG >&3
    if [ $i -eq $RUNS ]; then stop_machine quit; break; fi

    # Type a few letters into the notes, then wait for the write that follows
    # the quiet to begin. The first run lets it finish; every other run cuts
    # the power a varying moment into it.
    before=$(count $LOG 'snap: writing generation')
    keys right r u n spc $((i % 10)) >&3
    cutgen=0
    if ! waitcount $LOG 'snap: writing generation' $((before + 1)) 60; then
        echo "(boot $i: no write began within 60 s)"
        stop_machine quit
    elif [ $((i % 2)) -eq 0 ]; then
        # let this very generation finish -- an earlier one's line is
        # already in the log -- then a quit, which lets pending writes land
        g=$(grep -ao 'writing generation [0-9]*' $LOG | tail -1 | awk '{print $3}')
        waitlog $LOG "generation $g written" 90
        stop_machine quit
    else
        # 0.3 to 4.5 s into a write of about 5 s, spread over the runs
        d=$(( 3 + (i * 37) % 43 ))
        sleep $(( d / 10 )).$(( d % 10 ))
        stop_machine cut
        # a write begun and not finished is a cut that landed inside one;
        # that generation must not be there on the next boot
        if [ "$(count $LOG 'snap: writing generation')" -gt "$(count $LOG 'generation [0-9]* written')" ]; then
            inside=$((inside + 1))
            cutgen=$(grep -ao 'writing generation [0-9]*' $LOG | tail -1 | awk '{print $3}')
        fi
    fi
    # what finished raises the floor
    f=$(grep -ao 'generation [0-9]* written' $LOG | tail -1 | awk '{print $2}')
    [ -n "$f" ] && [ "$f" -gt "$floor" ] && floor=$f
    i=$((i + 1))
done

echo "--- the last boot ---"
grep -ao 'snap:.*' $BUILD/pl-$RUNS.log | tail -3 | cut -c1-100
echo "--- the run ---"
echo "$RUNS boots, $((RUNS / 2)) cut; $inside cuts landed inside a write; $torn generations skipped for their checksum; the newest finished generation was $floor"
echo "--- the checks ---"
if [ $RUNS -ge 8 ] && [ $inside -lt 1 ]; then echo "FAILED: no cut landed inside a write, so nothing was proven"; ok=0; fi
[ $ok = 1 ] && echo "the store came back whole after every cut, never the generation the cut fell into" || echo "power loss FAILED"
