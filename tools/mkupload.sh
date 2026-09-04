#!/bin/sh
# mkupload.sh -- the kernel's sources as one stream for the door (ssh -T, shell mode).
# - emits: make list, go, one "receive <n> bytes as <name>" plus raw bytes per source and header, a synthesized version.c, back
#   sh tools/mkupload.sh [list] [what the kernel should call itself]
cd "$(dirname "$0")/.."
LIST=${1:-kernel}
SAYS=${2:-built on the machine itself}

printf 'make list %s\n' "$LIST"
printf 'go %s\n' "$LIST"
for f in kernel/*.c kernel/*/*.c kernel/*/*/*.c \
         kernel/arch/x86_64/*.S kernel/user/*.S \
         kernel/include/eb/*.h kernel/net/gf25519.h kernel/gfx/*.h common/*.h; do
    [ -f "$f" ] || continue
    printf 'receive %s bytes as %s\n' "$(wc -c < "$f")" "$(basename "$f")"
    cat "$f"
done
V="const char erebus_version[] = \"$SAYS\";
"
printf 'receive %s bytes as version.c\n' "$(printf '%s' "$V" | wc -c)"
printf '%s' "$V"
printf 'back\n'
