#!/bin/sh
# mkusb.sh -- build/stick.img: GPT with an EFI system partition (loader, kernel) and a store partition.
#   SIZE_MB=400 sh tools/mkusb.sh
# - store partition type: E2EB0500-5354-4F52-4552-454255530001
# - on real hardware usb disks are not driven: the stick boots, its store partition is out of reach

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
