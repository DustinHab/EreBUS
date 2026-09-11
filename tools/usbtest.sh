#!/bin/sh
# usbtest.sh -- USB keyboard and mouse on xHCI, i8042 off.
# - keys can only arrive over usb; a text is made and run; the mouse is moved; screenshot at the end

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log

rm -f $BUILD/teststore.img $LOG $BUILD/usbtest.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "make text greet"
    say "go greet"
    say "write say hello"
    say "back"
    n=$(count $LOG 'user: hello')
    say "run greet"
    waitcount $LOG 'user: hello' $((n + 1)) 20
    keys m300,200 m300,200 m-100,50
    sleep 1
    echo "screendump $BUILD/usbtest.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE -machine q35,i8042=off \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbtest.ppm $BUILD/usbtest.png 2>/dev/null

echo "--- the controller and its devices ---"
grep -a 'usb:\|ps2:' $LOG | cut -c1-110
echo "--- what the script said, typed by usb ---"
grep -a 'user: hello' $LOG | grep -v 'ring 3'
# The test must be able to fail (0.9.9): both devices found on the
# controller, no ps/2 to fall back on, and the words arrived.
ok=1
grep -aq 'ps2:  no controller answers' $LOG && echo "no ps/2 controller, as arranged" || { echo "FAILED: a ps/2 controller answered"; ok=0; }
grep -aq 'usb:  1 keyboard and 1 mouse' $LOG && echo "the keyboard and the mouse were found on xhci" || { echo "FAILED: the usb devices were not found"; ok=0; }
grep -a 'user: hello' $LOG | grep -v 'ring 3' | grep -q . && echo "and the keys typed over usb ran the script" || { echo "FAILED: the script's hello never came"; ok=0; }
[ $ok = 1 ] && echo "usb input works with the i8042 off" || echo "usb input FAILED"
