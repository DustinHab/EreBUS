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
* **The journal.** The log is a text object in the graph:
  program utterances (attributed by the kernel, never self-signed),
  processes ending, generations written, collections. The reference to
  it is read-only, so history can be read by anyone who holds it and
  rewritten by nobody. The newest line sits above the footer and leads
  to the record; the snapshot carries the journal like everything else,
  so it survives reboots and appears in time travel as it stood then.
* **Programs are transparent to whoever holds them.** Focusing a
  running program shows two lists that are deliberately not the same:
  "points at" is your record of the giving, "it holds" is the kernel's
  record of what the program can actually reach -- narrowings,
  withdrawals and program-to-program passes included. Delegated
  authority that cannot be inspected by the one who delegated it is
  authority handed out and forgotten.
* **Reboot keeps the delegations.** The machine reads the wall clock at
  start-up (CMOS RTC), and a program object's payload carries a stamp
  tying it to one process in one boot -- identity no longer rests on
  where the allocator happened to put a struct. On restore, each
  program record is matched to its successor by name, its references
  are replayed onto it grant by grant (records pointing at records are
  re-pointed at successors), and the record's place and petname in the
  graph are taken over. The world comes back as it was left, including
  what programs held: after a reboot the courier passes its cargo on
  again, unprompted, and the journal shows it happening.
* **The standard programs.** The system ships with five, started at
  boot and standing in the graph, each doing one thing to whatever it
  is pointed at: the agent reports what it can and cannot do, the
  courier passes things on weakened, the clock keeps the time
  somewhere, the cipher turns writing over (and back -- it is its own
  inverse), the tally counts words and lines. A fresh system boots with
  the clock already wired: "the time" sits in the graph, ticking, held
  read-only -- a plain text object that one program keeps current. Two
  system calls came with them: clock, which answers without any
  capability because the time of day belongs to nobody, and a receive
  that can decline to wait, because a clock has a clock to keep. All
  five survive reboots with their delegations replayed, which has a
  visible consequence: the cipher turns its text again on every boot,
  because being handed something again means doing the job again.
* **Settings are a text object.** A table, one matter per row, the
  value in the right column: theme, save quiet, clock offset, pointer
  speed, hints, the scheduler's slice, and where a boot starts. Click
  into the value, edit it, and the system obeys as the letters land --
  the moment "light" is complete the shell is light, and the journal
  says so. No dialogue, no apply button, no registry: the kernel holds
  the only reader, the person holds the text read-write, and handing
  someone the reference read-only shows them how the system is set
  without letting them set it. The settings ride the snapshot like
  everything else, so they survive reboots and appear in time travel
  as they stood.
* **The activity table.** What the machine is doing, as an object the
  kernel rewrites once a second: uptime, cpu, memory, heap, objects,
  threads, and one row per running program with its share of the
  processor and the count of what it holds. The shares are not
  sampled: every handover of the processor books the interval to the
  thread that held it, so a row saying 3% means three hundredths of
  the last second, clock-measured -- and the boot thread, which only
  halts, is the idle in the summary. The reference is read-only; the
  machine reports, nobody edits the report.
* **The editor grew a caret.** Clicking into a text puts the caret
  there; typing inserts, backspace deletes before it. What made the
  settings table editable makes every text editable in the middle.
* **Three more standard programs.** sums fingerprints what it is shown
  -- the same object answers the same, anything changed answers
  differently, so a pair of journal lines settles whether a journey
  through other hands altered the cargo. watch keeps looking at what
  it holds and says, at most once a second, that it changed. wipe
  blanks what it is given -- write-only if need be, since blanking
  needs no right to read -- and its parting words point at the past,
  where the generations still hold what stood before.
* **The index.** The fourth way of looking, and the file manager's
  replacement: every object reachable from home, one line each -- id,
  kind, size, holders, the rights of the way in, the name, and for
  programs whether they run -- with the totals on top. Clicking a row
  walks there reference by reference through follow(), so the list is
  an overview and not a side door. What only the kernel and the
  programs hold is counted beneath the list, not shown: an overview of
  what you hold is not a window into what you do not.
