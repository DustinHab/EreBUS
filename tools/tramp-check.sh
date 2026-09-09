#!/bin/sh
# tramp-check.sh -- the AP-trampoline in kernel/arch/x86_64/ap_boot.S is
# machine code the kernel copies to low memory and starts each core at. It
# is checked in as a byte table because the machine's own assembler has no
# 16-bit mode. This assembles the readable reference (tools/ap_boot_ref.S)
# with clang and confirms the table still equals it, so the two cannot
# drift apart unnoticed.
cd "$(dirname "$0")/.."
REF=tools/ap_boot_ref.S
TAB=kernel/arch/x86_64/ap_boot.S
CC=${CC:-clang}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

$CC -target x86_64-unknown-none-elf -c "$REF" -o "$tmp/ref.o" 2>"$tmp/err" || {
    echo "the reference did not assemble:"; cat "$tmp/err"; exit 2; }

# ap_tramp_start is the first byte of .rodata; ap_tramp_params its length in code.
h=$(nm "$tmp/ref.o" | awk '/ ap_tramp_params$/{print $1}')
n=$(( 0x$h ))
objcopy -O binary --only-section=.rodata "$tmp/ref.o" "$tmp/ref.bin"
od -An -v -tx1 -N "$n" "$tmp/ref.bin" | tr -s ' \t' '\n' | grep -v '^$' > "$tmp/ref.hex"

# the committed table: the .byte lines between the two labels
awk '/^ap_tramp_start:/{f=1} /^ap_tramp_params:/{f=0} f&&/\.byte/' "$TAB" \
  | grep -oE '0x[0-9a-fA-F][0-9a-fA-F]?' | sed 's/^0x//' \
  | tr 'A-F' 'a-f' | awk '{printf "%02s\n",$0}' > "$tmp/tab.hex"

rc=$(wc -l < "$tmp/ref.hex"); tc=$(wc -l < "$tmp/tab.hex")
if [ "$rc" -ne "$tc" ]; then
    echo "DRIFT: table has $tc bytes, reference $rc bytes"; exit 1
fi
if diff "$tmp/tab.hex" "$tmp/ref.hex" >"$tmp/d"; then
    echo "ap_boot.S trampoline table matches tools/ap_boot_ref.S ($n bytes)"
else
    echo "DRIFT: ap_boot.S byte table differs from tools/ap_boot_ref.S"
    paste "$tmp/tab.hex" "$tmp/ref.hex" | grep -nvE '^\S+\s+(\S+)\t\1' | head -20
    exit 1
fi
