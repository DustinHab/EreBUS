# ---------------------------------------------------------------------
# Erebus -- Buildsystem
#
# Bewusst ein einzelner, gerade heruntergeschriebener Makefile: keine
# Generatoren, keine Konfigurationsschicht, keine Fremdbibliotheken. Was
# hier steht, kann man von oben nach unten lesen und nachvollziehen.
#
#   make          Lader, Kernel und startfaehiges Abbild bauen
#   make run      im QEMU starten (Fenster, serielle Ausgabe im Terminal)
#   make shot     kopflos starten und einen Bildschirmabzug ablegen
#   make debug    mit angehaltener CPU starten, wartet auf gdb
#   make clean    alles Erzeugte loeschen
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

# Aufloesung, die wir der Firmware per EDID anbieten.
XRES := 1280
YRES := 800

# --- Uebersetzerschalter ---------------------------------------------
# Gemeinsam: freistehend (keine C-Bibliothek), kein Red Zone (der waere
# bei Unterbrechungen sofort zerstoert), keine Vektoreinheiten (die
# muessten erst eingeschaltet und bei jedem Wechsel gesichert werden).
COMMON_FLAGS := -ffreestanding -nostdlib -mno-red-zone \
                -mno-mmx -mno-sse -mno-sse2 \
                -fno-pic -fno-pie -fno-common \
                -fno-omit-frame-pointer \
                -std=c11 -O2 -g \
                -Wall -Wextra -Wshadow -Wvla \
                -I$(ROOT) -I$(ROOT)/kernel/include

# Lader: PE/COFF fuer UEFI, MS-ABI, 16-Bit-wchar wie es die Firmware
# erwartet. Ohne Stapelschutz, weil es hier noch keinen panic() gibt.
BOOT_FLAGS := -target x86_64-unknown-windows $(COMMON_FLAGS) \
              -fshort-wchar -fno-stack-protector

# Kernel: ELF, SysV-ABI, mit Stapelschutz.
KERN_FLAGS := -target x86_64-unknown-none-elf $(COMMON_FLAGS) \
              -fstack-protector-strong

# --- Quellen ----------------------------------------------------------
BOOT_SRC := boot/boot.c

KERN_C   := kernel/main.c \
            kernel/lib/fmt.c \
            kernel/lib/string.c \
            kernel/lib/panic.c \
            kernel/lib/harden.c \
            kernel/hw/serial.c \
            kernel/gfx/fb.c
KERN_S   := kernel/arch/x86_64/start.S

KERN_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(KERN_C)) \
            $(patsubst %.S,$(BUILD)/%.o,$(KERN_S))

FONT   := kernel/gfx/font8x16.h
LOADER := $(BUILD)/BOOTX64.EFI
KERNEL := $(BUILD)/kernel.elf
IMAGE  := $(BUILD)/esp.img

.PHONY: all run shot debug clean info
all: $(IMAGE)

# --- Zeichensatz ------------------------------------------------------
$(FONT): tools/mkfont.py
	@echo "  ZEICHEN $@"
	@$(PY) tools/mkfont.py $(UNIFONT) $@

# --- Lader ------------------------------------------------------------
$(LOADER): $(BOOT_SRC) boot/efi.h common/bootinfo.h common/elf64.h
	@mkdir -p $(BUILD)
	@echo "  CC      $(BOOT_SRC)"
	@$(CC) $(BOOT_FLAGS) -c $(BOOT_SRC) -o $(BUILD)/boot.o
	@echo "  LINK    $@"
	@$(PELINK) -subsystem:efi_application -entry:efi_main -nodefaultlib \
	           $(BUILD)/boot.o -out:$@

# --- Kernel -----------------------------------------------------------
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
	@echo "  GROESSE $$(stat -c%s $@) Bytes"

# --- Startfaehiges Abbild --------------------------------------------
# Eine schlichte FAT32-Partition ohne Partitionstabelle. UEFI erkennt
# das als EFI-Systempartition; fuer echte Datentraeger baut
# tools/mkusb.sh spaeter ein GPT-Abbild daraus.
$(IMAGE): $(LOADER) $(KERNEL)
	@echo "  ABBILD  $@"
	@dd if=/dev/zero of=$@ bs=1M count=64 status=none
	@mkfs.vfat -F 32 -n EREBUS $@ >/dev/null
	@mmd   -i $@ ::/EFI ::/EFI/BOOT ::/erebus
	@mcopy -i $@ $(LOADER) ::/EFI/BOOT/BOOTX64.EFI
	@mcopy -i $@ $(KERNEL) ::/erebus/kernel.elf
	@echo "  FERTIG  $@"

$(BUILD)/OVMF_VARS.fd: $(OVMF_VARS)
	@mkdir -p $(BUILD)
	@cp $(OVMF_VARS) $@

# --- Ausfuehren -------------------------------------------------------
QEMU := qemu-system-x86_64 \
  -machine q35 -m 512M -cpu qemu64 \
  -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
  -drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
  -drive format=raw,file=$(IMAGE) \
  -vga none -device VGA,edid=on,xres=$(XRES),yres=$(YRES) \
  -net none

run: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	$(QEMU) -serial stdio

# Kopflos starten, kurz laufen lassen, Bildschirm abziehen, beenden.
# Der Umweg ueber den Monitor ist der einzige Weg, ohne Fenster an ein
# Bild zu kommen.
shot: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	@rm -f $(BUILD)/screen.ppm $(BUILD)/serial.log
	@{ sleep 6; echo "screendump $(BUILD)/screen.ppm"; sleep 2; echo quit; } | \
	  $(QEMU) -display none -monitor stdio \
	          -serial file:$(BUILD)/serial.log >/dev/null 2>&1 || true
	@$(PY) tools/ppm2png.py $(BUILD)/screen.ppm $(BUILD)/screen.png
	@echo "--- serielle Ausgabe ---"
	@cat $(BUILD)/serial.log || true

debug: $(IMAGE) $(BUILD)/OVMF_VARS.fd
	$(QEMU) -serial stdio -s -S

info:
	@echo "Lader : $(LOADER)"
	@echo "Kernel: $(KERNEL)"
	@echo "Abbild: $(IMAGE)"
	@echo "OVMF  : $(OVMF_CODE)"

clean:
	rm -rf $(BUILD)