* **Mark, take, put.** Drag across a text and the stretch is marked;
  take lifts the letters and they travel with the shell -- across
  objects, across views, even out of a past generation -- until put
  sets them down at the caret. Marking asks only for reading; put is
  the part that asks for write. Typing over a mark replaces it.
* **Withdrawal, one row at a time.** The capability list of a running
  program carries an x per row: whoever holds the right to give also
  holds the right to take back, the voice and the letter box included,
  and what another program passed along is reachable by the same
  gesture. The four views sit in the footer as four clickable words.
* **Search.** Typing while the index is open searches everything
  reachable -- the names things are shown under and the words the
  texts say. A content answer brings the line it was found in; enter
  goes to the first answer. With no paths to remember, asking for what
  a thing says is how a thing is found again.
* **Programs start from the palette.** The add palette offers the
  standard programs alongside text, bytes, list: making one is the
  same act as making anything -- it comes to exist and the reference
  is how it is held. An instance begins with a voice and a letter box;
  what it can touch is decided by pointing its object at things.
* **Measured memory, and a floor.** The activity table's mem column
  counts every frame a process owns, tables included, at the moment of
  asking. proc_create refuses when the table of the living is full or
  the free memory is down to its last megabytes -- the reserve belongs
  to what already runs.
* **Pictures.** A ninth type: a width, a height, one ink per cell,
  small enough that every lens honestly shows all of it. The picture
  lens paints -- sixteen fixed inks, deliberately not the theme's, a
  stroke drawing the line between pointer reports -- and bytes shows
  the same cells raw. Ink 0 is the paper, which is what a wiped
  picture is full of.
* **Texts become programs.** The runner: press run beside any readable
  text and a process starts, granted its own words read-only as the
  first gift, reading them line by line as it goes. The text stays
  live -- edit a running script and the next pass runs the new words;
  cut the reference and the program is words again. The language fits
  on one page and the page is in the graph ("the language", beside the
  seed objects): variables a..z, say, wait, get and put against "it"
  (the latest gift), if, skip, back, stop. Refusal lands in r rather
  than erring: a script learns what it may do the way every program
  here does, by trying. A wrong word stops the run and names its line.
  The interpreter is the one C file in the user section, held to the
  same discipline as the assembly programs: no globals, no string
  literals, nothing that leans on kernel memory from ring 3.
* **Scripts speak, keep time, and come back.** tell sends the rest of
  the line to "it" -- hand a running script another program and the
  two are talking, kernel-checked like every send. time reads the
  clock (the one capability-free call), rest sleeps on it, show says
  a variable and its value, which is the window a writer of programs
  is owed. The language page is kernel-authored and refreshed at
  boot, so it always states what the system of today understands --
  older graphs gain it on their next start. And a script's record
  survives a reboot the way the standard programs do: its words are
  still there, so the words simply run again, regranted what the
  record held.
