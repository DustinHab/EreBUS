#!/bin/sh
# pipe-input.sh -- work with an input object, and the answer names who did it.
# - A (alpha) welcomes work; B (beta) writes a recipe that reads its input's first eight bytes and answers them
# - 'ask task with in' sends the input ahead of the recipe; A's log shows it arrive; B's answer says "(by alpha)"
# - words typed through the screen terminal on both machines

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/peer-esp.img $BUILD/teststore.img $BUILD/serial.log $BUILD/input-b.ppm
dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

key_name() {
    case "$1" in
        [a-z0-9]) echo "$1" ;;
        [A-Z])    echo "shift-$(echo "$1" | tr 'A-Z' 'a-z')" ;;
        '|')      echo shift-backslash ;;
        '.')      echo dot ;;
        '-')      echo minus ;;
        ' ')      echo spc ;;
        *)        echo spc ;;
    esac
}
keys() { for k in "$@"; do case "$k" in pause) sleep 1.6 ;; *) echo "sendkey $k"; sleep 0.35 ;; esac; done; }
say() { s="$1"; while [ -n "$s" ]; do c="${s%"${s#?}"}"; s="${s#?}"; keys "$(key_name "$c")"; done; keys ret pause; }

# --- A: alpha at 10.9.9.20, welcomes work from everyone ---
{
    sleep 24
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "write work | welcomed"
    say "back"
    say "back"
    sleep 150
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:8013 \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# --- B: beta at 10.9.9.21, points at alpha, asks with an input ---
# The recipe waits three times: the way home, the range's high end,
# then the input; 'get a 0' reads the input's first eight bytes as a
# number, which is what it answers.
{
    sleep 24
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "write peer | 10.9.9.20 7800"
    say "back"
    say "back"
    say "make text task"
    say "go task"
    say "write wait"
    say "write wait"
    say "write wait"
    say "write get a 0"
    say "write answer a"
    say "write stop"
    say "back"
    say "make text in"
    say "go in"
    say "write hello"
    say "back"
    say "ask task with in"
    sleep 50
    say "read task"
    sleep 4
    echo "screendump $BUILD/input-b.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,connect=127.0.0.1:8013 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

wait $A_JOB 2>/dev/null
python3 tools/ppm2png.py $BUILD/input-b.ppm $BUILD/input-b.png 2>/dev/null

echo "--- B, the asker ---"
grep -a 'pipe:' $BUILD/serial.log | cut -c1-120
echo "--- A, the worker ---"
grep -a 'pipe:\|user: ' $BUILD/peer-serial.log | cut -c1-120 | tail -12
echo "--- the checks ---"
ok=1
grep -aq 'an input of .* bytes for work came from beta' $BUILD/peer-serial.log && echo "the input went ahead of the work" || { echo "FAILED: no input arrived at A"; ok=0; }
grep -aq 'pipe: running a job from' $BUILD/peer-serial.log && echo "A ran the recipe" || { echo "FAILED: A ran nothing"; ok=0; }
# "hello\n" and two zero bytes, read as one little-endian number
grep -aq 'pipe: job 1 answers: 11473676690792 (by alpha)' $BUILD/serial.log && echo "the recipe read the input's bytes, and the answer names alpha" || { echo "FAILED: the answer is not the input's bytes by alpha"; ok=0; }
[ $ok = 1 ] && echo "work travels with its input, and answers say who did it" || echo "work with an input FAILED"
