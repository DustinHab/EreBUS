#!/bin/sh
# usbstick.sh -- build/stick.img as a usb disk and the only disk, booted twice.
# - the firmware boots the loader off the stick; the kernel reaches the stick through xhci and finds
#   the store partition on it; what is typed on boot 1 must be back on boot 2
# - no sata disk at all: the store lives on the stick or nowhere

cd "$(dirname "$0")/.."
. tools/testlib.sh
need_stick
fresh_vars $BUILD/test-vars.fd
rm -f $BUILD/usbstick-1.log $BUILD/usbstick-2.log

boot() {
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -device qemu-xhci,id=xhci \
      -drive if=none,id=stick,file=$BUILD/stick.img,format=raw \
      -device usb-storage,bus=xhci.0,drive=stick \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 >/dev/null 2>&1
}

{
    bootwait $BUILD/usbstick-1.log
    keys right o n e spc d i s k
    waitlog $BUILD/usbstick-1.log 'generation 1 written' 40
    sleep 1
    echo quit
} | boot $BUILD/usbstick-1.log

{
    bootwait $BUILD/usbstick-2.log
    waitlog $BUILD/usbstick-2.log 'graph restored' 20
    sleep 1
    echo quit
} | boot $BUILD/usbstick-2.log

echo "--- first boot ---"
grep -a 'usb:  \(a disk\|ports\|[0-9]* disk\)\|blk:\|snap:' $BUILD/usbstick-1.log | cut -c1-110
echo "--- second boot ---"
grep -a 'blk:\|snap:' $BUILD/usbstick-2.log | cut -c1-110
echo "--- the checks ---"
ok=1
if grep -aq 'usb:  a disk,' $BUILD/usbstick-1.log; then echo "the stick was found as a usb disk"; else echo "FAILED: no usb disk found"; ok=0; fi
if grep -aq 'store partition on the usb disk' $BUILD/usbstick-1.log; then echo "the store partition on it was found"; else echo "FAILED: no store partition found on the usb disk"; ok=0; fi
if grep -aq 'blk:  self test passed' $BUILD/usbstick-1.log; then echo "a sector written to it reads back"; else echo "FAILED: the block self test on usb did not pass"; ok=0; fi
if grep -aq 'generation 1 written' $BUILD/usbstick-1.log; then echo "a generation was written"; else echo "FAILED: nothing written"; ok=0; fi
if grep -aq 'graph restored from generation' $BUILD/usbstick-2.log; then echo "the second boot found it"; else echo "FAILED: the second boot found nothing"; ok=0; fi
[ $ok = 1 ] && echo "a usb stick carries the whole system" || echo "the usb stick FAILED"
