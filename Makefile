# ---------------------------------------------------------------------
# EreBUS -- build system
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
                -std=c11 -O2 -g -MMD -MP \
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
            kernel/gfx/shell.c \
            kernel/gfx/html.c \
            kernel/hw/ps2.c \
            kernel/hw/xhci.c \
            kernel/net/sha1.c \
            kernel/net/ccmp.c \
            kernel/net/wifi.c \
            kernel/hw/pci.c \
            kernel/hw/ahci.c \
            kernel/mm/pmm.c \
            kernel/mm/vmm.c \
            kernel/mm/kheap.c \
            kernel/obj/object.c \
            kernel/obj/cap.c \
            kernel/obj/port.c \
            kernel/obj/snapshot.c \
            kernel/obj/blob.c \
            kernel/obj/journal.c \
            kernel/obj/settings.c \
            kernel/obj/activity.c \
            kernel/sched/thread.c \
            kernel/sched/proc.c \
            kernel/net/nic.c \
            kernel/net/e1000.c \
            kernel/net/igb.c \
            kernel/net/rtl8139.c \
            kernel/net/rtl8169.c \
            kernel/net/sha256.c \
            kernel/net/x25519.c \
            kernel/net/aes_gcm.c \
            kernel/net/crypto_selftest.c \
            kernel/net/tls.c \
            kernel/obj/bundle.c \
            kernel/fs/fat.c \
            kernel/fs/gpt.c \
            kernel/fs/settle.c \
            kernel/fs/install.c \
            kernel/net/pipe.c \
            kernel/net/nodes.c \
            kernel/net/net.c \
            kernel/net/sha512.c \
            kernel/net/ed25519.c \
            kernel/net/ssh.c \
            kernel/net/update.c \
            kernel/lib/base64.c \
            kernel/lang/asm.c \
            kernel/lang/big.c \
            kernel/lang/ld.c \
            kernel/lang/gnu.c \
            kernel/lib/names.c \
            kernel/lang/cc.c \
            kernel/term/term.c \
            kernel/user/runner.c \
            kernel/user/foreman.c \
            kernel/user/reckon.c \
            kernel/user/pulse.c \
            kernel/arch/x86_64/syscall.c \
            kernel/arch/x86_64/gdt.c \
            kernel/arch/x86_64/trap.c
KERN_S   := kernel/arch/x86_64/start.S \
            kernel/arch/x86_64/isr.S \
            kernel/arch/x86_64/switch.S \
            kernel/arch/x86_64/entry.S \
            kernel/user/programs.S \
            kernel/user/agent.S \
            kernel/user/courier.S \
            kernel/user/clock.S \
            kernel/user/cipher.S \
            kernel/user/tally.S \
            kernel/user/sums.S \
            kernel/user/watch.S \
            kernel/user/wipe.S \
            kernel/user/fetch.S

KERN_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(KERN_C)) \
            $(patsubst %.S,$(BUILD)/%.o,$(KERN_S)) \
            $(BUILD)/version.o

# What this build calls itself: the nearest tag, how far past it, and
# the commit -- with "-dirty" when the tree has changes not committed.
# Every kernel had said "0.1" until now, which made two builds a
# fortnight apart indistinguishable on a screen.
VERSION ?= $(shell git -C $(ROOT) describe --tags --always --dirty 2>/dev/null || echo unnumbered)


FONT   := kernel/gfx/font8x16.h
LOADER := $(BUILD)/BOOTX64.EFI
KERNEL := $(BUILD)/kernel.elf
IMAGE  := $(BUILD)/esp.img
# The test rig's disk and firmware store, deliberately not the live
# machine's. tools/vm.sh runs the machine the person actually uses, on
# build/store.img; everything make starts is a test and works on its
# own copies, so a running VM and a test run never write the same file.
STORE  := $(BUILD)/teststore.img

.PHONY: all run shot fault wx stack trace-stack desktop trace-input persist agent relay sweep cchost \
        look debug clean info
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

