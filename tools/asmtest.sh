#!/bin/sh
# asmtest.sh -- assembler on the machine, through the screen's terminal.
# - assembles "the machine" page, runs the image, checks its output
# - assembles a text with an error, checks the reported line
# - second boot on the same store: the built program must return

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log
LOG2=$BUILD/serial2.log

rm -f $BUILD/teststore.img $LOG $LOG2 $BUILD/asmtest.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

boot() {
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
      -drive format=raw,file=$BUILD/esp.img \
      -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -serial file:$1 >/dev/null 2>&1
}

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go aside"
    say "assemble the machine"
    keys pause
    say "run the machine code"
    waitlog $LOG 'user: hello from the m' 20
    say "back"
    say "make text bad"
    say "go bad"
    say "write movv rax, 1"
    say "back"
    say "assemble bad"
    say "journal"
    sleep 1
    echo "screendump $BUILD/asmtest.ppm"
    sleep 1
    echo quit
} | boot $LOG

python3 tools/ppm2png.py $BUILD/asmtest.ppm $BUILD/asmtest.png 2>/dev/null

echo "--- first boot: built and run ---"
grep -a 'running an image\|user: hello from' $LOG

# The second boot: nothing typed. The image the person made is part
# of the world that comes back.
{
    bootwait $LOG2
    waitlog $LOG2 'running an image' 20
    sleep 1
    echo quit
} | boot $LOG2

echo "--- second boot: came back on its own ---"
grep -a 'restored\|running an image\|user: hello from' $LOG2
