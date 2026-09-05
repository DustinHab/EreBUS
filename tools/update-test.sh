#!/bin/sh
# update-test.sh -- signed self-update from a local release source, over http.
# A "newer" kernel (9.9.9) is built and packaged (build/update.pkg: magic, signature, version, kernel).
# The machine is pointed at a local server that qemu's guestfwd sends to the host.
#  boot 1: the package's signature is corrupted -> the machine refuses it, installs nothing.
#  boot 2: the real package -> the machine installs it and reboots; the boot banner then reads 9.9.9.
# Two boots because guestfwd forwards one connection per run; each boot's single check uses it.
cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/upd.log
PORT=${UPDPORT:-8099}
SRV=$BUILD/upd-srv

rm -rf $SRV; mkdir -p $SRV
rm -f $BUILD/upd-store.img $BUILD/upd-vars.fd $BUILD/upd-esp.img $LOG

# Build and package the newer kernel, then keep a good and a bad package.
make -s VERSION=9.9.9 >/dev/null || { echo "make 9.9.9 failed"; exit 1; }
sh tools/sign-release.sh >/dev/null || { echo "package failed"; exit 1; }
cp build/update.pkg $SRV/update.pkg.good
# a bad package: flip a byte inside the signature (offset 8..71)
python3 - "$SRV/update.pkg.good" "$SRV/update.pkg" <<'PY'
import sys
b = bytearray(open(sys.argv[1],'rb').read()); b[20] ^= 0xff
open(sys.argv[2],'wb').write(b)
PY

# Restore the current-version kernel for the machine to boot from.
make -s >/dev/null || exit 1
fresh_store $BUILD/upd-store.img
fresh_vars $BUILD/upd-vars.fd
cp build/esp.img $BUILD/upd-esp.img

# The release server on the host loopback; the guest reaches it at
# 10.0.2.100:80, which qemu's guestfwd sends to this port on the host.
cat > $BUILD/upd-serve.py <<'PY'
import sys, os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
os.chdir(sys.argv[2])
class H(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *a):
        sys.stderr.write((fmt % a) + "\n"); sys.stderr.flush()
ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
PY
python3 $BUILD/upd-serve.py $PORT $SRV >$BUILD/upd-srv.log 2>&1 &
SRV_JOB=$!
sleep 1

NET="-device e1000,netdev=n0 -netdev user,id=n0,guestfwd=tcp:10.0.2.100:80-tcp:127.0.0.1:$PORT"
LOG1=$BUILD/upd1.log
LOG2=$BUILD/upd2.log
rm -f $LOG1 $LOG2

# --- boot 1: a bad signature must be refused ---
{
    bootwait $LOG1
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write update | auto http://10.0.2.100"
    say "back"
    say "back"
    sleep 1
    say "update check"
    waitlog $LOG1 'signature did not verify\|a signed kernel is installed' 220
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/upd-vars.fd \
  -drive format=raw,file=$BUILD/upd-esp.img \
  -drive id=store,file=$BUILD/upd-store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  $NET -serial file:$LOG1 >/dev/null 2>&1

# --- boot 2: the good package installs and reboots into 9.9.9 ---
cp $SRV/update.pkg.good $SRV/update.pkg
fresh_vars $BUILD/upd-vars.fd
{
    bootwait $LOG2
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write update | auto http://10.0.2.100"
    say "back"
    say "back"
    sleep 1
    say "update check"
    waitlog $LOG2 'a signed kernel is installed' 220
    waitlog $LOG2 'EreBUS 9.9.9' 90
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/upd-vars.fd \
  -drive format=raw,file=$BUILD/upd-esp.img \
  -drive id=store,file=$BUILD/upd-store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  $NET -serial file:$LOG2 >/dev/null 2>&1

kill $SRV_JOB 2>/dev/null
wait 2>/dev/null

echo "--- boot 1 (bad signature) ---"
grep -a 'update:\|attention: update' $LOG1 | cut -c1-120 | tail -8
echo "--- boot 2 (good package) ---"
grep -a 'update:\|attention: update\|EreBUS ' $LOG2 | cut -c1-120 | tail -10
echo "--- the checks ---"
ok=1
grep -aq 'signature did not verify' $LOG1 && echo "a package with a bad signature was refused" || { echo "FAILED: bad signature not refused"; ok=0; }
grep -aq 'a signed kernel is installed' $LOG1 && { echo "FAILED: a bad package was installed"; ok=0; } || echo "and nothing was installed from it"
grep -aq 'a signed kernel is installed' $LOG2 && echo "a correctly signed newer kernel was installed" || { echo "FAILED: good kernel not installed"; ok=0; }
grep -aq 'EreBUS 9.9.9' $LOG2 && echo "the machine rebooted into the updated kernel" || { echo "FAILED: did not come up as 9.9.9"; ok=0; }
[ $ok = 1 ] && echo "signed self-update installs a newer release and refuses a forged one" || echo "self-update FAILED"
