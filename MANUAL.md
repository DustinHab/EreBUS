# EreBUS Manual

For EreBUS 0.5.2. This manual is updated with every release; the version it describes is the one on the releases page.

Contents: 1 What EreBUS is · 2 Getting it running · 3 The screen · 4 The graph · 5 The terminal · 6 Settings · 7 System pages · 8 Programs · 9 Scripts and far work · 10 Building programs and the kernel · 11 Storage · 12 Network · 13 Nodes and the pipe · 14 Real hardware · 15 Building from source and testing · 16 Versions

---

## 1 What EreBUS is

- An operating system for x86_64 UEFI machines: own loader, own kernel, own compiler, assembler and linker.
- There are no files and no paths. There is a graph of typed **objects**: texts, bytes, lists, pictures, programs.
- A **reference** is a slot in an object that points at another object. It carries **rights**: `r` read, `w` write, `g` give (hand the reference on). Rights only narrow on the way, never widen.
- Nothing can be looked up by name. A program, a person or an ssh visitor reaches exactly what a reference grants. There is no root account, no permissions table and no lookup by name.
- Names are **petnames** on references, not on objects. The same object can be "notes" in one list and "the idea, read only" in another.
- The graph is saved automatically: when changes stop, the kernel writes a **snapshot** (a generation) to the store. Older generations can be read (time travel).
- Programs run in ring 3 and start with two capabilities: a send-only port to the console and their own message port. Everything else is handed to them explicitly.
- Machines running EreBUS are **nodes**: they discover one another on the network, authenticate with an Ed25519 key, exchange objects, run tasks for one another and install kernels on one another, each within the rights the other has granted.

Vocabulary used below:

| Word | Meaning |
|---|---|
| home | the root list; where a walk starts |
| walk, trail | the references followed from home to where you stand |
| focus | the object you stand on |
| lens | a way of showing the focus: text, bytes, structure, picture, page |
| view (mode) | focus, graph, columns, index, split, terminal |
| store | the partition the graph is saved on |
| generation | one snapshot of the graph |
| the door | the ssh server |
| the pipe | the encrypted UDP channel to other nodes |
| the desk | the queue of tasks for other machines |
| knock | the pipe's session handshake (HELLO/WELCOME); signed with the door key |

---

## 2 Getting it running

### 2.1 The image

- `erebus.iso` from the releases page. It is a hybrid image: write it raw to a usb stick (`dd if=erebus.iso of=/dev/sdX bs=4M`, Rufus in DD mode, balenaEtcher) or burn it to a disc.
- Firmware: UEFI boot only, Secure Boot off (the loader is unsigned).
- QEMU: `qemu-system-x86_64 -machine q35 -m 512M -bios /usr/share/OVMF/OVMF.fd -cdrom erebus.iso`

### 2.2 First start

1. The machine boots to the desktop from the stick. Without a store it has no memory: nothing typed survives a restart.
2. If a keyboard is present and at least one disk is on the bus, the start-up lists the disks and asks which one to take. Type the disk's number, then `yes`. Escape, or two minutes of silence, continues without a store.
3. Taking a disk erases it: it gets a boot volume (loader and kernel) and a store partition. The machine then boots from that disk on its own.
4. Less drastic ways are in the terminal: `settle in partition P of disk N` (only that partition becomes the store) or `settle in the free space of disk N` (a store is made in unpartitioned room; nothing else is touched). `disks` lists what is on the bus first.

### 2.3 Updating an installed machine

- From a newer stick: boot the stick, type `install this kernel` in the terminal, remove the stick, `restart`. The store is untouched.
- From a built kernel: `install kernel.elf`, `restart`. The previous kernel stays as kernel.old; the loader returns to it if the new kernel does not come up twice.
- From another node: on this machine `allow <that node> update`; on the other `update <this node>`. See 13.7.

---

## 3 The screen

There are no windows, no task bar and no menus. Every visible control is clickable; keys are shortcuts for the same actions.

### 3.1 Layout

