#!/bin/sh
# selfprograms.sh -- the programs the kernel carries (the picture decoder, the page renderer) built with the
# machine's own compiler and linker on the host (cchost), as the images the loader takes, then wrapped as the C
# texts the kernel carries.
# - the host build does the same with clang and lld (Makefile); the self-build (tools/selfbuild.sh) and the
#   on-machine build (tools/selfkernel.sh, through tools/mkupload.sh) take these, so a self-built kernel's
#   programs are the machine's compiler's work too
# - writes build/self/<program>.img and build/self/<program>_image.c for decoder and renderer
cd "$(dirname "$0")/.."
OUT=build/self
mkdir -p $OUT
make -s cchost >/dev/null 2>&1 || { echo "cchost does not build"; exit 1; }
H="kernel/include/eb/*.h sdk/erebus.h"

build_program() {     # build_program <name> <sources...>
    name=$1; shift
    objs=""
    for f in "$@"; do
        o=$OUT/${name}_$(basename "$f" .c).obj
        r=$(./build/cchost cc "$f" -o "$o" $H programs/$name/*.h 2>&1 | tail -n 1)
        case "$r" in
            ok:*) objs="$objs $o" ;;
            *) echo "FAILED  $f: $r"; return 1 ;;
        esac
    done
    r=$(./build/cchost ld $OUT/$name.img $objs 2>&1 | tail -n 1)
    case "$r" in
        ok:*) ;;
        *) echo "FAILED  link of $name: $r"; return 1 ;;
    esac
    python3 tools/mkimage.py wrap $OUT/$name.img $OUT/${name}_image.c ${name}_image || return 1
    echo "$name: $r -> $OUT/${name}_image.c"
}

build_program decoder programs/decoder/*.c programs/lib/lib.c kernel/lib/inflate.c || exit 1
build_program renderer programs/renderer/*.c programs/lib/lib.c || exit 1
