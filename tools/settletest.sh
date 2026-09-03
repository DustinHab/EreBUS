#!/bin/sh
# settletest.sh -- settling on a disk, from a machine that runs off a stick.
#
# The stick image is put on a usb storage device and the machine boots
# from it; the only sata disk is blank. Without a boot disk on the bus
# the kernel offers the blank disk instead of taking it. Through the
# terminal: 'disks', 'settle on disk 1', 'yes' -- the disk gets a boot
# volume with the loader and the kernel, and a store -- then a text is
# made, so there is something to keep. The second boot has no stick at
# all: the machine comes up from the disk it settled on and finds the
# text.

cd "$(dirname "$0")/.."
BUILD=build
sh tools/mkusb.sh >/dev/null || exit 1
rm -f $BUILD/settle.img $BUILD/settle-1.log $BUILD/settle-2.log $BUILD/settle.ppm
dd if=/dev/zero of=$BUILD/settle.img bs=1M count=512 status=none
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

echo "--- first boot: from the stick, settling on the blank disk ---"
{
    # The machine asks during start-up whether it should take the disk.
    # This test is about doing it from the terminal instead, so the
    # offer is declined and the words are typed out.
    sleep 22
    keys esc
    sleep 12
    keys tab tab tab tab tab pause
    keys d i s k s ret pause pause
    keys s e t t l e spc o n spc d i s k spc 1 ret pause pause
    keys y e s ret
    sleep 60
    keys m a k e spc t e x t spc k e p t ret pause
    sleep 8
    echo "screendump $BUILD/settle.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -drive id=disk,file=$BUILD/settle.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1 \
  -serial file:$BUILD/settle-1.log >/dev/null 2>&1
python3 tools/ppm2png.py $BUILD/settle.ppm $BUILD/settle.png 2>/dev/null
grep -a 'blk:\|boot: the loader\|snap:' $BUILD/settle-1.log | cut -c1-110

echo "--- second boot: from the disk alone ---"
# Fresh firmware variables: this firmware, having once booted from the
# stick, keeps looking for it rather than walking the disks anew. A
# real machine's firmware falls back to the disks on its own.
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
{
    sleep 24
    echo quit
} | qemu-system-x86_64 $COMMON \
  -drive id=disk,file=$BUILD/settle.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.0 \
  -serial file:$BUILD/settle-2.log >/dev/null 2>&1
grep -a 'blk:\|snap:' $BUILD/settle-2.log | cut -c1-110

echo "--- the checks ---"
ok=1
if grep -aq "'settle on disk 1'" $BUILD/settle-1.log; then echo "the blank disk was offered, not taken"; else echo "FAILED: the blank disk was not offered"; ok=0; fi
if grep -aq 'blank partition on port 1; it is the store now' $BUILD/settle-1.log; then echo "the store was made and adopted"; else echo "FAILED: no store was adopted"; ok=0; fi
if grep -aq 'generation 1 written' $BUILD/settle-1.log; then echo "a generation was written to it"; else echo "FAILED: nothing was written"; ok=0; fi
if grep -aq 'store partition on the boot disk' $BUILD/settle-2.log; then echo "the second boot came from the disk"; else echo "FAILED: the second boot did not come from the disk"; ok=0; fi
if grep -aq 'graph restored from generation' $BUILD/settle-2.log; then echo "and found the graph"; else echo "FAILED: the graph was not found"; ok=0; fi
[ $ok = 1 ] && echo "settling on a disk works" || echo "settling on a disk FAILED"
