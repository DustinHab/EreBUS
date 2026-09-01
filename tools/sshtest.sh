#!/bin/sh
# sshtest.sh -- the door, knocked on from outside.
#
# Boots one machine with its port 22 reachable from the host, writes
# a fresh client key into the settings through the screen's terminal
# (the honest way: a "door |" line, typed), and then uses the host's
# own ssh client against it: one command by exec, two lines through a
# pipe, one through a forced pty. A second key that was never written
# into the settings must be turned away. The serial log names the
# host key's fingerprint; the client's first-visit line must match.

cd "$(dirname "$0")/.."
BUILD=build

# The client's keys live on a filesystem that knows what 0600 means;
# a key on the shared drive is world-readable there, and ssh rightly
# refuses to touch one.
KEYS=/tmp/erebus-sshtest
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/sshkey ]  || ssh-keygen -q -t ed25519 -N '' -f $KEYS/sshkey
[ -f $KEYS/sshkey2 ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/sshkey2
PUB=$(cut -d' ' -f1,2 $KEYS/sshkey.pub)

rm -f $BUILD/teststore.img $BUILD/serial.log $BUILD/known_hosts \
      $BUILD/ssh-exec.txt $BUILD/ssh-pipe.txt $BUILD/ssh-pty.txt \
      $BUILD/ssh-refused.txt
dd if=/dev/zero of=$BUILD/teststore.img bs=1M count=32 status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/test-vars.fd

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
    keys b a c k ret pause
    sleep 75
    echo quit
} | qemu-system-x86_64 -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::2222-:22 \
  -display none -monitor stdio \
  -serial file:$BUILD/serial.log >/dev/null 2>&1 &
Q_JOB=$!

# The key line takes a while to type; the door is open from boot,
# but only turns a key it has been told about.
sleep 78

rm -f $KEYS/known_hosts
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=$KEYS/known_hosts \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p 2222"

echo "--- exec: look ---"
$SSH -i $KEYS/sshkey someone@127.0.0.1 look > $BUILD/ssh-exec.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-exec.txt

echo "--- pipe: go system, where ---"
printf 'go system\nwhere\n' | $SSH -T -i $KEYS/sshkey someone@127.0.0.1 \
    > $BUILD/ssh-pipe.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-pipe.txt

echo "--- pty: time ---"
printf 'time\n' | $SSH -tt -i $KEYS/sshkey someone@127.0.0.1 \
    > $BUILD/ssh-pty.txt 2>&1
echo "exit $?"
tr -d '\r' < $BUILD/ssh-pty.txt

echo "--- a key the door was never told about ---"
$SSH -i $KEYS/sshkey2 someone@127.0.0.1 look > $BUILD/ssh-refused.txt 2>&1
echo "exit $?"
cat $BUILD/ssh-refused.txt

wait $Q_JOB 2>/dev/null

echo "--- the machine's side ---"
grep -a 'ssh:\|door:\|crypto:' $BUILD/serial.log
echo "--- the host key the client saw ---"
ssh-keygen -lf $KEYS/known_hosts 2>/dev/null | head -1
