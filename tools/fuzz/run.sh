#!/bin/sh
# run.sh -- fuzz the language tools for a while, seeded with the
# kernel's own sources, its assembly files, and objects.
#
#   sh tools/fuzz/run.sh [seconds per tool]

cd "$(dirname "$0")/../.."
SECS=${1:-60}
OUT=build/fuzz
mkdir -p $OUT/corpus $OUT/crashes
clang -O1 -g -std=c11 -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer \
    -Wno-unused-function -Wno-incompatible-library-redeclaration -Ikernel/include \
    -o $OUT/lang_fuzz tools/fuzz/lang_fuzz.c kernel/lang/cc.c kernel/lang/asm.c \
    kernel/lang/ld.c kernel/lang/gnu.c 2>&1 | grep -E 'error' | head
[ -x $OUT/lang_fuzz ] || { echo "the fuzzer does not build"; exit 1; }

# seeds: a leading byte picks the tool
i=0
for f in tools/cc/proof.c kernel/lib/string.c kernel/net/sha256.c kernel/lang/big.c; do
    { printf '\000'; cat "$f"; } > $OUT/corpus/c$i; i=$((i + 1))
done
for f in tools/cc/proof.c.asm; do
    [ -f "$f" ] && { printf '\001'; cat "$f"; } > $OUT/corpus/a$i; i=$((i + 1))
done
for f in kernel/arch/x86_64/start.S kernel/user/agent.S kernel/arch/x86_64/isr.S; do
    { printf '\002'; cat "$f"; } > $OUT/corpus/g$i; i=$((i + 1))
done
if [ -f build/self/string.obj ]; then { printf '\003'; cat build/self/string.obj; } > $OUT/corpus/o$i; fi

echo "fuzzing for $SECS seconds..."
ASAN_OPTIONS=detect_leaks=0 $OUT/lang_fuzz -max_total_time=$SECS -max_len=65536 \
    -artifact_prefix=$OUT/crashes/ -print_final_stats=1 $OUT/corpus 2>&1 \
    | grep -E 'ERROR|SUMMARY|runtime error|stat::|#[0-9]+ +(NEW|pulse|DONE)|crash|timeout|artifact' | tail -25
ls $OUT/crashes 2>/dev/null | head
