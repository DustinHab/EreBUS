#!/bin/sh
# pipe-identity.sh -- a peer with a changed key at a known address is refused.
# - run pipe-two.sh: A and B meet, B writes A's key into its nodes table
# - A returns with a fresh store (new door key, same address); B sends again
# - B must refuse: nothing crosses, the journal names the reason

cd "$(dirname "$0")/.."
. tools/testlib.sh
ALOG=$BUILD/peer-serial.log
BLOG=$BUILD/serial.log
PORT=${PIPEPORT:-8010}

echo "=== first meeting ==="
sh tools/pipe-two.sh | grep -a 'proven\|remembered\|carried\|plaintext' | cut -c1-120

echo "=== second meeting: a fresh machine at the same address ==="
rm -f $BUILD/peerstore.img $BUILD/peer-vars.fd $ALOG $BLOG
fresh_store $BUILD/peerstore.img
fresh_vars $BUILD/peer-vars.fd
fresh_vars $BUILD/test-vars.fd

# A again: fresh, claims 10.9.9.20, welcomes work, waits.
{
    bootwait $ALOG
    keys tab tab tab t h e m e ret \
         ret a d d r e s s spc shift-backslash spc \
         1 0 dot 9 dot 9 dot 2 0 \
         ret w o r k spc shift-backslash spc w e l c o m e d
    waitfile $BUILD/identity-done 120
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/peer-vars.fd \
  -drive format=raw,file=$BUILD/peer-esp.img \
  -drive id=store,file=$BUILD/peerstore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,listen=127.0.0.1:$PORT \
  -serial file:$ALOG >/dev/null 2>&1 &
A_JOB=$!

sleep 2
rm -f $BUILD/identity-done

# B as it was left: standing on its notes, the peer set. Send goes
# straight out -- and comes back refused.
{
    bootwait $BLOG
    waitlog $ALOG 'by claim' 60
    sleep 2
    keys corner m100,20 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 m100,0 \
         m100,0 m100,0 m100,0 m95,0 click
    waitlog $BLOG 'nothing was sent\|carried' 40
    sleep 1
    echo "screendump $BUILD/pipe-refused.ppm"
    sleep 1
    touch $BUILD/identity-done
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 \
  -netdev socket,id=n0,connect=127.0.0.1:$PORT \
  -serial file:$BLOG >/dev/null 2>&1

wait $A_JOB 2>/dev/null
python3 tools/ppm2png.py $BUILD/pipe-refused.ppm $BUILD/pipe-refused.png 2>/dev/null

echo "--- B, the sender ---"
grep -a 'pipe' $BLOG | cut -c1-130
echo "--- the checks ---"
ok=1
if grep -aq 'not the one remembered' $BLOG; then echo "the impostor was recognised"; else echo "FAILED: no refusal in the log"; ok=0; fi
if grep -aq 'carried' $BLOG; then echo "FAILED: something crossed anyway"; ok=0; else echo "nothing crossed"; fi
if grep -aq 'nothing was sent' $BLOG; then echo "the send was called off"; else echo "FAILED: the send was not called off"; ok=0; fi
[ $ok = 1 ] && echo "identity under the handshake holds" || echo "identity under the handshake FAILED"
