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
grep -a 'user: check\|all checks\|some checks\|running an image\|proc: .*ended' $LOG | cut -c1-100
grep -a 'panic\|exception 1[34]' $LOG | grep -v 0x40337e | head -3
# The test must be able to fail: a proof that never ran (the disk not
# mounted, the compile refused) said nothing and was counted green until
# 0.9.9. Every check must have been said, and none may be bad.
ok=1
grep -aq 'running an image' $LOG && echo "proof.c was compiled on the machine and run" || { echo "FAILED: proof.c did not compile or run on the machine"; ok=0; }
grep -aq 'user: all checks ok' $LOG && echo "every check said ok" || { echo "FAILED: the checks did not all say ok"; ok=0; }
grep -a 'user: check' $LOG | grep -q ' bad' && { echo "FAILED: a check said bad"; ok=0; }
[ "$(count $LOG 'user: check')" -ge 28 ] && echo "and there were $(count $LOG 'user: check') of them" || { echo "FAILED: only $(count $LOG 'user: check') checks were said"; ok=0; }
[ $ok = 1 ] && echo "the compiler's proof holds on the machine" || echo "the compiler's proof FAILED"
