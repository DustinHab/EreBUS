#!/bin/sh
# doorboot.sh -- a machine started for a door test, in the background.
#
#   DOOR_EXTRA="<more qemu arguments>" . tools/doorboot.sh
#
# Boots a copy of the door store (tools/door.sh makes it) with port 22
# reachable as 2222 on the host, waits until the door is open, and
# leaves QEMU running as $DOOR_PID with its serial log in
# $DOOR_SERIAL. door_say "<line>" runs one terminal line through ssh
# and prints the answer; door_stop ends the machine. The extra
# arguments travel in a variable: a sourced script has no arguments
# of its own under a plain sh.

cd "$(dirname "$0")/.."
BUILD=build
KEYS=$HOME/.erebus-door
# the store and the key belong together: without either, both anew
[ -f $BUILD/door-store.img ] && [ -f $KEYS/sshkey ] || sh tools/door.sh >/dev/null || { echo "no door store"; exit 1; }
DOOR_DIR=$BUILD/door
mkdir -p $DOOR_DIR
DOOR_SERIAL=$DOOR_DIR/serial.log
rm -f $DOOR_SERIAL $DOOR_DIR/store.img $DOOR_DIR/esp.img $KEYS/known_hosts
cp $BUILD/door-store.img $DOOR_DIR/store.img
cp $BUILD/esp.img $DOOR_DIR/esp.img
cp /usr/share/OVMF/OVMF_VARS_4M.fd $DOOR_DIR/vars.fd

qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$DOOR_DIR/vars.fd \
  -drive format=raw,file=$DOOR_DIR/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$DOOR_DIR/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::2222-:22 \
  -display none -monitor none \
  -serial file:$DOOR_SERIAL $DOOR_EXTRA >/dev/null 2>&1 &
DOOR_PID=$!

n=0
while [ $n -lt 90 ]; do
    sleep 2; n=$((n + 2))
    grep -a -q "the door's key" $DOOR_SERIAL 2>/dev/null && break
done
sleep 2

# Each visit through the door is a terminal session of its own,
# standing at the root: lines that build on one another go together,
# one call, one line per argument, through a pipe.
door_say() {
    for line in "$@"; do printf '%s\n' "$line"; done |
    ssh -T -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
        -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p 2222 \
        -i $KEYS/sshkey someone@127.0.0.1 2>&1 | tr -d '\r'
}

door_stop() {
    kill $DOOR_PID 2>/dev/null
    wait $DOOR_PID 2>/dev/null
}
