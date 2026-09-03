#!/bin/sh
# usbplug.sh -- a mouse pulled out and put back in.
#
# A machine that only finds what was already plugged in when it started
# is a machine you have to restart to change a mouse. The port watch
# has always been there; what it lacked was the pause the standard asks
# for between a plug touching its socket and the port being reset. A
# device is not ready to answer the instant it is electrically present,
# and one asked too early answers nothing -- which on an emulator never
# shows, because there the device is ready before the plug is in.
#
# So: boot with a keyboard and a mouse, take the mouse away, put it
# back, and then use it. The pointer moving at the end is the proof
# that the second enumeration produced a working device and not merely
# a hopeful log line.

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/plugstore.img $BUILD/plug-serial.log $BUILD/usbplug.ppm
dd if=/dev/zero of=$BUILD/plugstore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/plug-vars.fd

{
    sleep 24
    echo "device_del m1"
    sleep 6
    echo "device_add usb-mouse,bus=xhci.0,id=m1"
    sleep 8
    echo "mouse_move 300 200"; sleep 0.2
    echo "mouse_move 300 200"; sleep 0.2
    echo "mouse_move -100 50";  sleep 0.2
    sleep 2
    echo "screendump $BUILD/usbplug.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35,i8042=off -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/plug-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/plugstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci \
  -device usb-kbd,bus=xhci.0 \
  -device usb-mouse,bus=xhci.0,id=m1 \
  -display none -monitor stdio \
  -serial file:$BUILD/plug-serial.log >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbplug.ppm $BUILD/usbplug.png 2>/dev/null

echo "--- what the machine said ---"
grep -a 'usb:' $BUILD/plug-serial.log | cut -c1-112
echo "--- the checks ---"
ok=1
[ "$(grep -ac 'a mouse' $BUILD/plug-serial.log)" -ge 2 ] \
  && echo "the mouse was found twice: once at the start, once on being plugged back in" \
  || { echo "FAILED: the mouse was not found again"; ok=0; }
grep -aq 'is gone' $BUILD/plug-serial.log \
  && echo "and the machine noticed it leaving" \
  || { echo "FAILED: unplugging went unnoticed"; ok=0; }
grep -aq 'self test passed -- a pointer' $BUILD/plug-serial.log \
  && echo "and a pointer's own layout is read as written, wheel and all" \
  || { echo "FAILED: the reading of a pointer's layout is wrong"; ok=0; }
grep -aq 'and a wheel' $BUILD/plug-serial.log \
  && echo "the mouse that is here described a wheel, and it was found" \
  || { echo "FAILED: no wheel was found in the mouse's description"; ok=0; }
[ $ok = 1 ] && echo "a mouse can be unplugged and plugged back in" || echo "plugging back in FAILED"
