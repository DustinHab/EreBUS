#!/bin/sh
# pipe-line.sh -- two machines, one conversation.
#
# A listens, B connects; the same cable as pipe-two.sh. B names A as
# its peer and speaks first: standing on the line, the letters gather
# in the bottom row, enter says them. No seal stands yet, so the say
# knocks first and the word follows the seal. A is already standing
# on its own line, sees the word arrive, and answers. Both screens
# are kept, and the wire dump must hold no clear word -- a talk one
# can read off the cable is not a talk, it is a broadcast.

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/peer-esp.img $BUILD/teststore.img $BUILD/serial.log \
      $BUILD/pipe-line-a.ppm $BUILD/pipe-line-b.ppm $BUILD/line-wire.dump
dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

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

# The walk from anywhere at home to the line: clamp the selection to
# the top, step down to the system shelf, open it, clamp again, step
# down to the line, focus it.
TO_LINE="up up up up up up up up down down down down down right
         up up up up up up down down down down right"

# --- machine A: claim 10.9.9.20, stand on the line, answer ------------
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         left left $TO_LINE
    sleep 22
    keys h e a r d spc y o u ret
    sleep 8
    echo "screendump $BUILD/pipe-line-a.ppm"
    sleep 18
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
  -object filter-dump,id=fd0,netdev=n0,file=$BUILD/line-wire.dump \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# --- machine B: claim 10.9.9.21, name A as peer, speak first ----------
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 1 \
         ret p e e r spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 spc 7 8 0 0 \
         left left $TO_LINE \
         h i spc o v e r spc t h e r e ret
    sleep 32
    echo "screendump $BUILD/pipe-line-b.ppm"
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

python3 tools/ppm2png.py $BUILD/pipe-line-a.ppm $BUILD/pipe-line-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-line-b.ppm $BUILD/pipe-line-b.png 2>/dev/null

echo "--- speaker (B) ---"
grep -a 'pipe' $BUILD/serial.log
echo "--- answerer (A) ---"
grep -a 'pipe' $BUILD/peer-serial.log

echo "--- the wire ---"
if grep -aq 'hi over there\|heard you' $BUILD/line-wire.dump; then
    echo "FAILED: the words crossed in the clear"
else
    echo "no clear words on the wire"
fi
if grep -aq 'EBPX' $BUILD/line-wire.dump; then
    echo "pipe packets seen"
else
    echo "FAILED: no pipe packets in the dump"
fi
