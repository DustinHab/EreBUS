#!/bin/sh
# sticktest.sh -- the whole system on one disk, twice booted.
#
# build/stick.img alone: no second disk, no exchange disk. The kernel
# must find the store partition on the disk it booted from, keep what
# is typed on the first boot, and have it back on the second.

cd "$(dirname "$0")/.."
BUILD=build
sh tools/mkusb.sh >/dev/null || exit 1
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
rm -f $BUILD/stick-1.log $BUILD/stick-2.log

keys() {
    for k in "$@"; do echo "sendkey $k"; sleep 0.3; done
}

boot() {
    qemu-system-x86_64 -machine q35 -m 512M -cpu max \
      -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/stick.img \
      -vga none -device VGA,edid=on,xres=1280,yres=800 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -display none -monitor stdio \
      -serial file:$1 >/dev/null 2>&1
}

{
    sleep 24
    keys right o n e spc d i s k
    sleep 5
    echo quit
} | boot $BUILD/stick-1.log

{
    sleep 22
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
