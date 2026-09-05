# EreBUS

Object-based, capability-secured operating system for x86_64 UEFI. Own boot loader, own kernel, own tools.

| Unix | EreBUS |
|---|---|
| Everything is a file | Everything is a typed object |
| Paths in one global namespace | References only; no global namespace |
| Permissions are checked | The reference is the permission (capability) |
| Saving into files | Versioned snapshots of the object graph |
| Byte streams | Typed messages |
| An application opens a file | A window is a view onto an object |

Consequence: a program can only reach what it was handed. No root, no lookup by name, no way to acquire authority. `kernel/include/eb/object.h` has no `obj_find()`.

The manual: [MANUAL.md](MANUAL.md) (screen, terminal words, settings, programs, scripts, building, storage, network, nodes). Updated with every release.

## Building

Requirements (Linux; here WSL2 with Ubuntu):

    apt install clang lld nasm make qemu-system-x86 ovmf mtools \
                dosfstools xorriso gdb unifont python3-pil

Targets:

    make          # loader, kernel, bootable image build/esp.img
    make run      # QEMU, serial on the terminal
    make shot     # headless; build/screen.png, build/serial.log
    make debug    # halted, gdb on port 1234
    make clean
    sh tools/mkiso.sh          # build/erebus.iso
    sh build/battery.sh        # all regression tests; KVM when /dev/kvm is writable, 6 lanes, about 4 minutes
    sh build/kvm-battery.sh    # the same, plus the self-built kernel

From Windows: `wsl -d Ubuntu -- bash -lc "cd /mnt/c/erebus && make run"`.

## Layout

    boot/            UEFI loader (PE/COFF, MS ABI); boot.c, efi.h
    common/          shared by loader and kernel: bootinfo.h, elf64.h
    kernel/
      arch/x86_64/   start.S, isr.S, gdt.c, trap.c, syscall.c, linker.ld
      hw/            pci, ahci, xhci, ps2, cpu, timers, serial, 8259
      gfx/           framebuffer, font, shell (desktop), html lens
      fs/            fat, gpt, settle (disks), install (boot-time offer)
      lang/          cc (C compiler), asm, gnu (AT&T translator), ld (linker)
      net/           e1000, igb, rtl8139, rtl8169, net, tls, ssh, pipe, wifi, crypto
      obj/           object store, capabilities, ports, snapshots, blob log, settings, journal
      sched/         threads, processes, scheduler
      mm/            frame allocator, page tables, heap
      term/          terminal words
      user/          ring-3 programs (runner, foreman, standard programs)
      include/eb/    headers
    tools/           test scripts, image builders, helpers
    build/           outputs; battery.sh, kvm.sh, commit.sh, doorcheck.sh

## Design decisions

- Own UEFI loader: no foreign code in the trusted base.
- Kernel in the upper half at -2 GiB; user programs in the lower half; no page-table switch on syscall.
- Bitmap frame allocator: bookkeeping in one checkable region, 16 KiB per GiB.
- Font: GNU Unifont 8x16, embedded at build time.
- Serial as first output; it costs 20 ms per log line at 115200 baud (`make shot SERIAL=null` to compare).
- No SSE/MMX in the kernel; the vector unit is enabled for user programs only and saved per process.
- 8259 before APIC: works without ACPI parsing.
- Interrupt stubs as a 16-byte table; `tools/check-isr.sh` verifies the layout.
- No task bar, no window frames; every visible control is clickable; keyboard is a shortcut.
- Names live on the reference, not the object.
- Nothing is saved by hand; the kernel writes a snapshot when changes stop.
- Everything hardware-dependent self-tests at boot and says so in the log.

## Status

### Kernel core
- [x] UEFI loader: GOP, ELF loading, memory map, hands loader and kernel files to the kernel (bootinfo v3)
- [x] GDT/TSS with fault stacks, 256 IDT vectors, crash reports with symbol names
- [x] PIT tick, TSC calibrated against the PIT, RTC read at boot, clock set from the net
- [x] Direct map, per-section page tables, NX, CR0.WP, SMEP, SMAP, UMIP, W^X (`make wx`)
- [x] PAT: framebuffer mapped write-combining
- [x] Bitmap frame allocator, next-fit heap, guard pages under thread stacks
- [x] Preemptive round-robin threads, per-thread CPU accounting
- [x] Ring-3 processes, own address spaces, 8 system calls, registers zeroed on return to user
- [x] Processes reaped by the next thread through the scheduler
- [x] Kernel version from `git describe` (build/version.c), shown in the boot log and the desktop

