#!/bin/sh
# sign-release.sh -- build the signed update package the self-updater fetches.
#   build/update.pkg = "EBUPDATE" (8) | signature (64) | version (24, padded) | kernel.elf
# The signature is ed25519 over the version and kernel together, made with
# release-key.pem at the repo root (never committed). Publish update.pkg,
# build/version and build/SHA256SUMS as release assets ("update.pkg",
# "version", "SHA256SUMS"); the machine reads <base>/version first and
# fetches <base>/update.pkg only when newer. SHA256SUMS lets a third party
# check a published file against a reproducible build of the same tag.
cd "$(dirname "$0")/.."
KEY=${RELEASE_KEY:-release-key.pem}
[ -f "$KEY" ] || { echo "no release key at $KEY"; exit 1; }
[ -f build/kernel.elf ] || { echo "build/kernel.elf is missing -- run make first"; exit 1; }

VER=$(sed -n 's/.*erebus_version\[\] = "\([^"]*\)".*/\1/p' build/version.c)
[ -n "$VER" ] || { echo "could not read the version"; exit 1; }

# The signed part: version padded to exactly 24 bytes, then the kernel.
printf '%-24.24s' "$VER" > build/pkg.signed
cat build/kernel.elf >> build/pkg.signed
openssl pkeyutl -sign -inkey "$KEY" -rawin -in build/pkg.signed -out build/pkg.sig || exit 1

# The package: magic, signature, then the signed part.
printf 'EBUPDATE' > build/update.pkg
cat build/pkg.sig  >> build/update.pkg
cat build/pkg.signed >> build/update.pkg
rm -f build/pkg.signed build/pkg.sig

# The small file the updater reads first: publish it as the asset "version" beside update.pkg.
printf '%s\n' "$VER" > build/version

# A manifest of the release artifacts. The build is reproducible
# (tools/reproduce.sh; the loader carries no timestamp), so a third party
# who builds the same tag gets the same bytes and can confirm against
# these hashes that a published file came from this source -- determinism
# turned into provenance. Publish it as the release asset "SHA256SUMS".
: > build/SHA256SUMS
for f in kernel.elf BOOTX64.EFI erebus.iso update.pkg; do
    [ -f "build/$f" ] && ( cd build && sha256sum "$f" ) >> build/SHA256SUMS
done

echo "built build/update.pkg ($(wc -c < build/update.pkg) bytes) and build/version for version $VER"
echo "wrote build/SHA256SUMS:"
cat build/SHA256SUMS
