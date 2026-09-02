#!/bin/sh
# selfbuild-machine.sh -- the kernel, built ON the machine.
#
# Every kernel source and header rides in on the exchange disk. The
# terminal takes them in as "the disk", builds the list -- every c
# text through the machine's compiler, every .S through its gnu
# translation, all of it through its linker in the kernel's shape --
# and writes kernel.elf back out. The host then boots what the machine
# made. That is the whole distance: a kernel that builds itself.
#
# The build runs under emulation and takes minutes; the script waits
# for the kernel's own report on the serial line.

cd "$(dirname "$0")/.."
BUILD=build
OUT=$BUILD/self-machine
mkdir -p $OUT

rm -f $BUILD/teststore.img $BUILD/serial.log $OUT/disk.img
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
dd if=/dev/zero of=$OUT/disk.img bs=1M count=32 status=none
mkfs.vfat -F 32 $OUT/disk.img >/dev/null
for f in kernel/*.c kernel/*/*.c kernel/*/*/*.c kernel/arch/x86_64/*.S kernel/user/*.S \
         kernel/include/eb/*.h kernel/net/gf25519.h kernel/gfx/*.h common/*.h; do
    [ -f "$f" ] && mcopy -i $OUT/disk.img "$f" ::"$(basename "$f")"
done
echo "sources on the disk: $(mdir -i $OUT/disk.img :: | tail -2 | head -1)"
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

{
    sleep 30
    keys tab tab tab tab tab pause
    keys g o spc s y s t e m ret pause
    keys b u i l d spc t h e spc d i s k ret
    # wait for the kernel's report, up to the limit
    n=0
    while [ $n -lt ${LIMIT:-900} ]; do
        sleep 5; n=$((n + 5))
        grep -a -q 'build: ' $BUILD/serial.log 2>/dev/null && break
        # a fault in the kernel itself ends the wait; the one a ring-3
        # program provokes on purpose at boot does not
        grep -a -q 'panic\|exception .* at 0xffffffff' $BUILD/serial.log 2>/dev/null && break
    done
    sleep 4
    keys w r i t e spc o u t spc t h e spc d i s k ret
    # writing 1.5 MiB through a polled disk one sector at a time takes
    # a while under emulation; the kernel says when it is done
    n=0
    while [ $n -lt 240 ]; do
        sleep 5; n=$((n + 5))
        grep -a -q 'fat:  wrote' $BUILD/serial.log 2>/dev/null && break
    done
    sleep 3
    echo "screendump $OUT/screen.ppm"
    sleep 2
    echo "info registers"
    sleep 1
    echo quit
} | qemu-system-x86_64 -machine q35 -m 1024M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -drive id=xchg,file=$OUT/disk.img,format=raw,if=none \
  -device ide-hd,drive=xchg,bus=ide.2 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >$OUT/monitor.log 2>&1

python3 tools/ppm2png.py $OUT/screen.ppm $OUT/screen.png 2>/dev/null
echo "--- the machine said ---"
grep -a 'fat: \|build: \|snap: \|panic\|kern: exception' $BUILD/serial.log | cut -c1-140 | tail -12
grep -a -E '^RIP=|^RAX=' $OUT/monitor.log | head -2
rm -f $OUT/kernel.elf
mcopy -i $OUT/disk.img ::KERNEL.ELF $OUT/kernel.elf 2>/dev/null && ls -l $OUT/kernel.elf