- **Top:** the trail (the references followed so far, each clickable to go back there), then the name of the focus (click to rename), then a row of chips that fit the focus (see 3.4).
- **Middle:** the focus through the current lens. Lens tabs stand above it; click one, or hold control and press its digit.
- **Below the middle:** the newest journal line (click it to open the journal).
- **Bottom:** the views as words (click one, or press tab to cycle), a hint about what typing does here, and far right `turn off`.
- Generation ticks stand in the header: click one to read that generation; page up / page down step through them; escape returns to now.

### 3.2 Views

| View | What it shows |
|---|---|
| focus | one object, through a lens |
| graph | the reachable graph as a map; drag the empty ground to pan, wheel to zoom, click a node to go there |
| columns | the walk as columns, like a file browser |
| index | everything reachable as a list with id, kind, size, holder, way, name; typing filters by name and text; enter goes to the first hit; escape clears |
| split | two walks side by side: the left one writes, the right one reads |
| terminal | the words of chapter 5; keys go to the terminal |

### 3.3 Keys and mouse

| Key | Does |
|---|---|
| tab | next view |
| up / down | pick a reference (in a page: scroll) |
| right, enter | follow the picked reference; in a writable text enter makes a new line |
| left | one step back |
| delete, backspace | edit the text at the caret |
| page up / page down | one generation back / forward (time travel) |
| escape | back to now; close the machine chooser; clear the index search |
| control + digit | switch lens |
| letters | go into the focused text at the caret; in the index they search; in the terminal they form the line |

Mouse: click anything you can see; drag in a text to mark; the wheel scrolls and, in the graph, zooms. `take` lifts the marked letters into a held buffer (shown in the header), `put` inserts them at the caret, `x` drops them. Typing replaces marked letters.

### 3.4 Chips

Chips appear in the header when they fit the focus:

| Chip | On | Does |
|---|---|---|
| add | a writable list | opens the palette: text, bytes, list, picture, script, page, task, the standard programs, and references held for placing |
| run | a text or an image | runs it as a program; the program object lands in the focus |
| end | a running program | ends it |
| send | a readable text, bytes or picture | carries it to the peer; with no peer named, a chooser scans and one click sends |
| ask | a text | hands it to the desk as a task (chapter 9) |
| pack / unpack | a list / a packed bytes object | a list folded into one bytes object, and back |
| copy | anything readable | a copy laid beside it |
| assemble, compile, build, link | texts and lists | chapter 10 |
| install, restart | a kernel.elf | chapter 10 |
| take in / write out | the exchange disk's list | chapter 11 |
| x beside a capability | a running program's structure | revokes that capability |
| x beside a reference | a list | lets the reference go into the bin; in the bin, for good |

### 3.5 Lenses

- **text**: the letters; editable when the reference allows writing; the caret is where you click.
- **bytes**: hex dump with offsets.
- **structure**: the object's references with their rights and names; for programs also what they hold.
- **picture**: sixteen inks; pick one and draw when the reference allows writing.
- **page**: a text read as HTML (headings, lists, tables, links, forms with GET). The address line is editable; enter fetches. Back and forward chips keep a history.

---

## 4 The graph

- **home** holds the person's things on top and three shelves: `programs` (the standard programs), `system` (the machine's own pages, chapter 7), `arrivals` (what other nodes sent), and a `bin` once something was let go.
- A reference shows its rights as three letters: `rw-` read and write, `r--` read only, `rwg` also giveable.
- Letting go of a reference moves it into the bin; letting go in the bin deletes it. Objects with no reference left are freed; cycles are collected in the snapshot's quiet moment.
- Names on references are yours: rename by clicking the name. The object's own name is shown only where no petname was given.
- The snapshot runs by itself after the settings' `save` seconds of quiet. Generations are kept in a ring on the store; older ones can be read, never changed.

---

## 5 The terminal

One sentence shape: a verb, a name, and `to`, `at` or `with` when two things meet. Names may contain spaces. Numbers count slots. `help` lists the words on the machine.

The terminal walks like the shell does: it stands on an object and can go only where references lead. Over the door (ssh) every visitor has a session of their own, beginning at home.

### 5.1 Looking around

