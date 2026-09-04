#!/bin/sh
# usbhub.sh -- keyboard and mouse behind a USB hub, i8042 off.
# - hub on the root port, keyboard on hub port 1, mouse on hub port 2
# - a text is typed and run; the mouse is moved; screenshot at the end

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/hubstore.img $BUILD/hub-serial.log $BUILD/usbhub.ppm
dd if=/dev/zero of=$BUILD/hubstore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/hub-vars.fd

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
    keys m a k e spc t e x t spc h u b ret pause
    keys g o spc h u b ret pause
    keys w r i t e spc s a y spc t h r o u g h ret pause
    keys b a c k ret pause
    keys r u n spc h u b ret
    sleep 8
    keys m300,200 m300,200 m-100,50
    sleep 2
    echo "screendump $BUILD/usbhub.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35,i8042=off -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/hub-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/hubstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci \
  -device usb-hub,id=hub,bus=xhci.0,port=1 \
  -device usb-kbd,bus=xhci.0,port=1.1 \
  -device usb-mouse,bus=xhci.0,port=1.2 \
  -display none -monitor stdio \
  -serial file:$BUILD/hub-serial.log >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbhub.ppm $BUILD/usbhub.png 2>/dev/null

echo "--- the controller, the hub, and what hangs off it ---"
grep -a 'usb:\|ps2:' $BUILD/hub-serial.log | cut -c1-110
echo "--- the checks ---"
ok=1
grep -aq 'a hub' $BUILD/hub-serial.log && echo "the hub was found and taken on" || { echo "FAILED: no hub"; ok=0; }
grep -aq 'through a hub on .*a keyboard' $BUILD/hub-serial.log && echo "a keyboard was reached behind it" || { echo "FAILED: no keyboard behind the hub"; ok=0; }
grep -aq 'through a hub on .*a mouse' $BUILD/hub-serial.log && echo "a mouse was reached behind it" || { echo "FAILED: no mouse behind the hub"; ok=0; }
grep -aq 'user: through' $BUILD/hub-serial.log && echo "the keys arrived: a text was typed and run" || { echo "FAILED: nothing was typed through"; ok=0; }
[ $ok = 1 ] && echo "keyboard and mouse behind a hub work" || echo "the hub path FAILED"
