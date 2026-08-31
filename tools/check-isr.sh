#!/bin/sh
# check-isr.sh -- verify the interrupt stub table.
#
# The C side computes the address of vector n as isr_stubs + n * 16.
# If a stub ever grew past 16 bytes, every vector above it would point
# into the middle of an instruction, and the failure would look like
# random corruption rather than a build problem. So check it.
#
# Also confirms that the ten vectors which receive an error code from
# the processor do NOT push a zero of their own, and that all others do.

set -e
ELF=${1:-build/kernel.elf}

BASE=$(nm "$ELF" | awk '/ isr_stubs$/ { print $1 }')
COMMON=$(nm "$ELF" | awk '/ isr_common$/ { print $1 }')

if [ -z "$BASE" ]; then
    echo "check-isr: isr_stubs not found in $ELF" >&2
    exit 1
fi

echo "isr_stubs at 0x$BASE, isr_common at 0x$COMMON"

# Vectors the processor pushes an error code for.
ERRVEC=" 8 10 11 12 13 14 17 21 29 30 "

fail=0
objdump -d "$ELF" > /tmp/erebus-isr.txt

vec=0
while [ $vec -lt 256 ]; do
    addr=$(printf '%x' $((0x$BASE + vec * 16)))

    # First instruction of this stub.
    first=$(grep -A1 "^  *$addr:" /tmp/erebus-isr.txt | head -1 | \
            sed 's/.*\t//')

    case "$ERRVEC" in
        *" $vec "*) want_push0=0 ;;
        *)          want_push0=1 ;;
    esac

    case "$first" in
        "push   \$0x0"*)  got_push0=1 ;;
        *)                got_push0=0 ;;
    esac

    # Vector 0 legitimately pushes 0 twice (zero fill plus vector 0),
    # so only the mismatch direction is interesting.
    if [ "$want_push0" != "$got_push0" ] && [ "$vec" != "0" ]; then
        echo "check-isr: vector $vec at 0x$addr starts with '$first'," \
             "expected push0=$want_push0" >&2
        fail=1
    fi

    vec=$((vec + 1))
done

rm -f /tmp/erebus-isr.txt

if [ $fail -eq 0 ]; then
    echo "check-isr: all 256 stubs land on their 16-byte slot"
else
    exit 1
fi
