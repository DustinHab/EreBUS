#!/bin/sh
# kvm.sh <script...> -- runs a test script with tools/kvm-shim/qemu-system-x86_64 (adds -accel kvm) first in PATH
cd "$(dirname "$0")/.."
chmod +x tools/kvm-shim/qemu-system-x86_64
PATH="$(pwd)/tools/kvm-shim:$PATH"
export PATH
exec sh "$@"
