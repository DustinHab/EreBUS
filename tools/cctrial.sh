#!/bin/sh
# cctrial.sh -- kernel files through the machine's compiler, on the host.
#
# Measures the distance to a kernel built on the machine: each file is
# included whole under a borrowed main and pushed through cchost. Three
# outcomes are told apart: an image (the file stands on its own), a
# compile that only lacks other files' names at the assembling step
# (the linking question, not the compiler's), and a file the compiler
# cannot read yet, with the line it names.
#
#   cctrial.sh            a handful of files
#   cctrial.sh all        every kernel file
#   cctrial.sh <files>    those

cd "$(dirname "$0")/.."
make -s cchost >/dev/null 2>&1

if [ "$1" = "all" ]; then
    FILES=$(ls kernel/*/*.c kernel/*/*/*.c 2>/dev/null | grep -v '^kernel/user/')
else
    FILES="${*:-kernel/lib/base64.c kernel/lib/string.c kernel/obj/journal.c kernel/net/sha256.c kernel/net/x25519.c kernel/obj/settings.c kernel/lang/asm.c kernel/obj/object.c}"
fi

images=0; compiled=0; failed=0; total=0
for f in $FILES; do
    total=$((total + 1))
    cp "$f" build/trial_target.c
    r=$(./build/cchost tools/cc/trial.c build/trial_target.c kernel/include/eb/*.h \
        kernel/net/gf25519.h kernel/gfx/*.h common/*.h 2>&1 | tail -1)
    case "$r" in
        ok:*) images=$((images + 1)); tag="image   " ;;
        "assemble: "*"not a name laid down"*) compiled=$((compiled + 1)); tag="compiled"; r="(needs other files' names to stand alone)" ;;
        *) failed=$((failed + 1)); tag="FAILED  " ;;
    esac
    printf '%s  %-28s %s\n' "$tag" "$f" "$r"
done
echo
echo "$total kernel files: $images stand alone as images, $compiled compile and wait for a linker, $failed the compiler cannot read yet"
