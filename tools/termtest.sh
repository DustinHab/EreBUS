#!/bin/sh
# termtest.sh -- the terminal through the screen: make a text, write a script, run it, find, journal.
# - proof: the final screenshot and the serial log

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log

rm -f $BUILD/teststore.img $LOG $BUILD/termtest.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "make text greet"
    say "go greet"
    say "write say hello"
    say "back"
    n=$(count $LOG 'user: hello')
    say "run greet"
    waitcount $LOG 'user: hello' $((n + 1)) 20
    say "find hello"
    say "journal"
    sleep 1
    echo "screendump $BUILD/termtest.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/termtest.ppm $BUILD/termtest.png 2>/dev/null

echo "--- what the script said ---"
# The runner pads its words to eight bytes with zeros, so the line
# cannot be matched to its end; the boot-time greeting is the one
# other "hello", and it names its ring.
grep -a 'user: hello' $LOG | grep -v 'ring 3'