| Word | Does |
|---|---|
| `look [name]` | what stands here, or what that points at |
| `go <name>` | follow a reference (`go 3` follows slot 3) |
| `back` | one step back |
| `home` | back to the start |
| `where` | the walk so far |
| `find <words>` | search names and texts everywhere you reach |
| `read [name]` | the thing itself: letters, bytes, size |

### 5.2 Things

| Word | Does |
|---|---|
| `write <words>` | add a line to the text you stand on |
| `make text <name>` | a fresh text, laid in here |
| `make list <name>` | a fresh list, laid in here |
| `copy <name>` | a copy laid beside it |
| `rename <name> to <new name>` | a new petname |
| `let go <name>` | into the bin; in the bin, for good |
| `give <name> to <program>` | hand a program a reference (the terminal's "point at") |
| `end <name>` | end a running program |
| `run <name>` | run a text (script) or an image as a program, here |
| `receive <n> bytes as <name>` | a text made here, filled with the next n bytes of this session as they are: how a file comes in through the door (`cat file \| ssh -T ...` after the word) |

### 5.3 Programs and building (chapter 10)

`assemble <name>`, `compile <name>`, `link <list>`, `build <list>`, `install <name>`, `install this kernel`, `restart`, `version`.

### 5.4 Disks (chapter 11)

`disks`, `settle on disk N`, `settle in partition P of disk N`, `settle in the free space of disk N`, `yes`, `take in <list>`, `write out <list>`.

### 5.5 Network and wireless (chapter 12)

`address`, `networks`, `join <name> [with <passphrase>]`, `leave`, `wifi`.

### 5.6 Nodes and the other machines (chapter 13)

`scan`, `found`, `point at <name or address>`, `send <name>`, `ask <name> [with <object>]`, `say <words>`, `nodes`, `allow <node> work|update|all|nothing`, `update <node> [with <kernel.elf>]`, `update all`.

### 5.7 The machine

| Word | Does |
|---|---|
| `journal` | the last things that happened |
| `time` | the wall clock and the uptime |
| `version` | what the running kernel calls itself |
| `help` | the words |

---

## 6 Settings

The settings are one text on the system shelf: `matter | value`, one per line. A line takes effect as it is typed. The later line wins when a matter appears twice. Lines without a bar do nothing.

| Matter | Values | Meaning |
|---|---|---|
| `theme` | `dark`, `light` | colours |
| `save` | `N second(s)` (1..60) | quiet time before a snapshot |
| `clock` | `on time`, `N hours ahead`, `N hours behind` | offset from the net's UTC |
| `pointer` | `slow`, `normal`, `quick` | mouse speed |
| `hints` | `shown`, `hidden` | the footer hint |
| `slice` | `N ms` (10..500) | scheduler time slice |
| `start` | `where i left`, `at home` | where the next start stands |
| `name` | a word | what this machine calls itself to other nodes |
| `address` | `by lease`, `a.b.c.d` | DHCP, or a claimed address |
| `peer` | `nobody`, `a.b.c.d port`, a node's name | where `send`, `ask` and `say` go |
| `work` | `refused`, `welcomed` | whether every proven node may have this machine run tasks; per-node leave is given in `nodes` |
| `keys` | `english`, `german` | keyboard layout |
| `door` | `ssh-ed25519 AAAA...` | a public key that may come in through the door; up to four lines |
| `wlan` | `<ssid> = <passphrase>` | written by the station when a join worked; the network is rejoined at start |

Older systems had `known |` lines (address and key of machines met); they are carried into the nodes table at start and no longer written.

---

## 7 System pages

All on the `system` shelf; all ordinary objects, saved with the graph.

| Page | Rights | Content |
|---|---|---|
| `log` | read | the journal: uptime, who, what; a ring of 8 KiB |
| `settings` | read, write | chapter 6 |
| `activity` | read | uptime, cpu, memory, heap, objects, threads, and one row per process with cpu share, holds, memory; rewritten every second |
| `network` | read | every node with address, version, seconds since last heard, free memory, work flag, session state; machines heard but not authenticated; desk and transfer state; rewritten every 2 s |
| `nodes` | read, write | chapter 13.2 |
| `the time` | read | the wall clock, ticking |
| `the line` | read | the conversation with other nodes (`say`) |
| `the machine`, `the compiler`, `the language` | read | what the kernel, the C compiler and the script language understand, refreshed at every boot |
| `the door key` | none | the ed25519 pair the door and the pipe identify with; letting it go makes a fresh one at the next start |
| `the disk` | read, write | the exchange disk's files, when a FAT disk stands beside the store (chapter 11) |
| `the served` | -- | a list you make by that name; while it exists, its texts and pictures are served on port 80 (12.3) |

---

## 8 Programs

### 8.1 The standard programs

Started from the add palette; the running program lands in the list you stand on. Each begins holding only the console and its letter box. Point it at something (click it in the structure lens and choose, or `give <name> to <program>`) and it receives that reference with the rights you hold, narrowed further where the program itself decides so.

| Program | Does when pointed at something |
|---|---|
| agent | reports what it can see and tries to change it; whether that works is the kernel's decision, not a check the agent makes |
| courier | keeps a letter box it is given; passes any other cargo on, read-only |
| clock | writes the time of day into the object, once a second |
| cipher | rotates every letter by thirteen, in place; its own inverse; needs write |
| tally | counts words and lines; needs only read |
| sums | says one checksum; the same before and after a journey means unchanged |
| watch | says when the opening of the object changes, at most once a second |
| wipe | zeroes the payload; needs only write |
| fetch | given a text whose first line is `host/path`, fetches the page into it (http, or https sealed but unverified) |
| foreman | given a task text, hands it to the desk and watches for the answer; `again N` in the first line repeats it every N seconds |
| reckon | given a text, writes the answer after every line that ends in `=` (whole numbers, `+ - * / %`, parentheses) |
| pulse | given a picture and the activity page, paints memory and cpu share into the picture, one column a second |

### 8.2 What a program holds

- The structure lens of a running program shows "it holds": every capability, with rights. `x` beside one revokes it.
- `end` ends a program; everything it held is let go.
- A program cannot print; it can only send to the console port it was born with. What it says lands in the journal under its name.
- Eight system calls exist: exit, yield, send, receive, read, write, pass, clock. None opens, finds or grants anything.

---

## 9 Scripts and far work

### 9.1 The language

Any text can be a program: stand on it and press `run`, or `run <name>`. The text is read line by line as it runs; edit it and the next pass through a line runs the new words.

| Line | Does |
|---|---|
| `say <words>` | up to 24 letters to the console |
| `tell <words>` | the same to "it", when it listens |
| `answer <n or v>` | the value, in digits, to the first gift after the words (the way home), else to "it" |
| `show x` | say a variable and its value |
| `wait` | sleep until the next gift or message |
| `set x <n or v>` | also `add`, `sub`, `mul`, `div` |
| `get x <offset>` | x = eight bytes of "it" at that offset |
| `put x <offset>` | eight bytes of x into "it" |
| `time x` | x = the second of the day |
| `rest <n or v>` | sleep that many seconds |
| `if x < <n or v>` | also `=` and `>`; false skips the next line |
| `skip <n>` / `back <n>` | jump |
| `note ...` | a remark |
| `stop` | the end |

- Variables `a`..`z` hold signed 64-bit numbers. `r` holds the result of the last get, put, tell or answer: 0 done, -1 refused.
- "it" is the latest capability the script was given after its words. `m` is the number attached to the latest gift or message.
- A refused get, put or tell sets r to -1 and continues; the kernel does not hand out an object the script has no right to.

### 9.2 Far work

A task is a text sent to other machines (`ask <task>`, or the ask chip). It runs there in the interpreter with a time budget (20 s) and holds only its words and a reply port ("the way home").

- The recipe begins with `wait`: the first gift is the reply port; `m` is the low end of the range. A second `wait` delivers the high end as `m`. `answer <v>` sends the result to the reply port.
- `split P from LO to HI` as the first line divides the task into P parts over the machines that answered the scan willing; each part runs with its own stretch; numeric answers are summed, word answers are gathered in order.
- The answer is written back into the task as a `= ...` line when the task came writable, else laid into arrivals as `answer N`. It names the machines that gave it: `50005000 (4 parts by alpha, beta)`.
- `ask <task> with <object>`: the object (text, bytes or picture, up to 8 MiB) goes ahead of every part; the script receives it as its third gift, so a third `wait` makes it "it" and `get` reads it.
- Pointing a task at the foreman program hands it in without another click; `again N` in its first line repeats it every N seconds.
- A far machine works only when its settings say `work | welcomed`, or its nodes table says this machine may `work` (chapter 13).

---

## 10 Building programs and the kernel

### 10.1 Words

| Word | Does |
|---|---|
| `assemble <text>` | a text of instructions becomes an image (bytes) |
| `compile <text>` | a text of C becomes a text of assembly beside it, and that an image |
| `link <list>` | joins the objects in a list into one image, or a kernel when a text lays down `kmain` |
| `build <list>` | compiles and assembles every `.c` and `.S` text in the list (headers beside them), then links; runs in the background and reports in the journal |
| `run <image>` | runs the image |
| `install <kernel.elf>` | the kernel the next start runs; the running one becomes kernel.old |
| `install this kernel` | the loader and kernel this machine booted from, onto the boot disk; store untouched |
| `restart` | saves and starts again |

### 10.2 The assembler

- Intel order, lowercase, one instruction a line; `;` starts a remark; labels end in a colon.
- Numbers decimal, hex with `0x`, or a letter in single quotes. Memory in brackets: `[rax]`, `[rbx + 8]`, `[name]`, `[gs:8]`; `byte`, `dword`, `qword` say the width when only an immediate does.
- Sections: `section text` (or `code`), `rodata`, `data`, `bss` (`res n` only), `user`. `db`, `dw`, `dd`, `dq` lay down values, strings in double quotes for `db`; `res n` lays down zeros.
- Names are public unless they begin with a dot or are declared `private`. The GNU/AT&T dialect (the kernel's own `.S` files) is translated automatically.

### 10.3 The compiler

- C in the shape people write it: `char short int long` signed or not, `float double`, pointers, arrays, structs and unions by value, bit fields, typedefs, enums, functions with up to six parameters and varargs, function pointers, all operators with their precedence, `if while for do switch goto`, `sizeof`, casts, initializers with designators, the preprocessor with function-like macros, `#if` arithmetic and `#include` of a text lying beside the source, inline assembly in the GNU form.
- Not there: 128-bit types, `va_arg` of a struct. No library: `main` receives the two handles a program starts with; `syscall(nr, ...)` is the door to the kernel.
- The assembly the compiler made lies beside the source as a text, to be read.

### 10.4 The kernel

- A list holding every kernel source text (`.c`, `.S`) and header, plus a `version.c` saying what the kernel calls itself, builds with `build <list>` into `kernel.elf` (82 objects; about 30 s on a desktop processor).
- `tools/mkupload.sh` on a development machine streams the sources through the door: `sh tools/mkupload.sh kernel "0.5.0" | ssh -T someone@<address>`; then `build kernel`, `go kernel`, `install kernel.elf`, `restart`.
- `version` afterwards says the text the build was given.

---

## 11 Storage

- **Disks**: AHCI, up to eight. Roles: the boot disk (port 0, the FAT volume with `\EFI\BOOT\BOOTX64.EFI` and `\erebus\kernel.elf`), the store (a GPT partition of type `E2EB0500-5354-4F52-4552-454255530001`), and an exchange disk (FAT32) when one stands beside them.
- **Settling** (2.2): the store is where the graph lives; a disk taken whole boots the machine; a partition or free space only holds the store. Foreign disks are never written.
- **Snapshots**: two alternating slots, generation number and checksum, sixteen generations kept; objects from 4 KiB up go into a content-addressed blob log that is compacted when full.
- **Exchange disk**: its root directory appears as `the disk` on the system shelf (`take in <list>`); `write out <list>` writes texts and bytes back under 8.3 names. Files up to 4 MiB in, 16 MiB out.
- **USB sticks** boot the machine but cannot hold the store.

---

## 12 Network

### 12.1 Address and pages

- The address comes by DHCP, or is claimed with `address | a.b.c.d`; `address` shows the card, its MAC and the address.
- `fetch`: point a text at it whose first line is `host/path`; the page is written into the text and shown through the page lens. `https` pages arrive over TLS 1.3 without certificate verification; the shell marks them accordingly.
- The palette's `page` is a text template for this.

### 12.2 The door (ssh)

- Put a public key into the settings: `door | ssh-ed25519 AAAA...` (the line of an `id_ed25519.pub`). Up to four keys.
- The machine's own key is `the door key`; its fingerprint is printed at boot and written into the journal (`the door's key is SHA256:...`).
- Connect with any client: `ssh someone@<address>` (the user name is only recorded). One visitor at a time.
- An exec command runs one line (`ssh host look`); a shell session takes lines from stdin (`printf 'look\naddress\n' | ssh -T host`).
- `receive <n> bytes as <name>` in a shell session takes the next n raw bytes of the session into a new text: how a file comes in.

### 12.3 The web server

- Make a list named `the served` (at home or on the system shelf). While it exists its texts are served on port 80 as pages and its pictures as BMP; let it go and nothing is served.

### 12.4 Wireless

- `networks` lists what is in the air; `join <name> with <passphrase>` joins (WPA2-PSK or open); the passphrase is asked for when not given and never echoed; `leave`; `wifi` shows the station.
- A join that worked is remembered as a `wlan |` line and rejoined at start.
- Today's radio is the test bench's virtual access point; a driver for a real wireless chip does not exist yet. Cabled cards: Intel e1000 family (8254x, 82574L, I217 to I219), Intel igb family (82575 to I211), RTL8139, RTL8168/8169 (untested on silicon).

---

## 13 Nodes and the pipe

### 13.1 What a node is

- A node is identified by its door key (Ed25519). Every session handshake on the pipe is signed with it.
- Trust on first use: at the first handshake the key is written into the nodes table together with the name the machine sent. Afterwards the address is bound to that key: a different key from a known address is rejected until the row is removed. A known key from a new address updates the row's address.
- Names are unverified claims. Rights are not: the `may` column is written locally.

### 13.2 The nodes table

`nodes` on the system shelf, one row per node:

    name         | key (base64)                                | address           | version       | may
    bochum       | AAAAC3Nz...                                 | 10.9.9.21 7800    | 0.5.0         | work update

- The kernel writes key, address and version; it rewrites the table when a node appears, moves or changes version.
- The person edits `name` (a petname) and `may`: `work`, `update`, `all`, or nothing. `allow <node> work update`, `allow <node> all`, `allow <node> nothing` write the column from the terminal.
- Removing a row forgets the node. Adding a row by hand (name, key) trusts a node before it is met.
- `nodes` in the terminal lists the rows with when each was last heard.

### 13.3 Discovery

- `scan` sends a SEEK datagram by broadcast and to the peer; `found` lists the answers: address, name, work flag, free memory.
- Every node with an address receives a SEEK every 30 s (heartbeat); the `network` page is updated from the answers.
- A HERE answer carries the node's key, version and up to four other addresses heard within the last 90 s; an address not known locally is sent one SEEK per minute at most. This propagates discovery across routers, where broadcast does not reach.
- `point at <name or address>` writes the `peer` line in the settings; the send chooser does the same with one click.

### 13.4 Sending objects

- `send <name>` or the send chip transfers a readable text, bytes or picture to the peer, encrypted. It is placed in the receiver's `arrivals` list with read and write rights, named as the sender named it.
- Only data crosses: type, name, payload. No references, no rights, no programs.
- Transfers read from and write into objects directly, windowed (HAVE every 8 chunks, TAKEN at the end), up to 8 MiB; a refusal is answered with a reason code.
- `pack` folds a list into one bytes object; `unpack` rebuilds the list.

### 13.5 The line

- `say <words>` appends a line to `the line` on every node with an open session; without one, a session with the peer is opened first. Lines from other nodes appear under their names.

### 13.6 Work

- See 9.2. A machine runs a task when its settings say `work | welcomed` (any authenticated node) or its row for the requesting node contains `work`.
- Divided tasks are dealt round-robin to the machines that answered the scan with the work flag set; a busy machine is asked again after 2 s; a refusing machine is removed from the job.
- Answers name the machines that produced them; results are not verified.

### 13.7 Updating a node

- On the machine to be updated: `allow <sender> update`.
- On the sender: `update <node>` sends the kernel this machine booted from; `update <node> with <kernel.elf>` sends a built kernel object; `update all` sends to every node with an address, one after the other.
- The receiver checks the sender's key against its nodes row, installs the kernel as kernel.elf (the previous one as kernel.old) and restarts after 3 s. If the new kernel fails to start twice, the loader boots kernel.old.
- Both journals record the outcome; the `network` page shows the new version after the node's next heartbeat.

### 13.8 Limits

- Identity is trust on first use; no third party verifies a key.
- Results of far work are not verified.
- Broadcast discovery covers the local network only; across routers a node must be entered as peer once, after which gossip and heartbeat keep it known.
- One transfer at a time per node; one job at a time per worker.

---

## 14 Real hardware

Verified on an ASUS X99 board (Broadwell-E, UEFI from 2015):

- Secure Boot must be off; the machine boots to the desktop, installs on a SATA SSD, and is reachable over ssh.
- USB keyboards and mice work at the root ports and behind hubs, with hotplug; the wheel needs the report descriptor, which the driver reads.
- Cabled network: Intel I218 (chipset) and I210 (add-in) both work; the I210 is not asked while its PHY sleeps without a cable; the I218 is never reset.
- The framebuffer is mapped write-combining and never read back; the first boots without that took seconds per log line.

---

## 15 Building from source and testing

- Requirements (Linux; WSL2 with Ubuntu works): `clang lld nasm make qemu-system-x86 ovmf mtools dosfstools xorriso gdb unifont python3-pil`.
- `make` builds loader, kernel and `build/esp.img`; `make run` starts QEMU with the serial log on the terminal; `sh tools/mkiso.sh` builds `build/erebus.iso`; `sh tools/mkusb.sh` a stick image.
- `sh build/battery.sh` runs the regression tests: one build, 21 tests in parallel lanes (`LANES`, default 6), each in its own directory on the Linux file system (`PAR`, default `/tmp/erebus-par`), then renew alone because it rebuilds the kernel. Logs, screenshots and QEMU stderr are copied back to `build/par/<test>/`. A test is stopped after `TEST_LIMIT` seconds (480); a failed or stopped test runs once more, marked "2nd try" in the summary. KVM is used when `/dev/kvm` is writable (`NOKVM=1` forces TCG). The summary lists seconds per test; the full output is in `build/battery.log`. About 4 minutes on 32 cores. `sh build/kvm-battery.sh` adds the kernel built on the machine itself.
- `sh build/battery.sh --one <test>` runs a single test that way; `BUILD=<dir> sh tools/<test>.sh` does the same by hand.
- The tests drive the real screen through QEMU's monitor and wait on serial log lines (`tools/testlib.sh`: `waitlog`, `waitcount`, `waitfile`, `bootwait`); see the table in README.md.
- The kernel's version comes from `git describe`; a tag `X.Y.Z` on the commit makes the boot line `EreBUS X.Y.Z (x86_64)`.

---

## 16 Versions

| Version | Date | What came |
|---|---|---|
| 0.4.4 | 2026-09-04 | first published version: loader, kernel, objects and capabilities, snapshots, desktop and terminal, compiler, assembler and linker, self-build through the door, AHCI and FAT, USB input, Intel and Realtek network cards, TLS client, ssh door, sealed object pipe with far work, WPA2 station, boot-time install offer |
| 0.5.0 | 2026-09-04 | nodes: identity by key, the nodes table and rights per node, kernel updates through the pipe, the network page, heartbeat and gossip, work with input objects, answers with provenance, `version`, `peer` by name |
| 0.5.1 | 2026-09-05 | compiler fix: members of a struct that contains an inner struct body were resolved against the inner body's members; a self-built kernel's compiler could not compile anything. selfkernel test extended to a second generation. Runtime messages reworded. Test battery parallel and under KVM. |
| 0.5.2 | 2026-09-05 | console messages of the assembler programs reworded to factual wording. Test battery: relay, agent and persist as scripts in the parallel lanes, per-test directories and time limit, pipe tests driven through the terminal; 22 tests in about 4 minutes. No change to what the machine does. |
