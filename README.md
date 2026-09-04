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
  still there on the next. A generation slot holds a megabyte; texts
  and bytes from four kilobytes up lie in a log of their own behind the
  ring, once each under the SHA-256 of their contents, and the
  generation keeps hash and place. Unchanged sources cost nothing per
  save, an edit adds one entry, and when the log is full it is
  compacted down to what some generation still refers to -- the oldest
  generations giving way if even that is not enough. An entry that
  does not hash to its name is never handed back; a generation whose
  entry is gone is refused whole and the one before it tried.
  tools/bigpersist.sh carries a source, a header and a kernel image in
  and reads all three back after a boot without the disk;
  tools/logfull.sh cuts the store down until a dozen versions of one
  source fill the log, and shows the compaction, the generations let
  go, and the latest text whole after the next boot.
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
  and a silent one gets the emulator's well-known defaults. Four
  drivers behind one seam. Intel's older family, which covers the
  8254x the emulators offer, the PCI Express parts that followed, and
  the ones built into a chipset up to the I219 -- one driver, because
  the rings live at the same addresses and the descriptors have the
  same shape across all of it; what differs is where the card's own
  address is read from, and that a chipset card shares its wire with
  the firmware's management and so is never reset. Intel's later
  family for the 82576, I210, I211 and I350, which moved the rings
  into a block per queue and gave the descriptors a second form.
  The RTL8139, and the RTL8168/8169 family, that last one written
  against documentation and honest about never having met its silicon.
  Which card carries the traffic is not the order of the bus: every
  driver is asked first to pass over a card with no cable in it, and
  only then to take anything at all, because a board with two sockets
  has one cable more often than not. tools/nictest.sh boots each
  family with an address of its own and waits for a lease, which is
  both rings proven at once; its last boot is two Intel cards of
  different families with the socket pulled on one of them.
  The network is a capability, not an API: the fetch
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
  the same words, surviving its reboot. Honest limit: anyone who can
  reach the port can lay things into arrivals -- quarantined as
  inert data, but arriving unasked.
* **The pipe travels sealed.** Before anything crosses, the sender
  knocks: HELLO carries a fresh x25519 key, WELCOME answers with the
  other side's, and both ends derive AES-128-GCM keys -- one per
  direction, a counter per record, replays refused, the envelope
  header bound in as associated data. Offers, chunks and
  acknowledgements only ever travel inside these envelopes; a plain
  one arriving is turned away. Keys are fresh at every knock and
  kept nowhere. The proof run records the wire at the receiver's own
  card and greps the dump: the words of the notes do not appear on
  it. Honest limits, the same ones https carries here: the knock is
  private, not proven -- an impostor who answers the knock first is
  spoken to, because there is no identity under the keys yet -- and
  discovery (SEEK/HERE) speaks plainly, since names are claims
  either way. If the crypto self-test fails at boot, the pipe
  refuses to carry anything at all rather than fall back to plain.
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
* **Sending starts where the thing is.** The send word stands in the
  header of every readable data object whether a peer is set or not;
  hiding it would leave no trail to why. Pressed with a peer set, it
  sends. Pressed with nobody set, the choosing opens under the word
  itself: a scan goes out unasked, whoever answers stands there as a
  line, and one click points the pipe and lets the thing go in the
  same breath. Escape or a click elsewhere puts it away. The proof
  script walks exactly this shortest path: stand on the notes, press
  send, click the machine that answered.
* **Far work.** Beside send stands ask: the focused text travels to
  the peer as a recipe, runs over there, and what it answers comes
  home -- a journal line saying "job 1 answers: 42", and the answer
  as a small text in arrivals. This is the first stone of a
  decentralised machine: any Erebus can lend its processor to
  another, and what crosses is still only substance -- the recipe is
  words in the one-page language, never machine code. The visiting
  script runs under the same interpreter as any local text, holding
  exactly two things: its words, read-only, and a send-only way
  home. It cannot see the graph, the disk, or the network; the
  capability table it was born with is the whole world it can touch,
  and the memory quota bounds the rest. Its first gift is the way
  home -- wait, then answer, is the whole convention -- and it
  carries a time budget the interpreter itself enforces: loops,
  rests and waits all end when the clock says so, which is why a
  visiting script cannot outstay its welcome. Lending the processor
  is a standing decision of the owner: the settings ship saying
  "work | refused", and only "work | welcomed" changes that. One job
  at a time; a repeated ask gets the old answer repeated, never a
  second run. The proof run computes 6 times 7 across the cable and
  hears 42 back, then sends a second recipe that loops forever and
  hears, honestly, that it ran out of time.
