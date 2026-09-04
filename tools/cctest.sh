#!/bin/sh
# cctest.sh -- C compiler on the machine, through the screen's terminal.
# - compiles "the compiler" page, runs the image, checks hello and the sum
# - compiles a text with an error, checks the reported line

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log

rm -f $BUILD/teststore.img $LOG $BUILD/cctest.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go aside"
    say "compile the compiler"
    keys pause pause
    say "run the compiler code"
    waitlog $LOG 'user: 5050' 20
    say "back"
    say "make text bad"
    say "go bad"
    say "write long main() { return x; }"
    say "back"
    say "compile bad"
    say "journal"
    sleep 1
    echo "screendump $BUILD/cctest.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/cctest.ppm $BUILD/cctest.png 2>/dev/null

echo "--- compiled and run ---"
grep -a 'running an image\|user: hello from c\|user: 5050' $LOG
