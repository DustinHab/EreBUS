#!/bin/sh
# kvm-battery.sh -- the pre-release full gate: the regression battery under
# KVM, then the self-build check (tools/selfbuild.sh build), which compiles
# every kernel source with the machine's own compiler on the host.
#
# The battery hard-gates the release (exit non-zero on any failure).
#
# The self-build reports every file it cannot compile, not just the first.
# Two are known and documented (README, Known limits): bn.c uses 128-bit
# integers and ap_boot.S is 16-bit real-mode assembly, neither of which
# the machine's compiler implements yet. Any OTHER failure is a regression
# and fails the gate -- which is how a broken directive/comment or a lost
# header shows up (both have happened). When the two limits are lifted the
# self-build succeeds outright and tools/selfkernel.sh can boot the result.
cd "$(dirname "$0")/.."
rc=0

sh tools/battery.sh 2>&1 | tee build/kvm-battery.log
if ! grep -q 'battery done:.* 0 tests with failures' build/kvm-battery.log; then
    echo "battery reported failures"
    rc=1
fi

echo "== self-build (host, every source)"
sb=$(sh tools/selfbuild.sh build 2>&1)
echo "$sb" | grep -E 'FAILED|link:|did not become|kernel.elf'
got=$(echo "$sb" | sed -n 's/^FAILED  \([^:]*\):.*/\1/p' | sort)
known=$(printf '%s\n' kernel/arch/x86_64/ap_boot.S kernel/net/bn.c | sort)
if [ -z "$got" ]; then
    echo "self-build: every source compiled -- the documented limits are gone"
elif [ "$got" = "$known" ]; then
    echo "self-build: reaches the two documented limits (bn.c 128-bit, ap_boot.S 16-bit); no regression"
else
    echo "self-build: unexpected failures -- a regression beyond the known limits:"
    echo "$got"
    rc=1
fi

[ $rc = 0 ] && echo "== kvm-battery: green (battery; self-build at the documented limits)" \
            || echo "== kvm-battery: FAILED"
exit $rc
