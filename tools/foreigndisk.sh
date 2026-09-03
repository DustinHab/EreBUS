#!/bin/sh
# foreigndisk.sh -- a disk that is not ours is left exactly as found.
#
# The store disk is written from sector 1024 up with nothing around
# it, which on a real machine could be somebody's system disk. Here
# the machine is booted with a disk that carries a partition table
# and data in its first sectors. The kernel must refuse it as a
# store, write nothing to it -- the image is compared byte for byte
# afterwards -- and come up without a memory all the same.

cd "$(dirname "$0")/.."
BUILD=build
rm -f $BUILD/foreign.img $BUILD/foreign-serial.log
dd if=/dev/urandom of=$BUILD/foreign.img bs=512 count=2048 status=none
dd if=/dev/zero of=$BUILD/foreign.img bs=1M count=32 seek=1 status=none
printf '\125\252' | dd of=$BUILD/foreign.img bs=1 seek=510 conv=notrunc status=none
before=$(md5sum < $BUILD/foreign.img)
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

{
    # With no store the machine asks during start-up whether it should
    # take a disk. Declining is part of what is being tested here: the
    # offer must be refusable, and refusing it must leave the disk
    # exactly as it was and still bring the machine up.
    sleep 22
    echo "sendkey esc"
    sleep 20
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/foreign.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$BUILD/foreign-serial.log >/dev/null 2>&1

after=$(md5sum < $BUILD/foreign.img)
echo "--- what the machine said ---"
grep -a 'blk:\|snap:\|disk: \|shell: starting' $BUILD/foreign-serial.log | cut -c1-110
echo "--- the checks ---"
ok=1
if grep -aq 'not ours' $BUILD/foreign-serial.log; then echo "the disk was refused"; else echo "FAILED: the disk was not refused"; ok=0; fi
if grep -aq 'a disk can be given to it now' $BUILD/foreign-serial.log; then echo "the machine offered it, and took escape for an answer"; else echo "FAILED: no offer was made"; ok=0; fi
if [ "$before" = "$after" ]; then echo "the disk is byte for byte as it was"; else echo "FAILED: the disk was written to"; ok=0; fi
if grep -aq 'shell: starting' $BUILD/foreign-serial.log; then echo "the machine came up without it"; else echo "FAILED: the machine did not come up"; ok=0; fi
[ $ok = 1 ] && echo "a foreign disk is safe here" || echo "a foreign disk is NOT safe here"
