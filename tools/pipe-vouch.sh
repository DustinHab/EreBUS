#!/bin/sh
# pipe-vouch.sh -- a node vouches for a key, and a peer that allows it pins the key before meeting.
# - A (alpha) trusts a generated key by hand as 'gamma'; B (beta) meets A
# - A vouches for gamma BEFORE B allows it: B ignores the vouch (a stranger's word pins nothing)
# - B 'allow alpha vouch'; A vouches again: B pins gamma's key, and its fingerprint == the one A trusted
# - both machines driven through the screen terminal; each its own MAC

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/vouch-a.log
BLOG=$BUILD/vouch-b.log
PORT=${VOUCHPORT:-8016}

# A generated ed25519 public line to stand in for a third machine 'gamma'.
KEYS=/tmp/erebus-vouch
mkdir -p $KEYS && chmod 700 $KEYS
[ -f $KEYS/gamma ] || ssh-keygen -q -t ed25519 -N '' -f $KEYS/gamma
GPUB=$(cut -d' ' -f1,2 $KEYS/gamma.pub)

rm -f $BUILD/vouch-a.img $BUILD/vouch-b.img $BUILD/vouch-a-vars.fd $BUILD/vouch-b-vars.fd \
      $BUILD/vouch-a-esp.img $ALOG $BLOG \
      $BUILD/v-a-ready $BUILD/v-b-met $BUILD/v-a-first $BUILD/v-b-allowed $BUILD/v-a-done $BUILD/v-done
fresh_store $BUILD/vouch-a.img
fresh_store $BUILD/vouch-b.img
fresh_vars $BUILD/vouch-a-vars.fd
fresh_vars $BUILD/vouch-b-vars.fd
cp $BUILD/esp.img $BUILD/vouch-a-esp.img

# --- A: alpha, trusts gamma by hand, vouches once too early, then again after B allows ---
{
    bootwait $ALOG
    keys tab tab tab tab tab pause
    say "trust gamma $GPUB"
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write name | alpha"
    say "back"
    say "back"
    waitlog $ALOG '10.9.9.20 by claim' 40
    touch $BUILD/v-a-ready
    # once B has met us, vouch before B allows -- B must ignore this one
    waitlog $ALOG 'session with 10.9.9.21' 90
    waitfile $BUILD/v-b-met 60
    sleep 1
    say "vouch gamma"
    waitlog $ALOG 'vouched for gamma' 20
    touch $BUILD/v-a-first
    # after B allows our vouches, vouch again -- this one must pin
    waitfile $BUILD/v-b-allowed 60
    sleep 1
    say "vouch gamma"
    waitlog $ALOG 'vouched for gamma' 20
    touch $BUILD/v-a-done
    waitfile $BUILD/v-done 90
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/vouch-a-vars.fd \
  -drive format=raw,file=$BUILD/vouch-a-esp.img \
  -drive id=store,file=$BUILD/vouch-a.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:20 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>&1 &
A_JOB=$!

waitport $PORT 30 || echo "(A never opened port $PORT)"
sleep 1

# --- B: beta, meets A, allows alpha's vouches only after the first (ignored) one ---
{
    bootwait $BLOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.21"
    say "write name | beta"
    say "write peer | 10.9.9.20 7800"
    say "back"
    say "back"
    waitlog $BLOG '10.9.9.21 by claim' 40
    waitfile $BUILD/v-a-ready 90
    sleep 1
    say "say hello"
    waitlog $BLOG 'session with 10.9.9.20' 40
    touch $BUILD/v-b-met
    # let A's premature vouch arrive and be ignored
    waitfile $BUILD/v-a-first 60
    sleep 2
    say "allow alpha vouch"
    sleep 1
    touch $BUILD/v-b-allowed
    waitlog $BLOG "vouches for .gamma." 40
    say "go system"
    say "go nodes"
    sleep 1
    echo "screendump $BUILD/vouch-nodes.ppm"
    sleep 1
    touch $BUILD/v-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/vouch-b-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/vouch-b.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=52:54:00:aa:99:21 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>&1

wait $A_JOB 2>/dev/null
python3 tools/ppm2png.py $BUILD/vouch-nodes.ppm $BUILD/vouch-nodes.png 2>/dev/null

echo "--- A (alpha) ---"
grep -a 'pipe:\|node ' $ALOG | cut -c1-120 | tail -8
echo "--- B (beta) ---"
grep -a 'pipe:\|node ' $BLOG | cut -c1-120 | tail -12

echo "--- the checks ---"
ok=1
afp=$(grep -a "node 'gamma' trusted before meeting" $ALOG | sed 's/.*key //; s/;.*//' | tr -d ' \r' | tail -1)
echo "    gamma's key (as A trusted it):  $afp"
# the premature vouch (before B allowed) must NOT have pinned anything
pins=$(count $BLOG "vouches for .gamma.")
[ "$pins" = "1" ] && echo "B ignored the vouch until it allowed alpha, then honoured exactly one" || { echo "FAILED: expected one honoured vouch, saw $pins"; ok=0; }
# and the key B pinned must be the very key A trusted
bfp=$(grep -a "vouches for 'gamma'" $BLOG | sed 's/.*key //; s/ pinned.*//' | tr -d ' \r' | tail -1)
echo "    gamma's key (as B pinned it):   $bfp"
if [ -n "$afp" ] && [ "$afp" = "$bfp" ]; then
    echo "B pinned gamma's key before meeting, and it is the key A vouched for"
else
    echo "FAILED: B did not pin the vouched key"; ok=0
fi
[ $ok = 1 ] && echo "a vouch from an allowed node pins a key before meeting" || echo "vouching FAILED"
