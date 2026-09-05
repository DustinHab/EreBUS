#!/bin/sh
# run.sh -- fuzz the parsers for a while under the address and undefined-behaviour sanitizers.
# - tools: lang (compiler, assembler, linker), pki (certificates, keys, signatures), html (the page renderer)
# - seeds: the kernel's own sources and objects for lang, the certificate fixtures for pki, the manual for html
# - corpora and crashes under build/fuzz/<tool>/; a crash file replays with build/fuzz/<tool>/<tool>_fuzz <file>
#
#   sh tools/fuzz/run.sh [seconds per tool] [tool ...]
cd "$(dirname "$0")/../.."
SECS=${1:-60}
shift 2>/dev/null
TOOLS=${*:-"lang pki html"}
CC="clang -O1 -g -std=c11 -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer \
    -Wno-unused-function -Wno-incompatible-library-redeclaration -I. -Ikernel/include"

build() {   # build <tool> <sources...>
    t=$1; shift
    mkdir -p build/fuzz/$t/corpus build/fuzz/$t/crashes
    $CC -o build/fuzz/$t/${t}_fuzz tools/fuzz/${t}_fuzz.c "$@" 2>&1 | grep -E 'error' | head
    [ -x build/fuzz/$t/${t}_fuzz ] || { echo "the $t fuzzer does not build"; return 1; }
}

seed_lang() {
    C=build/fuzz/lang/corpus
    i=0
    for f in tools/cc/proof.c kernel/lib/string.c kernel/net/sha256.c kernel/lang/big.c; do
        { printf '\000'; cat "$f"; } > $C/c$i; i=$((i + 1))
    done
    [ -f tools/cc/proof.c.asm ] && { printf '\001'; cat tools/cc/proof.c.asm; } > $C/a$i; i=$((i + 1))
    for f in kernel/arch/x86_64/start.S kernel/user/agent.S kernel/arch/x86_64/isr.S; do
        { printf '\002'; cat "$f"; } > $C/g$i; i=$((i + 1))
    done
    [ -f build/self/string.obj ] && { printf '\003'; cat build/self/string.obj; } > $C/o$i
}

seed_pki() {
    C=build/fuzz/pki/corpus
    i=0
    for f in tools/pki/fixtures/*.der build/pki/*.der; do
        [ -f "$f" ] || continue
        { printf '\000'; cat "$f"; } > $C/c$i
        { printf '\002'; openssl x509 -in "$f" -inform DER -pubkey -noout 2>/dev/null | openssl pkey -pubin -outform DER 2>/dev/null; } > $C/k$i
        { printf '\005'; cat "$f"; } > $C/d$i
        i=$((i + 1))
    done
    python3 - $C/chain0 tools/pki/fixtures/github-leaf.der tools/pki/fixtures/github-e36.der <<'PY'
import sys, struct
out = bytearray(b"\x01")
for f in sys.argv[2:]:
    b = open(f, "rb").read(); out += struct.pack(">H", len(b)) + b
open(sys.argv[1], "wb").write(out)
PY
}

seed_html() {
    C=build/fuzz/html/corpus
    printf '<html><body><h1>Title</h1><p>Some <b>bold</b> and <a href="/x?y=1">a link</a>.</p><ul><li>one</li><li>two</li></ul><table><tr><td>a</td><td>b</td></tr></table><form action="/go" method="get"><input name="q" value="hi"><input type="submit" value="Go"></form><pre>  kept\n spaces</pre><blockquote>quoted</blockquote>&amp;&#65;&lt;</body></html>' > $C/page0
    head -c 20000 MANUAL.md > $C/manual0
}

run() {   # run <tool> <max_len>
    t=$1
    echo "=== $t: fuzzing for $SECS s"
    ASAN_OPTIONS=detect_leaks=0 build/fuzz/$t/${t}_fuzz -max_total_time=$SECS -max_len=$2 -timeout=20 \
        -artifact_prefix=build/fuzz/$t/crashes/ -print_final_stats=1 build/fuzz/$t/corpus 2>&1 \
        | grep -E 'ERROR|SUMMARY|runtime error|stat::number|stat::new|crash|timeout|artifact' | tail -12
    ls build/fuzz/$t/crashes 2>/dev/null | head
}

for t in $TOOLS; do
    case $t in
        lang) build lang kernel/lang/cc.c kernel/lang/asm.c kernel/lang/ld.c kernel/lang/gnu.c && seed_lang && run lang 65536 ;;
        pki)  build pki kernel/net/asn1.c kernel/net/bn.c kernel/net/p256.c kernel/net/rsa.c kernel/net/x509.c kernel/net/sha256.c && seed_pki && run pki 8192 ;;
        html) build html kernel/gfx/html.c && seed_html && run html 32768 ;;
        *) echo "no such tool: $t" ;;
    esac
done
