#!/bin/sh
# pipe-update.sh -- a kernel through the pipe: B updates A, once A's nodes table lets it.
# - A boots a kernel built as "older"; B the current one; both on one cable (QEMU socket netdev)
# - words typed through the screen terminal: names alpha/beta, addresses, B points at A and says hello (the knock writes both rows)
# - B's first 'update alpha' is declined (A's row for beta grants nothing); A: 'allow beta update'; B's second goes through
# - A installs the kernel, restarts, and its second boot line names the current version

cd "$(dirname "$0")/.."
BUILD=build

make -s VERSION=older >/dev/null || exit 1
cp $BUILD/esp.img $BUILD/peer-esp.img
make -s >/dev/null || exit 1
NOW=$(sed -n 's/.*"\(.*\)".*/\1/p' $BUILD/version.c)

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/teststore.img $BUILD/serial.log $BUILD/update-a.ppm $BUILD/update-b.ppm
dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

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

# --- A: alpha at 10.9.9.20, runs the older kernel, lets beta update it later ---
{
    sleep 24
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "back"
    say "back"
    sleep 45
    say "allow beta update"
    sleep 20
    say "nodes"
    sleep 110
    echo "screendump $BUILD/update-a.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:8012 \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# --- B: beta at 10.9.9.21, points at alpha, knocks, updates it ---
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
    say "say hello"
    sleep 6
    say "update alpha"
    sleep 40
    say "update alpha"
    sleep 75
    say "nodes"
    sleep 20
    echo "screendump $BUILD/update-b.ppm"
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
  -netdev socket,id=n0,connect=127.0.0.1:8012 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

wait $A_JOB 2>/dev/null
python3 tools/ppm2png.py $BUILD/update-a.ppm $BUILD/update-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/update-b.ppm $BUILD/update-b.png 2>/dev/null

echo "--- B, the updater ---"
grep -a 'pipe:' $BUILD/serial.log | cut -c1-120
echo "--- A, the updated ---"
grep -a 'pipe:\|EreBUS .* (x86_64)\|boot:' $BUILD/peer-serial.log | cut -c1-120
echo "--- the checks ---"
ok=1
grep -aq "remembered in nodes as 'beta'" $BUILD/peer-serial.log && echo "A wrote beta's key into its nodes table" || { echo "FAILED: A has no row for beta"; ok=0; }
grep -aq 'may not update this machine; declined' $BUILD/peer-serial.log && echo "the first kernel was declined: beta had no right" || { echo "FAILED: no refusal"; ok=0; }
grep -aq 'declined it (it does not let this machine update it)' $BUILD/serial.log && echo "and B heard why" || { echo "FAILED: B did not hear the refusal"; ok=0; }
grep -aq 'came from beta; installed for the next start' $BUILD/peer-serial.log && echo "after 'allow beta update' the kernel was taken and installed" || { echo "FAILED: the kernel was not installed"; ok=0; }
grep -aq 'carried .* bytes across, sealed' $BUILD/serial.log && echo "B carried it across" || { echo "FAILED: B did not finish the transfer"; ok=0; }
boots=$(grep -ac 'EreBUS .* (x86_64)' $BUILD/peer-serial.log)
last=$(grep -a 'EreBUS .* (x86_64)' $BUILD/peer-serial.log | tail -1)
echo "A booted $boots times; last: $last"
[ "$boots" -ge 2 ] && echo "$last" | grep -q "EreBUS $NOW " && echo "A runs the kernel B sent: $NOW" || { echo "FAILED: A does not run the new kernel"; ok=0; }
[ $ok = 1 ] && echo "a node updates another through the pipe, by leave" || echo "the update through the pipe FAILED"
