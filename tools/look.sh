#!/bin/sh
# look.sh -- boot, press a few keys, photograph the screen.
#
# The shell has three ways of looking at the same state and they are
# reached by pressing keys, so seeing them from outside means sending
# keys from outside. A script rather than a makefile target because the
# arguments survive being passed through several shells this way.
#
#   tools/look.sh <output.png> [key ...]
#
# Key names are QEMU's: tab, down, right, spc, a, b, ...

set -e
OUT=$1
shift

BUILD=build
QEMU="qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/OVMF_VARS.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -net none -display none -monitor stdio"

rm -f "$BUILD/screen.ppm"

# The store persists between runs and is not part of the build, so a
# cleaned tree has to grow one before QEMU will start.
[ -f "$BUILD/store.img" ] || dd if=/dev/zero of="$BUILD/store.img" \
    bs=1M count=32 status=none

# The firmware's own variable store is scratch space: it remembers boot
# entries between runs and can end up pointing at an image that has
# since been rebuilt, at which case nothing boots and the log is empty.
# Nothing of ours lives in it, so it starts fresh every time.
cp /usr/share/OVMF/OVMF_VARS_4M.fd "$BUILD/OVMF_VARS.fd"

{
    sleep 9
    # Each argument is either a mouse instruction or a key name. The
    # mouse ones are written without spaces so they survive being passed
    # through several shells, which the quoted form does not.
    #
    #   m<dx>,<dy>   move the pointer by that much
    #   click        press and release the left button
    #   anything else is a QEMU key name
    for k in "$@"; do
        case "$k" in
            home)
                # A PS/2 mouse only reports how far it moved, and the
                # monitor clamps a single report to about a hundred
                # steps, so there is no way to say "go to this point".
                # Driving it hard into the corner gives a known origin
                # to count from; the shell stops the pointer at the
                # edge, which is what makes it work.
                # With a pause between each. Sent back to back, some of
                # them arrive faster than the interrupt handler drains
                # the controller and are simply lost, which leaves the
                # pointer somewhere unpredictable -- and a test whose
                # starting point is unpredictable proves nothing.
                # Twenty steps, not a guess: the framebuffer is
                # 1920x1200 (the loader takes the largest mode the
                # firmware offers, whatever the EDID hint said), so
                # driving home from the far corner needs the full
                # twenty. Fourteen was enough for the smaller screen
                # this was first written against, and left every later
                # "home" short by whatever the last position was.
                i=0
                while [ $i -lt 20 ]; do
                    echo "mouse_move -100 -100"
                    sleep 0.1
                    i=$((i + 1))
                done
                ;;
            m*,*)
                echo "mouse_move ${k#m}" | tr ',' ' '
                # The same pause as above, and for the same reason: two
                # reports sent back to back can outrun the controller.
                sleep 0.1
                ;;
            click)
                echo "mouse_button 1"
                sleep 0.2
                echo "mouse_button 0"
                ;;
            *)
                echo "sendkey $k"
                ;;
        esac
        sleep 0.3
    done
    sleep 1
    echo "screendump $BUILD/screen.ppm"
    sleep 2
    echo quit
} | $QEMU -serial file:$BUILD/serial.log >/dev/null 2>&1 || true

python3 tools/ppm2png.py "$BUILD/screen.ppm" "$OUT"
