#!/bin/sh
# cctest.sh -- C compiler on the machine, through the screen's terminal.
# - compiles "the compiler" page, runs the image, checks hello and the sum
# - compiles a text with an error, checks the reported line

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/cctest.ppm
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
    keys g o spc a s i d e ret pause
    keys c o m p i l e spc t h e spc c o m p i l e r ret pause pause pause
    keys r u n spc t h e spc c o m p i l e r spc c o d e ret pause pause
    keys b a c k ret pause
    keys m a k e spc t e x t spc b a d ret pause pause
    keys g o spc b a d ret pause
    keys w r i t e spc l o n g spc m a i n spc shift-9 shift-0 spc shift-bracket_left spc r e t u r n spc x semicolon spc shift-bracket_right ret pause pause
    keys b a c k ret pause
    keys c o m p i l e spc b a d ret pause pause
    keys j o u r n a l ret pause
    sleep 6
    echo "screendump $BUILD/cctest.ppm"
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

python3 tools/ppm2png.py $BUILD/cctest.ppm $BUILD/cctest.png 2>/dev/null

echo "--- compiled and run ---"
grep -a 'running an image\|user: hello from c\|user: 5050' $BUILD/serial.log
