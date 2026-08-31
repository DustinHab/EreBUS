#!/usr/bin/env python3
"""
check-isr.py -- verify the interrupt stub table in a built kernel.

The C side computes the address of vector n as isr_stubs + n * 16. If a
stub ever grew past 16 bytes, every vector above it would point into the
middle of an instruction, and the failure would look like random
corruption rather than a build problem. So check it against the binary.

Also confirms that the ten vectors which receive an error code from the
processor do NOT push a zero of their own, and that all the others do.

  usage:  check-isr.py [kernel.elf]
"""
import re
import subprocess
import sys

# Vectors for which the processor itself pushes an error code.
ERROR_VECTORS = {8, 10, 11, 12, 13, 14, 17, 21, 29, 30}

STUB_SIZE = 16
STUB_COUNT = 256


def symbol(elf, name):
    out = subprocess.run(["nm", elf], capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] == name:
            return int(parts[0], 16)
    sys.exit(f"check-isr: symbol {name} not found in {elf}")


def disassemble(elf):
    """Address -> first instruction text, for the whole binary."""
    out = subprocess.run(["objdump", "-d", elf], capture_output=True,
                         text=True, check=True).stdout
    insns = {}
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*\t(.*)$", line)
        if m:
            insns[int(m.group(1), 16)] = m.group(2).strip()
    return insns


def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else "build/kernel.elf"

    base = symbol(elf, "isr_stubs")
    common = symbol(elf, "isr_common")
    insns = disassemble(elf)

    print(f"isr_stubs at 0x{base:016x}, isr_common at 0x{common:016x}")

    # isr_common sits directly behind the table, so the gap is the proof
    # that every slot is the size it claims to be.
    expected_gap = STUB_COUNT * STUB_SIZE
    if common - base != expected_gap:
        sys.exit(f"check-isr: table spans {common - base} bytes, "
                 f"expected {expected_gap}")

    problems = 0
    for vec in range(STUB_COUNT):
        addr = base + vec * STUB_SIZE
        first = insns.get(addr)

        if first is None:
            print(f"check-isr: vector {vec} at 0x{addr:x} has no "
                  f"instruction -- slot boundary is wrong", file=sys.stderr)
            problems += 1
            continue

        pushes_zero = first.startswith("push") and "$0x0" in first
        wants_zero = vec not in ERROR_VECTORS

        # Vector 0 pushes zero twice over (the filler and its own
        # number), so the first instruction looks the same either way.
        if vec == 0:
            continue

        if pushes_zero != wants_zero:
            kind = "should" if wants_zero else "should not"
            print(f"check-isr: vector {vec} at 0x{addr:x} {kind} push a "
                  f"filler zero, but starts with '{first}'", file=sys.stderr)
            problems += 1

    if problems:
        sys.exit(f"check-isr: {problems} problem(s) found")

    print(f"check-isr: all {STUB_COUNT} stubs sit on their "
          f"{STUB_SIZE}-byte slot, error-code vectors handled correctly")


if __name__ == "__main__":
    main()