* **The home has shelves.** A root where every program, page and
  list lay on one level read like a drawer tipped out. Two lists
  sort it: "programs" holds what runs -- the ten standard programs
  live there -- and "system" holds the machine's own pages: the
  time, the log, the settings, the activity. The person's material
  stays on top, arrivals beside it. The sorting is the system's,
  not the seed's: a graph restored from an older day is shelved the
  same way on the way in, keeping every petname and every right,
  and everything that used to find these pages -- the journal
  click, the clock's wiring, the record replay -- walks the shelf
  as a real path of references, not a shortcut.
* **The map moves under the hand.** The graph view was fixed and
  cut off whatever lay past the screen's edge. Now the wheel zooms
  it around the pointer -- boxes shrink and grow, labels stand
  aside when their boxes get too small to carry them -- and taking
  hold of empty ground drags the whole map. Clicking a node still
  walks to it; only the ground pans.
* **Everything that holds more than it shows scrolls.** The mouse
  grew its wheel -- the driver knocks the IntelliMouse knock, and a
  device that knows it answers with a fourth byte -- and one scroll
  mechanism serves every area: the rendered page, the focused text,
  the byte view, the structure lens, the references panel, and the
  index. The wheel works over whichever area it is over; the
  clickable edge pages up and down; a thumb says where one is.
  Typing pulls the window to the caret, so writing below the fold
  is never invisible. The journal opens at its end -- the newest
  line is why one came -- and stays pinned there as lines arrive,
  until scrolling back up to read history unpins it.
* **The desk deals the work out.** Far work grew from one ask at a
  time into a queue that divides: a task whose first line says
  "split P from LO to HI" is cut into parts, each carrying its own
  stretch of the range -- a visiting script's first wait reads the
  low end as m, its second the high end -- and the desk deals the
  parts round-robin to the machines that answered the scan willing,
  collects their numbers, and sums them. Four parts of a sum over
  two lent machines come back as one total, written into the task
  itself as "= 50005000 (4 parts)"; a busy neighbour is not a
  failure but a turn to wait for, an unreachable one costs the part
  one retry elsewhere. The palette offers "task" ready-seeded with
  exactly this shape. Honest limits: the desk runs one job at a
  time, a worker lends one part at a time, the only combining rule
  is the sum -- anything else is the recipe's to encode -- and the
  asking machine does not lend its own processor to its own parts
  yet.
* **The foreman, and the desk's door.** Asking far work is now a
  capability: the wire's port takes a task under the WORK tag, from
  any program that was handed it, which makes automatic
  distribution something the graph can say -- who holds the wire
  can send work out, and nobody else. The foreman is the program
  that uses it: point tasks at it and each is handed to the desk,
  watched until the answer stands written inside it, and announced
  -- no click after the pointing. A task whose first line carries
  "again N" is handed in anew every N seconds, the old answers left
  behind as history, which turns a task into a standing order: a
  measurement, a sum, a check, recomputed across the machines on
  the wire while nobody watches. The foreman holds the wire and
  what it is given, and nothing else.
* **The everyday grew in.** Letting go of a reference steps it into
  a bin on home first, petname and rights along; inside the bin the
  letting go is final. "turn off" at the footer's far right saves
  the graph and puts the machine to sleep at the ports firmware
  listens on. The clock sets itself from the net soon after the
  wires are up and hourly after, utc under the settings' own
  offset. A running program can be ended by whoever may give to it:
  the thread is condemned and finishes at its next step into the
  kernel, through the same exit and reaping a voluntary end takes
  -- the honest note being that a program which never asks the
  kernel for anything cannot be caught this way, and none shipped
  is like that.
* **Bundles.** "pack" folds a readable list -- texts, bytes,
  pictures, lists in lists, four levels, loops walked once -- into
  one BYTES object sized for the pipe; "unpack" builds the list
  back. A folder of things crosses machines as one send.
* **The keys speak german when asked.** "keys | german" turns the
  same keyboard into qwertz with umlauts, sharp s, the right alt's
  third meanings and the extra key beside the left shift; the font
  carried the glyphs all along. The seed stays english.
* **The split.** A fifth way of looking: two independent walks side
  by side, each with its own path, selection and place in its text.
  The left one writes -- caret, chips and header belong to it; the
  right one reads and walks. The same object open in both is one
  object seen twice, never a copy.
