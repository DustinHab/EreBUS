#!/bin/sh
# installtest.sh -- the offer made at start-up, on a disk that is not empty.
#
# This is the case a person actually has: a machine booted from the
# stick, and a disk in it with somebody else's system on it. The kernel
# refuses such a disk as a store on its own, correctly, and so the
# machine comes up with no memory -- and until now the only way on was
# to already know the words "settle on disk 1".
#
# So the machine asks while it starts. Here it is answered with the
# number of the disk and then the word "yes", both typed through the
# keyboard the same way a person would; the disk is emptied, given a
# boot volume with this system and a store, and adopted on the spot. A
# text is made afterwards so there is something to keep, and the second
# boot runs from that disk with no stick present at all and finds it.

cd "$(dirname "$0")/.."
BUILD=build
sh tools/mkusb.sh >/dev/null || exit 1
rm -f $BUILD/install.img $BUILD/install-1.log $BUILD/install-2.log $BUILD/install.ppm

# A disk with somebody else's system on it: a partition table, a boot
# signature, and data where a filesystem would be.
dd if=/dev/zero of=$BUILD/install.img bs=1M count=512 status=none
dd if=/dev/urandom of=$BUILD/install.img bs=512 count=2048 conv=notrunc status=none
printf '\125\252' | dd of=$BUILD/install.img bs=1 seek=510 conv=notrunc status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

COMMON="-machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio"

echo "--- first boot: from the stick, answering the question ---"
{
    # The question comes up during start-up, before the desktop.
    sleep 22
    keys 1
    sleep 3
    keys y e s ret
    sleep 60
    # and then the desktop, to make something worth keeping
    keys tab tab tab tab tab pause
    keys m a k e spc t e x t spc k e p t ret pause
    sleep 8
    echo "screendump $BUILD/install.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/install.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$BUILD/install-1.log >/dev/null 2>&1
python3 tools/ppm2png.py $BUILD/install.ppm $BUILD/install.png 2>/dev/null
grep -a 'disk:\|blk:\|snap:' $BUILD/install-1.log | cut -c1-116

echo "--- second boot: from the disk alone, no stick ---"
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
{
    sleep 24
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/install.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$BUILD/install-2.log >/dev/null 2>&1
grep -a 'disk:\|blk:\|snap:' $BUILD/install-2.log | cut -c1-116

echo "--- the checks ---"
ok=1
grep -aq 'a disk can be given to it now' $BUILD/install-1.log && echo "the machine asked, without being asked to" || { echo "FAILED: no question"; ok=0; }
grep -aq 'everything on disk 1 will be lost' $BUILD/install-1.log && echo "it said what would be lost before doing it" || { echo "FAILED: no warning"; ok=0; }
grep -aq 'laying down the boot volume' $BUILD/install-1.log && echo "the disk was emptied and given a boot volume" || { echo "FAILED: no boot volume"; ok=0; }
grep -aq 'generation 1 written' $BUILD/install-1.log && echo "and a generation was written to the store" || { echo "FAILED: nothing was kept"; ok=0; }
grep -aq 'store partition on the boot disk' $BUILD/install-2.log && echo "the second boot came from that disk" || { echo "FAILED: the second boot did not come from the disk"; ok=0; }
grep -aq 'graph restored from generation' $BUILD/install-2.log && echo "and found what was made before" || { echo "FAILED: the graph was not found"; ok=0; }
[ $ok = 1 ] && echo "installing onto a disk that was not empty works" || echo "the install offer FAILED"
