#!/bin/sh
# pipe-foreman.sh -- a task pointed at the foreman is dealt out and answered without a click.
# - the foreman hands it to the desk over the wire; the answer is written into the task

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/peer-esp.img $BUILD/teststore.img $BUILD/serial.log \
      $BUILD/foreman-b.ppm
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

# --- machine A: claim 10.9.9.20, welcome work, wait -----------------
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    sleep 110
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

# --- machine B: address and peer; a task and a foreman from the
# palette; the task pointed at the foreman. Then hands off. --------
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 1 \
         ret p e e r spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 spc 7 8 0 0 \
         left left \
         home m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m0,100 m0,100 m0,69 click \
         m0,154 click ret \
         home m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m0,100 m0,100 m0,91 click \
         m0,100 m0,100 m0,100 m0,96 click ret \
         right \
         home m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m0,100 m0,37 click \
         m0,100 m0,100 m0,100 m0,100 m0,100 m0,100 m0,82 click ret
    sleep 45
    echo "screendump $BUILD/foreman-b.ppm"
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

python3 tools/ppm2png.py $BUILD/foreman-b.ppm $BUILD/foreman-b.png 2>/dev/null

echo "--- what the foreman said (B) ---"
grep -ao 'user: .*' $BUILD/serial.log | tail -6
echo "--- the desk (B) ---"
grep -a 'pipe: job' $BUILD/serial.log
echo "--- the worker (A) ---"
grep -ac 'pipe: running a job' $BUILD/peer-serial.log | sed 's/^/    jobs run: /'
