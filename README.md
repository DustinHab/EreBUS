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
      obj/           object store, capabilities, message ports
      sched/         threads and the scheduler
      mm/            frame allocator, page tables, kernel heap
    tools/           mkfont.py, ppm2png.py, check-isr.py

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
* **M5 — done.** Preemptive round-robin threads, each with a guard page
  under its stack. Message ports carrying typed messages: a tag, a few
  values, and capabilities that arrive in the receiver's table with the
  rights the sender let go of. Send and receive are separate rights, so
  a send-only capability lets someone call a service without reading its
  mail. `make stack` runs a thread off the end of its stack and shows
  the whole chain hold: guard page, page fault, no stack left, double
  fault onto the TSS stack, readable report.
* **M6 — done.** Processes in ring 3, each with its own address space
  sharing the kernel's upper half. Four system calls in total: exit,
  yield, send, receive. A program starts holding exactly one capability
  and every register but that one is zeroed on the way out of the
  kernel. It cannot print -- it can only ask a console server to, and
  only because it was handed a send-only capability to that server's
  port. A second program reaches for kernel memory on purpose: it is
  ended, and the machine carries on.
* **M7 — done.** Double buffering with a damage rectangle, PS/2
  keyboard and mouse, and a desktop whose windows are views onto
  objects. Three windows open on one object -- as text, as bytes, as the
  object itself -- and typing into the writable one changes what the
  other two show. There is nothing to save, no format to agree on, and
  the read-only windows cannot alter a byte because their capability
  does not resolve for a write. make desktop types into it through
  QEMU's monitor and photographs the result.
* **M9 — done.** PCI enumeration, an AHCI driver, and persistence by
  snapshot. What goes to disk is the object graph itself: walk from the
  roots, follow every reference, write the reachable set down. Two slots
  written alternately, each with a generation and a checksum, so an
  interrupted write costs the changes since the last one and never the
  graph. Nothing is saved by hand -- the system writes when changes stop
  arriving. make persist types into a window on one boot and shows it
  still there on the next.
* **The shell.** No task bar, no window frames, no close and minimise --
  none of them mean anything when a view is just somewhere you are
  looking. Three ways of seeing one navigation state: focus with the
  path beside it, the reachable graph, the path as columns. Names live
  on the reference rather than the object, so an object handed to you
  cannot announce itself as something it is not. Everything visible that
  does something can be clicked; the keyboard is a shortcut, never the
  only way.
* **Time.** The snapshot ring keeps sixteen generations, so stepping
  back is reading an older state rather than undoing anything. The
  shell's own position is an object in the graph, which means it is
  written out with everything else -- so going back in time returns you
  to where you were standing then, not merely to the data.
* **The second party.** A running program is an object in the graph,
  and handing it a capability is the same gesture as making any other
  reference: point the program at the thing. The agent starts holding a
  console and its own letter box, cannot name anything else, and finds
  out what it may do by trying -- read yes, write no, then nothing at
  all, as the reference above it is narrowed and withdrawn. Two system
  calls were added for it (read and write on a held capability), making
  six. `make agent` walks the whole arc and prints what the program
  said at each step.
* **The cycle collector.** Reference counts free almost everything the
  moment it is let go, but a cycle holds itself up. `obj_collect()`
  finds the roots by arithmetic rather than by list: every reference is
  counted, the graph's own share of each count can be recomputed by
  looking, and whatever remains is a holder outside the graph -- a
  capability table, a kernel pointer. What no root reaches is freed,
  undelivered messages included. It runs in the same quiet moment as
  the snapshot, and the self test at boot proves the exact count: a
  loose cycle goes, a held one stays, and nothing else is touched.
  `make sweep` measures the cost: thirty thousand unreachable objects
  swept in single-digit milliseconds, with the heap giving back every
  byte -- and the sweep is the pause, since it runs with interrupts
  off. The same measurement caught the heap's first-fit walk going
  quadratic under load, which is why the heap is next-fit now.
* **Processes die whole.** A finished or faulted thread goes onto a
  list, and the next thread through the scheduler reaps it: stack,
  struct, and -- through a hook -- the process it was the last living
  part of. Domain, letter box, address space, all of it given back; the
  program object in the graph stays as a record and shows "ended". The
  exiting thread cannot free any of this itself: it is standing on the
  stack in question, running in the address space about to go, which is
  exactly why somebody else does it afterwards. Underneath, the heap,
  the frame bitmap and the reference counts stopped assuming a single
  thread; teardown from one thread while another allocates was the
  first thing that would have broken the assumption.
* **Programs hand each other things.** Pointing one program at another
  introduces them: what arrives is the other's letter box, send-only --
  a program is a party to be spoken to, not a thing to be read, and
  introducing somebody requires holding them with the grant right,
  because introducing IS passing on. A seventh system call, pass, lets
  a program send a held capability onward inside a message, attenuated
  by a mask it chooses; the checks live in the same port_send path as
  every other transfer, so a program can only give away less than it
  holds. `make relay` shows the whole arc: the courier is introduced to
  the agent, is handed a note read-write, and passes it on read-only by
  its own decision -- which the agent then runs into, enforced.
* M10 — booting real hardware from a USB stick

## A note on Secure Boot

The loader is unsigned, so Secure Boot has to be off on real hardware.
A signing chain of our own would be a separate project.
