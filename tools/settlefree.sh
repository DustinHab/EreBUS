#!/bin/sh
# settlefree.sh -- 'settle in the free space of disk N' beside a foreign partition.
# - disk: one foreign partition full of random bytes, free space after it
# - boot from the stick, decline the boot-time offer, settle in the free space, make a text
# - the foreign bytes must be unchanged; a second boot must find the text

cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
LOG1=$BUILD/free-1.log
LOG2=$BUILD/free-2.log
rm -f $BUILD/free.img $LOG1 $LOG2
dd if=/dev/zero of=$BUILD/free.img bs=1M count=256 status=none
sgdisk -o -n 1:2048:+64M -t 1:0700 -c 1:"OTHER" $BUILD/free.img >/dev/null
dd if=/dev/urandom of=$BUILD/free.img bs=1M seek=1 count=64 conv=notrunc status=none
before=$(dd if=$BUILD/free.img bs=1M skip=1 count=64 status=none | md5sum)
fresh_vars $BUILD/test-vars.fd

COMMON="$QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -drive id=disk,file=$BUILD/free.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1"

echo "--- first boot: settling in the free space ---"
{
    # Decline the offer made during start-up: this test is about
    # keeping the partitions that are already there, which is what the
    # words do and what taking the disk whole would not.
    waitlog $LOG1 'disk: >' 60
    sleep 1
    keys esc
    bootwait $LOG1
    keys tab tab tab tab tab pause
    say "settle in the free space of disk 1"
    keys pause
    say "yes"
    waitlog $LOG1 'it is the store now' 60
    sleep 1
    say "make text kept"
    waitlog $LOG1 'generation 1 written' 40
    sleep 1
    echo quit
} | qemu-system-x86_64 $COMMON -serial file:$LOG1 >/dev/null 2>&1
grep -a 'blk:\|snap:' $LOG1 | cut -c1-110

echo "--- the table afterwards ---"
sgdisk -p $BUILD/free.img 2>/dev/null | sed -n '/^Number/,$p'
after=$(dd if=$BUILD/free.img bs=1M skip=1 count=64 status=none | md5sum)

echo "--- second boot: stick and disk again ---"
{
    waitlog $LOG2 'graph restored\|shell: ' 90
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON -serial file:$LOG2 >/dev/null 2>&1
grep -a 'blk:\|snap:' $LOG2 | cut -c1-110

echo "--- the checks ---"
ok=1
if grep -aq 'blank partition on port 1; it is the store now' $LOG1; then echo "a store was made in the free space"; else echo "FAILED: no store was made"; ok=0; fi
if [ "$before" = "$after" ]; then echo "the other partition is byte for byte as it was"; else echo "FAILED: the other partition changed"; ok=0; fi
if sgdisk -p $BUILD/free.img 2>/dev/null | grep -q 'EREBUS STORE'; then echo "the table shows the store beside it"; else echo "FAILED: no store in the table"; ok=0; fi
if grep -aq 'graph restored from generation' $LOG2; then echo "the second boot found the graph"; else echo "FAILED: the graph was not found"; ok=0; fi
[ $ok = 1 ] && echo "settling in the free space works" || echo "settling in the free space FAILED"
