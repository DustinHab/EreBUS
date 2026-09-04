#!/bin/sh
# pipe-identity.sh -- a peer with a changed key at a known address is refused.
# - run pipe-two.sh: A and B meet, B remembers A's key as a "known |" line
# - A returns with a fresh store (new door key, same address); B sends again
# - B must refuse: nothing crosses, the journal names the reason

cd "$(dirname "$0")/.."
BUILD=build

echo "=== first meeting ==="
sh tools/pipe-two.sh | grep -a 'proven\|remembered\|carried\|plaintext' | cut -c1-120

echo "=== second meeting: a fresh machine at the same address ==="
rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log $BUILD/serial.log
dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

keys() {
    for k in "$@"; do
        case "$k" in
            home)
                i=0
                while [ $i -lt 20 ]; do
                    echo "mouse_move -100 -100"; sleep 0.1; i=$((i+1))
                done ;;
            m*,*) echo "mouse_move ${k#m}" | tr ',' ' '; sleep 0.1 ;;
            click) echo "mouse_button 1"; sleep 0.2; echo "mouse_button 0" ;;
            *) echo "sendkey $k" ;;
        esac
        sleep 0.25
    done
}

# A again: fresh, claims 10.9.9.20, welcomes work, waits.
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    sleep 40
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:8010 \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# B as it was left: standing on its notes, the peer set. Send goes
# straight out -- and comes back refused.
{
    sleep 25
    keys home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m95,0 click
    sleep 12
    echo "screendump $BUILD/pipe-refused.ppm"
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
  -netdev socket,id=n0,connect=127.0.0.1:8010 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

wait $A_JOB 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-refused.ppm $BUILD/pipe-refused.png 2>/dev/null

echo "--- B, the sender ---"
grep -a 'pipe' $BUILD/serial.log | cut -c1-130
echo "--- the checks ---"
ok=1
if grep -aq 'not the one remembered' $BUILD/serial.log; then echo "the impostor was recognised"; else echo "FAILED: no refusal in the log"; ok=0; fi
if grep -aq 'carried' $BUILD/serial.log; then echo "FAILED: something crossed anyway"; ok=0; else echo "nothing crossed"; fi
if grep -aq 'nothing was sent' $BUILD/serial.log; then echo "the send was called off"; else echo "FAILED: the send was not called off"; ok=0; fi
[ $ok = 1 ] && echo "identity under the knock holds" || echo "identity under the knock FAILED"
