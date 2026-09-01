#!/bin/sh
# asmtest.sh -- the machine programmed on itself.
#
# Through the screen's terminal: assemble the page that describes the
# machine (it is written to assemble as it stands), run the image it
# made, and hear it speak. Then a text with a mistake in it, to see
# the assembler say which line. Then the machine boots again on the
# same store: the program a person built must come back the way a
# script does.

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/asmtest.ppm
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

boot() {
    qemu-system-x86_64 -machine q35 -m 512M -cpu max \
      -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -vga none -device VGA,edid=on,xres=1280,yres=800 \
      -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -display none -monitor stdio \
      -serial file:$1 >/dev/null 2>&1
}

{
    sleep 24
    keys tab tab tab tab tab pause
    keys g o spc a s i d e ret pause
    keys a s s e m b l e spc t h e spc m a c h i n e ret pause pause
    keys r u n spc t h e spc m a c h i n e spc c o d e ret pause pause
    keys b a c k ret pause
    keys m a k e spc t e x t spc b a d ret pause pause
    keys g o spc b a d ret pause
    keys w r i t e spc m o v v spc r a x comma spc 1 ret pause pause
    keys b a c k ret pause
    keys a s s e m b l e spc b a d ret pause pause
    keys j o u r n a l ret pause
    sleep 6
    echo "screendump $BUILD/asmtest.ppm"
    sleep 2
    echo quit
} | boot $BUILD/serial.log

python3 tools/ppm2png.py $BUILD/asmtest.ppm $BUILD/asmtest.png 2>/dev/null

echo "--- first boot: built and run ---"
grep -a 'running an image\|user: hello from' $BUILD/serial.log

# The second boot: nothing typed. The image the person made is part
# of the world that comes back.
{
    sleep 30
    echo quit
} | boot $BUILD/serial2.log

echo "--- second boot: came back on its own ---"
grep -a 'restored\|running an image\|user: hello from' $BUILD/serial2.log
