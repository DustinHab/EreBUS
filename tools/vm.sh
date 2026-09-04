#!/bin/sh
# vm.sh -- the live VM in a window; one instance, a new start replaces the old.
#   tools/vm.sh          start (or restart) with the existing store
#   tools/vm.sh fresh    start with an empty store
cd "$(dirname "$0")/.."

if [ ! -f build/esp.img ]; then
    echo "vm.sh: build/esp.img is missing -- run make first" >&2
    exit 1
fi

pkill -f qemu-system-x86_64 2>/dev/null
sleep 1

[ "$1" = "fresh" ] && rm -f build/store.img
[ -f build/store.img ] || dd if=/dev/zero of=build/store.img \
    bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd build/OVMF_VARS.fd

# The display is asked for by name so a failure says why, instead of a
# silently headless machine. SDL rather than GTK: under WSLg the GTK
# backend opens its window and then never paints into it.
exec qemu-system-x86_64 \
  -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=build/OVMF_VARS.fd \
  -drive format=raw,file=build/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=build/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -net none \
  -display sdl \
  -name "EreBUS" \
  -serial file:build/vm-serial.log
