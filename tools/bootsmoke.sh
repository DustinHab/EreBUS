#!/bin/sh
# bootsmoke.sh -- boot the built image headless and confirm the kernel
# reaches idle and its self-tests pass. No KVM required, so it suits a
# plain CI runner under TCG: it polls the serial log for the marker
# rather than sleeping a fixed time, because TCG boot time varies.
#
# usage: sh tools/bootsmoke.sh          (LIMIT seconds to wait for idle)
#        LIMIT=360 sh tools/bootsmoke.sh
cd "$(dirname "$0")/.."
BUILD=build
OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}
OVMF_VARS=${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}
LIMIT=${LIMIT:-300}

touch kernel/gfx/font8x16.h    # use the committed font, do not regenerate
make >/dev/null 2>&1 || { echo "build failed"; make 2>&1 | tail -20; exit 2; }
# A scratch object-graph disk; the store rule's target is an absolute
# path, so make it here rather than asking make for a relative name.
[ -f "$BUILD/teststore.img" ] || dd if=/dev/zero of="$BUILD/teststore.img" bs=1M count=32 status=none
cp "$OVMF_VARS" "$BUILD/smoke-vars.fd"
LOG=$BUILD/smoke-serial.log
: > "$LOG"

qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$BUILD/smoke-vars.fd" \
  -drive format=raw,file="$BUILD/esp.img" \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file="$BUILD/teststore.img",format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -serial "file:$LOG" >/dev/null 2>&1 &
QPID=$!

i=0
while [ "$i" -lt "$LIMIT" ]; do
    grep -qa 'kern: idle' "$LOG" 2>/dev/null && break
    kill -0 "$QPID" 2>/dev/null || break
    sleep 2
    i=$((i + 2))
done
kill "$QPID" 2>/dev/null
wait "$QPID" 2>/dev/null

echo "--- self-test and boot markers ---"
grep -a 'self test passed\|certificate checks ready\|kern: idle' "$LOG" | head -40

ok=1
grep -qa 'kern: idle' "$LOG" || { echo "FAILED: did not reach idle within $LIMIT s"; ok=0; }
grep -qa 'tls:  self test passed' "$LOG" || { echo "FAILED: the tls self-test line was not seen"; ok=0; }
if [ "$ok" = 1 ]; then
    echo "boot smoke passed: the kernel booted, self-tested and reached idle"
    exit 0
else
    echo "boot smoke FAILED; last of the log:"
    tail -30 "$LOG"
    exit 1
fi