### Objects
- [x] Typed objects (text, bytes, list, picture, program, ...), reference counts, reference slots
- [x] Protection domains with capability tables; handles carry a generation; rights only narrow
- [x] Message ports; capabilities travel with the rights the sender let go of
- [x] Cycle collector (`obj_collect`), runs in the snapshot's quiet moment (`make sweep`)
- [x] Snapshots: two alternating slots, generation + checksum, 16 generations kept, time travel in the shell
- [x] Blob log for objects from 4 KiB up, content-addressed (SHA-256), compaction
- [x] Journal as a read-only text object
- [x] Settings as a text object, applied as typed (`theme`, `save`, `clock`, `pointer`, `hints`, `slice`, `start`, `name`, `address`, `peer` by address or node name, `work`, `keys`, `door |`, `wlan |`, `update |`)
- [x] Activity table rewritten once a second
- [x] Nodes table `nodes` (`name | key | address | version | may`): one row per machine met through the pipe; the kernel writes key, address, version; the person writes name and `may` (`work`, `update`, `vouch`, `all`)
- [x] `network` page: every node with address, version, last heard, free memory, work flag, seal state; machines heard but not met; desk and transfer state
- [x] `attention` page: the notable subset of the log (a failed far job, a node gone quiet), with an unseen count in the status line
- [x] Program records survive reboots; delegations replayed by name

### Desktop and terminal
- [x] Views: focus, graph (zoom/pan), columns, index, split, terminal; text/bytes/structure/picture/html lenses
- [x] Editor with caret, mark/take/put, find bar, scrolling everywhere
- [x] Journal, settings, activity, "the machine", "the compiler", "the language" pages under `system`
- [x] Add palette: text, bytes, list, picture, task, standard programs
- [x] Bin for let-go references; `turn off`; `restart`
- [x] German keyboard layout (`keys | german`)
- [x] Terminal grammar: verb, name, `to`/`at`/`with`; words: `help look where go back home find read write make copy rename let go run give end scan found point at send ask say build link compile assemble install take in write out disks settle yes networks join leave wifi address receive restart version nodes allow forget trust vouch renew update`
- [x] Boot-time offer: with no store and a keyboard present, the start-up lists the disks and takes a number, then `yes`; escape or 2 minutes of silence continues without a store

### Programs and languages
- [x] Standard programs: agent, courier, clock, cipher, tally, sums, watch, wipe, reckon, pulse, foreman
- [x] Script language (one page): variables a..z, say, wait, get, put, if, skip, back, stop, tell, time, rest, show
- [x] Assembler (own dialect) and GNU/AT&T translator
- [x] C compiler: C11 subset incl. structs by value, bit fields, varargs, inline asm (GNU form), float/double, preprocessor
- [x] Linker: objects, kernel shape (kmain) or program image; symbol table for crash reports
- [x] `build <list>`: every .c/.S text in a list, headers beside them, background thread, report in the journal
- [x] `install <bytes>`: kernel.new → kernel.elf, previous as kernel.old; the loader falls back after two failed starts
- [x] `install this kernel`: loader and kernel the machine booted from onto the boot disk; store untouched
- [x] `receive <n> bytes as <name>`: a file in through the door as raw bytes
- [x] Self-build: `tools/selfbuild.sh` (host, cchost), `tools/selfkernel.sh` (on the machine, through the door, under KVM: 82 objects in ~30 s)
- [x] Fuzzing of compiler, assembler, linker (`tools/fuzz/run.sh`)

### Storage
- [x] PCI enumeration; AHCI, up to 8 disks; roles: boot disk (port 0), store, exchange disk
- [x] GPT read/write; store partition type `E2EB0500-5354-4F52-4552-454255530001`
- [x] FAT32: read, write, directories, rename, format; boot volume found via GPT EFI partition, MBR, or LBA 0
- [x] Exchange disk: `take in`, `write out` (root directory, 8.3 names, 64 KiB per file)
- [x] Foreign disks are never written (`tools/foreigndisk.sh`)
- [x] `disks`, `settle on disk N`, `settle in partition P of disk N`, `settle in the free space of disk N`, each confirmed with `yes`
- [x] USB stick image (`tools/mkusb.sh`, `tools/stick.ps1`), hybrid ISO (`tools/mkiso.sh`)

