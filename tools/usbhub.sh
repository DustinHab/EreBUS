#!/bin/sh
# usbhub.sh -- keyboard and mouse behind a USB hub, i8042 off.
# - hub on the root port, keyboard on hub port 1, mouse on hub port 2
# - a text is typed and run; the mouse is moved; screenshot at the end

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/hub-serial.log

rm -f $BUILD/hubstore.img $LOG $BUILD/usbhub.ppm
fresh_store $BUILD/hubstore.img
fresh_vars $BUILD/hub-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "make text hub"
    say "go hub"
    say "write say through"
    say "back"
    say "run hub"
    waitlog $LOG 'user: through' 20
    keys m300,200 m300,200 m-100,50
    sleep 1
    echo "screendump $BUILD/usbhub.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE -machine q35,i8042=off \
  -drive if=pflash,format=raw,file=$BUILD/hub-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/hubstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -device qemu-xhci,id=xhci \
  -device usb-hub,id=hub,bus=xhci.0,port=1 \
  -device usb-kbd,bus=xhci.0,port=1.1 \
  -device usb-mouse,bus=xhci.0,port=1.2 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/usbhub.ppm $BUILD/usbhub.png 2>/dev/null

echo "--- the controller, the hub, and what hangs off it ---"
grep -a 'usb:\|ps2:' $LOG | cut -c1-110
echo "--- the checks ---"
ok=1
grep -aq 'a hub' $LOG && echo "the hub was found and taken on" || { echo "FAILED: no hub"; ok=0; }
grep -aq 'through a hub on .*a keyboard' $LOG && echo "a keyboard was reached behind it" || { echo "FAILED: no keyboard behind the hub"; ok=0; }
grep -aq 'through a hub on .*a mouse' $LOG && echo "a mouse was reached behind it" || { echo "FAILED: no mouse behind the hub"; ok=0; }
grep -aq 'user: through' $LOG && echo "the keys arrived: a text was typed and run" || { echo "FAILED: nothing was typed through"; ok=0; }
[ $ok = 1 ] && echo "keyboard and mouse behind a hub work" || echo "the hub path FAILED"
