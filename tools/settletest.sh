#!/bin/sh
# settletest.sh -- 'settle on disk N' from the stick onto a blank sata disk.
# - boot from the usb stick; decline the boot-time offer; 'disks', 'settle on disk 1', 'yes'; make a text
# - second boot from the disk alone must find the text

cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
LOG1=$BUILD/settle-1.log
LOG2=$BUILD/settle-2.log
rm -f $BUILD/settle.img $LOG1 $LOG2 $BUILD/settle.ppm
dd if=/dev/zero of=$BUILD/settle.img bs=1M count=512 status=none
fresh_vars $BUILD/test-vars.fd

COMMON="$QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -device e1000,netdev=n0 -netdev user,id=n0"

echo "--- first boot: from the stick, settling on the blank disk ---"
{
    # The machine asks during start-up whether it should take the disk.
    # This test is about doing it from the terminal instead, so the
    # offer is declined and the words are typed out.
    waitlog $LOG1 'disk: >' 60
    sleep 1
    keys esc
    bootwait $LOG1
    keys tab tab tab tab tab pause
    say "disks"
    keys pause
    say "settle on disk 1"
    keys pause
    say "yes"
    waitlog $LOG1 'it is the store now' 120
    sleep 1
    say "make text kept"
    waitlog $LOG1 'generation 1 written' 40
    sleep 1
    echo "screendump $BUILD/settle.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -drive id=disk,file=$BUILD/settle.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$LOG1 >/dev/null 2>&1
python3 tools/ppm2png.py $BUILD/settle.ppm $BUILD/settle.png 2>/dev/null
grep -a 'blk:\|boot: the loader\|snap:' $LOG1 | cut -c1-110

echo "--- second boot: from the disk alone ---"
# Fresh firmware variables: this firmware, having once booted from the
# stick, keeps looking for it rather than walking the disks anew. A
# real machine's firmware falls back to the disks on its own.
fresh_vars $BUILD/test-vars.fd
{
    waitlog $LOG2 'graph restored\|shell: ' 90
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/settle.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$LOG2 >/dev/null 2>&1
grep -a 'blk:\|snap:' $LOG2 | cut -c1-110

echo "--- the checks ---"
ok=1
if grep -aq "'settle on disk 1'" $LOG1; then echo "the blank disk was offered, not taken"; else echo "FAILED: the blank disk was not offered"; ok=0; fi
if grep -aq 'blank partition on port 1; it is the store now' $LOG1; then echo "the store was made and adopted"; else echo "FAILED: no store was adopted"; ok=0; fi
if grep -aq 'generation 1 written' $LOG1; then echo "a generation was written to it"; else echo "FAILED: nothing was written"; ok=0; fi
if grep -aq 'store partition on the boot disk' $LOG2; then echo "the second boot came from the disk"; else echo "FAILED: the second boot did not come from the disk"; ok=0; fi
if grep -aq 'graph restored from generation' $LOG2; then echo "and found the graph"; else echo "FAILED: the graph was not found"; ok=0; fi
[ $ok = 1 ] && echo "settling on a disk works" || echo "settling on a disk FAILED"
