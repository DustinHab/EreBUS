#!/bin/sh
# cctrial.sh -- kernel files through the machine's compiler, on the host.
#
# Measures the distance to a kernel built on the machine: each file is
# included whole under a borrowed main and pushed through cchost. The
# first thing the compiler cannot read is the line it names.

cd "$(dirname "$0")/.."
make -s cchost >/dev/null 2>&1

FILES="${*:-kernel/lib/base64.c kernel/lib/string.c kernel/obj/journal.c kernel/net/sha256.c kernel/net/x25519.c kernel/obj/settings.c kernel/lang/asm.c kernel/obj/object.c}"
ok=0; total=0
for f in $FILES; do
    total=$((total + 1))
    cp "$f" build/trial_target.c
    r=$(./build/cchost tools/cc/trial.c build/trial_target.c kernel/include/eb/*.h kernel/net/gf25519.h 2>&1 | tail -1)
    case "$r" in ok:*) ok=$((ok + 1)) ;; esac
    printf '%-28s %s\n' "$f" "$r"
done
echo "$ok of $total kernel files compile and assemble"