### Input
- [x] PS/2 keyboard and mouse
- [x] USB xHCI: firmware handoff, port power, Intel port routing, root ports, hubs, hotplug with settle time
- [x] HID: keyboards (boot protocol), mice via report descriptor (buttons, 8/16-bit axes, wheel, report ids), composite devices
- [x] Key repeat in the driver

### Network
- [x] ARP, DHCP, DNS, ICMP echo, TCP client, HTTP/1.0 fetch with redirects, static address (`address | a.b.c.d`)
- [x] Drivers: e1000 family (8254x, 8257x/82574L, 82577–I219), igb family (82575/82576/82580/I350/I210/I211), RTL8139, RTL8168/8169
- [x] Card choice: first a card with link, then any card; unknown cards named in the log
- [x] TLS 1.3 client: X25519, AES-128-GCM, SHA-256; certificates not verified
- [x] SSH door (server): curve25519-sha256, ssh-ed25519, aes128-gcm@openssh.com; keys from `door |` lines; exec and shell sessions
- [x] Object pipe between machines: X25519 handshake signed with the door key, AES-128-GCM records
- [x] Nodes: identity is the key; the first handshake writes a row into `nodes`; a different key from a known address is rejected until the row is removed; a known key from a new address updates the row
- [x] Rights per node: `allow <node> work|update|vouch|all|nothing`; far work runs for a node when `work | welcomed` or its row contains `work`; a kernel is installed only from a node whose row contains `update`; a node's signed vouches pin keys only when its row contains `vouch`
- [x] Transfers read from and write into objects directly, windowed (HAVE/TAKEN), up to 8 MiB; refusals carry a reason code
- [x] `update <node>`: sends this machine's kernel; the receiver installs it and restarts; `update <node> with <kernel.elf>`; `update all`; the loader falls back to kernel.old after two failed starts
- [x] Self-update: `update | auto` fetches a signed release package (`update.pkg`), verifies its ed25519 signature against a key built into the kernel, installs and restarts; `update check` on demand; the signature (not the transport) is the safeguard, so no certificate checking is needed
- [x] Discovery: broadcast scan, heartbeat to every known node every 30 s, HERE carries key, version and up to four other addresses (propagation across routers)
- [x] Far work: `ask <task>`, `ask <task> with <object>` (the input rides to each worker -- a script's third gift, a compiled worker's letter box), `as code`, `across N` and any combination, split tasks summed or concatenated, answers name the machines that produced them (`42 (4 parts by alpha, beta)`), foreman for recurring tasks
- [x] Vouching: `vouch <node>` sends a signed statement that a key is recognised; a node that `allow`s the voucher `vouch` pins the key before meeting it (identity beyond trust on first use, no rights implied)
- [x] The line: a shared text for `say` between nodes
- [x] `pack`/`unpack`: a list as one bytes object for the pipe

### Wireless
- [x] Station: scan, WPA2-PSK (PBKDF2, 4-way handshake, CCMP, GTK unwrap), open networks, auto-join of remembered networks
- [x] Crypto self-tests at boot (SHA-1, HMAC, PBKDF2, CCM, RFC 3394)
- [x] Test bench radio: 802.11 frames inside Ethernet frames to `tools/wifi-ap.py`
- [ ] Driver for a real radio chip

### Real hardware (ASUS X99, Broadwell-E, UEFI 2015)
- [x] Boots to the desktop; installed on a SATA SSD; reachable over ssh
- [x] Findings applied: framebuffer must be write-combining and never read back; USB ports need power and the chipset's port routing; a NIC must stop bus mastering before reset; the I210's PHY sleeps without a cable and is not asked then; the PCH NIC (I218) is never reset

## Tests

