#!/bin/sh
# bigpersist.sh -- big objects across a boot.
#
# Three files ride in on the exchange disk: a source of some 150 KiB, a
# header below the size at which payloads leave the generation, and a
# kernel image of two megabytes. The first boot takes them in and
# writes a generation; the second boot has no exchange disk at all and
# must still hold all three, byte for byte in length, read through the
# door.

cd "$(dirname "$0")/.."
BUILD=build
mkdir -p $BUILD/big
rm -f $BUILD/big/disk.img
dd if=/dev/zero of=$BUILD/big/disk.img bs=1M count=16 status=none
mkfs.vfat -F 32 $BUILD/big/disk.img >/dev/null 2>&1
mcopy -i $BUILD/big/disk.img kernel/lang/cc.c ::cc.c
mcopy -i $BUILD/big/disk.img kernel/include/eb/blob.h ::blob.h
mcopy -i $BUILD/big/disk.img build/kernel.elf ::kernel.elf

DOOR_EXTRA="-drive id=xchg,file=$BUILD/big/disk.img,format=raw,if=none -device ide-hd,drive=xchg,bus=ide.2"
. tools/doorboot.sh

echo "--- first boot: taken in ---"
door_say "go system" "go the disk" "look"
n=0
while [ $n -lt 60 ]; do
    grep -a -q 'snap: generation' $DOOR_SERIAL && break
    sleep 2; n=$((n + 2))
done
sleep 2
grep -a 'fat:  took\|snap:\|blob:' $DOOR_SERIAL | cut -c1-100
door_stop
cp $DOOR_SERIAL $BUILD/big/serial-1.log

echo "--- second boot: no exchange disk ---"
DOOR_EXTRA=""
DOOR_KEEP=1
. tools/doorboot.sh
grep -a 'snap: graph restored\|blob:' $DOOR_SERIAL | cut -c1-100
door_say "go system" "go the disk" "look" "read cc.c" "read blob.h" "read kernel.elf" > $BUILD/big/answer.txt
door_stop

echo "--- the checks ---"
ok=1
want=$(wc -c < kernel/lang/cc.c)
more=$(grep -a -o 'and [0-9]* more letters' $BUILD/big/answer.txt | head -1 | tr -dc '0-9')
got=$((2000 + ${more:-0}))
if [ "$got" = "$want" ]; then echo "cc.c: $got letters, as long as the source"; else echo "cc.c: $got letters, but the source has $want"; ok=0; fi
if grep -q -F "$(sed -n '2p' kernel/lang/cc.c)" $BUILD/big/answer.txt; then echo "cc.c: its second line came back"; else echo "cc.c: its second line is missing"; ok=0; fi
if grep -q -F 'bool blob_compact(const u8 *live, u32 count);' $BUILD/big/answer.txt; then echo "blob.h: its lines came back"; else echo "blob.h: not read back"; ok=0; fi
kwant=$(wc -c < build/kernel.elf)
kgot=$(grep -a -o '[0-9]* bytes; the first rows' $BUILD/big/answer.txt | head -1 | tr -dc '0-9')
if [ "$kgot" = "$kwant" ]; then echo "kernel.elf: $kgot bytes, as many as went in"; else echo "kernel.elf: $kgot bytes, but $kwant went in"; ok=0; fi
if grep -a -q 'blob: [0-9]* big objects in the log' $DOOR_SERIAL; then echo "the log was read at boot"; else echo "no log line at boot"; ok=0; fi
[ $ok = 1 ] && echo "big objects survive the boot" || echo "something did not come back"