# The version, as one small file made on every run and rewritten only
# when it would say something new -- so a commit changes what the
# kernel calls itself without touching any other object, and a build
# that changed nothing relinks nothing.
$(BUILD)/version.c: FORCE
	@mkdir -p $(BUILD)
	@v='const char erebus_version[] = "$(VERSION)";'; \
	 if [ ! -f $@ ] || [ "$$(cat $@)" != "$$v" ]; then echo "$$v" > $@; fi

$(BUILD)/version.o: $(BUILD)/version.c
	@echo "  CC      version $(VERSION)"
	@$(CC) $(KERN_FLAGS) -c $< -o $@

FORCE:

# The C files that run in ring 3. They live in the kernel image but
# are mapped read-only into every process, so they may not lean on the
# kernel's rodata: no stack protector (the guard is a kernel symbol)
# and no jump tables (they are rodata with absolute addresses).
$(BUILD)/kernel/user/runner.o: kernel/user/runner.c $(FONT)
	@mkdir -p $(dir $@)
	@echo "  CC      kernel/user/runner.c (ring 3)"
	@$(CC) $(KERN_FLAGS) -fno-stack-protector -fno-jump-tables \
	       -c kernel/user/runner.c -o $@

$(BUILD)/kernel/user/foreman.o: kernel/user/foreman.c $(FONT)
	@mkdir -p $(dir $@)
	@echo "  CC      kernel/user/foreman.c (ring 3)"
	@$(CC) $(KERN_FLAGS) -fno-stack-protector -fno-jump-tables \
	       -c kernel/user/foreman.c -o $@

$(BUILD)/kernel/user/reckon.o: kernel/user/reckon.c $(FONT)
	@mkdir -p $(dir $@)
	@echo "  CC      kernel/user/reckon.c (ring 3)"
	@$(CC) $(KERN_FLAGS) -fno-stack-protector -fno-jump-tables \
	       -c kernel/user/reckon.c -o $@

$(BUILD)/kernel/user/pulse.o: kernel/user/pulse.c $(FONT)
	@mkdir -p $(dir $@)
	@echo "  CC      kernel/user/pulse.c (ring 3)"
	@$(CC) $(KERN_FLAGS) -fno-stack-protector -fno-jump-tables \
	       -c kernel/user/pulse.c -o $@

# Linked twice: once with an empty name table, to learn where the code
# lies, and again with the table of those names. The table sits after
# the data and before the bss, so the second link moves no code.
$(KERNEL): $(KERN_OBJ) kernel/arch/x86_64/linker.ld tools/mknames.py
	@echo "  LINK    $@ (first, to learn the names)"
	@$(PY) tools/mknames.py > $(BUILD)/names0.c
	@$(CC) $(KERN_FLAGS) -c $(BUILD)/names0.c -o $(BUILD)/names0.o
	@$(LD) -T kernel/arch/x86_64/linker.ld -nostdlib -z noexecstack \
	       -o $(BUILD)/kernel.stage1.elf $(KERN_OBJ) $(BUILD)/names0.o
	@nm -n $(BUILD)/kernel.stage1.elf | $(PY) tools/mknames.py names > $(BUILD)/names.c
	@$(CC) $(KERN_FLAGS) -c $(BUILD)/names.c -o $(BUILD)/names.o
	@echo "  LINK    $@"
	@$(LD) -T kernel/arch/x86_64/linker.ld -nostdlib -z noexecstack \
	       -o $@ $(KERN_OBJ) $(BUILD)/names.o
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

# A second disk, kept across runs. This is where the object graph goes;
# the boot disk is left alone.
$(STORE):
	@mkdir -p $(BUILD)
	@dd if=/dev/zero of=$@ bs=1M count=32 status=none

# Copied fresh on every run, deliberately. The firmware writes boot
# entries into its variable store, and a stale entry pointing at an
# image that has since been rebuilt boots into nothing: empty log,
# black screen, looks exactly like a kernel crash and is not one. That
# hunt has been run often enough.
$(BUILD)/test-vars.fd: $(OVMF_VARS) FORCE
	@mkdir -p $(BUILD)
	@cp $(OVMF_VARS) $@

