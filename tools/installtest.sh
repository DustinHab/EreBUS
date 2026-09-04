#!/bin/sh
# installtest.sh -- the boot-time offer, on a disk that already holds data.
# - boot from the stick beside a disk with a partition table and data
# - answer the offer with the disk number and "yes" through the keyboard
# - a text is made; the second boot from the disk alone must find it

cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
LOG1=$BUILD/install-1.log
LOG2=$BUILD/install-2.log
rm -f $BUILD/install.img $LOG1 $LOG2 $BUILD/install.ppm

# A disk with somebody else's system on it: a partition table, a boot
# signature, and data where a filesystem would be.
dd if=/dev/zero of=$BUILD/install.img bs=1M count=512 status=none
dd if=/dev/urandom of=$BUILD/install.img bs=512 count=2048 conv=notrunc status=none
printf '\125\252' | dd of=$BUILD/install.img bs=1 seek=510 conv=notrunc status=none
fresh_vars $BUILD/test-vars.fd

COMMON="$QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -device e1000,netdev=n0 -netdev user,id=n0"

echo "--- first boot: from the stick, answering the question ---"
{
    # The question comes up during start-up, before the desktop.
    waitlog $LOG1 'disk: >' 60
    sleep 1
    keys 1
    waitcount $LOG1 'disk: >' 2 20
    sleep 1
    keys y e s ret
    bootwait $LOG1
    say "make text kept"
    waitlog $LOG1 'generation 1 written' 40
    sleep 1
    echo "screendump $BUILD/install.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/install.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$LOG1 >/dev/null 2>&1
python3 tools/ppm2png.py $BUILD/install.ppm $BUILD/install.png 2>/dev/null
grep -a 'disk:\|blk:\|snap:' $LOG1 | cut -c1-116

echo "--- second boot: from the disk alone, no stick ---"
fresh_vars $BUILD/test-vars.fd
{
    waitlog $LOG2 'graph restored\|shell: ' 90
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/install.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$LOG2 >/dev/null 2>&1
grep -a 'disk:\|blk:\|snap:' $LOG2 | cut -c1-116

echo "--- the checks ---"
ok=1
grep -aq 'a disk can be given to it now' $LOG1 && echo "the machine asked, without being asked to" || { echo "FAILED: no question"; ok=0; }
grep -aq 'everything on disk 1 will be lost' $LOG1 && echo "it said what would be lost before doing it" || { echo "FAILED: no warning"; ok=0; }
grep -aq 'laying down the boot volume' $LOG1 && echo "the disk was emptied and given a boot volume" || { echo "FAILED: no boot volume"; ok=0; }
grep -aq 'generation 1 written' $LOG1 && echo "and a generation was written to the store" || { echo "FAILED: nothing was kept"; ok=0; }
grep -aq 'store partition on the boot disk' $LOG2 && echo "the second boot came from that disk" || { echo "FAILED: the second boot did not come from the disk"; ok=0; }
grep -aq 'graph restored from generation' $LOG2 && echo "and found what was made before" || { echo "FAILED: the graph was not found"; ok=0; }
[ $ok = 1 ] && echo "installing onto a disk that was not empty works" || echo "the install offer FAILED"