* **The exchange disk.** A FAT32 disk beside the store is the
  bridge to the rest of the world: at boot its root directory's
  files come in as objects on "the disk" under system -- texts when
  they read like text -- and "write out" on that list lays new
  entries down as files under plain 8.3 names. Read and write both
  proven against mtools from outside. Honest limits: FAT32 only,
  the root directory only, 64 KiB a file, long names read but not
  written.
* **Copy and find.** Every readable text, bytes, picture or list
  offers a copy chip; the copy lies beside the original, named
  after it, a list copied flat with the same references and rights.
  The text lens grew a find bar -- the word at the tab row's right
  end, or ctrl+f: matches underline in place with a count, enter
  walks the caret match to match, escape closes, and following a
  reference closes it too, since a needle belongs to the text it
  was typed over.
* **reckon.** The calculator is a program like any other: point it
  at a text and every line ending in an equals sign gets its answer
  written after it -- sums, products, remainders, parentheses,
  negatives. A line it cannot read gets a question mark instead of
  a guess, and answered lines no longer end in the sign, so running
  it twice changes nothing.
* **pulse.** The machine's vitals drawn instead of listed: given a
  writable picture and the readable activity page it paints one
  column every other second -- used memory a blue bar from the
  bottom, the processor's share a red mark, a grey cursor walking
  ahead. The picture is an object, so the history rides the
  snapshot and the drawing goes on across a reboot. The breath
  between columns is deliberate: a canvas touched every second
  would never be still enough to save.
* **The terminal.** The sixth way of looking: the system spoken to
  in lines. The grammar is one sentence shape everywhere -- a verb,
  a name, and "to" or "at" when two things meet; names may have
  spaces, numbers count slots, and there are no flags, no paths, no
  punctuation to remember. The walk is the same walk the shell makes
  with clicks: go follows a reference the standpoint can see and
  only ever narrows what is held. look, read, write, make, copy,
  rename, let go, run, give, end, find across everything reachable,
  and the wire's whole reach -- scan, found, point at, send, ask,
  say. The core knows nothing of screens: it takes lines and appends
  to a transcript, the screen's view is one feeder, and a remote
  line arriving over the network later is meant to be another,
  through the same doors -- once the identity question below has an
  answer, because a terminal that anyone on the wire may feed is not
  a door, it is a hole.
* **The machine programmed on itself.** An assembler lives in the
  system: a text of x86-64 instructions in the ordinary words --
  mov, add, cmp, jne, call, syscall, brackets for addresses, section
  code and section data, db and dq -- becomes an image, laid beside
  the text; "run" on the image loads it into a process of its own,
  code read-and-execute, data read-and-write, never both, and hands
  it the same two handles and eight system calls every shipped
  program gets. The page "the machine" in the aside is the whole
  contract -- registers at entry, the calls, the message layout --
  written so that the page itself assembles: stand on it, press
  assemble, run what it made, and it says hello. A program built
  this way comes back at the next boot the way a script does; it is
  part of the world. This is what "without restrictions" means here:
  anything the processor can do in ring 3 can be written on the
  machine, run on the machine, and kept on the machine -- and it
  still holds only what it is given, because the loader changes
  nothing about the rules, only about who wrote the code. The honest
  edge: the kernel itself is C, and a C compiler is a project of its
  own; until one exists, the kernel is built outside and everything
  above it can be built inside.
