# Contributing

EreBUS is one system built to one standard: everything hardware-dependent
self-tests at boot, every wire and disk format has its number in one
place, and no change ships until the regression battery is green. A
contribution is held to the same standard.

## Building

Linux (here WSL2 with Ubuntu):

    apt install clang lld nasm make qemu-system-x86 ovmf mtools \
                dosfstools xorriso gdb unifont python3-pil

    make          # loader, kernel, bootable image build/esp.img
    make run      # QEMU, serial on the terminal
    make shot     # headless; build/screen.png, build/serial.log
    make debug    # halted, gdb on port 1234
    make clean

From Windows: `wsl -d Ubuntu -- bash -lc "cd /mnt/c/erebus && make run"`.

## Testing

- One regression test: `sh tools/<test>.sh` (the table is in `MANUAL.md`).
  A test uses KVM when `/dev/kvm` is writable and TCG otherwise.
- The whole battery: `sh tools/battery.sh` -- one build, then 37 tests,
  most in parallel lanes, three run alone. It prints seconds per test,
  any `FAILED` lines, and a total. A failed or timed-out test runs once
  more before it is reported.
- The battery must be green before a change is committed for release.
  "Quality over compute time": a slow but decisive test run is preferred
  to a fast uncertain one.
- `sh tools/reproduce.sh` builds twice and checks the kernel and loader
  come out byte-identical. `sh tools/bootsmoke.sh` boots the image once
  under emulation and confirms the self-tests pass and the kernel reaches
  idle. Both run in CI on every push (`.github/workflows/ci.yml`); the
  runners have no KVM, so CI runs under TCG and the full battery stays a
  local, pre-release step.

Any change to a wire or disk parser must come with a fuzzer input or a
new case; the fuzzers are under `tools/fuzz/`.

## What a change must include

- A test that exercises the change. A bug fix comes with a regression
  test that fails before the fix and passes after it.
- If the change alters what the machine does on the network, on disk, or
  on screen, an update to `MANUAL.md` (and `README.md` where it lists the
  same fact).
- A release-notes line in `MANUAL.md` when the change is user-visible.

## Coding style

- C11, freestanding, compiled with clang. No dependence on a hosted libc.
- The kernel uses no SSE/MMX/AVX. The vector unit is enabled for user
  programs only and saved per process. Do not introduce floating point or
  vector code into the kernel.
- Match the surrounding code: its naming, its comment density, its
  idioms. Read the file before adding to it.
- Comments and documentation are factual and technical. State what the
  code does and why. No metaphors, no decoration, no restating the code
  in prose.
- Keep the trusted base small. A new primitive, parser, or driver is
  attack surface; add it only when it earns its place, and make it
  self-test at boot like the rest.

## Concurrency

The kernel runs on several processors. When adding code that touches
shared state:

- Follow the established lock order: `sched_lock -> cap_lock -> obj_lock
  -> kheap -> pmm`, and `vmm_lock -> pmm`. The object and capability
  layers never call the scheduler, so the order stays acyclic.
- Hardware-touching kernel threads stay on the boot processor (thread
  affinity, `may_roam` false); interrupts from devices are handled there.
  Only work that touches no device -- user processes, compute workers --
  may roam across processors.

## Commits

- Commit messages are in English, factual, and describe the change and
  its reason. No decorative language.
- Subject line in the imperative, lower case, area-prefixed where it
  helps (e.g. `net: ...`, `smp: ...`, `x509: ...`), kept short; a body
  when the reason is not obvious from the diff.
- No automated authorship trailers.
- `release-key.pem` is never committed; it is in `.gitignore`.
- Tags are the bare version number, without a leading letter.

## Reporting security issues

Do not open a public issue for a security flaw. See `SECURITY.md`.

## License

By contributing you agree that your contribution is licensed under the
project's license, AGPL-3.0-or-later (see `LICENSE`).
