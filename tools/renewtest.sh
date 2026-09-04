#!/bin/sh
# renewtest.sh -- a newer system put onto an installed machine, and the
# machine's memory kept.
#
# The case: Erebus was settled on a disk some time ago, and a stick with
# a newer build arrives. Booting the stick beside that disk brings up
# the newer kernel with the disk's store -- the same graph, the same
# desktop, and no way to tell from looking that anything is new. The
# person wants the disk to start with the new system from now on, and
# wants nothing they made to be lost. 'settle' cannot do it: it empties
# the disk, and refuses the disk whose store is in use anyway.
#
# So: build one system and settle it onto a disk from a stick. Build a
# second that calls itself "renewed" and put it on the stick. Boot the
# stick beside the disk, make something, say 'install this kernel'.
# Then boot the disk alone: it must call itself "renewed", find the
# graph, and find the thing that was made.

cd "$(dirname "$0")/.."
BUILD=build
rm -f $BUILD/renew.img $BUILD/renew-1.log $BUILD/renew-2.log $BUILD/renew-3.log
dd if=/dev/zero of=$BUILD/renew.img bs=1M count=512 status=none

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
  -drive if=pflash,format=raw,file=$BUILD/renew-vars.fd \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio"

echo "--- first: the old system, settled onto the disk from a stick ---"
# The name goes through the environment, because mkusb.sh runs make
# again on its own and a name given on this command line alone would be
# undone by that second run.
export VERSION=settled
make -s >/dev/null || exit 1
sh tools/mkusb.sh >/dev/null || exit 1
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/renew-vars.fd
{
    sleep 22
    keys 1
    sleep 3
    keys y e s ret
    sleep 60
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$BUILD/renew-1.log >/dev/null 2>&1
grep -a 'Erebus \|laying down\|generation 1 written' $BUILD/renew-1.log | cut -c1-100

echo "--- second: the new system on the stick, beside the settled disk ---"
export VERSION=renewed
make -s >/dev/null || exit 1
sh tools/mkusb.sh >/dev/null || exit 1
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/renew-vars.fd
{
    sleep 26
    keys tab tab tab tab tab pause
    keys m a k e spc t e x t spc k e p t ret pause
    keys i n s t a l l spc t h i s spc k e r n e l ret
    sleep 20
    echo "screendump $BUILD/renew.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -device usb-kbd,bus=xhci.0 \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$BUILD/renew-2.log >/dev/null 2>&1
grep -a 'Erebus \|store partition\|graph restored\|are installed\|generation . written' $BUILD/renew-2.log | cut -c1-100

echo "--- third: the disk alone, no stick ---"
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/renew-vars.fd
{
    sleep 24
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/renew.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$BUILD/renew-3.log >/dev/null 2>&1
grep -a 'Erebus \|store partition\|graph restored\|fell back\|previous one' $BUILD/renew-3.log | cut -c1-100

# leave the tree calling itself what git says
unset VERSION
make -s >/dev/null
python3 tools/ppm2png.py $BUILD/renew.ppm $BUILD/renew.png 2>/dev/null
echo "--- the second boot's own lines ---"
grep -a 'boot:\|fat:' $BUILD/renew-2.log | cut -c1-100

echo "--- the checks ---"
ok=1
grep -aq 'Erebus settled' $BUILD/renew-1.log && echo "the old system settled onto the disk" || { echo "FAILED: the old system did not settle"; ok=0; }
grep -aq 'Erebus renewed' $BUILD/renew-2.log && grep -aq 'graph restored' $BUILD/renew-2.log \
  && echo "the new system booted from the stick and took up the disk's graph" || { echo "FAILED: the stick did not boot beside the disk"; ok=0; }
grep -aq 'are installed on the boot disk' $BUILD/renew-2.log && echo "it installed itself onto the disk" || { echo "FAILED: install this kernel did not install"; ok=0; }
grep -aq 'Erebus renewed' $BUILD/renew-3.log && echo "the disk alone now starts the new system" || { echo "FAILED: the disk still starts the old system"; ok=0; }
grep -aq 'graph restored' $BUILD/renew-3.log && echo "and the graph is still there" || { echo "FAILED: the graph was lost"; ok=0; }
grep -aq 'previous one' $BUILD/renew-3.log && { echo "FAILED: the loader fell back to the old kernel"; ok=0; } || echo "and the loader did not fall back"
[ $ok = 1 ] && echo "an installed machine takes a newer system and keeps its memory" || echo "renewing an installed machine FAILED"
