#!/bin/sh
# look.sh -- boot, send keys through the monitor, save a screenshot.
#   tools/look.sh <output.png> [key ...]
# - key names are QEMU's: tab, down, right, spc, a, b, ...; mouse words: m<dx>,<dy>, click, press, release, w<n> (wheel), home (pointer into the corner)
# - NIC and NETDEV override the card and the network back end; BUILD the directory (tools/testlib.sh)
# - waits for the shell before the first key; the serial log is $BUILD/serial.log

OUT=$1
shift
cd "$(dirname "$0")/.."
. tools/testlib.sh

# The stress and fault targets delete the image on their way out, and
# QEMU without a disk exits before the serial log is opened.
if [ ! -f "$BUILD/esp.img" ]; then
    echo "look.sh: $BUILD/esp.img is missing -- run make first" >&2
    exit 1
fi

rm -f "$BUILD/screen.ppm" "$BUILD/serial.log"
[ -f "$BUILD/teststore.img" ] || fresh_store "$BUILD/teststore.img"
fresh_vars "$BUILD/test-vars.fd"

{
    bootwait $BUILD/serial.log
    for k in "$@"; do
        case "$k" in
            home)    keys corner ;;
            press)   echo "mouse_button 1"; sleep 0.2 ;;
            release) echo "mouse_button 0"; sleep 0.2 ;;
            w-[0-9]*|w[0-9]*)
                # The wheel. qemu's monitor negates the device's sign:
                # w5 scrolls back up, w-5 onward.
                echo "mouse_move 0 0 ${k#w}"; sleep 0.1 ;;
            *)       keys "$k" ;;
        esac
    done
    sleep 1
    echo "screendump $BUILD/screen.ppm"
    sleep 1.5
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device ${NIC:-e1000},netdev=n0 \
  -netdev ${NETDEV:-user,id=n0,hostfwd=udp::7802-:7800} \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

python3 tools/ppm2png.py "$BUILD/screen.ppm" "$OUT"
