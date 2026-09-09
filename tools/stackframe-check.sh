#!/bin/sh
# stackframe-check.sh -- guard the self-hosting build against a stack
# overflow.
#
# The machine's own compiler walks the code tree with one recursive call
# per level of nesting (gen_stmt into gen_stmt, gen_expr into gen_expr).
# The build thread that runs it has a bounded stack, so the frame each of
# those functions reserves has to stay small: a fat per-level frame runs a
# deeply nested source into the thread's guard page and faults. That is a
# property of how clang compiled cc.c, invisible on the host (where the
# compiler has megabytes of stack) and only felt on the machine.
#
# This reads the frame those functions reserve in the built kernel and
# fails if any exceeds the bound, so a change that fattens the code walk is
# caught here at build time rather than as a double fault on the machine.
cd "$(dirname "$0")/.."
ELF=${1:-build/kernel.elf}
LIMIT=${LIMIT:-256}     # bytes of stack a single code-walk frame may reserve
[ -f "$ELF" ] || { echo "no $ELF -- build the kernel first"; exit 2; }

DIS=$(mktemp)
trap 'rm -f "$DIS"' EXIT
objdump -d "$ELF" > "$DIS" 2>/dev/null || { echo "objdump failed on $ELF"; exit 2; }

# The prologue's 'sub $0xNN,%rsp' is the frame. A function that only pushes
# reserves nothing and has no such line; that reads as a frame of zero.
frame_hex() {
    awk -v f="<$1>:" '
        index($0, f) { infn = 1; n = 0; next }
        infn && $0 == "" { exit }
        infn {
            if (++n > 24) exit
            if (match($0, /sub +\$0x[0-9a-f]+,%rsp/)) {
                match($0, /0x[0-9a-f]+/)
                print substr($0, RSTART, RLENGTH); exit
            }
        }' "$DIS"
}

rc=0
for fn in gen_stmt gen_expr gen_args gen_u128_binop; do
    if ! grep -q "<$fn>:" "$DIS"; then
        echo "FAILED: $fn not found in $ELF"; rc=1; continue
    fi
    hex=$(frame_hex "$fn")
    frame=0
    [ -n "$hex" ] && frame=$(( hex ))
    if [ "$frame" -gt "$LIMIT" ]; then
        echo "FAILED: $fn reserves $frame bytes of stack (limit $LIMIT)"; rc=1
    else
        echo "ok: $fn frame $frame bytes"
    fi
done

[ $rc = 0 ] && echo "code-walk stack frames within bounds ($LIMIT bytes)" \
            || echo "code-walk stack frames TOO LARGE -- the self-build may overflow on the machine"
exit $rc