* **The way out.** Enough of the internet to fetch a page: ARP, DHCP,
  DNS over UDP, a one-conversation TCP client carrying a single
  HTTP/1.0 request, and the machine answers pings. Who we are is
  asked, not assumed -- whatever network answers the lease decides,
  and a silent one gets the emulator's well-known defaults. Three
  drivers behind one seam: Intel's 8254x (QEMU, VirtualBox, VMware),
  the RTL8139 (proven against QEMU's model), and the RTL8168/8169
  family that sits in most machines with a cable socket -- that last
  one written against documentation and honest about never having met
  its silicon. The network is a capability, not an API: the fetch
  program is born holding "the wire", send-only, and whoever holds
  fetch can ask for pages while whoever does not cannot knock. A
  request is a text whose first line names the page; the answer lands
  in the same object at the rights the capability carried. Outbound
  only, one errand at a time, plain http -- TLS is honestly absent,
  and the refusal for https says so. No wifi: firmware blobs and a
  crypto stack are a different project, and pretending otherwise
  would be the lie this file avoids.
* **The html lens: a browser.** A fifth way of seeing a text -- as the
  page it is. Headings stand out and are ruled off, paragraphs wrap,
  lists indent and number, quotes step in, tables set their cells
  side by side, preformatted text keeps its spaces, links take their
  colour. An editable address line with back and forward arrows; a go
  button; the raw markup one lens tab away. Forms are real: text
  fields fill in, and a button assembles the query and browses there,
  so a search box works. The service follows redirects and introduces
  itself as erebus/0.1. Honest ceiling, and it is a real one: a text
  browser. No CSS, no JavaScript, no images beyond their alt words,
  and -- the big one -- no TLS, so https links are asked plainly and
  the many sites that answer only over https dead-end at their own
  redirect. Making it reach that half of the web means a TLS stack,
  which is its own milestone, not a corner to cut here.
* **TLS 1.3, the sealed half of the web.** One suite, chosen because
  every server speaks it: X25519 for the key agreement, AES-128-GCM
  for the record, SHA-256 for the schedule -- all written here, in
  kernel/net, and checked at start-up against the published vectors
  (FIPS, RFC 7748, the NIST GCM suite, and RFC 8448's worked key
  schedule). A failure there disables https rather than pretending.
  https links now go over a real handshake: ClientHello, the server's
  encrypted flight, its Finished checked, ours sent, the request and
  answer sealed. The browser marks a page "sealed" when it came that
  way. The honest limit, said in the word chosen and in the boot log:
  the certificate is not verified -- there is no RSA/ECDSA and no root
  store yet -- so the channel proves privacy against eavesdroppers,
  not the identity of who answered. Table-based AES leaks timing,
  noted and accepted for a single-person machine speaking outward.
* **The object pipe.** Objects travel between Erebus machines as a
  core system function, not a thing to wire up: any readable data
  object offers "send" in the header, the kernel carries its
  substance -- type, own name, payload; never references, never
  rights -- over udp to the peer the settings name, and what arrives
  on the other side is laid into the always-present arrivals list as
  a plain fresh object wearing its sender's name as a claim. Nothing
  that runs can cross, and authority cannot cross at all: the wire
  moves data, trust stays a human act. Offer, ordered chunks, one
  acknowledgement; no answer is retried thrice and then said aloud.
  Underneath, the stack learned what any real lan needs: neighbours
  on the same street are ARPed directly instead of sent through the
  gateway, and a machine can claim its address in the settings
  ("address | 10.9.9.20") for wires that have no landlord.
  tools/pipe-two.sh proves it: two machines on one emulated cable,
  the notes sent from one, found lying in the other's arrivals with
  the same words, surviving its reboot. Honest limits: the pipe is
  not yet sealed (it travels plainly, unlike https), and anyone who
  can reach the port can lay things into arrivals -- quarantined as
  inert data, but arriving unasked; sealing and greeting are the
  pipe's next milestones.
* **Finding each other.** Standing on the arrivals list shows the
  pipe whole: where it points, and a scan that calls out on the wire.
  Machines running this system answer with the name their settings
  give them ("name | erebus"), the answering machines stand there as
  clickable lines, and one click on a found machine points the pipe
  at it -- by writing the peer line into the settings the way a hand
  would, so the record stays honest and the journal says so. A seek
  is answered and remembered both ways: after one scan, both
  machines know each other. Connect is a click, send is a click,
  arrival is a journal line; no address needs typing when the other
  machine is on your own wire. The call is a broadcast, so it
  reaches your wire and not the world beyond a router; the named
  peer is also asked directly, wherever it lives.
* M10 — booting real hardware from a USB stick
* Certificate verification — turning the seal from privacy into
  identity: RSA/ECDSA signatures and a root store
* A sealed pipe — the object pipe over the same cryptography https
  uses, so substance crosses machines unreadable to the road

## A note on Secure Boot

The loader is unsigned, so Secure Boot has to be off on real hardware.
A signing chain of our own would be a separate project.
