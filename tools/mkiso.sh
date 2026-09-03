#!/bin/sh
# mkiso.sh -- build/erebus.iso: the system as an iso, bootable the way
# any other is.
#
# UEFI boots an iso through El Torito: a FAT image inside the iso is
# handed to the firmware as if it were a small disk, and the firmware
# runs \EFI\BOOT\BOOTX64.EFI from it. The same FAT image is marked as
# an EFI system partition in a GPT written into the iso's first
# sectors, so the file written whole onto a usb stick -- dd, Rufus in
# dd mode, balenaEtcher -- boots too. There is no BIOS half: the
# machine has to boot the UEFI way, with Secure Boot off.
#
# A machine booted from the iso runs without a memory and says so;
# 'disks' and 'settle' in the terminal make one.

cd "$(dirname "$0")/.."
make -s >/dev/null || exit 1
ROOT=build/iso
rm -rf $ROOT build/erebus.iso
mkdir -p $ROOT/EFI/BOOT $ROOT/erebus

# The boot image: FAT16, sixteen megabytes -- FAT32 wants more clusters
# than a small image has, and the firmware reads either.
EFIIMG=$ROOT/EFI/erebus.img
dd if=/dev/zero of=$EFIIMG bs=1M count=16 status=none
mkfs.vfat -F 16 -n EREBUS $EFIIMG >/dev/null
mmd   -i $EFIIMG ::/EFI ::/EFI/BOOT ::/erebus
mcopy -i $EFIIMG build/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
mcopy -i $EFIIMG build/kernel.elf ::/erebus/kernel.elf

# The same two files lie in the iso's own tree as well, for anyone who
# wants to look at them; the firmware uses the image above.
cp build/BOOTX64.EFI $ROOT/EFI/BOOT/BOOTX64.EFI
cp build/kernel.elf $ROOT/erebus/kernel.elf
cp README.md $ROOT/README.md

xorriso -as mkisofs -o build/erebus.iso -V EREBUS -r -J -joliet-long \
    -e EFI/erebus.img -no-emul-boot \
    -efi-boot-part --efi-boot-image \
    $ROOT 2>&1 | grep -v '^xorriso : UPDATE\|^Drive\|^Media\|^Writing\|^ISO image\|^Added\|libisofs: NOTE'

ls -l build/erebus.iso | cut -c24-
