#!/bin/sh
# irqtest.sh -- the devices interrupt instead of being polled, and the processor idles between.
# - boot 1: e1000 (its legacy line through the 8259), ahci and xhci (messages); 'load' twice around a quiet
#   spell must say the processor was idle; the wire must have carried the lease and the usb keyboard the keys
# - boots 2-4: e1000e, igb, rtl8139 -- each attaches its interrupt and gets its lease over it
cd "$(dirname "$0")/.."
. tools/testlib.sh

boot() {   # boot <log> <nic> [extra qemu args]
    LOG=$1; NIC=$2; shift 2
    rm -f $LOG
    fresh_store $BUILD/irq-store.img
    fresh_vars $BUILD/irq-vars.fd
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/irq-vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -drive id=store,file=$BUILD/irq-store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device $NIC,netdev=n0 -netdev user,id=n0 \
      "$@" -serial file:$LOG >/dev/null 2>&1
}

L1=$BUILD/irq-1.log
{
    bootwait $L1
    waitlog $L1 'by lease' 30
    waitlog $L1 'kern: idle' 30
    keys tab tab tab tab tab pause
    say "load"
    sleep 6
    say "load"
    waitcount $L1 'load: idle' 2 10 || sleep 2
    echo quit
} | boot $L1 e1000 -machine q35,i8042=off -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0

lease() {   # lease <log> <nic> [extra]: a boot that only needs its address
    LOG=$1
    { bootwait $LOG; waitlog $LOG 'by lease\|no lease' 40; sleep 1; echo quit; } | boot "$@"
}
lease $BUILD/irq-2.log e1000e
lease $BUILD/irq-3.log igb
lease $BUILD/irq-4.log rtl8139

echo "--- boot 1: the controllers ---"
grep -a 'apic:\|interrupts by\|polled\|no interrupt' $L1 | cut -c1-120
echo "--- boot 1: the load ---"
grep -a 'load: idle' $L1 | cut -c1-140
for b in 2 3 4; do
    echo "--- boot $b ---"
    grep -a 'net:  .*interrupts by\|net:  .*polled\|by lease' $BUILD/irq-$b.log | cut -c1-120
done

echo "--- the checks ---"
ok=1
grep -aq 'apic: local controller' $L1 && echo "the local controller is on" || { echo "FAILED: no local controller"; ok=0; }
grep -aq 'net:  82540em: interrupts by line' $L1 && echo "the e1000 interrupts on its legacy line" || { echo "FAILED: the e1000 has no interrupt"; ok=0; }
grep -aq 'blk:  ahci interrupts by msi' $L1 && echo "the disk controller interrupts by message" || { echo "FAILED: ahci without msi"; ok=0; }
grep -aq 'usb:  interrupts by msi' $L1 && echo "the usb controller interrupts by message" || { echo "FAILED: xhci without msi"; ok=0; }
grep -aq 'by lease' $L1 && echo "the lease came over the interrupt-driven card" || { echo "FAILED: no lease on boot 1"; ok=0; }
grep -aq 'load: idle' $L1 && echo "keys came through the usb keyboard" || { echo "FAILED: no usb keys"; ok=0; }
idle=$(grep -a 'load: idle' $L1 | tail -1 | sed 's/.*idle \([0-9]*\)%.*/\1/')
[ -n "$idle" ] && [ "$idle" -ge 90 ] && echo "the processor was idle $idle% of a quiet spell" || { echo "FAILED: idle share '$idle' below 90%"; ok=0; }
grep -aq 'net:  82574l: interrupts by msi' $BUILD/irq-2.log && grep -aq 'by lease' $BUILD/irq-2.log \
    && echo "the e1000e interrupts by message and got its lease" || { echo "FAILED: e1000e"; ok=0; }
grep -aq 'net:  82576: interrupts by' $BUILD/irq-3.log && grep -aq 'by lease' $BUILD/irq-3.log \
    && echo "the igb interrupts and got its lease" || { echo "FAILED: igb"; ok=0; }
grep -aq 'net:  rtl8139: interrupts by line' $BUILD/irq-4.log && grep -aq 'by lease' $BUILD/irq-4.log \
    && echo "the rtl8139 interrupts on its line and got its lease" || { echo "FAILED: rtl8139"; ok=0; }
[ $ok = 1 ] && echo "the devices interrupt and the processor idles" || echo "interrupts FAILED"
