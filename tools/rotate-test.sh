#!/bin/sh
# rotate-test.sh -- a signed rotation moves a machine to a new release key.
# A test key is made; a newer kernel (9.9.9) is packaged twice, once signed with the release key
# and once with the test key; a rotation to the test key is signed with the release key.
#  boot 1: the source offers the rotation and the release-key package -> the machine rotates,
#          then refuses that package (its key is the previous one now); nothing installed.
#  boot 2: the same store, the test-key package -> installed, the machine reboots into 9.9.9;
#          the rotation is read again and changes nothing.
#  boot 3: a fresh store, the test-key package, no rotation -> refused: the test key is nobody's
#          without the rotation.
cd "$(dirname "$0")/.."
. tools/testlib.sh
PORT=${ROTPORT:-8098}
SRV=$BUILD/rot-srv
TESTKEY=$BUILD/rot-test.pem

rm -rf $SRV; mkdir -p $SRV
rm -f $BUILD/rot-store.img $BUILD/rot-vars.fd $BUILD/rot-esp.img $BUILD/rot[123].log $TESTKEY
openssl genpkey -algorithm ed25519 -out $TESTKEY 2>/dev/null || { echo "no openssl"; exit 1; }

make -s VERSION=9.9.9 >/dev/null || { echo "make 9.9.9 failed"; exit 1; }
sh tools/sign-release.sh >/dev/null || { echo "package failed"; exit 1; }
cp build/update.pkg $SRV/update.pkg.release
RELEASE_KEY=$TESTKEY sh tools/sign-release.sh >/dev/null || { echo "test-key package failed"; exit 1; }
cp build/update.pkg $SRV/update.pkg.test
sh tools/sign-rotation.sh release-key.pem $TESTKEY "test rotation" >/dev/null || { echo "rotation failed"; exit 1; }
cp build/rotate $SRV/rotate.signed
make -s >/dev/null || exit 1
sh tools/sign-release.sh >/dev/null    # leave build/update.pkg as the current version's

cat > $BUILD/rot-serve.py <<'PY'
import sys, os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
os.chdir(sys.argv[2])
class H(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *a):
        sys.stderr.write((fmt % a) + "\n"); sys.stderr.flush()
ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
python3 $BUILD/rot-serve.py $PORT $SRV >$BUILD/rot-srv.log 2>&1 &
SRV_JOB=$!
sleep 1
FWD="user,id=n0,guestfwd=tcp:10.0.2.100:80-cmd:nc -N 127.0.0.1 $PORT"
printf '9.9.9\n' > $SRV/version

boot() {   # $1 log; the monitor script on stdin
    qemu-system-x86_64 $QEMU_BASE \
      -drive if=pflash,format=raw,file=$BUILD/rot-vars.fd \
      -drive format=raw,file=$BUILD/rot-esp.img \
      -drive id=store,file=$BUILD/rot-store.img,format=raw,if=none \
      -device ide-hd,drive=store,bus=ide.1 \
      -device e1000,netdev=n0 -netdev "$FWD" -serial file:$1 >/dev/null 2>&1
}
point_at_source() {
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write update | auto http://10.0.2.100"
    say "back"
    say "back"
    sleep 1
}

# --- boot 1: rotation offered with the release-key package: rotates, then refuses it ---
fresh_store $BUILD/rot-store.img
fresh_vars $BUILD/rot-vars.fd
cp build/esp.img $BUILD/rot-esp.img
cp $SRV/rotate.signed $SRV/rotate
cp $SRV/update.pkg.release $SRV/update.pkg
LOG1=$BUILD/rot1.log
{
    bootwait $LOG1
    point_at_source
    say "update check"
    waitlog $LOG1 'signature did not verify\|a signed kernel is installed' 220
    # the rotation is a settings line; let a generation carry it to the disk
    n=$(count $LOG1 'generation [0-9]* written')
    waitcount $LOG1 'generation [0-9]* written' $((n + 1)) 60
    sleep 1
    echo quit
} | boot $LOG1

# --- boot 2: the same store, the test-key package: installed ---
cp $SRV/update.pkg.test $SRV/update.pkg
fresh_vars $BUILD/rot-vars.fd
LOG2=$BUILD/rot2.log
{
    bootwait $LOG2
    point_at_source
    say "update check"
    waitlog $LOG2 'a signed kernel is installed\|signature did not verify' 220
    waitlog $LOG2 'EreBUS 9.9.9' 90
    sleep 1
    echo quit
} | boot $LOG2

# --- boot 3: a fresh store, no rotation, the test-key package: refused ---
rm -f $SRV/rotate
fresh_store $BUILD/rot-store.img
fresh_vars $BUILD/rot-vars.fd
cp build/esp.img $BUILD/rot-esp.img
LOG3=$BUILD/rot3.log
{
    bootwait $LOG3
    point_at_source
    say "update check"
    waitlog $LOG3 'signature did not verify\|a signed kernel is installed' 220
    sleep 1
    echo quit
} | boot $LOG3

kill $SRV_JOB 2>/dev/null
wait 2>/dev/null

echo "--- boot 1 (rotation, then the old key's package) ---"
grep -a 'update:\|attention: update' $LOG1 | cut -c1-120 | tail -6
echo "--- boot 2 (the new key's package) ---"
grep -a 'update:\|attention: update\|EreBUS ' $LOG2 | cut -c1-120 | tail -8
echo "--- boot 3 (no rotation, the new key's package) ---"
grep -a 'update:\|attention: update' $LOG3 | cut -c1-120 | tail -4
echo "--- the checks ---"
ok=1
grep -aq 'the release key was rotated' $LOG1 && echo "the rotation was read and applied" || { echo "FAILED: no rotation applied"; ok=0; }
grep -aq 'signature did not verify' $LOG1 && echo "the previous key's package was refused after it" || { echo "FAILED: the old key's package was not refused"; ok=0; }
grep -aq 'a signed kernel is installed' $LOG1 && { echo "FAILED: a package was installed on boot 1"; ok=0; } || echo "and nothing was installed"
grep -aq 'a signed kernel is installed' $LOG2 && echo "the new key's package was installed" || { echo "FAILED: the new key's package was not installed"; ok=0; }
grep -aq 'EreBUS 9.9.9' $LOG2 && echo "and the machine rebooted into it" || { echo "FAILED: did not come up as 9.9.9"; ok=0; }
grep -aq 'the release key was rotated' $LOG2 && { echo "FAILED: the rotation was applied twice"; ok=0; } || echo "the rotation read again changed nothing"
grep -aq 'signature did not verify' $LOG3 && echo "without the rotation the new key is nobody's" || { echo "FAILED: the new key was accepted without a rotation"; ok=0; }
[ $ok = 1 ] && echo "a signed rotation moves the machine to a new release key" || echo "rotation FAILED"