FORCE:

# --- running ----------------------------------------------------------
# Which card the test machine wears. The default is the Intel e1000;
# "make run NIC=rtl8139" proves the other driver against the same
# tests, which is as close to real hardware as an emulator gets.
NIC ?= e1000

QEMU := qemu-system-x86_64 \
  -machine q35 -m 512M -cpu max \
  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
  -drive if=pflash,format=raw,file=$(BUILD)/test-vars.fd \
  -drive format=raw,file=$(IMAGE) \
  -vga none -device VGA,edid=on,xres=$(XRES),yres=$(YRES) \
  -drive id=store,file=$(STORE),format=raw,if=none \
  -device ide-hd,drive=store,bus=ide.1 \
  -device $(NIC),netdev=n0 \
  -netdev user,id=n0,hostfwd=udp::7802-:7800

run: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
	$(QEMU) -serial stdio

# Start headless, let it settle, grab the screen, quit. Going through
# the monitor is the only way to get an image without a window.
shot: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
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

# Runs a thread off the bottom of its stack. The guard page turns that
# into a fault at the offending instruction instead of a silent
# overwrite of whatever sits below, and the separate interrupt stacks in
# the TSS are what let the fault be reported at all.
stack:
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@$(MAKE) --no-print-directory EXTRA=-DEREBUS_TEST_FAULT=3 shot
	@mv $(BUILD)/screen.png $(BUILD)/screen-stack.png
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@echo "  DONE    $(BUILD)/screen-stack.png"

# Same as "stack", but with QEMU logging every exception it delivers and
# refusing to reset. When a fault produces no report at all, this is the
# only way to see what the processor actually did.
trace-stack:
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@$(MAKE) --no-print-directory EXTRA=-DEREBUS_TEST_FAULT=3 $(IMAGE)
	@rm -f $(BUILD)/qemu.log
	@{ sleep 9; echo quit; } | $(QEMU) -display none -monitor stdio \
	    -serial file:$(BUILD)/serial.log -no-reboot -no-shutdown \
	    -d int,cpu_reset -D $(BUILD)/qemu.log >/dev/null 2>&1 || true
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@echo "  DONE    $(BUILD)/qemu.log"

# Boots, waits for the desktop, types into the focused window through
# QEMU's monitor, and photographs the result. Typing is the only way to
# show that input reaches a window and that the other views of the same
# object follow along.
desktop: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
	@rm -f $(BUILD)/screen.ppm
	@{ sleep 8; \
	   for k in t y p e d spc l i v e; do \
	     echo "sendkey $$k"; sleep 0.2; done; \
	   sleep 1; echo "screendump $(BUILD)/screen.ppm"; \
	   sleep 2; echo quit; } | \
	  $(QEMU) -display none -monitor stdio \
	          -serial file:$(BUILD)/serial.log >/dev/null 2>&1 || true
	@$(PY) tools/ppm2png.py $(BUILD)/screen.ppm $(BUILD)/screen-desktop.png
	@echo "  DONE    $(BUILD)/screen-desktop.png"

# Which interrupt vectors actually reach the processor while keys are
# being sent. 0x21 is line 1 (keyboard), 0x2c is line 12 (mouse).
trace-input: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
	@rm -f $(BUILD)/qemu.log
	@{ sleep 8; echo "sendkey a"; echo "sendkey b"; echo "sendkey c"; \
	   sleep 2; echo quit; } | \
	  $(QEMU) -display none -monitor stdio -serial file:$(BUILD)/serial.log \
	          -d int -D $(BUILD)/qemu.log >/dev/null 2>&1 || true
	@echo "vectors seen after the keys were sent:"
	@grep -oE 'v=[0-9a-f]+' $(BUILD)/qemu.log | sort | uniq -c | sort -rn | head

# Two boots: typed into on the first, restored on the second (tools/persisttest.sh).
persist: $(IMAGE)
	@sh tools/persisttest.sh

