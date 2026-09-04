#!/bin/sh
# cctest2.sh -- compiler feature checks on the machine.
# - proof.c and words.h arrive on the exchange disk as objects under "the disk"
# - compiled and run through the terminal; each check prints ok or bad

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log
SRC=${1:-tools/cc/proof.c}            # another text rides in under the same name

rm -f $BUILD/teststore.img $LOG $BUILD/ccdisk.img $BUILD/cctest2.ppm
fresh_store $BUILD/teststore.img
dd if=/dev/zero of=$BUILD/ccdisk.img bs=1M count=16 status=none
mkfs.vfat -F 32 $BUILD/ccdisk.img >/dev/null
mcopy -i $BUILD/ccdisk.img "$SRC" ::proof.c
mcopy -i $BUILD/ccdisk.img tools/cc/words.h ::words.h
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go the disk"
    say "compile proof.c"
    keys pause pause
    say "run proof.c code"
    waitlog $LOG 'all checks\|some checks' 30
    sleep 1
    echo "screendump $BUILD/cctest2.ppm"
    sleep 1
    echo "info registers"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -drive id=xchg,file=$BUILD/ccdisk.img,format=raw,if=none \
  -device ide-hd,drive=xchg,bus=ide.2 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG > $BUILD/cctest2-monitor.txt 2>&1

python3 tools/ppm2png.py $BUILD/cctest2.ppm $BUILD/cctest2.png 2>/dev/null

echo "--- the checks, as the program said them ---"
grep -a 'user: \|running an image\|proc: .*ended' $LOG
grep -a 'panic\|exception 1[34]' $LOG | grep -v 0x40337e | head -3
