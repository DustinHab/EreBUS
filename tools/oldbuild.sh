#!/bin/sh
# oldbuild.sh [tag] -- builds a released tag once into build/old-<tag>/ (esp.img, kernel.elf), from a git worktree.
# The compatibility tests boot it: a store it made under today's kernel (tools/oldstore.sh), and a
# node of it beside one of today (tools/oldpipe.sh). Built once and kept; remove the directory to rebuild.
cd "$(dirname "$0")/.."
TAG=${1:-0.9.6}
OUT=build/old-$TAG
[ -f $OUT/esp.img ] && [ -f $OUT/kernel.elf ] && exit 0
git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || { echo "no tag $TAG (git fetch --tags?)"; exit 1; }
WT=${OLD_WT:-/tmp/erebus-old-$TAG}
rm -rf "$WT"
git worktree prune
git worktree add -f "$WT" "$TAG" >/dev/null 2>&1 || { echo "could not check out $TAG"; exit 1; }
( cd "$WT" && touch kernel/gfx/font8x16.h && make -s VERSION="$TAG" >/dev/null 2>&1 ) || {
    echo "the $TAG build failed"; git worktree remove --force "$WT"; exit 1; }
mkdir -p $OUT
cp "$WT/build/esp.img" "$WT/build/kernel.elf" $OUT/
git worktree remove --force "$WT"
echo "built $TAG into $OUT"
