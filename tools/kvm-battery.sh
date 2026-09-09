#!/bin/sh
# kvm-battery.sh -- the pre-release full gate: the regression battery under
# KVM, then the self-hosting test.
#
# The battery hard-gates the release. Then the machine's own compiler,
# assembler and linker build the whole kernel on the host
# (tools/selfbuild.sh build), and the self-built kernel is booted: it must
# reach idle and pass its certificate self-test, which is bn.c's 128-bit
# arithmetic exercised through rsa and ecdsa. A source the machine cannot
# compile, or a self-built kernel that does not boot, fails the gate.
cd "$(dirname "$0")/.."
rc=0

sh tools/battery.sh 2>&1 | tee build/kvm-battery.log
if ! grep -q 'battery done:.* 0 tests with failures' build/kvm-battery.log; then
    echo "battery reported failures"
    rc=1
fi

echo "== self-build (host: build the kernel with the machine's own tools)"
sh tools/selfbuild.sh build 2>&1 | tee build/selfbuild.log
if ! grep -q 'link: ok:' build/selfbuild.log; then
    echo "self-build did not link:"
    grep -E 'FAILED|did not become' build/selfbuild.log
    rc=1
else
    echo "== boot the self-built kernel"
    KERNEL=build/self/kernel.elf sh tools/kvm.sh tools/selfbuild.sh >/dev/null 2>&1
    if grep -qa 'kern: idle' build/self/serial.log &&
       grep -qa 'certificate checks ready' build/self/serial.log; then
        echo "self-build: the machine built its own kernel; it boots and its crypto self-tests pass"
    else
        echo "self-build: the self-built kernel did not boot cleanly"
        tail -20 build/self/serial.log
        rc=1
    fi
fi

[ $rc = 0 ] && echo "== kvm-battery: green (battery and a booting self-built kernel)" \
            || echo "== kvm-battery: FAILED"
exit $rc
