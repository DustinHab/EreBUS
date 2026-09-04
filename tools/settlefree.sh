#!/bin/sh
# settlefree.sh -- 'settle in the free space of disk N' beside a foreign partition.
# - disk: one foreign partition full of random bytes, free space after it
# - boot from the stick, decline the boot-time offer, settle in the free space, make a text
# - the foreign bytes must be unchanged; a second boot must find the text

cd "$(dirname "$0")/.."
BUILD=build
sh tools/mkusb.sh >/dev/null || exit 1
rm -f $BUILD/free.img $BUILD/free-1.log $BUILD/free-2.log
dd if=/dev/zero of=$BUILD/free.img bs=1M count=256 status=none
sgdisk -o -n 1:2048:+64M -t 1:0700 -c 1:"OTHER" $BUILD/free.img >/dev/null
dd if=/dev/urandom of=$BUILD/free.img bs=1M seek=1 count=64 conv=notrunc status=none
before=$(dd if=$BUILD/free.img bs=1M skip=1 count=64 status=none | md5sum)
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

COMMON="-machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -drive id=stick,file=$BUILD/stick.img,format=raw,if=none \
  -device qemu-xhci,id=xhci -device usb-storage,drive=stick,bus=xhci.0 \
  -drive id=disk,file=$BUILD/free.img,format=raw,if=none \
  -device ide-hd,drive=disk,bus=ide.1"

echo "--- first boot: settling in the free space ---"
{
    # Decline the offer made during start-up: this test is about
    # keeping the partitions that are already there, which is what the
    # words do and what taking the disk whole would not.
    sleep 22
    keys esc
    sleep 12
    keys tab tab tab tab tab pause
    keys s e t t l e spc i n spc t h e spc f r e e spc s p a c e spc o f spc d i s k spc 1 ret pause pause
    keys y e s ret
    sleep 15
    keys m a k e spc t e x t spc k e p t ret pause
    sleep 8
    echo quit
} | qemu-system-x86_64 $COMMON -serial file:$BUILD/free-1.log >/dev/null 2>&1
grep -a 'blk:\|snap:' $BUILD/free-1.log | cut -c1-110

echo "--- the table afterwards ---"
sgdisk -p $BUILD/free.img 2>/dev/null | sed -n '/^Number/,$p'
after=$(dd if=$BUILD/free.img bs=1M skip=1 count=64 status=none | md5sum)

echo "--- second boot: stick and disk again ---"
{
    sleep 24
    echo quit
} | qemu-system-x86_64 $COMMON -serial file:$BUILD/free-2.log >/dev/null 2>&1
grep -a 'blk:\|snap:' $BUILD/free-2.log | cut -c1-110

echo "--- the checks ---"
ok=1
if grep -aq 'blank partition on port 1; it is the store now' $BUILD/free-1.log; then echo "a store was made in the free space"; else echo "FAILED: no store was made"; ok=0; fi
if [ "$before" = "$after" ]; then echo "the other partition is byte for byte as it was"; else echo "FAILED: the other partition changed"; ok=0; fi
if sgdisk -p $BUILD/free.img 2>/dev/null | grep -q 'EREBUS STORE'; then echo "the table shows the store beside it"; else echo "FAILED: no store in the table"; ok=0; fi
if grep -aq 'graph restored from generation' $BUILD/free-2.log; then echo "the second boot found the graph"; else echo "FAILED: the graph was not found"; ok=0; fi
[ $ok = 1 ] && echo "settling in the free space works" || echo "settling in the free space FAILED"
