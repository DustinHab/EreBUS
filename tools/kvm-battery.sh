#!/bin/sh
# kvm-battery.sh -- the pre-release full gate: the regression battery under
# KVM, then the self-hosting proof, on the machine and not only on the host.
#
# 1. The 37-test battery hard-gates the release.
# 2. The code-walk stack frames are checked in the built kernel: the
#    machine's own compiler recurses per level of nesting, and a fat
#    per-level frame overflows the build thread's stack on the machine
#    (tools/stackframe-check.sh). This is cheap and catches at build time
#    what would otherwise be a double fault only seen on the machine.
# 3. The host self-build (tools/selfbuild.sh build) compiles every source
#    with the machine's own tools and reports all that fail at once, then
#    boots the result. This is the fast, full blocker list.
# 4. The on-machine self-build (tools/selfkernel.sh) is the real proof: the
#    sources go through the door onto the emulated machine, it builds,
#    installs and boots its own kernel there -- on its own 128 KiB build
#    stack, not the host compiler's megabytes -- and that kernel's own
#    compiler builds a kernel again. The host build cannot show this; it was
#    the missing test behind an earlier false "self-hosting is whole" claim.
cd "$(dirname "$0")/.."
rc=0

sh tools/battery.sh 2>&1 | tee build/kvm-battery.log
if ! grep -q 'battery done:.* 0 tests with failures' build/kvm-battery.log; then
    echo "battery reported failures"
    rc=1
fi

echo "== stack frames of the code walk (a self-build overflow guard)"
if ! sh tools/stackframe-check.sh; then
    echo "the compiler's code-walk frames are too large -- the self-build may overflow"
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

echo "== on-machine self-build (through the door: build, install, boot, build again)"
sh tools/kvm.sh tools/selfkernel.sh 2>&1 | tee build/selfkernel.log
if ! grep -q 'the machine builds, installs and boots its own kernel through the door' build/selfkernel.log; then
    echo "on-machine self-build failed:"
    grep -E 'FAILED|exception|double fault|stack overflow' build/selfkernel.log build/selfk.log | head
    rc=1
fi

[ $rc = 0 ] && echo "== kvm-battery: green (battery, and a self-built kernel that boots on the machine)" \
            || echo "== kvm-battery: FAILED"
exit $rc
