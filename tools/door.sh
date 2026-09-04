#!/bin/sh
# door.sh -- makes build/door-store.img: a store whose settings hold the test key.
# - types the "door |" line once through the screen; door tests copy this store
# - key under $HOME: the shared drive is world-readable (ssh refuses), /tmp does not persist

cd "$(dirname "$0")/.."
BUILD=build
KEYS=$HOME/.erebus-door
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/sshkey ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/sshkey
PUB=$(cut -d' ' -f1,2 $KEYS/sshkey.pub)

rm -f $BUILD/door-store.img $BUILD/door-serial.log
dd if=/dev/zero of=$BUILD/door-store.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/door-vars.fd

key_name() {
    case "$1" in
        [a-z0-9]) echo "$1" ;;
        [A-Z])    echo "shift-$(echo "$1" | tr 'A-Z' 'a-z')" ;;
        '+')      echo shift-equal ;;
        '/')      echo slash ;;
        '=')      echo equal ;;
        ' ')      echo spc ;;
        '|')      echo shift-backslash ;;
        '-')      echo minus ;;
        '.')      echo dot ;;
        *)        echo spc ;;
    esac
}

keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep 1.6 ;;
            *) echo "sendkey $k"; sleep 0.35 ;;
        esac
    done
}

type_string() {
    s="$1"
    while [ -n "$s" ]; do
        c="${s%"${s#?}"}"
        s="${s#?}"
        keys "$(key_name "$c")"
    done
}

{
    sleep 24
    keys tab tab tab tab tab pause
    keys g o spc s y s t e m ret pause
    keys g o spc s e t t i n g s ret pause
    type_string "write door | $PUB"
    keys ret pause
    keys h o m e ret pause
    # the settings are saved once the machine has been quiet a moment
    n=0
    while [ $n -lt 60 ]; do
        sleep 3; n=$((n + 3))
        grep -a -q 'snap: generation' $BUILD/door-serial.log 2>/dev/null && break
    done
    sleep 3
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/door-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/door-store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -display none -monitor stdio \
  -serial file:$BUILD/door-serial.log >/dev/null 2>&1

if grep -a -q 'snap: generation' $BUILD/door-serial.log; then
    echo "door store ready: $BUILD/door-store.img (key $KEYS/sshkey)"
else
    echo "the door store was not saved"; exit 1
fi
