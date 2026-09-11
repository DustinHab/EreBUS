#!/bin/sh
# battery.sh -- the regression tests: one build, then the tests in parallel, each in its own directory.
# - every test runs with BUILD=$PAR/<test>/ on the Linux file system (default /tmp/erebus-par; /mnt/c stalls under parallel disk writes),
#   with esp.img and stick.img copied in; logs, screenshots and QEMU stderr are copied back to build/par/<test>/ afterwards
# - LANES (default 6) tests run at once; renew runs afterwards alone, because it runs make
# - a test is stopped after TEST_LIMIT seconds (default 480) and its QEMUs killed; a failed or stopped test runs once more, marked "2nd try"
# - KVM when /dev/kvm is writable (NOKVM=1 forces TCG); tools/testlib.sh has the waits and the key timing
# - prints seconds per test, FAILED lines, and the total; output per test in build/par/<test>.out, everything in build/battery.log
# - 'sh tools/battery.sh --one <test>' runs one test that way (the parallel lanes use it)
cd "$(dirname "$0")/.."
. tools/testlib.sh
LANES=${LANES:-6}
TEST_LIMIT=${TEST_LIMIT:-480}
PAR=${PAR:-/tmp/erebus-par}
OUT=build/par

PARALLEL="termtest usbtest usbhub usbplug asmtest cctest cctest2 sshtest sshmulti sshrekey pipe-two pipe-local pipe-update pipe-input pipe-code pipe-quorum pipe-rotate pipe-vouch pipe-task ssh-task sdktest relay agent persist powerloss limits stick usbstick usbunplug foreign settle settlefree install nictest wifi pkitest irqtest webtest"
# renew rebuilds the kernel, update-test and rotate-test rebuild it as 9.9.9, oldstore and
# oldpipe build the last released tag once, decoder-fault rebuilds the decoder program with its
# deliberate faults: none of these may share build/ with a lane.
SERIAL="renew update-test rotate-test oldstore oldpipe decoder-fault tlstest"

script_of() {
    case "$1" in
        stick) echo "sh tools/sticktest.sh" ;;
        foreign) echo "sh tools/foreigndisk.sh" ;;
        settle) echo "sh tools/settletest.sh" ;;
        install) echo "sh tools/installtest.sh" ;;
        renew) echo "sh tools/renewtest.sh" ;;
        wifi) echo "sh tools/wifitest.sh" ;;
        relay|agent|persist) echo "sh tools/$1test.sh" ;;
        *) echo "sh tools/$1.sh" ;;
    esac
}

# attempt <test>: one run in a fresh directory; output to $OUT/<test>.out. Returns 1 on FAILED or timeout.
attempt() {
    t=$1
    d=$PAR/$t
    rm -rf $d
    mkdir -p $d $OUT/$t
    cp build/esp.img $d/esp.img
    case "$t" in stick|usbstick|usbunplug|settle|settlefree|install) cp build/stick.img $d/stick.img ;; esac
    # A few lanes run on several processors, so the parallel scheduler,
    # the per-cpu paths and shared state (the journal, ports) are exercised
    # under real contention rather than only on a hand-started -smp boot.
    # Single-guest lanes only: a test that itself starts several machines
    # (the pipe/quorum tests) would multiply vCPUs and oversubscribe the
    # host, so those stay single-core; these run the kernel and its user
    # programs hard on one guest across four processors.
    smp=
    case "$t" in termtest|cctest2|irqtest) smp=4 ;; esac
    BUILD=$d OLDESP=build/esp-older.img SMP=$smp timeout -k 10 $TEST_LIMIT $(script_of $t) > $OUT/$t.out 2>&1
    rc=$?
    pkill -f "$d/" 2>/dev/null
    [ $rc = 124 ] && echo "FAILED: stopped after $TEST_LIMIT s" >> $OUT/$t.out
    cp $d/*.png $d/*.log $d/*.err $d/*.txt $OUT/$t/ 2>/dev/null
    grep -q 'FAILED' $OUT/$t.out && return 1
    return 0
}

if [ "$1" = "--one" ]; then
    t=$2
    t0=$(date +%s)
    if attempt $t; then
        echo $(( $(date +%s) - t0 )) > $OUT/$t.time
    else
        mv $OUT/$t.out $OUT/$t.first.out
        attempt $t && echo "$(( $(date +%s) - t0 )) (2nd try)" > $OUT/$t.time || echo $(( $(date +%s) - t0 )) > $OUT/$t.time
    fi
    exit 0
fi

: > build/battery.log
T0=$(date +%s)
make -s >/dev/null || { echo "make failed"; exit 1; }
make -s task-tool >/dev/null || { echo "task-tool failed"; exit 1; }
sh tools/mkusb.sh >/dev/null || { echo "mkusb failed"; exit 1; }
# pipe-update boots one machine on a kernel called "older": built once here, before anything runs
make -s VERSION=older >/dev/null && cp build/esp.img build/esp-older.img && make -s >/dev/null
rm -rf $OUT $PAR
mkdir -p $OUT $PAR
echo "build done after $(( $(date +%s) - T0 )) s; $(echo $PARALLEL | wc -w) tests in $LANES lanes, then $(echo $SERIAL | wc -w) alone (kvm=$KVM, dirs under $PAR)"

for t in $PARALLEL; do echo $t; done | xargs -P $LANES -n 1 sh tools/battery.sh --one
T1=$(date +%s)
echo "parallel part done after $(( T1 - T0 )) s"

# renew rebuilds the kernel twice, so it runs alone.
for t in $SERIAL; do sh tools/battery.sh --one $t; done

for t in $PARALLEL $SERIAL; do
    { echo "== $t"; [ -f $OUT/$t.first.out ] && { echo "-- first try"; cat $OUT/$t.first.out; echo "-- second try"; }; cat $OUT/$t.out; echo; } >> build/battery.log
    printf '%-12s %4s s  ' "$t" "$(cat $OUT/$t.time 2>/dev/null || echo '?')"
    if grep -q 'FAILED' $OUT/$t.out; then grep 'FAILED' $OUT/$t.out | head -1; else echo "ok"; fi
done
echo "== battery done: $(( $(date +%s) - T0 )) s, $(grep -l FAILED $OUT/*.out 2>/dev/null | grep -vc first) tests with failures"
