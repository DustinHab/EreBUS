#!/bin/sh
# usbplug.sh -- a mouse unplugged (device_del) and plugged back in (device_add), then used.
# - checks: found twice, "is gone" logged, pointer self-test passed, wheel found in the descriptor

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/plug-serial.log

rm -f $BUILD/plugstore.img $LOG $BUILD/usbplug.ppm
fresh_store $BUILD/plugstore.img
fresh_vars $BUILD/plug-vars.fd

{
    bootwait $LOG
    n=$(count $LOG 'a mouse')
    echo "device_del m1"
    waitlog $LOG 'is gone' 20
    sleep 1
    echo "device_add usb-mouse,bus=xhci.0,id=m1"
    waitcount $LOG 'a mouse' $((n + 1)) 20
    sleep 1
    echo "mouse_move 300 200"; sleep 0.2
    echo "mouse_move 300 200"; sleep 0.2
    echo "mouse_move -100 50";  sleep 0.2
    sleep 1
    echo "screendump $BUILD/usbplug.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE -machine q35,i8042=off \
  -drive if=pflash,format=raw,file=$BUILD/plug-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/plugstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci \
  -device usb-kbd,bus=xhci.0 \
  -device usb-mouse,bus=xhci.0,id=m1 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbplug.ppm $BUILD/usbplug.png 2>/dev/null

echo "--- what the machine said ---"
grep -a 'usb:' $LOG | cut -c1-112
echo "--- the checks ---"
ok=1
[ "$(grep -ac 'a mouse' $LOG)" -ge 2 ] \
  && echo "the mouse was found twice: once at the start, once on being plugged back in" \
  || { echo "FAILED: the mouse was not found again"; ok=0; }
grep -aq 'is gone' $LOG \
  && echo "and the machine noticed it leaving" \
  || { echo "FAILED: unplugging went unnoticed"; ok=0; }
grep -aq 'self test passed -- a pointer' $LOG \
  && echo "the pointer descriptor self test passed" \
  || { echo "FAILED: the pointer descriptor self test"; ok=0; }
grep -aq 'and a wheel' $LOG \
  && echo "the mouse described a wheel, and it was found" \
  || { echo "FAILED: no wheel was found in the mouse's description"; ok=0; }
[ $ok = 1 ] && echo "a mouse can be unplugged and plugged back in" || echo "plugging back in FAILED"
