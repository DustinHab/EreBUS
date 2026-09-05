#!/bin/sh
# pipe-rotate.sh -- trusting a key before meeting, and renewing a key after.
# - B trusts a generated key by hand ('trust friend ssh-ed25519 ...'): a row appears before any handshake
# - A and B meet (B says hello); then A 'renew key' announces its new key, signed old and new
# - B accepts the rotation and its row for alpha carries the new key: A's new fingerprint == the one B logs

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/rot-a.log
BLOG=$BUILD/rot-b.log
PORT=${ROTPORT:-8015}

# A generated ed25519 public line for the trust-before-meeting check.
KEYS=/tmp/erebus-rotate
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/friend ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/friend
FPUB=$(cut -d' ' -f1,2 $KEYS/friend.pub)

rm -f $BUILD/rot-a.img $BUILD/rot-b.img $BUILD/rot-a-vars.fd $BUILD/rot-b-vars.fd \
      $BUILD/rot-a-esp.img $ALOG $BLOG $BUILD/rot-ready $BUILD/rot-done $BUILD/rot-renewed
fresh_store $BUILD/rot-a.img
fresh_store $BUILD/rot-b.img
fresh_vars $BUILD/rot-a-vars.fd
fresh_vars $BUILD/rot-b-vars.fd
cp $BUILD/esp.img $BUILD/rot-a-esp.img

# --- A: alpha, waits, then renews its key once B has met it ---
{
    bootwait $ALOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "back"
    say "back"
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/rot-ready
    # wait until B has said hello (a session is proven), then renew
    waitlog $ALOG 'session with 10.9.9.21' 90
    sleep 2
    say "renew key"
    waitlog $ALOG 'the door key is renewed' 30
    touch $BUILD/rot-renewed
    waitfile $BUILD/rot-done 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/rot-a-vars.fd \
  -drive format=raw,file=$BUILD/rot-a-esp.img \
  -drive id=store,file=$BUILD/rot-a.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>&1 &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- B: beta, trusts a key by hand, then meets A ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "trust friend $FPUB"
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "write peer | 10.9.9.20 7800"
    say "back"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/rot-ready 90
    sleep 1
    say "say hello"
    waitlog $BLOG 'session with 10.9.9.20' 40
    waitfile $BUILD/rot-renewed 120
    waitlog $BLOG "renewed its key" 30
    sleep 1
    touch $BUILD/rot-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/rot-b-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/rot-b.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>&1

wait $A_JOB 2>/dev/null

echo "--- A (alpha) ---"
grep -a 'pipe:\|node ' $ALOG | cut -c1-120 | tail -8
echo "--- B (beta) ---"
grep -a 'pipe:\|node ' $BLOG | cut -c1-120 | tail -10

echo "--- the checks ---"
ok=1
grep -aq "node 'friend' trusted before meeting" $BLOG && echo "B trusted a key before meeting it" || { echo "FAILED: trust before meeting"; ok=0; }
newfp=$(grep -a 'the door key is renewed; now ' $ALOG | sed 's/.*now //; s/;.*//' | tr -d ' \r' | tail -1)
echo "    A's new key:  $newfp"
if [ -n "$newfp" ] && grep -aF "renewed its key; now $newfp" $BLOG >/dev/null; then
    echo "B moved alpha's row to the renewed key, signed old and new"
else
    echo "FAILED: the rotation did not reach B"; ok=0
fi
[ $ok = 1 ] && echo "a key is trusted before meeting and renewed after" || echo "identity rotation FAILED"