| Script | Proves |
|---|---|
| tools/termtest.sh | terminal words through the screen |
| tools/usbtest.sh | USB keyboard and mouse, i8042 off |
| tools/usbhub.sh | keyboard and mouse behind a hub |
| tools/usbplug.sh | mouse unplugged and plugged back in; report descriptor self-test |
| tools/asmtest.sh, cctest.sh, cctest2.sh | assembler and compiler on the machine (24 checks) |
| tools/sshtest.sh | door: exec, pipe, pty; foreign key refused |
| tools/sshmulti.sh | three ssh visitors served at once through the one door |
| tools/sshrekey.sh | the door survives a client-driven mid-session rekey |
| tools/pipe-two.sh | object pipe: discovered by scan, sealed, crosses; nothing in the clear on the wire |
| tools/pipe-identity.sh | the pipe refuses a changed key at a known address |
| tools/pipe-rotate.sh | a key trusted before meeting; a renewed key propagated to a peer, signed old and new |
| tools/pipe-update.sh | a kernel through the pipe: refused without the update right, installed and booted with it |
| tools/pipe-input.sh | far work with an input object -- to a recipe and to a compiled image that reads it from its letter box |
| tools/pipe-code.sh | compiled far work: a c task built and run on the worker, signed answer, a runaway task ended by the deadline, its failure raised on the attention page |
| tools/pipe-quorum.sh | the same task on two machines; the verified majority makes the result |
| tools/pipe-vouch.sh | a node vouches for a key; a peer that allows it pins the key before meeting, and ignores a vouch it has not allowed |
| tools/update-test.sh | self-update from a local release: a forged package is refused, a correctly signed newer one is installed and the machine reboots into it (needs release-key.pem and python3) |
| tools/pipe-work.sh, pipe-desk.sh, pipe-foreman.sh | far work, split tasks over three machines, standing tasks |
| tools/relaytest.sh, agenttest.sh, persisttest.sh | capability passing between programs, rights following the reference, snapshots (also `make relay`, `make agent`, `make persist`) |
| tools/sticktest.sh | one disk carries loader, kernel and store |
| tools/foreigndisk.sh | a foreign disk stays byte-identical |
| tools/settletest.sh, settlefree.sh | settling whole / in free space |
| tools/installtest.sh | boot-time offer on a non-empty disk |
| tools/renewtest.sh | newer stick installs onto a settled disk; store kept |
| tools/nictest.sh | every NIC family gets a lease; two cards, one cable |
| tools/wifitest.sh | WPA2 against the virtual access point |
| tools/selfkernel.sh (KVM) | kernel built on the machine from sources sent through the door |
| tools/selfbuild.sh, cctrial.sh | kernel built by the machine's compiler on the host |
| tools/fuzz/run.sh | fuzzing of the language tools |

`build/battery.sh` builds once, then runs 26 tests in parallel lanes (`LANES`, default 6), each in its own directory on the Linux file system (`PAR`, default `/tmp/erebus-par`; disk images on `/mnt/c` stall under parallel writes), then renew alone (it rebuilds the kernel twice). Logs, screenshots and QEMU stderr come back to `build/par/<test>/`. A test is stopped after `TEST_LIMIT` seconds (default 480); a failed or stopped test runs once more and is marked "2nd try". Every test sources `tools/testlib.sh`: KVM when `/dev/kvm` is writable (`NOKVM=1` for TCG), waits on serial log lines and marker files instead of fixed sleeps, `BUILD` points at the test's directory. The summary prints seconds per test. `build/kvm-battery.sh` adds selfkernel.

Measured on 32 cores under KVM: about 280 s for all 27 tests (before: 38 minutes sequential under KVM, 22 minutes under TCG). The longest is pipe-code, which twice waits out a compiled task's deadline.

## Using the ISO

- Download: releases page, `erebus.iso`.
- Write raw to a usb stick: `dd if=erebus.iso of=/dev/sdX bs=4M`, Rufus in DD mode, or balenaEtcher. Or burn a disc.
- Firmware: UEFI boot, Secure Boot off (the loader is unsigned). No BIOS mode.
- First start: the machine lists its disks and asks; a number and `yes` installs.
- Installed machine, newer stick: boot the stick, `install this kernel`, remove the stick, `restart`.
- QEMU: `qemu-system-x86_64 -machine q35 -m 512M -bios /usr/share/OVMF/OVMF.fd -cdrom erebus.iso`
- Remote: put a public key into the settings (`door | ssh-ed25519 ...`), read the address with `address`, connect with any ssh client.
- Several machines: `scan`, `point at <name>`, `say hello` (the handshake writes a row into both nodes tables); `allow <node> update` on the machine to be updated; `update <node>` on the other.

## Known limits

