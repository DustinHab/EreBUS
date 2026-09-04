#!/bin/sh
# mkupload.sh -- the kernel's sources as one stream for the door.
#
# Words and bytes in the order the door expects them: a list made and
# entered, every source received into it byte for byte under its own
# name, a version text made up on the spot so the built kernel says
# where it came from, and back out. Piped through one shell session
# without a pty, this is how a machine with no exchange disk is handed
# the whole tree. Headers go in too: the build finds them beside the
# sources by name.
#
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