* **The compiler.** C lives in the system now, in the shape a person
  writes it: char, short, int and long, signed or not; float and
  double; pointers, arrays, structs and unions, bit fields, packed
  records, typedefs, enums; functions with up to sixteen parameters
  (six in registers, the rest on the stack), variadic ones with
  va_arg, and pointers to them; the operators with their precedence;
  if, while, for, do, switch, goto; sizeof and casts; initializers
  with braces and designators; static locals; _Static_assert; a
  preprocessor with function-like macros, #if and #elif arithmetic,
  defined(), #include of a text lying beside the source, and
  <stdarg.h>, <stdbool.h>, <stdint.h>, <stddef.h> carried inside.
  "compile" on a text lays the assembly it became beside it -- to be
  read, which is the difference between a tool and a trick -- and
  the image beside that, through the same assembler. No library
  comes with it: main is called with the two handles a program
  starts holding, and syscall(nr, ...) is the door to the kernel.
  Floating point meant a kernel change: the vector unit is on for
  programs now, each program's registers saved and restored around
  it by the scheduler, and never used by the kernel itself, which is
  built without it -- the compiler holds a double's bits and makes
  them with whole-number arithmetic. The page "the compiler" is the
  contract and a program at once. Inline assembly is here in the form
  the kernel writes it -- asm volatile with outputs, inputs and
  clobbers, the a b c d S D r m i constraints, register variables tied
  to a name -- translated from the gnu dialect into the machine's
  own; structs come back by value, compound literals work, and a
  variadic call takes any number of arguments (a variadic callee gets
  them all on the stack, one row, which is this system's convention
  and no one else's). Proven twice over: a program of twenty-one
  checks, one per feature, compiled and run on the machine with every
  check answering ok; and the same compiler, built for the host from
  the same two files (make cchost), reading the kernel's own sources:
  tools/cctrial.sh pushes every kernel file through it, and every one
  compiles. The compiler reads its own text, and the assembler's and
  the linker's. Honest edges, on the page as well: a struct is handed
  to a function by pointer, never by value, and there is no 128-bit
  type.
* **The linker.** What the assembler makes is an object now -- the
  bytes of each section (text, rodata, data, bss, user), the names it
  lays down and the names it only uses, and every place in the bytes
  that wants a name's address -- and the linker joins objects into one
  thing that runs: an image for the loader, or the kernel's own ELF
  when one of the objects lays down kmain (linked at -2 GiB, loaded at
  2 MiB, three segments, the layout's names such as __kernel_start
  provided). C's static names are private to their text; the rest meet
  assembly on the same words, so kmain is kmain and _start is _start.
  A text with a main is still an image in one step; a text without one
  is an object that says which names it waits for. The kernel's own
  assembly files are in the gnu dialect, and a translator carries that
  into the machine's: .set, .rept, .if, numbered labels, AT&T order.
  In the terminal: compile and assemble as before, "link" on a list of
  objects, "build" on a list of texts (every .c and .S in it, then the
  link), "take in" and "write out" for the exchange disk. A build runs
  in a thread of its own and reports to the journal, so the screen
  keeps up while the compiler works; one build at a time, and the
  other tools wait for it, since they share its tables. Proof on the
  host: tools/selfbuild.sh builds the whole kernel with cchost -- the
  same cc.c, asm.c, gnu.c and ld.c the kernel carries -- and boots it
  in QEMU; it comes up to the desktop, with the network, the door and
  the snapshots working. Proof on the machine: tools/selfbuild-machine.sh
  carries every source and header in on the exchange disk, the
  terminal takes them in and builds the list -- seventy-one objects
  in a quarter of a minute -- into kernel.elf, writes it out, and the
  host boots what the machine made: it comes up the same way. What
  comes in from the disk is the machine's own from then on -- it lies
  in the log of big objects and comes back after a boot without the
  disk; the disk's version is taken again only once the copy is let go.
  A kernel the tools build stays transient: it is reproducible from
  the sources, and a new one per build would fill the log with
  history nobody asked for. Three bugs the clang build had hidden: a far return of
  doublewords where quadwords were meant, imul with a number silently
  encoded as imul with rax, and initializer address tables cut off at
  sixteen entries. Honest edges: there is no 128-bit type; the boot loader is still
  built outside. A struct goes to a function by value the way one
  comes back from it: the caller keeps a nameless copy and passes its
  address, the callee works on that copy -- its own convention, the
  kernel being built by nothing else. The generator's output is
  tidied on the way out: numbers and plain variables go straight into
  the second operand, a plain variable is assigned or counted in
  place, and a peephole over each function folds address-and-load
  pairs, push-and-pop pairs, jumps to the next line, and the
  set/movzx/test/je shape of every condition into one conditional
  jump. Functions start on sixteen-byte boundaries, which came out of
  a measurement: QEMU's translator chains jumps only within a page,
  and a boot took twice as long because the console's scroll loop had
  come to lie across one. The self-built kernel's text is a sixth
  smaller than before and it boots faster than the unoptimized one.
* **Installing.** The boot disk is a disk of its own to the kernel
  now, and "install" on a kernel's bytes -- the word in the terminal,
  the chip on the bytes -- lays it down in \erebus as kernel.new,
  then turns the names: the running kernel becomes kernel.old, the
  new one kernel.elf, and the count of starts goes to zero. "restart"
  saves the graph and starts the machine again. The loader keeps the
  count: it raises it before every start, the kernel clears it once
  it is up, and two starts that never cleared it mean the installed
  kernel does not come up -- the loader puts kernel.old back under
  the name it reads, boots it, and that kernel says in the journal
  what happened. So the loop is closed: change a source on the
  machine, build, install, restart, and the machine runs what it
  built; a mistake costs two failed starts and nothing else. Proof in
  tools/install-test.sh: the self-built kernel installed and started
  from inside, saying it was built here; then a kernel whose first
  instruction is ud2, installed the same way, and the machine back on
  the previous kernel two starts later. The FAT reader learned
  directories and the writer to replace, rename and delete for this,
  and one fat sector is held between uses, which made writing a
  kernel out five times faster.
* **Tests through the door.** tools/door.sh types a test key into the
  settings once, the honest way, and keeps that store; tools/doorboot.sh
  starts a copy of it with the door reachable, and door_say runs one
  terminal line over ssh and hands the answer back as text. No key
  timings, no screen coordinates: tools/doortest.sh runs the
  compiler's proof that way. The older tests keep typing on the
  screen, which is also a test of the screen.
* **Fuzzing the tools.** tools/fuzz/run.sh builds the compiler, the
  assembler in both dialects and the linker under libFuzzer with the
  address and undefined-behaviour sanitizers, seeds them with the
  kernel's own sources, and lets a machine look for the input that
  makes one of them read or write where it should not. Its first
  minutes found the signed overflows in the expression evaluators,
  which are unsigned now.
* **Names in the crash report.** The kernel carries a table of its
  code's names, between the data and the bss, and the report reads a
  fault's address and every step of the trace back to a name and an
  offset. The outside build links twice -- once to learn the
  addresses, once with the table -- and the machine's own linker
  writes the table itself, in the same shape, so a self-built kernel
  reports the same way.
* **The door.** The terminal reached over the network, by real ssh,
  so the client anyone already has can knock: version 2,
  curve25519-sha256 for the exchange, ssh-ed25519 for the host's
  name and the visitor's, aes128-gcm@openssh.com on the wire -- one
  profile, chosen and tested like every primitive under the seal
  (SHA-512 and Ed25519 check themselves against their RFC vectors at
  start-up). Who may come in is not a user table: it is the
  "door |" lines in the settings, each one a public key pasted from
  an id_ed25519.pub -- the key is the person, and the identity
  question the seal left open is answered here the only honest way,
  by the person naming the key. A visitor who proves they hold one
  gets a terminal session of their own, walking the same graph from
  the same beginning as the screen. The host's own key is made once
  and kept in the graph as "the door key" under system, held with no
  rights at all: it exists where everything exists, and nobody reads
  it. The fingerprint stands in the boot log and the journal, for
  checking the client's first-visit warning against. Honest limits:
  one visitor at a time, no rekeying (a client that asks for it
  after an hour is let go), and the plain port is the door -- there
  is no knocking on a sealed pipe first.
* **The line.** One running conversation with whoever else is on
  the pipe, kept as a read-only text on the system shelf. With it
  in focus the footer becomes a mouth: letters gather, enter says
  them, and the word crosses inside the seal -- with no seal up but
  a peer named, the say knocks first and the word follows. Proven
  with two machines and a wire dump holding no clear word; the
  identity caveat below wears these clothes too, since a name on
  the line is the sender's claim, not a proof.
* **What a result is worth.** An answer is a claim by the machine
  that answered, nothing more. Nothing here proves the arithmetic
  was actually done: there are no attestations, no redundant runs,
  no majority votes, and the journal's wording keeps that visible --
  "job 1 answers: 42" reports what the far side said, not what is
  true. On your own wire, among your own machines, a result is
  worth exactly what those machines are worth to you, which for the
  network this pipe is built to span -- one person's machines
  lending each other their processors -- is the honest and
  sufficient answer. In an open network it would not be: there, ask
  a machine you have reason to trust, or ask the same job of two
  and compare, by hand, knowing the pipe does neither for you.
  Writing this down is the feature; a distributed system that
  implies its results are verified when they are not would be lying
  in its architecture.
* **Identity under the knock — done.** The door's key is the
  machine's identity for the pipe as well: a knock and its answer
  carry the machine's public key and a signature over the exchange
  -- session, both fresh keys, a fixed word -- so a signature cannot
  be lifted from one exchange into another. The first machine to
  answer at an address is believed and written into the settings as
  a "known |" line, address and key in ssh's letters; from then on
  whoever answers there must prove it holds that key, or the knock
  is turned away, the send does not go, and the journal says so. A
  machine that comes without a key where one is remembered is turned
  away too. The honest limits: the first meeting is trust on first
  use, as ssh has it, and a key that has legitimately changed is
  forgotten by removing its line, by hand. tools/pipe-identity.sh
  lets two machines meet, then gives the answering one a fresh
  identity at the same address and shows the second send refused.
* **USB keyboards and mice — done.** kernel/hw/xhci.c drives the
  host controller every machine since about 2012 has: the firmware
  handed over, the controller reset and given its rings, the root
  ports walked, each device given a slot and an address, asked who it
  is, and -- when it is a keyboard or a mouse -- set to the boot
  protocol with its interrupt endpoint polled by a thread. Reports
  become the bytes a PS/2 keyboard would have sent and go into the
  same queues, so the shell never learns which wire a key came down;
  held keys repeat the way a keyboard's own electronics used to.

  A device is not asked what class it is and taken at its word: a
  receiver carrying a keyboard and a mouse calls itself class zero,
  meaning the answer is further in, so every interface is walked and
  each one that types or points gets an endpoint of its own on the
  same slot. Hubs are driven rather than merely named -- their ports
  powered, watched, reset, and what is behind them enumerated with the
  route the controller needs to reach it, which is where most of what
  a person touches actually hangs. tools/usbtest.sh boots with the
  i8042 switched off and QEMU's usb keyboard and mouse on an xHCI
  controller, and types the terminal's day's work through them;
  tools/usbhub.sh does the same with both of them behind a hub and
  nothing at all on the machine's own ports.

  A pointer is not read the boot way. The boot protocol was defined
  for a firmware setup screen and has three bytes in it -- buttons and
  two directions -- so a mouse read that way cannot scroll, whatever
  wheel it has. Every pointer describes its own layout, and that
  description is read and taken apart: which bits are the buttons,
  where each direction lives and how many bits it uses, where the
  wheel is, and whether reports are numbered. That also gets the mice
  that count in sixteen bits. Where the description cannot be made
  sense of, the boot protocol is asked for and the old three bytes are
  read, because a pointer that moves without scrolling beats one that
  does neither. The reading proves itself at start-up against three
  descriptions written out by hand, because the emulated mouse is too
  kind to be a test: it sends its wheel whatever it was asked for, so
  it looks the same whether the description was read or ignored.

  A device plugged in while the machine runs is found, and this took a
  pause to get right: something electrically present is not yet ready
  to answer, and a port reset too early gets nothing back. Under
  emulation the device is ready before the plug is in, so the pause
  looks like superstition until a real socket proves otherwise.
  tools/usbplug.sh pulls a mouse out, puts it back, and uses it.

  Not driven, and said so: USB disks and anything isochronous.
  Written but not yet proven against a real one: two inputs on a
  single device, because no emulated device offers that shape.
* **M10, the disk half — done.** tools/mkusb.sh makes build/stick.img:
  a GPT disk with an EFI system partition carrying the loader and the
  kernel, and a partition of the store's kind -- a type GUID of this
  system's own, E2EB0500-5354-4F52-4552-454255530001 -- which the
  kernel finds on any disk, the boot disk included. One disk carries
  the whole system that way; a machine with somebody else's system on
  the rest of the disk lends this one a partition. tools/stick.ps1
  writes the image to a usb stick on Windows, refusing anything that
  is not a stick; dd does it elsewhere. tools/sticktest.sh boots the
  image as the only disk twice and finds on the second boot what was
  typed on the first. What is still missing for a real machine: usb
  disks are not driven, so the stick boots the machine but its own
  store partition is out of reach -- the store is then a partition of
  this kind, or a blank disk, on SATA, or the machine runs without a
  memory and says so. Real hardware has not been booted yet.
* **Settling — done.** A machine that runs off the stick, or whose
  disks are somebody else's, comes up without a memory and says so in
  the journal, with the words that make one. In the terminal, `disks`
  shows every disk on the bus with its partitions, their kinds and the
  room left between them. `settle on disk N` takes a disk whole: a
  partition table, a boot volume with the loader and this very kernel
  -- the loader hands both files over at boot for this -- and a store
  on the rest. `settle in partition P of disk N` makes one partition
  the store and touches nothing else on the disk. `settle in the free
  space of disk N` makes a store in the largest gap and leaves every
  partition as it is -- how a machine with somebody else's system on
  it lends this one a corner. Each says exactly what is lost and does
  nothing until told `yes`; then the running machine adopts the new
  store on the spot and keeps its graph there from then on. A disk
  taken whole boots the machine the next time. A blank disk beside a
  machine that runs off a stick is offered, never taken; beside a
  machine that boots from its own disk it is the store, as before.
  tools/settletest.sh boots the stick image on usb with a blank disk
  on sata, settles on it through the terminal, and boots the disk
  alone; tools/settlefree.sh settles in the free space beside a
  partition full of random bytes and compares those bytes afterwards.

  The words are the whole mechanism, but somebody starting the system
  for the first time does not know them, so the machine asks. Where
  there is nowhere to keep anything, at least one disk, and a keyboard
  to answer on, start-up stops once and shows what is on the bus:
  a number takes that disk, escape carries on without a memory, and
  nothing is written until the word `yes` is typed out after being
  told exactly what will be lost. A machine nobody is sitting at
  never sees the question -- no keyboard, or nobody answering for two
  minutes, and it boots straight through. The disk at the first port
  is no longer refused on the grounds of being first: a machine
  running from a stick has somebody else's system there, and that is
  the disk a person most often means. tools/installtest.sh boots the
  stick beside a disk that already carries a partition table and data,
  answers the question the way a person would, and boots the disk
  alone afterwards.

  A machine already settled takes a newer system without being
  emptied. Booting a newer stick beside the disk brings up the new
  kernel with the disk's store -- the same graph, the same desktop, and
  nothing on the screen to say anything changed, which is why the
  kernel now names itself at the top of the boot log and at the far
  right of the desktop, from the nearest tag and commit. `install this
  kernel` then puts the loader and kernel the machine started with onto
  the boot disk: the kernel it had steps aside as kernel.old, the
  loader is replaced under the one name the firmware reads, and the
  store is not touched. `settle` could not do this -- it empties the
  disk, and refuses the disk whose store is in use. tools/renewtest.sh
  settles one build onto a disk, boots a second that calls itself
  "renewed" from a stick beside it, installs, and boots the disk alone:
  it must call itself renewed and still find the graph.

  And the loop closes. A machine with no exchange disk had no way to be
  handed a text except a line at a time; `receive <n> bytes as <name>`
  makes a text of that size where the session stands and takes the
  next n bytes of the session as its contents, nothing inside them
  looked at, the door opening its window again by as much as it took.
  tools/mkupload.sh turns the source tree into one such stream -- every
  source and header under its own name, and a version text made up on
  the spot -- and a shell session through the door carries it in.
  `build kernel` then compiles all of it with the machine's own tools,
  `install kernel.elf` puts the result on the boot disk beside the old
  one, and `restart` boots it. tools/selfkernel.sh does exactly that
  against the emulator under KVM (`sh build/kvm.sh tools/selfkernel.sh`):
  the kernel that comes up says it was built on the machine itself.
  Eighty-two objects in about thirty seconds on a real processor.
* Certificate verification — turning the seal from privacy into
  identity: RSA/ECDSA signatures and a root store
* Work at scale, further — several desk jobs advancing at once, a
  worker lending more than one part at a time, the asking machine
  lending its own processor too, and repeated answers compared by
  the system instead of by hand

## Wireless

The machine has a wireless station: kernel/net/wifi.c hears the
networks around, joins one the WPA2 way, and seals every frame after
that. In the terminal, `networks` lists what is in the air -- name,
channel, signal, and whether it is open, wpa2, or something this
station does not speak; `join <name>` asks for the passphrase, which
is typed unseen (dots on the screen, dots on the door's terminal) and
never written into the transcript; `join <name> with <passphrase>`
gives it in one line; `leave` leaves; `wifi` says where the station
stands and what its address is. A network joined once is written into
the settings as a `wlan |` line -- name and passphrase, readable by
whoever holds the settings, which is the person -- and joined again on
its own the next time it is heard.

The WPA2 way, in full and with nothing borrowed: the passphrase and
the network's name become the master key by 4096 rounds of HMAC-SHA1;
the four-way handshake mixes it with two fresh nonces and both
addresses into the session keys; the group key arrives wrapped and is
unwrapped; every data frame is AES-CCM with a packet number that never
repeats, so a frame recorded cannot be played back; a wrong passphrase
is turned away at the third message and the station says so. SHA-1,
HMAC, PBKDF2, CCM and the key unwrapping each prove themselves at boot
against the published test vectors.

What the machine does not have is a radio chip's driver. QEMU has no
wireless card to emulate, and every real chip needs a driver of its
own with the maker's firmware. So the radio the station speaks through
today is the test bench's: 802.11 frames carried inside ethernet frames
down the wire to tools/wifi-ap.py, a virtual access point on the host
that beacons, greets, runs the handshake with a passphrase of its own,
seals and unseals with CCMP, and behind the seal leases an address and
pings the station -- everything but the antenna. tools/wifitest.sh
runs the whole thing: a wrong passphrase turned away, the right one
typed unseen, the lease through the seal, the pings answered, and a
second boot joining on its own from memory. A real chip's driver plugs
in under the same three verbs -- send a frame, hand up what arrived,
name the antenna -- and the first one will be for a usb dongle the
author can hold, reached from QEMU through usb passthrough.

## Trying it: the iso

The latest build is on the releases page as
[erebus.iso](https://github.com/DustinHab/Erebus/releases/latest/download/erebus.iso).
It boots the way any other iso does: burn it, or write it whole onto a
usb stick -- `dd if=erebus.iso of=/dev/sdX bs=4M` on Linux, Rufus in
dd mode or balenaEtcher on Windows -- and start the machine from it,
the UEFI way, with Secure Boot off. There is no BIOS half. In QEMU:

```bash
qemu-system-x86_64 -machine q35 -m 512M -bios /usr/share/OVMF/OVMF.fd -cdrom erebus.iso
```

A machine booted from the iso runs whole, but without a memory: it
says so in the journal, and `disks` in the terminal shows where one
could be made -- `settle on disk N` for a disk of its own, `settle in
the free space of disk N` beside somebody else's system. A usb
keyboard and mouse at the root ports work; a usb disk does not yet, so
the store has to be on sata. tools/mkiso.sh builds the iso;
tools/isotest.sh boots it both ways.

## A note on Secure Boot

The loader is unsigned, so Secure Boot has to be off on real hardware.
A signing chain of our own would be a separate project.

## A note on other people's disks

The store is written from sector 1024 up, with no partition table and
no file system around it. On a real machine the disk after the boot
disk could be somebody's system disk, so a disk is claimed as the
store only when it carries the machine's mark in its first sector, or
is blank below the ring -- a fresh image, a wiped disk -- in which case
the mark is written and the disk is the store from then on. Anything
else is left exactly as found, read and written by nobody here, and
the machine comes up without a memory and says so at boot.
tools/foreigndisk.sh boots with a disk that carries a partition table
and data, and compares it byte for byte afterwards.

## A note on real machines

The first boot on real hardware -- an X99 board, a Broadwell-E, a
UEFI firmware from 2015 -- reached the desktop. Three things about a
real machine that emulation had never shown are worth writing down,
because each of them was invisible until then and obvious afterwards.

**The framebuffer is not memory.** It sits across a bus, uncached, and
a read from it is a round trip. The console scrolled by moving pixels
inside it, which reads every one of them, and each line of the boot log
cost nearly three seconds; the log took seven minutes to finish. Two
changes: the page tables now ask for write-combining on that mapping,
so neighbouring stores are gathered into whole lines instead of being
sent one at a time, and the back buffer -- which had been switched on
at the end of start-up, long after the log had paid for itself -- is
switched on at the first moment there is an allocator to ask. What it
starts from is no longer read back off the screen either: the fill
colour and how far the drawing got are remembered, so enabling it is a
fill and a few lines rather than a screenful of bus round trips.

**Ports without power report nothing.** A controller that says it
controls its own port power leaves the ports off after a reset, and
every socket then looks empty. They are powered now before anything is
looked at, and the ports that have something on them are named in the
log with their speed, so a machine that finds no keyboard says whether
that is because nothing was plugged in or because nothing was asked.

**Sockets can belong to a controller that is switched off.** Some
chipsets can route their slower sockets to either of two controllers
and leave the decision in four registers. Where the firmware pointed
them at a controller the machine no longer has, those sockets are dead.
The driver now claims everything the mask says it may claim.

**A card is not reset while it is still on the bus.** The firmware
leaves a network card running -- rings of its own, management traffic
of its own -- and a reset with a transfer still in flight is a thing a
bridge between the card and the processor may never recover from:
every read after it waits for a completion that does not come, and the
processor waits with it. The first boot with a cable in the I210's
socket stood at exactly that line. A card is now asked to finish what
it has on the bus and start nothing new, and is waited for, before the
reset; and after the reset it is waited for again until it has read
itself back in from its own memory. Its wire chip, which the I210 puts
to sleep when nothing is plugged in, is left alone when the card is
being passed over and woken before the first word when it is taken.

## A note on real processors

QEMU's emulator is forgiving in places a processor is not. The first
run under KVM -- the same QEMU, the real processor doing the work --
ended before the first program had run: sysret had put that program in
ring 3 with a stack selector of ring 0, because the emulator forces the
ring bits on and an AMD processor does not, and the first interrupt's
iretq refused to return to it. The syscall setup now carries the ring
bits in the base it hands the processor. `QEMU_EXTRA="-accel kvm" sh
tools/selfbuild.sh` boots a kernel that way when the host allows it,
and build/kvm-battery.sh runs the whole regression battery on the
processor by putting a stand-in for qemu that adds KVM ahead in the
path -- no test script knows the difference. With the sysret fix in,
every test of the battery passes there as on the emulator.