- No USB mass storage: a stick boots the machine but cannot hold the store.
- No wireless chip driver.
- TLS: privacy only, no certificate verification. Self-update does not lean on it -- the release package is ed25519-signed and verified against a key built into the kernel -- but a general https fetch is still not proof of the server's identity.
- Self-update checks by fetching the whole package (a cheap version pre-check is a later refinement), so it runs at most every six hours; the release private key, if lost, means deployed machines can no longer be sent a signed update.
- Far-work answers are signed by the node that produced them and checked against its key, but the computation itself is not otherwise verified; running the same task on several nodes and comparing is left for later.
- Node identity is trust on first use; `trust <name> <key>` pins one beforehand, `forget` re-pins a changed key, `renew key` rotates a key under the old key's signature, and `vouch` lets a node you have marked `vouch` pin a key for you -- but a vouch is only as good as your trust in the voucher, and no vouch is revoked once made.
- A quorum takes the answer a verified majority agree on, but does not otherwise check the computation; a compiled or quorum task takes no split range yet.
- A compiled task must fit one datagram (1024 bytes) and answers through the raw system-call ABI; one compile runs at a time per machine.
- The ssh door serves up to four visitors at once (a fifth displaces the longest-idle); it honours a client-driven rekey but does not force one.
- One ssh visitor at a time; no rekeying.
- RTL8168/8169 driver written from documentation, untested on silicon.
- Two HID inputs on one device: implemented, not tested on a real device.
- No 128-bit integer type in the compiler.

## Releases

- EreBUS 0.4.4: first published version.
- EreBUS 0.5.0: nodes: identity by key, rights per node, kernel updates through the pipe, network page, work with inputs and provenance.
- EreBUS 0.5.1: compiler fix (member lookup in structs with inner struct bodies); a self-built kernel can compile again; selfkernel test runs a second generation.
- EreBUS 0.5.2: console messages of the assembler programs reworded to factual wording; test battery parallel and reliable under load, 22 tests in about four minutes. No change to what the machine does.
- EreBUS 0.6.0: visual overhaul of the shell -- one warm ground, a single accent for agency and position, regions parted by rules, the focused name at double height. Structure, layout and behaviour unchanged.
- EreBUS 0.7.0: serious far work -- answers signed with the node's door key and verified against its key; a kernel-enforced deadline that ends a runaway job with no system calls; compiled tasks (`ask <task> as code`) built and run on the worker under that deadline; a job ledger on the system shelf.
- EreBUS 0.8.0: quorum far work (`ask <task> across N`), the result a verified majority agree on; `forget <node>` to re-pin a changed key; the ssh door serves several visitors at once and survives a mid-session rekey.
- EreBUS 0.8.1: identity beyond trust-on-first-use -- `trust <name> <key>` pins a node before it is met, `renew key` rotates the door key and tells known nodes (signed old and new); the network page shows uptime and the journal notes a node going quiet or coming back.
- EreBUS 0.8.2: the shell fills its own space -- home opens on a machine overview (load over the last minute, running programs, recent journal); a status line carries uptime, load, memory, threads, objects, nodes and address; the picked reference shows a preview of its target through the target's own lens. Structure and behaviour unchanged.
- EreBUS 0.8.3: far work takes an input in every form (`ask <task> with <object>` alongside `as code` and `across N`; a compiled worker reads it from its letter box); vouching -- `allow <node> vouch` honours a node's signed vouches and `vouch <node>` tells known nodes a key is one you recognise, so they pin it before meeting; an `attention` page gathers notable events (a failed job, a node gone quiet) with an unseen count in the status line.
- EreBUS 0.8.4: self-update from a signed release -- `update | auto` checks now and then and installs a newer version on its own; the package's ed25519 signature is verified against a key built into the kernel (the signature, not the transport, is what makes it safe), and the loader's kernel.old rollback still applies. `update check` looks on demand. Larger tcp receive window for faster downloads; clean connection close.
- EreBUS 0.8.5: self-update fixes so it works against a real release host -- the fetch carries the long signed redirect URLs a CDN returns (the request and Location buffers were too small and truncated the token), and no further check runs once an install is pending, so a machine updates and restarts exactly once. Verified end to end against the GitHub release.

## License

Copyright (C) 2026  DustinHab

EreBUS is free software, licensed under the **GNU Affero General Public
License, version 3 or (at your option) any later version** (AGPL-3.0-or-later).
You may use, study, share and modify it. If you distribute it — or run a
modified version that people reach over a network — you must pass on the
complete corresponding source under the same license (see section 13 for the
network case). There is no warranty. The full text is in the
[LICENSE](LICENSE) file.
