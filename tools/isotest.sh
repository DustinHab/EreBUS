#!/bin/sh
# isotest.sh -- build/erebus.iso booted as a cd and as a raw usb stick.
# - both must reach the shell without a store and offer to settle

cd "$(dirname "$0")/.."
BUILD=build
sh tools/mkiso.sh >/dev/null || exit 1
[ -f $BUILD/erebus.iso ] || { echo "no iso"; exit 1; }

boot() {
    cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
    { sleep ${WAIT:-26}; echo quit; } | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
      -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -vga none -device VGA,edid=on,xres=1280,yres=800 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -display none -monitor stdio "$@" >/dev/null 2>&1
}

echo "--- as a cd ---"
rm -f $BUILD/iso-cd.log
boot -drive id=cd,file=$BUILD/erebus.iso,media=cdrom,if=none,format=raw \
     -device ide-cd,drive=cd,bus=ide.2 -serial file:$BUILD/iso-cd.log
grep -a 'boot: the loader\|blk:\|shell: starting\|kern: idle' $BUILD/iso-cd.log | cut -c1-100

echo "--- as a usb stick ---"
rm -f $BUILD/iso-usb.log
boot -drive id=stick,file=$BUILD/erebus.iso,format=raw,if=none \
     -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
     -serial file:$BUILD/iso-usb.log
grep -a 'boot: the loader\|blk:\|shell: starting\|kern: idle' $BUILD/iso-usb.log | cut -c1-100

echo "--- the checks ---"
ok=1
grep -aq 'shell: starting' $BUILD/iso-cd.log  && echo "the cd boots to the shell"        || { echo "FAILED: the cd did not boot"; ok=0; }
grep -aq 'shell: starting' $BUILD/iso-usb.log && echo "the usb stick boots to the shell" || { echo "FAILED: the stick did not boot"; ok=0; }
[ $ok = 1 ] && echo "the iso works both ways" || echo "the iso FAILED"
