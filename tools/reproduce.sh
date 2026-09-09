#!/bin/sh
# reproduce.sh -- builds the kernel twice from the same tree and checks the
# bytes are identical, so a released binary can be traced back to its source.
#
# - both builds use one VERSION string, captured once here, so a tag change
#   mid-run cannot make them differ for a reason that is not the code
# - both builds go into the same directory path (removed between them), so
#   the source paths the debug info records are identical for both
# - kernel.elf is linked by ld.lld and BOOTX64.EFI by lld-link, both
#   deterministic; esp.img is a FAT volume with creation timestamps and is
#   not compared
# - no QEMU: this is a build check, it needs no KVM
#
# usage: sh tools/reproduce.sh          (uses /tmp/erebus-repro)
#        REPRO_DIR=/path sh tools/reproduce.sh
cd "$(dirname "$0")/.."

D=${REPRO_DIR:-/tmp/erebus-repro}
VER=$(git describe --tags --always --dirty 2>/dev/null || echo unnumbered)
echo "version under test: $VER"

hashes() {
    # kernel.elf and the loader, in a fixed order, as "<sha256>  <name>"
    for f in kernel.elf BOOTX64.EFI; do
        if [ -f "$D/$f" ]; then
            printf '%s  %s\n' "$(sha256sum < "$D/$f" | cut -d' ' -f1)" "$f"
        else
            printf 'MISSING  %s\n' "$f"
        fi
    done
}

build() {
    rm -rf "$D"
    mkdir -p "$D"
    # A fresh empty build directory forces every object, the loader and
    # the kernel to be built again; the committed font header is left as
    # it is (touched so make does not try to regenerate it), so the two
    # builds differ in nothing but the run. No -j, to keep it ordered.
    touch kernel/gfx/font8x16.h
    if ! make BUILD="$D" VERSION="$VER" all > "$D.log" 2>&1; then
        echo "build FAILED (see $D.log):"
        tail -20 "$D.log"
        exit 2
    fi
}

echo "--- first build ---"
build
A=$(hashes)
echo "$A"

echo "--- second build ---"
build
B=$(hashes)
echo "$B"

echo "--- result ---"
if [ "$A" = "$B" ]; then
    echo "REPRODUCIBLE: kernel.elf and BOOTX64.EFI are byte-identical across two builds"
    rm -rf "$D" "$D.log"
    exit 0
else
    echo "NOT REPRODUCIBLE: the two builds differ"
    echo "first:"
    echo "$A"
    echo "second:"
    echo "$B"
    exit 1
fi
