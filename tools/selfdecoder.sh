#!/bin/sh
# selfdecoder.sh -- the picture decoder (programs/decoder) built with the machine's own compiler and linker on
# the host (cchost), as the image the loader takes, then wrapped as the C text the kernel carries.
# - the host build does the same with clang and lld (Makefile); the self-build (tools/selfbuild.sh) and the
#   on-machine build (tools/selfkernel.sh, through tools/mkupload.sh) take this one, so a self-built kernel's
#   decoder is the machine's compiler's work too
# - writes build/self/decoder.img and build/self/decoder_image.c
cd "$(dirname "$0")/.."
OUT=build/self
mkdir -p $OUT
make -s cchost >/dev/null 2>&1 || { echo "cchost does not build"; exit 1; }

H="kernel/include/eb/*.h programs/decoder/*.h sdk/erebus.h"
objs=""
for f in programs/decoder/*.c kernel/lib/inflate.c; do
    o=$OUT/decoder_$(basename "$f" .c).obj
    r=$(./build/cchost cc "$f" -o "$o" $H 2>&1 | tail -n 1)
    case "$r" in
        ok:*) objs="$objs $o" ;;
        *) echo "FAILED  $f: $r"; exit 1 ;;
    esac
done
r=$(./build/cchost ld $OUT/decoder.img $objs 2>&1 | tail -n 1)
case "$r" in
    ok:*) ;;
    *) echo "FAILED  link: $r"; exit 1 ;;
esac
python3 tools/mkimage.py wrap $OUT/decoder.img $OUT/decoder_image.c decoder_image || exit 1
echo "decoder: $r -> $OUT/decoder_image.c"
