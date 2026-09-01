#!/bin/sh
# pipe-desk.sh -- three machines, one task, the desk deals the parts.
#
# Two machines welcome work; the third makes a task from the palette
# -- "split 4 from 1 to 10000" over the summing recipe -- and presses
# ask. The desk scans, finds both, deals the four parts round-robin,
# collects the four sums and writes their total into the task:
# 50005000, computed by other machines' processors. The mcast socket
# is the shared cable three QEMUs can stand on.

cd "$(dirname "$0")/.."
BUILD=build
MCAST=230.0.0.1:8021

rm -f $BUILD/desk-a1.img $BUILD/desk-a2.img $BUILD/desk-b.img \
      $BUILD/desk-a1-vars.fd $BUILD/desk-a2-vars.fd $BUILD/desk-b-vars.fd \
      $BUILD/desk-a1-esp.img $BUILD/desk-a2-esp.img \
      $BUILD/desk-a1.log $BUILD/desk-a2.log $BUILD/desk-b.log \
      $BUILD/desk-b.ppm
dd if=/dev/zero of=$BUILD/desk-a1.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/desk-a2.img bs=1M count=32 status=none
dd if=/dev/zero of=$BUILD/desk-b.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/desk-a1-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/desk-a2-vars.fd
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/desk-b-vars.fd
cp $BUILD/esp.img $BUILD/desk-a1-esp.img
cp $BUILD/esp.img $BUILD/desk-a2-esp.img

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

worker() {
    # $1 $2 = the address's last octet as two keys, then vars, esp,
    # store, log.
    {
        sleep 12
        keys tab tab tab t h e m e ret \
             ret a d d r e s s spc shift-backslash spc \
             1 0 dot 9 dot 9 dot $1 $2 \
             ret w o r k spc shift-backslash spc w e l c o m e d
        sleep 100
        echo quit
    } | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
      -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
      -drive if=pflash,format=raw,file=$3 \
      -drive format=raw,file=$4 \
      -vga none -device VGA,edid=on,xres=1280,yres=800 \
      -drive id=store,file=$5,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 \
      -netdev socket,id=n0,mcast=$MCAST \
      -display none -monitor stdio \
      -serial file:$6 >/dev/null 2>&1
}

worker 2 0 $BUILD/desk-a1-vars.fd $BUILD/desk-a1-esp.img \
       $BUILD/desk-a1.img $BUILD/desk-a1.log &
A1_JOB=$!
sleep 2
worker 2 2 $BUILD/desk-a2-vars.fd $BUILD/desk-a2-esp.img \
       $BUILD/desk-a2.img $BUILD/desk-a2.log &
A2_JOB=$!
sleep 2

# --- machine B: claim an address, name a peer as fallback, make a
# task from the palette, and press ask. The desk does the rest.
{
    sleep 12
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 1 \
         ret p e e r spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 spc 7 8 0 0 \
         left \
         home m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m0,100 m0,100 m0,100 m0,100 m0,100 m0,33 click \
         m0,154 click ret \
         right \
         home m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m100,0 m45,0 click
    sleep 55
    echo "screendump $BUILD/desk-b.ppm"
    sleep 2
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/desk-b-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/desk-b.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,mcast=$MCAST \
  -display none -monitor stdio \
  -serial file:$BUILD/desk-b.log >/dev/null 2>&1

wait $A1_JOB $A2_JOB 2>/dev/null

python3 tools/ppm2png.py $BUILD/desk-b.ppm $BUILD/desk-b.png 2>/dev/null

echo "--- the asker (B) ---"
grep -a 'pipe: job' $BUILD/desk-b.log
echo "--- worker one ---"
grep -ac 'pipe: running a job' $BUILD/desk-a1.log | sed 's/^/    jobs run: /'
echo "--- worker two ---"
grep -ac 'pipe: running a job' $BUILD/desk-a2.log | sed 's/^/    jobs run: /'
