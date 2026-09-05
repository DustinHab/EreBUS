# EreBUS Manual

For EreBUS 0.8.7. This manual is updated with every release; the version it describes is the one on the releases page.

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
- By itself, from a release: `update | auto` in settings has the machine keep itself current -- it checks now and then, and installs a newer signed release on its own. `update check` looks on demand. See 13.8.

---

## 3 The screen

There are no windows, no task bar and no menus. Every visible control is clickable; keys are shortcuts for the same actions.

### 3.1 Layout

- **Top:** the trail (the references followed so far, each clickable to go back there), then the name of the focus (click to rename), then a row of chips that fit the focus (see 3.4).
- **Middle:** the focus through the current lens. Lens tabs stand above it; click one, or hold control and press its digit. On home the middle instead shows an overview of the machine -- load over the last minute, the running programs, the recent journal; the lens tabs still switch to the raw views.
- **Right of the middle:** the focus's outgoing references, under "contents". The picked reference shows a preview of its target below the list, through the target's own lens (the first lines of a text, the picture, a program's slots).
- **Below the middle:** the machine's vitals on one line -- uptime, load, memory, threads, objects, nodes, address, and, when notable events are waiting, an `attention N` count in the accent -- then the newest journal line (click it to open the journal).
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

`scan`, `found`, `point at <name or address>`, `send <name>`, `ask <name> [with <object>] [as code] [across N]`, `say <words>`, `nodes`, `allow <node> work|update|vouch|all|nothing`, `forget <node>`, `trust <name> ssh-ed25519 ...`, `vouch <node>`, `renew key`, `update <node> [with <kernel.elf>]`, `update all`, `update check`.

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
| `update` | `off`, `auto`, `auto http://host` | keep current from a signed release; a url overrides the default source (13.8) |
| `tls` | `marked`, `strict` | what becomes of an https page whose server is not verified: let through and marked, or refused (12.5) |
| `authority` | a public key in base64 | a certificate authority of your own; servers under it count as verified; up to four lines (12.5) |

Older systems had `known |` lines (address and key of machines met); they are carried into the nodes table at start and no longer written.

---

## 7 System pages

All on the `system` shelf; all ordinary objects, saved with the graph.

| Page | Rights | Content |
|---|---|---|
| `log` | read | the journal: uptime, who, what; a ring of 8 KiB |
| `attention` | read | the notable subset of the log -- a failed far job, a node gone quiet; an unseen count shows in the status line and clears when this page is the focus |
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
| fetch | given a text whose first line is `host/path`, fetches the page into it (http, or https with the server verified where its authority is known, 12.5) |
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

### 9.3 Compiled tasks

- `ask <task> as code` sends the task as C source. The far machine compiles it with its own compiler, loads the image, and runs it under a deadline the kernel enforces -- a compiled program that never returns is ended when the budget runs out, even a bare `for(;;){}` with no system calls.
- The program is entered with its console capability in the first argument and its letter box in the second (`long main(long console, long inbox)`). It answers by sending one message: `syscall(2, console, 0x54584554, w0, w1, w2)` -- tag `"TEXT"`, up to 24 bytes across the three words. A run of ASCII digits is read as a number, anything else as text.
- An input sent with the task (`... with <object>`) arrives as a read-only capability in the letter box: `syscall(3, inbox, buffer, 0)` receives the message, its `caps[0]` (at byte 48 of the message) is the input's handle, and `syscall(4, handle, offset)` reads eight bytes at a time.
- The source must fit one datagram (1024 bytes); a compiled task takes no range yet; one compile runs at a time on a machine (the compiler's tables are shared), so a busy machine answers "busy" and the asker retries.

### 9.5 Quorum

- `ask <task> across N` runs the whole task on N distinct machines and takes the answer a verified majority agree on: `42  (agreed by 2 of 2)`. Only signed, verified answers count toward the majority.
- When no majority agrees, the disagreement is named rather than hidden: `no agreement -- alpha said 42, gamma said 41`. When fewer than N machines answer the scan willing, the job says so.
- Combine freely: `ask <task> as code across N with <object>` -- code, quorum and input in any order. Each of the N machines gets its own copy of the input.

### 9.4 Signed answers and the ledger

- Every answer is signed with the answering machine's door key over the job and the result, and checked against that node's key. A verified answer names the node plainly (`7 (by alpha)`); an answer whose signature does not check is marked `(unverified)`.
- `the ledger`, a read-only text on the system shelf, keeps one line per far-work job asked from this machine: its number, whether it was code, and the result or why it failed. It outlives the desk and the journal's ring.

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
- `fetch`: point a text at it whose first line is `host/path`; the page is written into the text and shown through the page lens. `https` pages arrive over TLS 1.3 with the server verified against the trusted authorities (12.5); the page lens marks the page `verified`, or `sealed, unverified` when the channel was encrypted but the server could not be verified.
- The palette's `page` is a text template for this.

### 12.2 The door (ssh)

- Put a public key into the settings: `door | ssh-ed25519 AAAA...` (the line of an `id_ed25519.pub`). Up to four keys.
- The machine's own key is `the door key`; its fingerprint is printed at boot and written into the journal (`the door's key is SHA256:...`).
- Connect with any client: `ssh someone@<address>` (the user name is only recorded). Several visitors at once, each with a terminal of their own; a fifth knock displaces the longest-idle visit.
- A long session renegotiates its keys on the client's schedule (`RekeyLimit`); the door completes the rekey and the session carries on. The door is for people reaching the machine, not for distributed work -- that is the object pipe's job (chapter 13).
- An exec command runs one line (`ssh host look`); a shell session takes lines from stdin (`printf 'look\naddress\n' | ssh -T host`).
- `receive <n> bytes as <name>` in a shell session takes the next n raw bytes of the session into a new text: how a file comes in.

### 12.3 The web server

- Make a list named `the served` (at home or on the system shelf). While it exists its texts are served on port 80 as pages and its pictures as BMP; let it go and nothing is served.

### 12.5 Verified servers (https)

- What is checked: the server's certificate chain is walked from its own certificate to a trusted authority -- every signature on the way (ECDSA P-256 or RSA, both with SHA-256), every certificate's dates against the clock, that a certificate in the middle is marked as an authority, and that the server's certificate names the host asked for (its subject alternative names; a wildcard stands for one label). Then the server's signature over the handshake is checked against the key in that certificate, which is what proves the server holds the key and not only a copy of the certificate.
- The trusted authorities built in are the intermediates that sign github.com (Sectigo DV E36) and the release cdn (Let's Encrypt YR1, YR2, YR3) -- the two hosts the self-update speaks to. The kernel says at start how many it carries (`tls: certificate checks ready`).
- An authority of your own: write `authority | <base64>` into the settings, the base64 being the authority certificate's public key as `openssl x509 -in ca.pem -pubkey -noout | openssl pkey -pubin -outform DER | base64 -w0` prints it. Up to four lines. A server whose chain reaches one of them counts as verified.
- What becomes of an unverified server: by default the page still comes, marked `sealed, unverified`, and the log and journal say why (`no trusted authority signs the chain`, `the certificate names no host that matches`, `a certificate in the chain has expired`, ...). `tls | strict` refuses such a page instead.
- The clock matters: dates are judged against the machine's clock, which comes from the real-time clock at start and from the net once a time server answered (`net: the clock was set from the net`).
- Limits: only the intermediates above are built in, not the roots (the roots sign with P-384 and SHA-384, which are not implemented); when github or the cdn move to another intermediate their pages are marked unverified until a kernel with the new authority is installed. No revocation checking. The self-update does not depend on any of this: its package is ed25519-signed (13.8).

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
- The person edits `name` (a petname) and `may`: `work`, `update`, `vouch`, `all`, or nothing. `allow <node> work update`, `allow <node> all`, `allow <node> nothing` write the column from the terminal.
- `forget <node>` drops the row: the node is met fresh next time, trust on first use again. This is how a changed key is accepted on purpose (forget the node, then let it knock) and how a wrong trust is undone.
- `trust <name> ssh-ed25519 AAAA...` writes a row before the node is met, so its first handshake is recognised rather than trusted on sight -- the key checked out of band beforehand. (Editing the text by hand does the same.)
- `vouch <node>` tells every other known node, over a signature, that this node's key is one you recognise. A node that has marked this machine `allow ... vouch` pins the vouched key before it ever meets it -- a third party's word, checked by signature, standing in for trust on first use. Only recognition travels: no rights ride along, and a vouch from a node you have not marked `vouch` is ignored.
- `renew key` rotates this machine's own door key: a fresh pair is made and announced to every known node, each announcement signed with the old key and the new so the far side moves its row for you without meeting you afresh; a node that does not already hold the old key ignores it and meets the new key later. Losing `the door key` object still makes a fresh key at the next start -- but then the peers do not know it, and each must `forget` and meet you again.
- `nodes` in the terminal lists the rows with when each was last heard.

### 13.3 Discovery

- `scan` sends a SEEK datagram by broadcast and to the peer; `found` lists the answers: address, name, work flag, free memory.
- Every node with an address receives a SEEK every 30 s (heartbeat); the `network` page is updated from the answers -- name, address, version, when last heard, free memory, uptime, whether it takes work, and whether a sealed or proven session stands. A known node not heard for 90 s is said to have gone quiet in the journal, and its return is said too.
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
- Answers are signed with the answering node's door key and checked against its key here, so a verified answer names the node plainly and an unverified one says so. A task sent `as code` is C the worker compiles and runs under a kernel-enforced deadline (9.3). Each asked job is recorded in `the ledger`.

### 13.7 Updating a node

- On the machine to be updated: `allow <sender> update`.
- On the sender: `update <node>` sends the kernel this machine booted from; `update <node> with <kernel.elf>` sends a built kernel object; `update all` sends to every node with an address, one after the other.
- The receiver checks the sender's key against its nodes row, installs the kernel as kernel.elf (the previous one as kernel.old) and restarts after 3 s. If the new kernel fails to start twice, the loader boots kernel.old.
- Both journals record the outcome; the `network` page shows the new version after the node's next heartbeat.

### 13.8 Self-update from a release

A machine on the network can keep itself current from a published release, with no other node involved.

- Turn it on with a settings line: `update | auto`. Off by default. `update check` in the terminal looks once, on demand, whichever way the setting is; because the check runs in the background and can take a while, its outcome is printed back into the terminal when it is done -- already current, a newer version installing, or the source unreachable -- as well as into the log.
- What happens: now and then (soon after start, then every six hours) the machine fetches an update package from the release source, reads the version inside it, and if it is newer than the running one, verifies the package's signature and -- only if it verifies -- installs the kernel and restarts. The loader's kernel.old rollback still applies, so a kernel that will not come up twice is backed out.
- Why it is safe on its own: the package is signed with the project's ed25519 key, and the matching public key is built into the kernel. The signature covers the version and the kernel together; a package that does not verify is refused. The network only decides *when* to update; the signature decides *what* may be installed. So a man in the middle, or a wrong file, cannot plant a kernel. The transport's own verification of github.com and the cdn (12.5) comes on top and is said in the log, but the update does not depend on it -- with `tls | strict` an unverified hop refuses the fetch, and the check reports the source as unreachable.
- The source: by default `https://github.com/DustinHab/EreBUS/releases/latest/download`, under which it fetches `update.pkg`. `update | auto http://host[:port]` in the value points it at another base -- a local server, for a test.
- The package `update.pkg` is `magic | signature | version | kernel.elf`, published as a release asset and built with `tools/sign-release.sh` from the private key that never leaves the build machine.
- A failed or refused update is said on the attention page (7); an installed one restarts the machine.

### 13.9 Limits

- Identity is trust on first use by default; `trust` pins a key beforehand, `renew key` rotates it under the old key's signature, and `vouch` lets a node you have marked `vouch` pin a key for you. Signing proves the answer came from the key in your nodes table, not that the key is really the machine you mean; a vouch is only as good as your trust in the voucher, and no vouch is revoked once made.
- A far-work result is signed by the node that produced it, but not otherwise checked: the answer is that node's word, not a proof the computation is right. Running the same task on several nodes and comparing is left for a later version.
- Broadcast discovery covers the local network only; across routers a node must be entered as peer once, after which gossip and heartbeat keep it known.
- One transfer at a time per node; one job at a time per worker.
- https verifies servers against a handful of built-in intermediates and the authorities written into the settings (12.5); there is no general root store and no revocation checking.

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
- `sh build/battery.sh` runs the regression tests: one build, 28 tests in parallel lanes (`LANES`, default 6), each in its own directory on the Linux file system (`PAR`, default `/tmp/erebus-par`), then renew, update-test and tlstest alone (renew rebuilds the kernel; the other two use qemu's one forwarded connection per boot). Logs, screenshots and QEMU stderr are copied back to `build/par/<test>/`. A test is stopped after `TEST_LIMIT` seconds (480); a failed or stopped test runs once more, marked "2nd try" in the summary. KVM is used when `/dev/kvm` is writable (`NOKVM=1` forces TCG). The summary lists seconds per test; the full output is in `build/battery.log`. About 4 minutes on 32 cores. `sh build/kvm-battery.sh` adds the kernel built on the machine itself.
- `sh build/battery.sh --one <test>` runs a single test that way; `BUILD=<dir> sh tools/<test>.sh` does the same by hand.
- The tests drive the real screen through QEMU's monitor and wait on serial log lines (`tools/testlib.sh`: `waitlog`, `waitcount`, `waitfile`, `bootwait`); see the table in README.md. `tools/pkitest.sh` runs on the host: it builds the certificate checker from the kernel's own files and tries it against openssl-made chains and the live github chains kept in `tools/pki/fixtures`.
- The built-in authorities come from `tools/pki/authorities.txt` (a certificate file and a name per line); `sh tools/mkauthorities.sh` regenerates `kernel/net/authorities.h` from them.
- The kernel's version comes from `git describe`; a tag `X.Y.Z` on the commit makes the boot line `EreBUS X.Y.Z (x86_64)`.

---

## 16 Versions

| Version | Date | What came |
|---|---|---|
| 0.4.4 | 2026-09-04 | first published version: loader, kernel, objects and capabilities, snapshots, desktop and terminal, compiler, assembler and linker, self-build through the door, AHCI and FAT, USB input, Intel and Realtek network cards, TLS client, ssh door, sealed object pipe with far work, WPA2 station, boot-time install offer |
| 0.5.0 | 2026-09-04 | nodes: identity by key, the nodes table and rights per node, kernel updates through the pipe, the network page, heartbeat and gossip, work with input objects, answers with provenance, `version`, `peer` by name |
| 0.5.1 | 2026-09-05 | compiler fix: members of a struct that contains an inner struct body were resolved against the inner body's members; a self-built kernel's compiler could not compile anything. selfkernel test extended to a second generation. Runtime messages reworded. Test battery parallel and under KVM. |
| 0.5.2 | 2026-09-05 | console messages of the assembler programs reworded to factual wording. Test battery: relay, agent and persist as scripts in the parallel lanes, per-test directories and time limit, pipe tests driven through the terminal; 22 tests in about 4 minutes. No change to what the machine does. |
| 0.6.0 | 2026-09-05 | visual overhaul of the shell: one warm ground with a single accent used only for agency and position (the caret, the write right, the picked row, the mode in use), regions parted by rules rather than filled boxes, the focused object's name at double height, region labels as spaced capitals, marked text in inverse video. The structure, the layout and what the machine does are unchanged. |
| 0.7.0 | 2026-09-05 | serious far work: answers signed with the node's door key and checked against its key (a verified answer names the node, an unverified one says so); a kernel-enforced deadline that ends a runaway job even with no system calls; compiled tasks (`ask <task> as code`) the worker compiles and runs under that deadline; a ledger of asked jobs on the system shelf. Also: an old store's settings table gains matters added since it was seeded. |
| 0.8.0 | 2026-09-05 | quorum far work (`ask <task> across N`): the whole task on N distinct machines, the result a verified majority agree on, disagreement named. `forget <node>` drops a node's row so a changed key can be re-pinned. The ssh door serves several visitors at once and survives a mid-session rekey. |
| 0.8.1 | 2026-09-05 | identity beyond trust-on-first-use: `trust <name> ssh-ed25519 ...` writes a node's key before it is met; `renew key` rotates this machine's door key and announces it to known nodes, each announcement signed with the old key and the new, so their rows move without meeting afresh. The network page shows each node's uptime, and a known node going quiet or coming back is said in the journal. |
| 0.8.2 | 2026-09-05 | the shell fills its own space: home opens on an overview of the machine -- load over the last minute, the running programs, the recent journal -- instead of its bare structure; a status line under the middle carries uptime, load, memory, threads, objects, nodes and address; the picked reference shows a preview of its target below the contents list, through the target's own lens. Structure and behaviour unchanged. |
| 0.8.3 | 2026-09-05 | far work takes an input in every form: `ask <task> with <object>` now rides alongside `as code` and `across N`, and a compiled worker receives the input on its letter box. Vouching: `allow <node> vouch` honours that node's signed vouches, and `vouch <node>` tells known nodes a key is one you recognise, so they pin it before meeting -- a third party's word, checked by signature, beyond trust on first use. An attention page on the system shelf gathers notable events (a failed job, a node gone quiet), with an unseen count in the status line that clears when the page is the focus. |
| 0.8.4 | 2026-09-05 | self-update from a signed release. `update \| auto` in settings has the machine check now and then for a newer version and, when one is out, fetch a package, verify its ed25519 signature against a key built into the kernel, install it and restart -- the loader's kernel.old rollback still the last net. The signature, not the transport, is what makes it safe, so it needs no certificate checking yet; the download follows redirects over http or tls. `update check` looks on demand. Also: the client tcp window is larger (faster large downloads), and a connection is closed cleanly (a FIN, not silence). |
| 0.8.5 | 2026-09-05 | self-update works against a real release host: the fetch now carries the long signed redirect urls a release CDN hands back (the request and location buffers were too small and cut off the token), and once an install is pending no further check runs, so the machine updates and restarts once. Verified end to end against the GitHub release. |
| 0.8.6 | 2026-09-05 | `update check` answers in the terminal where it was typed. The check runs in the network thread and can take a while, so it cannot reply on the same line; its outcome -- already current, a newer version installing, or the source unreachable -- is now printed back into the terminal when it is done, not only into the log. |
| 0.8.7 | 2026-09-05 | the tls client verifies the server (12.5): the certificate chain is walked to a trusted authority -- ECDSA P-256 and RSA (PKCS#1 v1.5) signatures with SHA-256, dates, host names from the subject alternative names, authority marks in the middle -- and the server's signature over the handshake (ECDSA or RSA-PSS) is checked against the certificate's key. Built-in authorities: Sectigo DV E36 for github.com, Let's Encrypt YR1-YR3 for the release cdn; `authority \|` adds one of your own; `tls \| strict` refuses an unverified server, otherwise the page is marked `sealed, unverified` and the journal says why. New at start: `tls: certificate checks ready`. The machine now keeps a full date, from the real-time clock and the net. |
