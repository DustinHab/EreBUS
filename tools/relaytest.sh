#!/bin/sh
# relaytest.sh -- one program hands another a capability, weakened on the way.
# - a note is made and filled; the courier is given the agent's letter box (send-only)
# - the note is handed to the courier read-write; the courier passes it on read-only (its own decision, enforced by the kernel)
# - the agent reads the note and its write is refused; no step is delegated by the shell
# - coordinates follow the root's layout: seed objects in 0-3, programs in 4, system in 5, arrivals in 6, the new note in 7;
#   home's add sits at y 269; in the palette the carries start at add+440 and move by 22 px per row

cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/relay-serial.log
rm -f $BUILD/teststore.img $LOG $BUILD/relay.ppm
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

# the pointer to the corner, then across to the add column
to_add() {
    keys corner
    i=0; while [ $i -lt 17 ]; do keys m100,0; i=$((i + 1)); done
}

{
    bootwait $LOG
    # add a text at home, open it, type the note, step back out
    to_add
    keys m0,100 m0,100 m0,69 click
    keys m0,22 click ret
    keys right
    keys p a s s spc i t left
    # to the courier in the programs list
    keys up up up right down right
    # its add: the agent carry gives the courier the agent's letter box
    to_add
    keys m0,100 m0,15 click
    keys m0,100 m0,100 m0,100 m0,100 m0,100 m0,100 m0,100 m0,4 click ret
    waitlog $LOG 'user: letter box received' 20
    # its add again: the note carry, read-write; the courier passes it on read-only
    to_add
    keys m0,100 m0,39 click
    keys m0,100 m0,100 m0,100 m0,100 m0,100 m0,100 m0,80 click ret
    waitlog $LOG 'user: write refused' 20
    sleep 1
    echo "screendump $BUILD/relay.ppm"
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0 -netdev user,id=n0 \
  -serial file:$LOG >/dev/null 2>&1

python3 tools/ppm2png.py $BUILD/relay.ppm $BUILD/relay.png 2>/dev/null

echo "--- what the programs said, in order ---"
grep -ao 'user: .*' $LOG | sed 's/^user: //; s/[[:space:]]*$//' | grep -v '^$' | sed 's/^/    /'
echo "--- the checks ---"
ok=1
got=$(grep -ao 'user: .*' $LOG | grep -o 'letter box received\|passed on read-only\|reads:  pass it\|write refused' | tr '\n' '|')
want='letter box received|passed on read-only|reads:  pass it|write refused|'
if [ "$got" = "$want" ]; then
    echo "the courier took the letter box, passed the note on read-only, and the agent could read but not write"
else
    echo "FAILED: the sequence was: $got"
    ok=0
fi
[ $ok = 1 ] && echo "a capability passed between programs arrives weakened" || echo "the relay test FAILED"
