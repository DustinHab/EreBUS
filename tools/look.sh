#!/bin/sh
# look.sh -- boot, press a few keys, photograph the screen.
#
# The shell has three ways of looking at the same state and they are
# reached by pressing keys, so seeing them from outside means sending
# keys from outside. A script rather than a makefile target because the
# arguments survive being passed through several shells this way.
#
#   tools/look.sh <output.png> [key ...]
#
# Key names are QEMU's: tab, down, right, spc, a, b, ...

set -e
OUT=$1
shift

BUILD=build
QEMU="qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/OVMF_VARS.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -net none -display none -monitor stdio"

rm -f "$BUILD/screen.ppm"

{
    sleep 9
    for k in "$@"; do
        echo "sendkey $k"
        sleep 0.3
    done
    sleep 1
    echo "screendump $BUILD/screen.ppm"
    sleep 2
    echo quit
} | $QEMU -serial file:$BUILD/serial.log >/dev/null 2>&1 || true

python3 tools/ppm2png.py "$BUILD/screen.ppm" "$OUT"
