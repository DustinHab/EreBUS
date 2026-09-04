#!/bin/sh
# foreigndisk.sh -- a disk with a partition table and data is never written.
# - boot beside such a disk; the kernel must refuse it as a store
# - the boot-time offer is declined with escape; the machine must still come up
# - the image is compared byte for byte afterwards

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/foreign-serial.log
rm -f $BUILD/foreign.img $LOG
dd if=/dev/urandom of=$BUILD/foreign.img bs=512 count=2048 status=none
dd if=/dev/zero of=$BUILD/foreign.img bs=1M count=32 seek=1 status=none
printf '\125\252' | dd of=$BUILD/foreign.img bs=1 seek=510 conv=notrunc status=none
before=$(md5sum < $BUILD/foreign.img)
fresh_vars $BUILD/test-vars.fd

{
    # With no store the machine asks during start-up whether it should
    # take a disk. Declining is part of what is being tested here: the
    # offer must be refusable, and refusing it must leave the disk
    # exactly as it was and still bring the machine up.
    waitlog $LOG 'disk: >' 60
    sleep 1
    echo "sendkey esc"
    bootwait $LOG
    sleep 2
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/foreign.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG >/dev/null 2>&1

after=$(md5sum < $BUILD/foreign.img)
echo "--- what the machine said ---"
grep -a 'blk:\|snap:\|disk: \|shell: starting' $LOG | cut -c1-110
echo "--- the checks ---"
ok=1
if grep -aq 'not ours' $LOG; then echo "the disk was refused"; else echo "FAILED: the disk was not refused"; ok=0; fi
if grep -aq 'a disk can be given to it now' $LOG; then echo "the machine offered it, and took escape for an answer"; else echo "FAILED: no offer was made"; ok=0; fi
if [ "$before" = "$after" ]; then echo "the disk is byte for byte as it was"; else echo "FAILED: the disk was written to"; ok=0; fi
if grep -aq 'shell: starting' $LOG; then echo "the machine came up without it"; else echo "FAILED: the machine did not come up"; ok=0; fi
[ $ok = 1 ] && echo "a foreign disk is safe here" || echo "a foreign disk is NOT safe here"
