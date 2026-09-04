#!/bin/sh
# sticktest.sh -- build/stick.img as the only disk, booted twice.
# - the store partition on the boot disk must be found; what is typed on boot 1 must be back on boot 2

cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
fresh_vars $BUILD/test-vars.fd
rm -f $BUILD/stick-1.log $BUILD/stick-2.log

boot() {
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/stick.img \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 >/dev/null 2>&1
}

{
    bootwait $BUILD/stick-1.log
    keys right o n e spc d i s k
    waitlog $BUILD/stick-1.log 'generation 1 written' 40
    sleep 1
    echo quit
} | boot $BUILD/stick-1.log

{
    bootwait $BUILD/stick-2.log
    waitlog $BUILD/stick-2.log 'graph restored' 20
    sleep 1
    echo quit
} | boot $BUILD/stick-2.log

echo "--- first boot ---"
grep -a 'blk:\|snap:' $BUILD/stick-1.log | cut -c1-100
echo "--- second boot ---"
grep -a 'blk:\|snap:' $BUILD/stick-2.log | cut -c1-100
echo "--- the checks ---"
ok=1
if grep -aq 'store partition on the boot disk' $BUILD/stick-1.log; then echo "the store partition was found on the boot disk"; else echo "FAILED: no store partition found"; ok=0; fi
if grep -aq 'generation 1 written' $BUILD/stick-1.log; then echo "a generation was written"; else echo "FAILED: nothing written"; ok=0; fi
if grep -aq 'graph restored from generation' $BUILD/stick-2.log; then echo "the second boot found it"; else echo "FAILED: the second boot found nothing"; ok=0; fi
[ $ok = 1 ] && echo "one disk carries the whole system" || echo "the one-disk system FAILED"
