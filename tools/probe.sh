#!/bin/sh
# probe.sh -- one boot, one screendump, monitor output shown (test-rig diagnosis).
cd "$(dirname "$0")/.."
cp /usr/share/OVMF/OVMF_VARS_4M.fd build/OVMF_VARS.fd
rm -f build/screen.ppm
{ sleep 9; echo "screendump build/screen.ppm"; sleep 2; echo quit; } | \
  qemu-system-x86_64 -machine q35 -m 512M -cpu max \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=build/OVMF_VARS.fd \
    -drive format=raw,file=build/esp.img \
    -vga none -device VGA,edid=on,xres=1280,yres=800 \
    -drive id=store,file=build/store.img,format=raw,if=none \
    -device ide-hd,drive=store,bus=ide.1 \
    -net none -display none -monitor stdio \
    -serial file:build/serial.log 2>&1 | tail -6
ls -la build/screen.ppm
