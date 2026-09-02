"""selfmap.py -- a fault's address, read back to a name.

    python3 tools/selfmap.py build/self/kernel.elf.map 0xffffffff80312345

Prints the nearest public name at or below the address, and the
distance.
"""
import sys

names = []
for line in open(sys.argv[1]):
    parts = line.split(None, 1)
    if len(parts) != 2:
        continue
    names.append((int(parts[0], 16), parts[1].strip()))
names.sort()
for a in sys.argv[2:]:
    addr = int(a, 16)
    best = None
    for v, n in names:
        if v <= addr:
            best = (v, n)
    if best is None:
        print(a, 'below every name')
    else:
        print(a, '=', best[1], '+', hex(addr - best[0]))
