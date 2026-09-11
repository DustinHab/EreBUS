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
    # a text that reads as a page -- an address on its first line, markup
    # after -- is shown through the html lens in the focus view: the
    # renderer program lays it out, in ring 3, and is ended after
    say "make text page"
    say "go page"
    say "write example.org/page"
    say "write <html><body><h1>a page</h1><p>laid out by a program</p></body></html>"
    keys tab tab pause
    waitlog $LOG '(renderer) ended' 20
    keys tab tab tab tab tab pause
    say "back"
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
# The test must be able to fail (0.9.9): the script's hello must be there.
if grep -a 'user: hello' $LOG | grep -v 'ring 3' | grep -q .; then echo "the text was made, written, run and heard"
else echo "FAILED: the script's hello never came"; fi
grep -aq '(renderer) ended; all capabilities released' $LOG && echo "a text that reads as a page was laid out by the renderer program in ring 3" || echo "FAILED: the html lens started no renderer"
