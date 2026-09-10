#!/bin/sh
# sign-rotation.sh -- the statement that moves deployed machines to a new release key.
#   sh tools/sign-rotation.sh <signing key.pem> <new key.pem> ["note"]   -> build/rotate
# build/rotate = "EBROTATE" (8) | signature (64) | new public key (32) | note (24, padded)
# The signature is ed25519 over the key and the note, made with the signing key, which must be
# one the machines trust now: a key built into the kernel, or the key of the last rotation.
# Publish build/rotate as the release asset "rotate" beside update.pkg. Every machine reads it
# on its next check and, once it verifies, accepts only packages signed with the new key --
# so from that release on, sign with the new key (RELEASE_KEY=<new.pem> sh tools/sign-release.sh).
cd "$(dirname "$0")/.."
SIGN=$1
NEW=$2
NOTE=${3:-rotated}
[ -n "$SIGN" ] && [ -f "$SIGN" ] && [ -n "$NEW" ] && [ -f "$NEW" ] ||
    { echo "usage: sh tools/sign-rotation.sh <signing key.pem> <new key.pem> [note]"; exit 1; }
mkdir -p build

openssl pkey -in "$NEW" -pubout -outform DER | tail -c 32 > build/rot.key || exit 1
[ "$(wc -c < build/rot.key)" = 32 ] || { echo "$NEW is not an ed25519 key"; exit 1; }
{ cat build/rot.key; printf '%-24.24s' "$NOTE"; } > build/rot.signed
openssl pkeyutl -sign -inkey "$SIGN" -rawin -in build/rot.signed -out build/rot.sig || exit 1
{ printf 'EBROTATE'; cat build/rot.sig build/rot.signed; } > build/rotate
rm -f build/rot.key build/rot.signed build/rot.sig

echo "wrote build/rotate ($(wc -c < build/rotate) bytes): a machine that reads it accepts only packages signed with $NEW from then on"