# Photographs the shell after sending a few keys, so the different ways
# of looking can actually be seen. KEYS is a list of QEMU key names and
# OUT is where the picture goes.
#
#   make look KEYS="down right"   step into the first reference
#   make look KEYS="tab"          the graph
#   make look KEYS="tab tab"      the columns
KEYS ?=
OUT  ?= $(BUILD)/screen-look.png

look: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
	@rm -f $(BUILD)/screen.ppm
	@{ sleep 9; \
	   for k in $(KEYS); do echo "sendkey $$k"; sleep 0.3; done; \
	   sleep 1; echo "screendump $(BUILD)/screen.ppm"; \
	   sleep 2; echo quit; } | \
	  $(QEMU) -display none -monitor stdio \
	          -serial file:$(BUILD)/serial.log >/dev/null 2>&1 || true
	@$(PY) tools/ppm2png.py $(BUILD)/screen.ppm $(OUT)
	@echo "  DONE    $(OUT)"

# The agent under changing rights (tools/agenttest.sh).
agent: $(IMAGE)
	@sh tools/agenttest.sh

# A capability passed from one program to another, weakened on the way (tools/relaytest.sh).
relay: $(IMAGE)
	@sh tools/relaytest.sh

# What the collector costs, measured. Rebuilds with the stress harness
# (ten thousand unreachable rings of three), boots headless, and prints
# the numbers -- including how long the machine stood still, because the
# sweep runs with interrupts off and that duration should be a printed
# fact, not a guess. Objects are cleaned away on both sides exactly as
# in "fault", and for the same reason.
# The machine's compiler and assembler as a host program: the same
# two files, so a text can be tried in a second before it is tried
# on the machine. Nothing runs here; the image is the machine's.
cchost: $(BUILD)/cchost
$(BUILD)/cchost: tools/cchost.c kernel/lang/cc.c kernel/lang/asm.c \
                 kernel/lang/ld.c kernel/lang/gnu.c \
                 kernel/include/eb/cc.h kernel/include/eb/asm.h kernel/include/eb/ld.h
	@mkdir -p $(BUILD)
	@echo "  HOST    $@"
	@clang -O1 -g -std=c11 -Wall -Wno-unused-function \
	    -Wno-incompatible-library-redeclaration -I$(ROOT)/kernel/include \
	    -o $@ tools/cchost.c kernel/lang/cc.c kernel/lang/asm.c \
	    kernel/lang/ld.c kernel/lang/gnu.c

sweep:
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)
	@$(MAKE) --no-print-directory EXTRA=-DEREBUS_STRESS_COLLECT $(IMAGE)
	@rm -f $(STORE)
	@$(MAKE) --no-print-directory $(STORE) $(BUILD)/test-vars.fd
	@{ sleep 14; echo quit; } | \
	  $(QEMU) -display none -monitor stdio \
	          -serial file:$(BUILD)/serial.log >/dev/null 2>&1 || true
	@grep -ao 'stress: .*' $(BUILD)/serial.log | sed 's/[[:space:]]*$$//'
	@rm -rf $(BUILD)/kernel $(KERNEL) $(IMAGE)

debug: $(IMAGE) $(STORE) $(BUILD)/test-vars.fd
	$(QEMU) -serial stdio -s -S

info:
	@echo "loader: $(LOADER)"
	@echo "kernel: $(KERNEL)"
	@echo "image : $(IMAGE)"
	@echo "ovmf  : $(OVMF_CODE)"

clean:
	rm -rf $(BUILD)

# Header dependencies, written by the compiler as it goes. Without
# these, editing a header changes nothing that make can see, and the
# next build quietly links yesterday's object file against today's
# declarations -- which does not fail, it just misbehaves.
#
# It goes last on purpose. An included file full of rules that arrives
# before the first real target makes one of those rules the default
# goal, and then "make" builds a single object file and reports success.
DEPS := $(KERN_OBJ:.o=.d) $(BUILD)/boot.d
-include $(DEPS)