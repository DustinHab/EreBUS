# ---------------------------------------------------------------------
# Erebus -- build system
#
# Deliberately a single, top-to-bottom makefile: no generator, no
# configuration layer, no third-party libraries. What is here can be
# read from the first line to the last.
#
#   make          build the loader, the kernel and a bootable image
#   make run      start it in QEMU (window, serial output in terminal)
#   make shot     start headless and save a screenshot plus the log
#   make fault    build with the deliberate fault test and screenshot it
#   make debug    start halted, waiting for gdb on port 1234
#   make clean    remove everything generated
# ---------------------------------------------------------------------

ROOT   := $(CURDIR)
BUILD  := $(ROOT)/build

CC     := clang
LD     := ld.lld
PELINK := lld-link
PY     := python3

UNIFONT   := /usr/share/unifont/unifont.hex
OVMF_CODE := /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS := /usr/share/OVMF/OVMF_VARS_4M.fd

# Resolution offered to the firmware over EDID.
XRES := 1280
YRES := 800

# Extra flags for the kernel, used by the "fault" target.
EXTRA ?=

# Where the serial port goes during "make shot". Override with
# SERIAL=null to time the framebuffer console on its own -- the serial
# line is slow enough to hide everything else.
SERIAL ?= file:$(BUILD)/serial.log

# --- compiler flags ---------------------------------------------------
# Shared: freestanding (no C library), no red zone (an interrupt would
# destroy it immediately), no vector units (they would have to be
# enabled first and saved on every context switch).
COMMON_FLAGS := -ffreestanding -nostdlib -mno-red-zone \
                -mno-mmx -mno-sse -mno-sse2 \
                -fno-pic -fno-pie -fno-common \
                -fno-omit-frame-pointer \
                -std=c11 -O2 -g \
                -Wall -Wextra -Wshadow -Wvla \
                -I$(ROOT) -I$(ROOT)/kernel/include

# Loader: PE/COFF for UEFI, MS ABI, 16-bit wchar as the firmware
# expects. No stack protector, because there is no panic() yet.
BOOT_FLAGS := -target x86_64-unknown-windows $(COMMON_FLAGS) \
              -fshort-wchar -fno-stack-protector

# Kernel: ELF, SysV ABI, with the stack protector.
#
# -mcmodel=kernel goes with the linker script: it tells the compiler
# that everything lives in the top 2 GiB of the address space, so
# references can stay 32-bit signed offsets. Building upper-half code
# without it produces addresses that are quietly wrong.
KERN_FLAGS := -target x86_64-unknown-none-elf $(COMMON_FLAGS) \
              -mcmodel=kernel -fstack-protector-strong $(EXTRA)

# --- sources ----------------------------------------------------------
BOOT_SRC := boot/boot.c

KERN_C   := kernel/main.c \
            kernel/lib/fmt.c \
            kernel/lib/string.c \
            kernel/lib/panic.c \
            kernel/lib/harden.c \
            kernel/hw/serial.c \
            kernel/hw/cpu.c \
            kernel/hw/pic.c \
            kernel/hw/time.c \
            kernel/gfx/fb.c \
            kernel/mm/pmm.c \
            kernel/mm/vmm.c \
            kernel/mm/kheap.c \
            kernel/arch/x86_64/gdt.c \
            kernel/arch/x86_64/trap.c
KERN_S   := kernel/arch/x86_64/start.S \
            kernel/arch/x86_64/isr.S

KERN_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(KERN_C)) \
            $(patsubst %.S,$(BUILD)/%.o,$(KERN_S))

FONT   := kernel/gfx/font8x16.h
LOADER := $(BUILD)/BOOTX64.EFI
KERNEL := $(BUILD)/kernel.elf
IMAGE  := $(BUILD)/esp.img

.PHONY: all run shot fault wx debug clean info
all: $(IMAGE)

# --- font -------------------------------------------------------------
$(FONT): tools/mkfont.py
	@echo "  FONT    $@"
	@$(PY) tools/mkfont.py $(UNIFONT) $@

