#!/bin/sh
# install-test.sh -- 'install' from inside the system, and the loader's fallback.
# - part 1: the self-built kernel (tools/selfbuild.sh) arrives on the exchange disk, is installed, restarted; the next start says it was built here
# - part 2: a kernel whose first instruction is ud2 is installed; after two failed starts (system_reset via the monitor) the loader restores kernel.old
# - both run on a copy of the boot disk

cd "$(dirname "$0")/.."
BUILD=build
OUT=$BUILD/install
mkdir -p $OUT
make -s cchost >/dev/null 2>&1
[ -f $BUILD/self/kernel.elf ] || sh tools/selfbuild.sh build >/dev/null 2>&1 || { echo "no self-built kernel"; exit 1; }

# a kernel that does not come up
printf 'section text\n_start:\n    ud2\n' > $OUT/broken.s
./build/cchost as $OUT/broken.s -o $OUT/broken.obj >/dev/null
./build/cchost ld $OUT/broken.elf kernel $OUT/broken.obj >/dev/null || { echo "no broken kernel"; exit 1; }

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

# waits up to $2 seconds for the pattern $1 to appear $3 times in the log
await() {
    n=0
    while [ $n -lt $2 ]; do
        sleep 3; n=$((n + 3))
        [ "$(grep -a -c "$1" $LOG 2>/dev/null)" -ge "$3" ] && return 0
    done
    return 1
}

# one run: the exchange disk carries $1 as kernel.elf; the terminal
# installs it and restarts. $2 says how many times the firmware's halt
# is to be answered with a reset before the outcome is awaited.
run() {
    LOG=$OUT/serial-$3.log
    rm -f $OUT/esp.img $OUT/store.img $OUT/disk.img $LOG
    cp $BUILD/esp.img $OUT/esp.img
    dd if=/dev/zero of=$OUT/store.img bs=1M count=32 status=none
    dd if=/dev/zero of=$OUT/disk.img bs=1M count=16 status=none
    mkfs.vfat -F 32 $OUT/disk.img >/dev/null 2>&1
    mcopy -i $OUT/disk.img "$1" ::kernel.elf
    cp /usr/share/OVMF/OVMF_VARS_4M.fd $OUT/vars.fd
    {
        sleep 30
        keys tab tab tab tab tab pause
        keys g o spc s y s t e m ret pause
        keys g o spc t h e spc d i s k ret pause
        keys i n s t a l l spc k e r n e l dot e l f ret
        await 'boot: a kernel of\|is not a kernel\|could not\|no room' 120 1
        sleep 2
        keys r e s t a r t ret
        resets=$2
        while [ $resets -gt 0 ]; do
            await 'Exception Type' 60 1 && sleep 1
            echo system_reset
            sleep 4
            # the next start's dump is the next occurrence
            resets=$((resets - 1))
            [ $resets -gt 0 ] && await 'Exception Type' 60 2
        done
        await 'shell: starting' 90 2
        sleep 4
        echo quit
    } | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
      -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
      -drive if=pflash,format=raw,file=$OUT/vars.fd \
      -drive format=raw,file=$OUT/esp.img \
      -vga none -device VGA,edid=on,xres=1280,yres=800 \
      -drive id=store,file=$OUT/store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -drive id=xchg,file=$OUT/disk.img,format=raw,if=none \
      -device ide-hd,drive=xchg,bus=ide.2 \
      -device e1000,netdev=n0 -netdev user,id=n0 \
      -display none -monitor stdio \
      -serial file:$LOG >/dev/null 2>&1
}

echo "--- part one: the self-built kernel, installed and started ---"
run $BUILD/self/kernel.elf 0 one
grep -a 'boot: a kernel\|system: restarting\|built by the machine\|erebus: the installed\|boot: the installed\|shell: starting\|Exception Type' $LOG | cut -c1-120
echo "the boot disk afterwards:"; mdir -i $OUT/esp.img ::/erebus | grep -i 'kernel\|tries'
rm -f $OUT/installed.elf
mcopy -i $OUT/esp.img ::/erebus/kernel.elf $OUT/installed.elf 2>/dev/null
cmp -s $OUT/installed.elf $BUILD/self/kernel.elf && echo "the installed kernel.elf is byte for byte the one that was installed" || echo "the installed kernel.elf DIFFERS from what was installed"

echo "--- part two: a kernel that does not come up, and the way back ---"
run $OUT/broken.elf 2 two
grep -a 'boot: a kernel\|system: restarting\|erebus: the installed\|boot: the installed\|shell: starting\|Exception Type' $LOG | cut -c1-120
echo "the boot disk afterwards:"; mdir -i $OUT/esp.img ::/erebus | grep -i 'kernel\|tries'
fsck.vfat -n $OUT/esp.img 2>&1 | tail -2
