#!/bin/sh
# nictest.sh -- every cabled NIC family, one boot each: e1000, e1000e, igb, rtl8139; all five boots at once.
# - pass: a DHCP lease arrives (both rings work) and the MAC given on the command line is read back
# - last boot: two Intel cards, one with its link set off; the linked one must be taken

cd "$(dirname "$0")/.."
. tools/testlib.sh
rm -f $BUILD/nic-*.log

# Every boot gets its own copy of the boot image: QEMU locks an image it
# opens for writing, so five machines cannot share one.
common() {                    # common <name>
    echo "$QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/nic-$1-vars.fd \
      -drive format=raw,file=$BUILD/nic-$1-esp.img \
      -drive id=store,file=$BUILD/nic-$1-store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1"
}

prepare() {                   # prepare <name>
    fresh_vars $BUILD/nic-$1-vars.fd
    fresh_store $BUILD/nic-$1-store.img
    cp $BUILD/esp.img $BUILD/nic-$1-esp.img
}

one() {                       # one <name> <qemu model> <mac>
    prepare $1
    { waitlog $BUILD/nic-$1.log 'by lease\|net:  no' 60; sleep 1; echo quit; } | qemu-system-x86_64 $(common $1) \
      -device $2,netdev=n0,mac=$3 -netdev user,id=n0 \
      -serial file:$BUILD/nic-$1.log >/dev/null 2>&1 &
}

one legacy e1000  52:54:00:aa:00:01
one pcie   e1000e 52:54:00:aa:00:02
one queues igb    52:54:00:aa:00:03
one realtek rtl8139 52:54:00:aa:00:04

# Two cards, one cable. The socket on the older-family card is pulled
# before the machine ever looks at it.
prepare choice
{
    echo "set_link n0 off"
    waitlog $BUILD/nic-choice.log 'by lease\|net:  no' 60
    sleep 1
    echo quit
} | qemu-system-x86_64 $(common choice) \
  -device e1000e,netdev=n0,mac=52:54:00:aa:00:05 -netdev user,id=n0 \
  -device igb,netdev=n1,mac=52:54:00:aa:00:06 -netdev user,id=n1 \
  -serial file:$BUILD/nic-choice.log >/dev/null 2>&1 &
wait
rm -f $BUILD/nic-*-esp.img

for n in legacy pcie queues realtek; do
    echo "--- $n ---"
    grep -a 'net:  ' $BUILD/nic-$n.log | cut -c1-112
done
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