# --- loader -----------------------------------------------------------
$(LOADER): $(BOOT_SRC) boot/efi.h common/bootinfo.h common/elf64.h
	@mkdir -p $(BUILD)
	@echo "  CC      $(BOOT_SRC)"
	@$(CC) $(BOOT_FLAGS) -c $(BOOT_SRC) -o $(BUILD)/boot.o
	@echo "  LINK    $@"
	@$(PELINK) -subsystem:efi_application -entry:efi_main -nodefaultlib \
	           $(BUILD)/boot.o -out:$@

# --- kernel -----------------------------------------------------------
$(BUILD)/%.o: %.c $(FONT)
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(KERN_FLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(CC) $(KERN_FLAGS) -c $< -o $@

$(KERNEL): $(KERN_OBJ) kernel/arch/x86_64/linker.ld
	@echo "  LINK    $@"
	@$(LD) -T kernel/arch/x86_64/linker.ld -nostdlib -z noexecstack \
	       -o $@ $(KERN_OBJ)
	@echo "  SIZE    $$(stat -c%s $@) bytes"

# --- bootable image ---------------------------------------------------
# A plain FAT32 volume with no partition table. UEFI recognises that as
# an EFI system partition; a GPT image for real media will come from
# tools/mkusb.sh later.
$(IMAGE): $(LOADER) $(KERNEL)
	@echo "  IMAGE   $@"
	@dd if=/dev/zero of=$@ bs=1M count=64 status=none
	@mkfs.vfat -F 32 -n EREBUS $@ >/dev/null
	@mmd   -i $@ ::/EFI ::/EFI/BOOT ::/erebus
	@mcopy -i $@ $(LOADER) ::/EFI/BOOT/BOOTX64.EFI
	@mcopy -i $@ $(KERNEL) ::/erebus/kernel.elf
	@echo "  DONE    $@"

$(BUILD)/OVMF_VARS.fd: $(OVMF_VARS)
	@mkdir -p $(BUILD)
	@cp $(OVMF_VARS) $@

# --- running ----------------------------------------------------------
QEMU := qemu-system-x86_64 \
  -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
  -drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
  -drive format=raw,file=$(IMAGE) \
  -vga none -device VGA,edid=on,xres=$(XRES),yres=$(YRES) \
  -net none

run: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	$(QEMU) -serial stdio

# Start headless, let it settle, grab the screen, quit. Going through
# the monitor is the only way to get an image without a window.
shot: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	@rm -f $(BUILD)/screen.ppm $(BUILD)/serial.log
	@{ sleep 9; echo "screendump $(BUILD)/screen.ppm"; sleep 2; echo quit; } | \
	  $(QEMU) -display none -monitor stdio \
	          -serial $(SERIAL) >/dev/null 2>&1 || true
	@$(PY) tools/ppm2png.py $(BUILD)/screen.ppm $(BUILD)/screen.png

# Rebuild the kernel with the deliberate fault and photograph the
# result.
#
# The object files are thrown away both before and after. Before,
# because make cannot see that a compiler flag changed and would happily
# reuse them. After, for the same reason in reverse: leaving objects
# built with the test flag behind would silently contaminate the next
# ordinary build, and a kernel that faults on purpose is not something
# to discover by accident.
fault:
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@$(MAKE) --no-print-directory EXTRA=-DEREBUS_TEST_FAULT=1 shot
	@mv $(BUILD)/screen.png $(BUILD)/screen-fault.png
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@echo "  DONE    $(BUILD)/screen-fault.png"

# The other half of the same idea: instead of touching memory that is
# not there, write to memory that is there but read-only. If W^X and
# CR0.WP are doing their job the store faults; if they are not, it
# silently succeeds and the kernel has rewritten its own code.
wx:
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@$(MAKE) --no-print-directory EXTRA=-DEREBUS_TEST_FAULT=2 shot
	@mv $(BUILD)/screen.png $(BUILD)/screen-wx.png
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@echo "  DONE    $(BUILD)/screen-wx.png"

debug: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	$(QEMU) -serial stdio -s -S

info:
	@echo "loader: $(LOADER)"
	@echo "kernel: $(KERNEL)"
	@echo "image : $(IMAGE)"
	@echo "ovmf  : $(OVMF_CODE)"

clean:
	rm -rf $(BUILD)
