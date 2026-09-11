#!/bin/sh
# mkupload.sh -- the kernel's sources as one stream for the door (ssh -T, shell mode).
# - emits: make list, go, one "receive <n> bytes as <name>" plus raw bytes per source and header, a synthesized version.c, back
#   sh tools/mkupload.sh [list] [what the kernel should call itself]
cd "$(dirname "$0")/.."
LIST=${1:-kernel}
SAYS=${2:-built on the machine itself}

printf 'make list %s\n' "$LIST"
printf 'go %s\n' "$LIST"
# The picture decoder is a program of its own, not one of the kernel's
# sources: it goes in as the text of its image, built by the machine's
# own compiler on the host (tools/selfdecoder.sh). The machine has no
# way yet to turn a program it built into such a text itself.
[ -f build/self/decoder_image.c ] || sh tools/selfdecoder.sh >/dev/null 2>&1 || exit 1
for f in kernel/*.c kernel/*/*.c kernel/*/*/*.c \
         kernel/arch/x86_64/*.S kernel/user/*.S \
         kernel/include/eb/*.h kernel/net/*.h kernel/gfx/*.h common/*.h \
         build/self/decoder_image.c; do
    [ -f "$f" ] || continue
    printf 'receive %s bytes as %s\n' "$(wc -c < "$f")" "$(basename "$f")"
    cat "$f"
done
V="const char erebus_version[] = \"$SAYS\";
"
printf 'receive %s bytes as version.c\n' "$(printf '%s' "$V" | wc -c)"
printf '%s' "$V"
printf 'back\n'
