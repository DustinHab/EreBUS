# Erebus

An object-based, capability-secured operating system for x86_64 UEFI.
Not a Unix rebuild — its own boot loader, its own kernel, its own ideas.

## The idea

| Unix | Erebus |
|---|---|
| Everything is a file | Everything is a typed object |
| Paths in one global namespace | References only; no global namespace |
| Permissions are checked | The reference *is* the permission (capability) |
| Saving into files | Versioned snapshots of the object graph |
| Byte streams (stdin/stdout, pipes) | Typed messages |
| An application opens a file | A window is a view onto an object |

The security consequence: a program cannot name anything it was not
handed. There is no all-powerful state to capture ("root"), no setuid
chain, and no way to *acquire* authority — it is only ever passed along,
and never strengthened in the process.

The absence is the argument. Look at `kernel/include/eb/object.h` and
note what is not there: no `obj_find()`, no lookup by name, no way to
enumerate what exists. Path traversal, symlink races, a library quietly
reading somewhere it should not — every one of those needs a namespace
to work in, and there is none to work in.

## Building

Needs a Linux environment (here: WSL2 with Ubuntu) and

    apt install clang lld nasm make qemu-system-x86 ovmf mtools \
                dosfstools xorriso gdb unifont python3-pil

then

    make          # loader, kernel and a bootable image
    make run      # start in QEMU, serial output in the terminal
    make shot     # headless, writes build/screen.png and build/serial.log
    make fault    # same, but with the deliberate fault test built in
    make debug    # start halted, waiting for gdb on port 1234
    make clean

From Windows, out of the project directory:

    wsl -d Ubuntu -- bash -lc "cd /mnt/c/erebus && make run"

## Layout

    boot/            UEFI boot loader (PE/COFF, MS ABI)
      efi.h          hand-written UEFI interface, no gnu-efi
      boot.c         GOP, file system, ELF loading, ExitBootServices
    common/          shared by the loader AND the kernel
      bootinfo.h     handover structure
      elf64.h        ELF header structures
    kernel/
      arch/x86_64/   start.S, linker.ld, gdt.c, isr.S, trap.c
      hw/            device drivers: serial, cpuid, 8259, timers
      gfx/           framebuffer, font, screen console
      lib/           kprintf, memory routines, panic, hardening
      include/eb/    headers
    tools/           mkfont.py, ppm2png.py, check-isr.sh

## Decisions, and why

**Our own boot loader instead of GRUB or Limine.** Booting is part of
the system. No foreign code between firmware and kernel also means no
foreign code in the trusted base of a security-focused system.

**Kernel in the upper half, at -2 GiB.** The lower half belongs to user
processes: every address space maps the kernel into the same high window
and a different program into the low one, so a system call needs no
change of page tables. The loader builds the first tables and jumps
straight to the virtual entry point, which keeps the kernel's own entry
path trivial — it wakes up already living at its own addresses.

Done at milestone 3 rather than at user mode, deliberately. Retrofitting
the split later would have touched the linker script, the loader and
every absolute address in the tree; at that point there were seven
source files, and three milestones on there would have been thirty.

**A bitmap frame allocator, not a free list.** A free list stores its
links inside the free pages themselves, so every allocation writes to
memory that by definition belongs to nobody — fast, and a place where a
stray write stays invisible until the allocator hands the same frame out
twice. The bitmap keeps its bookkeeping in one region that can be
checked, at a cost of one bit per frame: 16 KiB per gigabyte of memory.

**Font from GNU Unifont, embedded at build time.** Exactly 8x16, no
runtime rasteriser, no font file to read. Covers Latin plus box drawing
and block elements.

**Serial port as the first output.** It works before anything else does,
and it still works when the framebuffer has been scribbled over. QEMU
pipes it straight into the terminal.

It is also, by a wide margin, the slowest thing in the boot path. At
115200 baud a character takes 87 microseconds on the wire, and the
driver waits for each one: measured against the same build with the
serial sink removed, a log line costs 20 ms with it and 0.3 ms without.
Linux behaves identically with `console=ttyS0`. If a boot ever looks
sluggish, that is where the time went, not in the graphics —
`make shot SERIAL=null` shows the difference.

**No SSE or MMX in the kernel.** Vector registers would have to be
enabled first and saved on every context switch. Without them the switch
is cheaper and the boot path shorter.

**The 8259 controller before the APIC.** Every machine has one, or
emulates one, and it needs no ACPI tables parsed first. The local APIC
is the better answer and will replace it, but not while there are more
fundamental things missing.

**Interrupt stubs as a 16-byte table.** Vector *n* lives at
`isr_stubs + n * 16`, so the C side needs no table of 256 symbols. If a
stub ever outgrew its slot the failure would look like random
corruption, so `tools/check-isr.sh` verifies the layout against the
built binary.

## Status

* **M1 — done.** Own UEFI loader; the kernel takes over the display and
  the serial port and reads the memory map.
* **M2 — done.** GDT with a TSS and three fault stacks, IDT with 256
  vectors, readable crash reports with register dump and call trace,
  8259 remapped, PIT tick, TSC calibrated against the PIT, timestamped
  log lines, and two independent clocks checking each other at start-up.
* **M3 — done.** Kernel moved into the upper half with a direct map of
  physical memory; bitmap frame allocator with frames zeroed before
  reuse; the kernel's own page tables, mapped per section, replacing the
  loader's flat ones and dropping the identity mapping with them; NX,
  CR0.WP, SMEP, SMAP and UMIP switched on; a kernel heap that clears
  blocks on release. Each layer verifies itself at start-up, and
  `make wx` proves W^X by trying to write into the kernel's own code.
* **M4 — done.** Typed objects with reference-counted lifetimes and
  outgoing reference slots, so the store is a graph rather than a heap
  of blobs. Protection domains, each owning a capability table that is
  the complete extent of what it can reach. Handles are per-domain slot
  indices carrying a generation, so they mean nothing outside the table
  they came from and stop working for good once revoked. Rights only
  ever narrow on delegation. The self test checks all of that, including
  that guessed handles resolve to nothing in a domain that was given
  nothing.
* M5 — threads, scheduling, message passing
* M6 — user mode (ring 3), processes, separate address spaces
* M7 — compositor, windows, PS/2 input
* M8 — desktop: object browser and type viewers
* M9 — persistence: NVMe/AHCI, snapshot and restore
* M10 — booting real hardware from a USB stick

## A note on Secure Boot

The loader is unsigned, so Secure Boot has to be off on real hardware.
A signing chain of our own would be a separate project.
