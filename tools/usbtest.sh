#!/bin/sh
# usbtest.sh -- a keyboard and a mouse on usb, and no PS/2 at all.
#
# The machine is booted with its i8042 switched off and an xHCI
# controller carrying QEMU's usb keyboard and mouse. The keys typed
# through the monitor can only arrive by usb. The terminal is walked
# into and a text made and run, as termtest does; the mouse is moved
# and the screen at the end shows where the pointer went.

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/usbtest.ppm
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            m*,*) echo "mouse_move ${k#m}" | tr ',' ' '; sleep 0.1 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

{
    sleep 24
    keys tab tab tab tab tab pause
    keys m a k e spc t e x t spc g r e e t ret pause
    keys g o spc g r e e t ret pause
    keys w r i t e spc s a y spc h e l l o ret pause
    keys b a c k ret pause
    keys r u n spc g r e e t ret
    sleep 8
    keys m300,200 m300,200 m-100,50
    sleep 2
    echo "screendump $BUILD/usbtest.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35,i8042=off -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbtest.ppm $BUILD/usbtest.png 2>/dev/null

echo "--- the controller and its devices ---"
grep -a 'usb:\|ps2:' $BUILD/serial.log | cut -c1-110
echo "--- what the script said, typed by usb ---"
grep -a 'user: hello' $BUILD/serial.log | grep -v 'ring 3'
