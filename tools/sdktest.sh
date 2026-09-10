#!/bin/sh
# sdktest.sh -- a program written outside the kernel tree, against sdk/erebus.h, built and run on the machine.
# - the door is opened with a key typed into the settings; erebus.h and hello.c go in through it as texts of one list
# - the machine compiles hello beside its header and runs the image; a text is given to it
# - what it said must be in the log: hello, the clock, and the length of the text it was given
cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log
KEY=$BUILD/sdk_key
PORT=${SDKPORT:-2232}

rm -f $BUILD/teststore.img $LOG $KEY $KEY.pub $BUILD/sdk-*.txt $BUILD/sdk-ready $BUILD/sdk-done $BUILD/sdk-known
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd
ssh-keygen -q -t ed25519 -N '' -f $KEY
PUB=$(cut -d' ' -f1,2 $KEY.pub)

# The stream through the door: a list, the two texts, a compile, a run,
# a text given to the running program.
{
    printf 'make list sdk\n'
    printf 'go sdk\n'
    for f in sdk/erebus.h sdk/hello.c; do
        printf 'receive %s bytes as %s\n' "$(wc -c < $f)" "$(basename $f)"
        cat $f
    done
    printf 'make text words\n'
    printf 'go words\n'
    printf 'write forty-two bytes of words in a text\n'
    printf 'back\n'
    printf 'compile hello.c\n'
    printf 'run hello.c code\n'
    printf 'give words to hello.c code\n'
    printf 'back\n'
} > $BUILD/sdk-stream.txt

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write door | $PUB"
    say "back"
    say "back"
    touch $BUILD/sdk-ready
    waitfile $BUILD/sdk-done 120
    sleep 2
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0,hostfwd=tcp::$PORT-:22 \
  -serial file:$LOG >/dev/null 2>&1 &
Q_JOB=$!

waitfile $BUILD/sdk-ready 120 || echo "(the door was never opened)"
sleep 2
SSH="ssh -T -o StrictHostKeyChecking=no -o UserKnownHostsFile=$BUILD/sdk-known \
     -o IdentitiesOnly=yes -o ConnectTimeout=10 -o LogLevel=ERROR -p $PORT -i $KEY someone@127.0.0.1"
$SSH < $BUILD/sdk-stream.txt > $BUILD/sdk-out.txt 2>&1
waitlog $LOG 'user: given' 30
touch $BUILD/sdk-done
wait $Q_JOB 2>/dev/null

echo "--- the door said ---"
grep -a 'created\|assembly\|image\|running\|know\|error\|line' $BUILD/sdk-out.txt | head -8
echo "--- the program said ---"
grep -a 'user: ' $LOG | cut -c1-100
echo "--- the checks ---"
ok=1
grep -aq 'user: hello from the sdk' $LOG && echo "the program built against the sdk header and ran" || { echo "FAILED: the program did not say hello"; ok=0; }
grep -aq 'user: the clock answers' $LOG && echo "the clock call answered it" || { echo "FAILED: no clock"; ok=0; }
# the line written is 34 letters, and 'write' ends it with a newline: 35
grep -aq 'user: given 35 bytes' $LOG && echo "it read the text it was given, all 35 bytes" || { echo "FAILED: the given text was not read as 35 bytes"; ok=0; }
[ $ok = 1 ] && echo "a program from outside the tree runs on the machine through the written interface" || echo "sdk FAILED"
