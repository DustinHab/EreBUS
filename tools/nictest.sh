#!/bin/sh
# nictest.sh -- every cabled card this system drives, one boot each.
#
# A card is working when an address arrives by lease: the machine put a
# request on the wire and read the answer back off it, which is both
# rings proven in one go. Each card is also given an address of its own
# on the command line, so the log shows whether the driver read the
# card's name out of the right register rather than a plausible wrong
# one.
#
# The last boot is the case a real desktop board actually presents: two
# Intel cards of different families, and one cable. The card with the
# cable in it is the one worth having, and the machine has to choose it
# without being told which.

cd "$(dirname "$0")/.."
BUILD=build
rm -f $BUILD/nic-*.log

COMMON="-machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/nic-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/nicstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -display none -monitor stdio"

one() {                       # one <name> <qemu model> <mac>
    cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/nic-vars.fd
    rm -f $BUILD/nicstore.img
    dd if=/dev/zero of=$BUILD/nicstore.img bs=1M count=32 status=none
    { sleep 26; echo quit; } | qemu-system-x86_64 $COMMON \
      -device $2,netdev=n0,mac=$3 -netdev user,id=n0 \
      -serial file:$BUILD/nic-$1.log >/dev/null 2>&1
    echo "--- $1 ($2, $3) ---"
    grep -a 'net:  ' $BUILD/nic-$1.log | cut -c1-112
}

one legacy e1000  52:54:00:aa:00:01
one pcie   e1000e 52:54:00:aa:00:02
one queues igb    52:54:00:aa:00:03
one realtek rtl8139 52:54:00:aa:00:04

# Two cards, one cable. The socket on the older-family card is pulled
# before the machine ever looks at it.
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/nic-vars.fd
rm -f $BUILD/nicstore.img
dd if=/dev/zero of=$BUILD/nicstore.img bs=1M count=32 status=none
{
    echo "set_link n0 off"
    sleep 26
    echo quit
} | qemu-system-x86_64 $COMMON \
  -device e1000e,netdev=n0,mac=52:54:00:aa:00:05 -netdev user,id=n0 \
  -device igb,netdev=n1,mac=52:54:00:aa:00:06 -netdev user,id=n1 \
  -serial file:$BUILD/nic-choice.log >/dev/null 2>&1
echo "--- two cards, one cable ---"
grep -a 'net:  ' $BUILD/nic-choice.log | cut -c1-112

echo "--- the checks ---"
ok=1
check() {                     # check <name> <what the log must say> <words>
    if grep -aq "$2" $BUILD/nic-$1.log; then echo "$3"; else echo "FAILED: $3"; ok=0; fi
}
check legacy  '82540em at'          "the 8254x family was found and named"
check legacy  'aa:00:01'            "and read its own address"
check legacy  'by lease'            "and carried a request and an answer"
check pcie    '82574l at'           "the pci express family was found and named"
check pcie    'aa:00:02'            "and read its own address"
check pcie    'by lease'            "and carried a request and an answer"
check queues  '82576 at'            "the many-queue family was found and named"
check queues  'aa:00:03'            "and read its own address"
check queues  'by lease'            "and carried a request and an answer"
check realtek 'rtl8139'             "the realtek card still works"
check realtek 'by lease'            "and still carries traffic"
check choice  '82576 at'            "with two cards and one cable, the cabled one was taken"
check choice  'aa:00:06'            "and it is the one the cable is in"
check choice  'by lease'            "and it carried a request and an answer"
if grep -aq '82574l at' $BUILD/nic-choice.log; then echo "FAILED: the unplugged card was taken"; ok=0; else echo "the unplugged card was passed over"; fi
[ $ok = 1 ] && echo "every cabled card works" || echo "a cabled card FAILED"
