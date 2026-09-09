# Security

This document states what EreBUS defends, what it does not, and how to
report a flaw. It is written to be checked against the code, not to
reassure.

## What the system is

EreBUS is an object-based, capability-secured operating system for a
single-person x86_64 UEFI machine. A program reaches only the objects it
was handed: there is no global namespace, no `obj_find()`, no way to name
an object one was not given. Authority travels as a reference; holding
the reference is the permission. The kernel, its own UEFI loader, and the
built-in tools are the whole trusted base -- no foreign code runs in it.

The machine speaks outward as a client (http, https, ssh, a node pipe to
other EreBUS machines). It runs no listening internet service other than
the ssh door, which serves the machine's owner.

## Trusted computing base

- The UEFI loader (`boot/`), written here; no third-party boot code.
- The kernel (`kernel/`): scheduler, memory, object store, capabilities,
  drivers, the network stack, and the cryptographic primitives.
- The built-in trust anchors: the TLS root authorities in
  `kernel/net/authorities.h` and the release-signing public key compiled
  into the kernel.

Everything else -- user programs, the browser, the compiler, the shell --
runs in ring 3 and reaches the kernel only through the system-call ABI.

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
- Self-update packages are ed25519-signed; the signature, not the
  transport, is what authorises an update. A lost release private key
  cannot be recovered and cannot be replaced on already-deployed machines.
- Certificate chains are walked to a trusted authority with dates and host
  names checked, and a leaf whose names fall outside a signing authority's
  name constraints is refused (RFC 5280 dNSName permitted and excluded
  subtrees).

## Known limits and accepted risk

These are deliberate, bounded, and documented rather than hidden.

- **AES timing.** The AES is software with table lookups. Table timing is
  a real side channel. For a single-person machine speaking outward it is
  noted and accepted; a constant-time or hardware AES path is tracked
  work. The kernel keeps the vector units off, so there is no SSE state to
  save and nothing may use it.
- **No revocation checking.** A certificate revoked by its issuer is still
  accepted until it expires. There is no OCSP or CRL fetch.
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
  the test battery (`build/battery.sh`) or one of the fuzzers under
  `tools/fuzz/`.

There is no bounty. Reports are read and answered; a fix is released with
an entry in the release notes crediting the reporter if they wish.

## Handling of a report

1. Confirm and reproduce, ideally as a failing case in the battery.
2. Fix, and add a regression test that fails before the fix and passes
   after it.
3. The full battery must be green before the fix is released.
4. Release a signed update and record the flaw in the release notes.
