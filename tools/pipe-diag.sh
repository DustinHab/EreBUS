#!/bin/sh
# Half-path diagnosis: does a datagram from the host reach machine A?
cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peer-serial.log
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img
[ -f $BUILD/peerstore.img ] || dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none

qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev user,id=n0,hostfwd=udp::7801-:7800 \
  -display none -monitor none \
  -serial file:$BUILD/peer-serial.log &
A_PID=$!

sleep 14
echo "--- listeners on 7801 ---"
ss -ulnp 2>/dev/null | grep 7801 || echo "nothing listens"

HOSTIP=$(hostname -I | awk '{print $1}')
python3 - "$HOSTIP" <<'EOF'
import socket, sys
ip = sys.argv[1]
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.sendto(b"EBPXloopback0000", ("127.0.0.1", 7801))
s.sendto(b"EBPXhostaddr0000", (ip, 7801))
print("sent to 127.0.0.1:7801 and to %s:7801" % ip)
EOF

sleep 3
kill -9 $A_PID 2>/dev/null
echo "--- receiver log ---"
grep -a 'pipedbg\|pipe:' $BUILD/peer-serial.log || echo "nothing received"
