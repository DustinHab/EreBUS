#!/bin/sh
# termtest.sh -- the terminal through the screen: make a text, write a script, run it, find, journal.
# - proof: the final screenshot and the serial log

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/termtest.ppm
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
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
    sleep 12
    keys f i n d spc h e l l o ret pause
    keys j o u r n a l ret pause
    sleep 2
    echo "screendump $BUILD/termtest.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/termtest.ppm $BUILD/termtest.png 2>/dev/null

echo "--- what the script said ---"
# The runner pads its words to eight bytes with zeros, so the line
# cannot be matched to its end; the boot-time greeting is the one
# other "hello", and it names its ring.
grep -a 'user: hello' $BUILD/serial.log | grep -v 'ring 3'
