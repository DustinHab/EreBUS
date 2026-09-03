#!/bin/sh
# mkusb.sh -- build/stick.img: the whole system on one disk.
#
# A GPT image with two partitions: an EFI system partition holding the
# loader and the kernel, and a partition of the store's kind -- the
# type is a GUID of this system's own -- which the kernel finds on any
# disk and keeps the graph in. Written whole to a usb stick it boots a
# UEFI machine (Secure Boot off); written to an ssd it is the machine.
#
#   SIZE_MB=400 sh tools/mkusb.sh
#
# On a real machine the stick's own store partition is not reached
# yet: usb disks are not driven. The store is then a partition of this
# kind, or a blank disk, on SATA -- or the machine runs without one.

cd "$(dirname "$0")/.."
make -s >/dev/null || exit 1
IMG=build/stick.img
SIZE=${SIZE_MB:-400}
STORE_TYPE=E2EB0500-5354-4F52-4552-454255530001

rm -f $IMG
dd if=/dev/zero of=$IMG bs=1M count=$SIZE status=none
sgdisk -o \
    -n 1:2048:+64M -t 1:ef00 -c 1:"EREBUS BOOT" \
    -n 2:0:0 -t 2:$STORE_TYPE -c 2:"EREBUS STORE" \
    $IMG >/dev/null || { echo "sgdisk failed"; exit 1; }

# the boot partition: the 64 MiB volume the machine always boots from,
# laid into the partition whole
dd if=build/esp.img of=$IMG bs=1M seek=1 conv=notrunc status=none || { echo "no build/esp.img"; exit 1; }

sgdisk -p $IMG | sed -n '/^Number/,$p'
echo "$IMG: $(stat -c%s $IMG) bytes.  tools/stick.ps1 <disk> writes it on windows; dd if=$IMG of=/dev/sdX bs=4M on linux."
