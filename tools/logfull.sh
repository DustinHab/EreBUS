#!/bin/sh
# logfull.sh -- the blob log running full.
# - store of 19 MiB leaves a 2 MiB log; a 160 KiB source gets 14 edits, one generation each
# - the log must compact, drop the oldest generations only, and keep the latest text after a second boot

cd "$(dirname "$0")/.."
BUILD=build
mkdir -p $BUILD/logfull
rm -f $BUILD/logfull/disk.img
dd if=/dev/zero of=$BUILD/logfull/disk.img bs=1M count=8 status=none
mkfs.vfat -F 32 $BUILD/logfull/disk.img >/dev/null 2>&1
mcopy -i $BUILD/logfull/disk.img kernel/lang/cc.c ::cc.c

DOOR_EXTRA="-drive id=xchg,file=$BUILD/logfull/disk.img,format=raw,if=none -device ide-hd,drive=xchg,bus=ide.2"
DOOR_STORE_MB=19
. tools/doorboot.sh

# waits until the machine has written at least $1 generations
wait_gen() {
    n=0
    while [ $n -lt 60 ]; do
        [ "$(grep -a -c 'snap: generation [0-9]* written' $DOOR_SERIAL)" -ge "$1" ] && return 0
        sleep 1; n=$((n + 1))
    done
    return 1
}

wait_gen 1 || echo "no first generation"
rounds=14
i=1
while [ $i -le $rounds ]; do
    door_say "go system" "go the disk" "go cc.c" "write round $i" | grep -a -v 'written\.' | grep -a 'room\|only a text\|read-only'
    wait_gen $((i + 1)) || echo "round $i: no generation written"
    i=$((i + 1))
done
sleep 2
echo "--- the log, as the machine told it ---"
grep -a 'blob:\|dropped\|snap: no room\|snap: could not\|snap: the log' $DOOR_SERIAL | cut -c1-110
door_stop
cp $DOOR_SERIAL $BUILD/logfull/serial-1.log

echo "--- second boot ---"
DOOR_EXTRA=""
DOOR_KEEP=1
. tools/doorboot.sh
grep -a 'blob:\|snap: graph restored' $DOOR_SERIAL | cut -c1-110
door_say "go system" "go the disk" "read cc.c" > $BUILD/logfull/answer.txt
door_stop

echo "--- the checks ---"
ok=1
want=$(wc -c < kernel/lang/cc.c)
extra=0; i=1
while [ $i -le $rounds ]; do extra=$((extra + ${#i} + 7)); i=$((i + 1)); done
want=$((want + extra))
more=$(grep -a -o 'and [0-9]* more letters' $BUILD/logfull/answer.txt | head -1 | tr -dc '0-9')
got=$((2000 + ${more:-0}))
if [ "$got" = "$want" ]; then echo "cc.c: $got letters, the source plus $rounds lines"; else echo "cc.c: $got letters, but $want were expected"; ok=0; fi
if grep -a -q 'blob: compacted' $BUILD/logfull/serial-1.log; then echo "the log was compacted"; else echo "the log was never compacted"; ok=0; fi
if grep -a -q 'dropped' $BUILD/logfull/serial-1.log; then echo "old generations were dropped"; else echo "no generation was dropped"; ok=0; fi
if grep -a -q 'snap: could not\|snap: no room' $BUILD/logfull/serial-1.log; then echo "a save failed"; ok=0; fi
[ $ok = 1 ] && echo "the full log was handled" || echo "something went wrong with the full log"
