#!/bin/sh
# doortest.sh -- the compiler's proof, driven through the door.
#
# The same proof cctest2.sh types on the screen, run over ssh: no
# key timings, no coordinates, and the terminal's own answers come
# back as text. proof.c and words.h ride in on the exchange disk.

cd "$(dirname "$0")/.."
BUILD=build
mkdir -p $BUILD/door
rm -f $BUILD/door/disk.img
dd if=/dev/zero of=$BUILD/door/disk.img bs=1M count=16 status=none
mkfs.vfat -F 32 $BUILD/door/disk.img >/dev/null 2>&1
mcopy -i $BUILD/door/disk.img tools/cc/proof.c ::proof.c
mcopy -i $BUILD/door/disk.img tools/cc/words.h ::words.h

DOOR_EXTRA="-drive id=xchg,file=$BUILD/door/disk.img,format=raw,if=none -device ide-hd,drive=xchg,bus=ide.2"
. tools/doorboot.sh

echo "--- through the door ---"
door_say "go system" "go the disk" "compile proof.c" "run proof.c code"
sleep 4
echo "--- the checks ---"
grep -a 'user: check\|user: all\|user: some' $DOOR_SERIAL | cut -c1-60
door_stop
