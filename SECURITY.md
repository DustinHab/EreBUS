# Security

This document states what EreBUS defends, what it does not, and how to
report a flaw. It is written to be checked against the code, not to
reassure.

## What the system is

EreBUS is an object-based, capability-secured operating system for a
single-person x86_64 UEFI machine. A program reaches only the objects it
was handed: there is no global namespace, no `obj_find()`, no way to name
an object one was not given. Authority travels as a reference; holding
the reference is the permission.

Capability isolation governs what a running program can name and reach.
It is not, today, a privilege boundary for most of the code. The browser
and its HTML/CSS renderer, the C compiler, and the shell are compiled
into the kernel and run in ring 0 (`kernel/gfx/`, `kernel/lang/`); only a
small set of user programs -- about two and a half thousand lines under
`kernel/user/` -- runs in ring 3 behind the system-call ABI, against some
fifty thousand lines of ring-0 C. So a bug in a parser that reads
outside input -- a page, a stylesheet, an image, a certificate, a
compiled task -- is a bug in ring 0. Moving those parsers into ring 3 to
shrink the trusted base is the largest open piece of work and is not
done; until it is, capability isolation is what a correct kernel
enforces between programs, not a wall that contains a broken parser.

The machine speaks outward mostly as a client (http, https, ssh). Two
services listen:
- the ssh door, which authenticates a client against the door keys in
  the settings before it serves;
- the node pipe on UDP 7800 (`kernel/net/pipe.c`), which answers a
  discovery probe (`SEEK`) from any address with the machine's name,
  version, free memory and the claim of a public key. It is
  unauthenticated at this layer: node identity is trust on first use, so
  a probe both learns those facts and can assert a key, pinned only on
  first contact.

## Trusted computing base

- The UEFI loader (`boot/`), written here; no third-party boot code.
- The kernel (`kernel/`): scheduler, memory, object store, capabilities,
  drivers, the network stack, the cryptographic primitives, and -- until
  the ring-3 move above -- the browser, the renderer, the compiler and
  the shell.
- The built-in trust anchors: the TLS root authorities in
  `kernel/net/authorities.h` and the release-signing public key compiled
  into the kernel.

No foreign code runs in the trusted base: every line of it is in this
repository. That bounds who wrote it, not how large it is -- the base is
tens of thousands of lines of ring-0 C, which the ring-3 move is meant to
cut down.

## Cryptographic posture

- One TLS 1.3 suite: X25519 key exchange, AES-128-GCM, SHA-256. Signature
  verification for certificates covers ECDSA P-256/P-384 and RSA (PKCS#1
  v1.5 and PSS) with SHA-256/384/512.
- Every primitive self-tests at boot against published vectors
  (`crypto_selftest`, `pki_selftest`). TLS refuses to run if a self-test
  fails.
- Random bytes come from a SHA-256 entropy pool seeded by RDSEED, then
  RDRAND, then the cycle counter (`rand_bytes`). The stack-protector guard
  is drawn from it once, early in start-up (`stack_guard_init`), so it is
  not a compile-time constant.
- Secret-dependent comparisons (WPA2 MIC, GCM tags) use a constant-time
  equality (`ct_equal`); no early exit reveals where two values first
  differ.
- The AES is constant-time: its S-box (forward and inverse) is computed
  by inversion in GF(2^8), not looked up, and GHASH multiplies without a
  data-dependent branch, so no secret byte indexes memory or steers a
  branch. The computed S-box is checked against the reference for all 256
  inputs at boot. Constant time in the C source is not a guarantee at the
  machine-code level, but no secret-indexed table remains.
- Self-update packages are ed25519-signed; the signature, not the
  transport, is what authorises an update. Two release keys are built into
  the kernel: the one that signs releases, and a second kept apart and
  unused against the loss of the first -- a release signed with either is
  accepted. A signed rotation (`tools/sign-rotation.sh`, published as the
  asset `rotate`) moves deployed machines to one key, written into their
  settings; from then on that key alone is trusted and a package signed
  with the previous key is refused (`tools/rotate-test.sh`). A key lost
  after a rotation to it cannot be replaced on already-deployed machines;
  a key lost before one is covered by the other built-in key.
- Certificate chains are walked to a trusted authority with dates and host
  names checked, and a leaf whose names fall outside a signing authority's
  name constraints is refused (RFC 5280 dNSName permitted and excluded
  subtrees).

## Known limits and accepted risk

These are deliberate, bounded, and documented rather than hidden.

- **No revocation checking.** A certificate revoked by its issuer is still
  accepted until it expires. There is no OCSP or CRL fetch.
- **Constant time is at the source level.** The AES and the tag and MIC
  comparisons are written to run in constant time, but the C compiler is
  free to undo that; there is no machine-code check. No secret-indexed
  table or data-dependent branch remains in the source, which is what a
  reader can verify.
- **Node identity is trust on first use.** A node's key is pinned when
  first seen unless pinned beforehand with `trust`. A vouch is only as
  good as trust in the voucher.
- **Far-work results are signed, not proven.** An answer is signed by the
  node that produced it and checked against that node's key, but the
  computation is not otherwise verified except by running it across a
  quorum and comparing.
- **Untested drivers.** The RTL8168/8169 driver is written from
  documentation and untested on silicon; two-HID-on-one-device is
  implemented but untested on real hardware. These are stated in the
  manual's known limits.

## Supported versions

Fixes land on the current release line. There is one active line; older
released versions are not separately patched. The current version is in
the `version` file and shown in the release notes in `MANUAL.md`.

## Reporting a vulnerability

Report privately, not in a public issue.

- Preferred: open a private security advisory through the repository's
  GitHub "Security" tab (Report a vulnerability). This keeps the report
  and the fix private until a release is ready.
- Include: the version, what an attacker can reach, the steps or input
  that trigger it, and -- if you have one -- a minimal reproducer against
  the test battery (`tools/battery.sh`) or one of the fuzzers under
  `tools/fuzz/`.

There is no bounty. Reports are read and answered; a fix is released with
an entry in the release notes crediting the reporter if they wish.

## Handling of a report

1. Confirm and reproduce, ideally as a failing case in the battery.
2. Fix, and add a regression test that fails before the fix and passes
   after it.
3. The full battery must be green before the fix is released.
4. Release a signed update and record the flaw in the release notes.
