#!/bin/sh
# pipe-work.sh -- far work: A lends its processor to B.
# - A sets "work | welcomed"; B asks a recipe; the journal shows "job 1 answers: 42"
# - a second recipe loops forever; A's time budget ends it; B hears "it ran out of time"

cd "$(dirname "$0")/.."
BUILD=build

rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $BUILD/peer-serial.log \
      $BUILD/peer-esp.img $BUILD/teststore.img $BUILD/serial.log \
      $BUILD/work-a.ppm $BUILD/work-b.ppm
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

# --- machine A: claim 10.9.9.20, welcome work, and wait ------------------
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    sleep 115
    echo "screendump $BUILD/work-a.ppm"
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
  -netdev socket,id=n0,listen=127.0.0.1:8011 \
  -display none -monitor stdio \
  -serial file:$BUILD/peer-serial.log >/dev/null 2>&1 &
A_JOB=$!

sleep 3

# --- machine B: claim 10.9.9.21, point at A, then ask twice --------------
# Recipe one: wait for the way home, compute 6*7, answer it, stop.
# Recipe two: wait, then loop forever -- the budget must end it.
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
         m0,22 click ret \
         right \
         w a i t ret \
         s e t spc a spc 6 ret \
         m u l spc a spc 7 ret \
         a n s w e r spc a ret \
         s t o p ret \
         home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m45,0 click
    sleep 10
    keys left \
         home m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m0,100 m0,100 m0,91 click \
         m0,22 click ret \
         right \
         w a i t ret \
         b a c k spc 0 ret \
         home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m45,0 click
    sleep 45
    echo "screendump $BUILD/work-b.ppm"
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
  -netdev socket,id=n0,connect=127.0.0.1:8011 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1

wait $A_JOB 2>/dev/null

python3 tools/ppm2png.py $BUILD/work-a.ppm $BUILD/work-a.png 2>/dev/null
python3 tools/ppm2png.py $BUILD/work-b.ppm $BUILD/work-b.png 2>/dev/null

echo "--- asker (B) ---"
grep -a 'pipe: job' $BUILD/serial.log
echo "--- worker (A) ---"
grep -a 'pipe: running\|pipe: the job\|pipe: turned\|(work)' $BUILD/peer-serial.log
echo "--- what the visiting scripts said on A ---"
grep -ao 'user: .*' $BUILD/peer-serial.log | tail -4
