#!/bin/sh
# renewtest.sh -- 'install this kernel': a newer stick updates a settled disk, store kept.
# - build "settled", settle it onto a disk from the stick
# - build "renewed" onto the stick, boot beside the disk, make a text, 'install this kernel'
# - boot the disk alone: must say "renewed" and restore the graph; no loader fallback
# - runs make twice; the battery runs it after the parallel tests for that reason

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG1=$BUILD/renew-1.log
LOG2=$BUILD/renew-2.log
LOG3=$BUILD/renew-3.log
rm -f $BUILD/renew.img $LOG1 $LOG2 $LOG3
dd if=/dev/zero of=$BUILD/renew.img bs=1M count=512 status=none

COMMON="$QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/renew-vars.fd \
  -device e1000,netdev=n0 -netdev user,id=n0"

echo "--- first: the old system, settled onto the disk from a stick ---"
# The name goes through the environment, because mkusb.sh runs make
# again on its own and a name given on this command line alone would be
# undone by that second run.
export VERSION=settled
make -s >/dev/null || exit 1
sh tools/mkusb.sh >/dev/null || exit 1
cp build/stick.img $BUILD/renew-stick.img
fresh_vars $BUILD/renew-vars.fd
{
    waitlog $LOG1 'disk: >' 60
    sleep 1
    keys 1
    waitcount $LOG1 'disk: >' 2 20
    sleep 1
    keys y e s ret
    waitlog $LOG1 'generation 1 written' 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/renew-stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$LOG1 >/dev/null 2>&1
grep -a 'EreBUS \|laying down\|generation 1 written' $LOG1 | cut -c1-100

echo "--- second: the new system on the stick, beside the settled disk ---"
export VERSION=renewed
make -s >/dev/null || exit 1
sh tools/mkusb.sh >/dev/null || exit 1
cp build/stick.img $BUILD/renew-stick.img
fresh_vars $BUILD/renew-vars.fd
{
    bootwait $LOG2
    keys tab tab tab tab tab pause
    say "make text kept"
    say "install this kernel"
    waitlog $LOG2 'are installed on the boot disk' 60
    waitlog $LOG2 'generation . written' 40
    sleep 1
    echo "screendump $BUILD/renew.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/renew-stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$LOG2 >/dev/null 2>&1
grep -a 'EreBUS \|store partition\|graph restored\|are installed\|generation . written' $LOG2 | cut -c1-100

echo "--- third: the disk alone, no stick ---"
fresh_vars $BUILD/renew-vars.fd
{
    waitlog $LOG3 'graph restored\|shell: ' 90
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$LOG3 >/dev/null 2>&1
grep -a 'EreBUS \|store partition\|graph restored\|fell back\|previous one' $LOG3 | cut -c1-100

# leave the tree calling itself what git says
unset VERSION
make -s >/dev/null
sh tools/mkusb.sh >/dev/null
python3 tools/ppm2png.py $BUILD/renew.ppm $BUILD/renew.png 2>/dev/null
echo "--- the second boot's own lines ---"
grep -a 'boot:\|fat:' $LOG2 | cut -c1-100

echo "--- the checks ---"
ok=1
grep -aq 'EreBUS settled' $LOG1 && echo "the old system settled onto the disk" || { echo "FAILED: the old system did not settle"; ok=0; }
grep -aq 'EreBUS renewed' $LOG2 && grep -aq 'graph restored' $LOG2 \
  && echo "the new system booted from the stick and took up the disk's graph" || { echo "FAILED: the stick did not boot beside the disk"; ok=0; }
grep -aq 'are installed on the boot disk' $LOG2 && echo "it installed itself onto the disk" || { echo "FAILED: install this kernel did not install"; ok=0; }
grep -aq 'EreBUS renewed' $LOG3 && echo "the disk alone now starts the new system" || { echo "FAILED: the disk still starts the old system"; ok=0; }
grep -aq 'graph restored' $LOG3 && echo "and the graph is still there" || { echo "FAILED: the graph was lost"; ok=0; }
grep -aq 'previous one' $LOG3 && { echo "FAILED: the loader fell back to the old kernel"; ok=0; } || echo "and the loader did not fall back"
[ $ok = 1 ] && echo "an installed machine takes a newer system and keeps its memory" || echo "renewing an installed machine FAILED"
