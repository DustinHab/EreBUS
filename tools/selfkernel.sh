#!/bin/sh
# selfkernel.sh -- kernel built on the machine from sources sent through the door, installed, booted.
# - door key typed through the screen once; sources streamed over one ssh shell session (tools/mkupload.sh)
# - 'build kernel' in the background; the list is polled for kernel.elf; 'install kernel.elf'; 'restart'
# - the booted kernel must report the version text sent with the sources
# - run under KVM: sh build/kvm.sh tools/selfkernel.sh (minutes on a real processor, hours under TCG)
cd "$(dirname "$0")/.."
BUILD=build
KEY=$HOME/.ssh/erebus_remote
[ -f $KEY ] || ssh-keygen -q -t ed25519 -N '' -f $KEY
PUB=$(cut -d' ' -f1,2 $KEY.pub)
SAYS="built on the machine itself"
PORT=2224

rm -f $BUILD/selfk-store.img $BUILD/selfk.log $BUILD/selfk-up.txt $BUILD/selfk-journal.txt $BUILD/selfk.stream
dd if=/dev/zero of=$BUILD/selfk-store.img bs=1M count=64 status=none
cp $BUILD/esp.img $BUILD/selfk-esp.img
cp /usr/share/OVMF/OVMF_VARS_4M.fd $BUILD/selfk-vars.fd
sh tools/mkupload.sh kernel "$SAYS" > $BUILD/selfk.stream

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
keys() { for k in "$@"; do case "$k" in pause) sleep 1.6 ;; *) echo "sendkey $k"; sleep 0.35 ;; esac; done; }
type_string() { s="$1"; while [ -n "$s" ]; do c="${s%"${s#?}"}"; s="${s#?}"; keys "$(key_name "$c")"; done; }

# The machine runs until told to quit through this pipe; the key is
# typed at the start, and the rest of the time the pipe just waits.
mkfifo $BUILD/selfk-mon 2>/dev/null
{
    sleep 24
    keys tab tab tab tab tab pause
    keys g o spc s y s t e m ret pause
    keys g o spc s e t t i n g s ret pause
    type_string "write door | $PUB"
    keys ret pause
    keys b a c k ret pause
    # hold the monitor open until the test is done
    while [ ! -f $BUILD/selfk-quit ]; do sleep 2; done
    echo quit
} | qemu-system-x86_64 -machine q35 -m 1024M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=$BUILD/selfk-vars.fd \
  -drive format=raw,file=$BUILD/selfk-esp.img \
  -vga none -device VGA,edid=on,xres=1280,yres=800 \
  -drive id=store,file=$BUILD/selfk-store.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -display none -monitor stdio \
  -serial file:$BUILD/selfk.log >/dev/null 2>&1 &
rm -f $BUILD/selfk-quit
sleep 82

SSH="ssh -T -o StrictHostKeyChecking=no -o UserKnownHostsFile=$BUILD/selfk-known \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR \
     -o ServerAliveInterval=5 -o ServerAliveCountMax=2 -p $PORT -i $KEY someone@127.0.0.1"
rm -f $BUILD/selfk-known

echo "--- the sources go in ---"
$SSH < $BUILD/selfk.stream > $BUILD/selfk-up.txt 2>&1
grep -c 'created here' $BUILD/selfk-up.txt | sed 's/$/ texts arrived/'
grep -v 'created here\|send .* bytes now' $BUILD/selfk-up.txt | head -5

# 'build kernel' runs in the background on the machine. The list is
# polled for kernel.elf; the journal (a ring that forgets under the
# door's own lines) is gathered for a line that names a failure.
build_and_wait() {
    $SSH build kernel 2>&1 | head -3
    start=$(date +%s)
    outcome=""
    : > $BUILD/selfk-journal.txt
    while [ -z "$outcome" ]; do
        sleep 30
        $SSH journal >> $BUILD/selfk-journal.txt 2>&1
        printf 'go kernel\nlook\n' | $SSH > $BUILD/selfk-list.txt 2>&1
        if grep -q 'kernel.elf  bytes' $BUILD/selfk-list.txt; then outcome=built;
        elif grep -q 'build: [A-Za-z0-9_]*\.[cSs]: ' $BUILD/selfk-journal.txt; then outcome=failed; fi
        if [ $(( $(date +%s) - start )) -gt 1800 ]; then outcome=timeout; fi
    done
    echo "outcome: $outcome after $(( $(date +%s) - start )) s"
    grep 'build:' $BUILD/selfk-journal.txt | sort -u | tail -6
}

echo "--- build ---"
build_and_wait

if [ "$outcome" = built ]; then
    echo "--- install and restart ---"
    # 'restart' takes the machine down under the session; the client
    # is given a minute rather than waiting for a close.
    printf 'go kernel\ninstall kernel.elf\nrestart\n' | timeout 60 $SSH 2>&1 | grep -v 'terminal\.' | head -6
    sleep 45
    # Second generation: the self-built kernel's own compiler builds
    # the kernel again. A compiler that compiles itself wrongly boots
    # and then fails here.
    echo "--- build again, with the self-built compiler ---"
    printf 'go kernel\nlet go kernel.elf\n' | $SSH 2>&1 | grep -v 'terminal\.' | head -2
    outcome1=$outcome
    build_and_wait
    outcome2=$outcome
    outcome=$outcome1
fi
touch $BUILD/selfk-quit
wait
rm -f $BUILD/selfk-quit $BUILD/selfk-mon

echo "--- what the machine said about itself, each boot ---"
grep -a 'EreBUS .* (x86_64)\|the installed kernel\|previous one' $BUILD/selfk.log | cut -c1-100
echo "--- the checks ---"
ok=1
[ "$(grep -c 'created here' $BUILD/selfk-up.txt)" -ge 139 ] && echo "every source went in through the door" || { echo "FAILED: sources missing"; ok=0; }
[ "$outcome" = built ] && echo "the machine built a kernel from them with its own tools" || { echo "FAILED: no kernel came of it ($outcome)"; ok=0; }
grep -aq "EreBUS $SAYS" $BUILD/selfk.log && echo "and booted it: it says it was $SAYS" || { echo "FAILED: the self-built kernel did not come up"; ok=0; }
[ "$outcome2" = built ] && echo "the self-built kernel built a kernel too (second generation)" || { echo "FAILED: the self-built kernel's compiler did not build a kernel ($outcome2)"; ok=0; }
[ $ok = 1 ] && echo "the machine builds, installs and boots its own kernel through the door" || echo "the self-built kernel FAILED"
echo DONE
