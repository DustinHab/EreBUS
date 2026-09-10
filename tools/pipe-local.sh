#!/bin/sh
# pipe-local.sh -- discovery answers the own network and known nodes, not the world.
# - one machine on a multicast wire, its address claimed; discovery | local (the default, written to be sure)
# - a probe from another network (10.7.7.7) sends a SEEK: no HERE may come back, and the console says why
# - a probe from the own network (10.9.9.99) sends a SEEK: a HERE comes back, so the wire and the probe are known to work
# - the probe is tools/pipe-probe.py: not EreBUS, just frames on the wire
cd "$(dirname "$0")/.."
. tools/testlib.sh
LOG=$BUILD/serial.log
MCAST=230.0.0.1
MPORT=${MPORT:-9410}
MAC=52:54:00:aa:99:20

rm -f $BUILD/teststore.img $LOG $BUILD/probe-far.txt $BUILD/probe-near.txt $BUILD/pipe-local-ready
fresh_store $BUILD/teststore.img
fresh_vars $BUILD/test-vars.fd

{
    bootwait $LOG
    keys tab tab tab tab tab pause
    say "go system"
    say "go settings"
    say "write address | 10.9.9.20"
    say "write discovery | local"
    say "back"
    say "back"
    waitlog $LOG '10.9.9.20 by claim' 40
    sleep 1
    touch $BUILD/pipe-local-ready
    # the probes run outside; wait for both to have spoken
    waitfile $BUILD/probe-near.txt 60
    sleep 1
    echo quit
} | qemu-system-x86_64 $QEMU_BASE \
  -drive if=pflash,format=raw,file=$BUILD/test-vars.fd \
  -drive format=raw,file=$BUILD/esp.img \
  -drive id=store,file=$BUILD/teststore.img,format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device e1000,netdev=n0,mac=$MAC \
  -netdev socket,id=n0,mcast=$MCAST:$MPORT \
  -object filter-dump,id=fd0,netdev=n0,file=$BUILD/pipe-local.dump \
  -serial file:$LOG >/dev/null 2>&1 &
Q_JOB=$!

waitfile $BUILD/pipe-local-ready 120 || echo "(the machine never claimed its address)"

# A stranger from another network: a SEEK, and nothing may come back.
python3 tools/pipe-probe.py $MCAST $MPORT 10.7.7.7 52:54:00:77:77:07 10.9.9.20 $MAC 5 > $BUILD/probe-far.txt 2>&1
far_rc=$?
# A machine on the own network: a SEEK, and a HERE must come back.
python3 tools/pipe-probe.py $MCAST $MPORT 10.9.9.99 52:54:00:99:99:99 10.9.9.20 $MAC 8 > $BUILD/probe-near.txt 2>&1
near_rc=$?

wait $Q_JOB 2>/dev/null

echo "--- the machine ---"
grep -a 'net:.*claim\|pipe:' $LOG | cut -c1-120
echo "--- the probes ---"
echo "from another network: $(cat $BUILD/probe-far.txt)"
echo "from the own network: $(cat $BUILD/probe-near.txt)"
echo "--- the checks ---"
ok=1
[ $far_rc -ne 0 ] && echo "a seek from another network got no answer" || { echo "FAILED: a stranger's seek was answered"; ok=0; }
grep -aq 'a seek from 10.7.7.7 was not answered (discovery | local)' $LOG && echo "and the console said so" || { echo "FAILED: the refusal was not said"; ok=0; }
[ $near_rc -eq 0 ] && echo "a seek from the own network was answered" || { echo "FAILED: the own network's seek got no answer"; ok=0; }
[ $ok = 1 ] && echo "discovery answers the own network and not the world" || echo "discovery FAILED"
