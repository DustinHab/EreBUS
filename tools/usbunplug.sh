#!/bin/sh
# usbunplug.sh -- the store's stick unplugged while the machine runs, and plugged back in.
# - boot 1: the stick is the only disk and carries the store; a generation is written; the stick is unplugged
#   (device_del) -> the machine says so and the next changes wait; another stick with a store of its own is
#   plugged in -> refused, it is not the one; the original is plugged back in -> taken back by its identity,
#   the waiting changes are written as the next generation
# - boot 2: the same stick, restored from that generation
cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
cp $BUILD/stick.img $BUILD/unplug-other.img
fresh_vars $BUILD/unplug-vars.fd
L1=$BUILD/unplug-1.log
L2=$BUILD/unplug-2.log
rm -f $L1 $L2

# The disks as blockdevs: a -drive goes away with the device it was
# plugged into, a blockdev stays and can be plugged in again.
boot() {
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/unplug-vars.fd \
      -device qemu-xhci,id=xhci \
      -blockdev driver=file,node-name=stickf,filename=$BUILD/stick.img \
      -blockdev driver=raw,node-name=stick,file=stickf \
      -blockdev driver=file,node-name=otherf,filename=$BUILD/unplug-other.img \
      -blockdev driver=raw,node-name=other,file=otherf \
      -device usb-storage,id=st,bus=xhci.0,drive=stick \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 > $1.monitor 2>&1
}

{
    bootwait $L1
    keys right o n e spc d i s k
    waitlog $L1 'generation 1 written' 40
    echo "device_del st"
    waitlog $L1 'disk was unplugged' 20
    keys spc g o n e
    waitlog $L1 'no store to write to' 20
    echo "device_add usb-storage,id=st2,bus=xhci.0,drive=other"
    waitlog $L1 'not the one that was unplugged' 20
    n=$(count $L1 'is gone')
    echo "device_del st2"
    waitcount $L1 'is gone' $((n + 1)) 20
    sleep 1
    echo "device_add usb-storage,id=st3,bus=xhci.0,drive=stick"
    waitlog $L1 'disk is back' 20
    waitlog $L1 'generation 2 written' 40
    sleep 1
    echo quit
} | boot $L1

{
    bootwait $L2
    waitlog $L2 'graph restored' 20
    sleep 1
    echo quit
} | boot $L2

echo "--- first boot ---"
grep -a 'blk:\|snap:\|usb:  a disk' $L1 | cut -c1-120
echo "--- second boot ---"
grep -a 'blk:  the store\|snap:' $L2 | cut -c1-120
echo "--- the checks ---"
ok=1
grep -aq 'blk:  the store speaks format 1, id' $L1 && echo "the store carries its format and identity" || { echo "FAILED: no store identity"; ok=0; }
grep -aq 'generation 1 written' $L1 && echo "a generation was written to the stick" || { echo "FAILED: nothing written"; ok=0; }
grep -aq "the store's disk was unplugged" $L1 && echo "unplugging the stick was noticed" || { echo "FAILED: unplugging went unnoticed"; ok=0; }
grep -aq 'no store to write to' $L1 && echo "the changes made meanwhile waited" || { echo "FAILED: the changes did not wait"; ok=0; }
grep -aq 'not the one that was unplugged' $L1 && echo "another stick with a store was refused" || { echo "FAILED: another store was taken"; ok=0; }
grep -aq "the store's disk is back" $L1 && echo "the original stick was taken back" || { echo "FAILED: the stick was not taken back"; ok=0; }
grep -aq 'generation 2 written' $L1 && echo "and the waiting changes were written" || { echo "FAILED: no generation after the return"; ok=0; }
grep -aq 'graph restored from generation 2' $L2 && echo "the second boot found that generation" || { echo "FAILED: the second boot did not find generation 2"; ok=0; }
[ $ok = 1 ] && echo "the store's stick can be unplugged and plugged back in" || echo "unplugging the store FAILED"
