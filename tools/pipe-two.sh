#!/bin/sh
# pipe-two.sh -- two machines on one cable (QEMU socket netdev), one object crossing.
# - each machine claims its address in the settings; B stands on its notes and presses send
# - no peer set: the chooser scans, A answers, one click sends
# - A's serial log names the arrival; A's store shows it in arrivals

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/peer-esp.img $BUILD/teststore.img $BUILD/serial.log \
      $BUILD/pipe-choose.ppm $BUILD/pipe-wire.dump
dd if=/dev/zero of=$BUILD/peerstore.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/peer-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd
cp $BUILD/esp.img $BUILD/peer-esp.img

# Keys through the monitor, with the pauses ps/2 needs.
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

# --- machine A: claim 10.9.9.20, then wait ------------------------------
{
    sleep 12
    # find the settings through the index, append the address line
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    sleep 45
    echo "screendump $BUILD/pipe-a.ppm"
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
  -netdev socket,id=n0,listen=127.0.0.1:8010 \
  -object filter-dump,id=fd0,netdev=n0,file=$BUILD/pipe-wire.dump \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# --- machine B: claim 10.9.9.21, then send the shortest way there is:
# stand on the notes, press send. No peer is set, so the chooser opens
# under the word and scans by itself; one click on the machine that
# answers points the pipe and lets the notes go, in the same breath.
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 1 \
         left left up up up up up right \
         home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m95,0 click \
         m0,1 m0,1 m0,1 m0,1 m0,1 m0,1
    echo "screendump $BUILD/pipe-choose.ppm"
    sleep 1
    keys m105,49 click \
         m0,1 m0,1 m0,1 m0,1 m0,1 m0,1
    sleep 3
    echo "screendump $BUILD/pipe-b.ppm"
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

python3 tools/ppm2png.py $BUILD/pipe-a.ppm $BUILD/pipe-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-b.ppm $BUILD/pipe-b.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-choose.ppm $BUILD/pipe-choose.png 2>/dev/null

echo "--- sender (B) ---"
grep -a 'net:.*claim\|pipe' $BUILD/serial.log
echo "--- receiver (A) ---"
grep -a 'net:.*claim\|pipe' $BUILD/peer-serial.log

# The wire itself, recorded at A's card: the notes' words must not be
# on it, and the knock must be. A seal one cannot check is a story.
echo "--- the wire ---"
if grep -aq 'not a file' $BUILD/pipe-wire.dump; then
    echo "FAILED: the words crossed in the clear"
else
    echo "no plaintext on the wire"
fi
if grep -aq 'EBPX' $BUILD/pipe-wire.dump; then
    echo "pipe packets seen"
else
    echo "FAILED: no pipe packets in the dump"
fi
