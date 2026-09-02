#!/bin/sh
# selfbuild.sh -- the kernel, built with the machine's own tools on
# the host, and booted.
#
# Every kernel source goes through cchost: c through the compiler and
# the assembler, .S through the gnu translation, and all the objects
# through the linker in the kernel's shape. The result replaces
# kernel.elf on a copy of the boot disk, and QEMU boots it; what the
# kernel says on the serial line is the measure of how far the tools
# carry.
#
#   selfbuild.sh                 build and boot
#   selfbuild.sh build           build only
#   KERNEL=path selfbuild.sh     boot that kernel.elf instead (one the
#                                machine itself made, say)
#   TRACE=1                      QEMU logs every exception, stops at a reset

cd "$(dirname "$0")/.."
BUILD=build
OUT=$BUILD/self
mkdir -p $OUT

if [ -n "$KERNEL" ]; then
    cp "$KERNEL" $OUT/kernel.elf
    echo "booting $KERNEL"
else
    make -s cchost >/dev/null 2>&1 || { echo "cchost does not build"; exit 1; }
    rm -f $OUT/*.obj $OUT/*.asm $OUT/kernel.elf

    HEADERS="kernel/include/eb/*.h kernel/net/gf25519.h kernel/gfx/*.h common/*.h"
    CS=$(ls kernel/*.c kernel/*/*.c kernel/*/*/*.c 2>/dev/null)
    SS=$(ls kernel/arch/x86_64/*.S kernel/user/*.S)

    fails=0
    objs=""
    for f in $CS; do
        o=$OUT/$(basename "$f" .c).obj
        r=$(./build/cchost cc "$f" -o "$o" $HEADERS 2>&1 | tail -1)
        case "$r" in
            ok:*) objs="$objs $o" ;;
            *) echo "FAILED  $f: $r"; fails=$((fails + 1)) ;;
        esac
    done
    for f in $SS; do
        o=$OUT/$(basename "$f" .S)_S.obj
        r=$(./build/cchost gnu "$f" -o "$o" 2>&1 | tail -1)
        case "$r" in
            ok:*) objs="$objs $o" ;;
            *) echo "FAILED  $f: $r"; fails=$((fails + 1)) ;;
        esac
    done
    if [ $fails -ne 0 ]; then echo "$fails sources did not become objects"; exit 1; fi

    r=$(./build/cchost ld $OUT/kernel.elf kernel $objs 2>&1 | tail -1)
    echo "link: $r"
    case "$r" in ok:*) ;; *) exit 1 ;; esac
    ls -l $OUT/kernel.elf
    [ "$1" = "build" ] && exit 0
fi

# a boot disk with the self-built kernel on it
cp $BUILD/esp.img $OUT/esp.img
mcopy -o -i $OUT/esp.img $OUT/kernel.elf ::/erebus/kernel.elf
rm -f $OUT/store.img $OUT/serial.log $OUT/screen.ppm
dd if=/dev/zero of=$OUT/store.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $OUT/vars.fd

# TRACE=1 asks QEMU for every exception and stops at the first reset,
# so a triple fault says where it began. QEMU_EXTRA adds whatever else
# a measurement needs, -icount for instance.
TRACEOPTS="$QEMU_EXTRA"
if [ -n "$TRACE" ]; then TRACEOPTS="$TRACEOPTS -d int,cpu_reset -D $OUT/int.log -no-reboot -no-shutdown"; rm -f $OUT/int.log; fi

{
    sleep ${WAIT:-30}
    echo "screendump $OUT/screen.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max $TRACEOPTS \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$OUT/vars.fd \
  -drive format=raw,file=$OUT/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$OUT/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$OUT/serial.log >/dev/null 2>&1

python3 tools/ppm2png.py $OUT/screen.ppm $OUT/screen.png 2>/dev/null
echo "--- the self-built kernel said ---"
cut -c1-150 $OUT/serial.log | grep -a -v '^\[2J' | head -${LINES:-40}
