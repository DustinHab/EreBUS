#!/bin/sh
# testlib.sh -- shared by the test scripts: paths, KVM, key input, waiting on the serial log.
# - BUILD may be set by the caller (build/battery.sh runs every test in its own directory)
# - KVM is used when /dev/kvm is writable, unless NOKVM=1; then keys are sent faster
# - waitlog/waitcount/waitfile replace fixed sleeps; bootwait waits for the shell

BUILD=${BUILD:-build}
ROOT=$(pwd)
OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd

KVM=0
if [ -z "$NOKVM" ] && [ -w /dev/kvm ] && [ -f "$ROOT/build/kvm-shim/qemu-system-x86_64" ]; then
    KVM=1
    chmod +x "$ROOT/build/kvm-shim/qemu-system-x86_64"
    case ":$PATH:" in
        *":$ROOT/build/kvm-shim:"*) ;;
        *) PATH="$ROOT/build/kvm-shim:$PATH"; export PATH ;;
    esac
fi
if [ "$KVM" = 1 ]; then
    KEYDELAY=${KEYDELAY:-0.15}
    PAUSE=${PAUSE:-0.8}
    BOOTLIMIT=${BOOTLIMIT:-90}
else
    KEYDELAY=${KEYDELAY:-0.35}
    PAUSE=${PAUSE:-1.6}
    BOOTLIMIT=${BOOTLIMIT:-180}
fi

# The QEMU options every test shares; the caller adds drives, devices, the serial log.
QEMU_BASE="-machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=$OVMF_CODE \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -display none -monitor stdio"

# A fresh firmware variable store and a blank store image.
fresh_vars()  { cp $OVMF_VARS "$1"; }
fresh_store() { rm -f "$1"; dd if=/dev/zero of="$1" bs=1M count=${2:-32} status=none; }

# Keys and mouse through the monitor. m<dx>,<dy> moves, click clicks,
# corner drives the pointer into the top left corner (a PS/2 mouse only
# reports how far it moved, so this is the one known origin), pause waits.
keys() {
    for k in "$@"; do
        case "$k" in
            pause) sleep $PAUSE ;;
            corner)
                i=0
                while [ $i -lt 20 ]; do echo "mouse_move -100 -100"; sleep 0.1; i=$((i + 1)); done ;;
            m*,*) echo "mouse_move ${k#m}" | tr ',' ' '; sleep 0.1 ;;
            click) echo "mouse_button 1"; sleep 0.2; echo "mouse_button 0"; sleep 0.2 ;;
            *) echo "sendkey $k"; sleep $KEYDELAY ;;
        esac
    done
}

# count <file> <pattern>: how often the pattern is in the log now (0 for no file).
# grep -c exits 1 when the count is 0; the count itself is what matters here.
count() { c=$(grep -ac -- "$2" "$1" 2>/dev/null); echo "${c:-0}"; }

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
        ',')      echo comma ;;
        ';')      echo semicolon ;;
        '(')      echo shift-9 ;;
        ')')      echo shift-0 ;;
        '{')      echo shift-bracket_left ;;
        '}')      echo shift-bracket_right ;;
        ':')      echo shift-semicolon ;;
        '[')      echo bracket_left ;;
        ']')      echo bracket_right ;;
        '*')      echo shift-8 ;;
        '&')      echo shift-7 ;;
        '<')      echo shift-comma ;;
        '>')      echo shift-dot ;;
        *)        echo spc ;;
    esac
}

# A string, key by key.
type_string() {
    s="$1"
    while [ -n "$s" ]; do
        c="${s%"${s#?}"}"
        s="${s#?}"
        keys "$(key_name "$c")"
    done
}

# A terminal line: the string, enter, a pause.
say() { type_string "$1"; keys ret pause; }

# waitlog <file> <pattern> [seconds]: until the pattern appears in the log. 1 on timeout.
waitlog() {
    n=0
    while ! grep -aq -- "$2" "$1" 2>/dev/null; do
        sleep 0.25
        n=$((n + 1))
        [ $n -ge $(( ${3:-60} * 4 )) ] && return 1
    done
    return 0
}

# waitcount <file> <pattern> <count> [seconds]: until the pattern appears that many times.
waitcount() {
    n=0
    while [ "$(count "$1" "$2")" -lt "$3" ]; do
        sleep 0.25
        n=$((n + 1))
        [ $n -ge $(( ${4:-60} * 4 )) ] && return 1
    done
    return 0
}

# waitfile <path> [seconds]: until the file exists.
waitfile() {
    n=0
    while [ ! -e "$1" ]; do
        sleep 0.25
        n=$((n + 1))
        [ $n -ge $(( ${2:-60} * 4 )) ] && return 1
    done
    return 0
}

# waitport <port> [seconds]: until something listens on 127.0.0.1:<port>.
# Two machines on a socket netdev only meet if the listener is bound
# before the other connects; on a loaded host that takes more than the
# couple of seconds a fixed sleep would guess.
waitport() {
    n=0
    while ! ss -ltn 2>/dev/null | grep -q ":$1 "; do
        sleep 0.25
        n=$((n + 1))
        [ $n -ge $(( ${2:-30} * 4 )) ] && return 1
    done
    return 0
}

# bootwait <log>: until the shell is up, then a moment for the input to settle.
bootwait() {
    waitlog "$1" 'shell: ' $BOOTLIMIT || { echo "(no shell after ${BOOTLIMIT}s)" >&2; return 1; }
    sleep 1.5
}

# The stick image: built once by the battery and copied in, otherwise made here.
need_stick() {
    [ -f "$BUILD/stick.img" ] || { sh tools/mkusb.sh >/dev/null || exit 1; [ "$BUILD" = build ] || cp build/stick.img "$BUILD/stick.img"; }
}
