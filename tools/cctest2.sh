#!/bin/sh
# cctest2.sh -- the compiler's list, checked on the machine.
#
# The test program and the text it includes ride in on an exchange
# disk: they come in as objects on "the disk" under system, the
# terminal compiles them there, and the image runs. Every check says
# ok or bad on the console.

cd "$(dirname "$0")/.."
BUILD=build
SRC=${1:-tools/cc/proof.c}            # another text rides in under the same name

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/ccdisk.img $BUILD/cctest2.ppm
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/ccdisk.img bs=1M count=16 status=none
mkfs.vfat -F 32 $BUILD/ccdisk.img >/dev/null
mcopy -i $BUILD/ccdisk.img "$SRC" ::proof.c
mcopy -i $BUILD/ccdisk.img tools/cc/words.h ::words.h
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
    keys g o spc s y s t e m ret pause
    keys g o spc t h e spc d i s k ret pause
    keys c o m p i l e spc p r o o f dot c ret pause pause pause
    keys r u n spc p r o o f dot c spc c o d e ret pause pause pause
    sleep 4
    echo "screendump $BUILD/cctest2.ppm"
    sleep 2
    echo "info registers"
    sleep 1
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -drive id=xchg,file=$BUILD/ccdisk.img,format=raw,if=none \
  -device ide-hd,drive=xchg,bus=ide.2 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >$BUILD/cctest2.mon 2>&1

python3 tools/ppm2png.py $BUILD/cctest2.ppm $BUILD/cctest2.png 2>/dev/null

echo "--- the checks ---"
grep -a 'user: \|running an image\|proc: .*ended' $BUILD/serial.log
grep -a 'panic\|exception 1[34]' $BUILD/serial.log | grep -v 0x40337e | head -3
