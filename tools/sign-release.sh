#!/bin/sh
# sign-release.sh -- build the signed update package the self-updater fetches.
#   build/update.pkg = "EBUPDATE" (8) | signature (64) | version (24, padded) | kernel.elf
# The signature is ed25519 over the version and kernel together, made with
# release-key.pem at the repo root (never committed). Publish update.pkg as
# a release asset; the machine fetches <base>/update.pkg.
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

echo "built build/update.pkg ($(wc -c < build/update.pkg) bytes) for version $VER"
