"""mknames.py -- the kernel's name table as a C file.

    python3 tools/mknames.py                  an empty table, for the first link
    nm -n kernel.stage1.elf | python3 tools/mknames.py names
                                              the table of the code's names

The shape is the one kernel/lib/names.c reads and the machine's own
linker writes: u32 count, u32 offset of the letters, entries of
{ u64 addr; u32 name; u32 pad } in address order, then the letters.
Only code is named -- text symbols -- so that the table, which lies
between the data and the bss, moves no address it names when its own
size changes between the first link and the second.
"""
import sys

entries = []
if len(sys.argv) > 1 and sys.argv[1] == 'names':
    for line in sys.stdin:
        parts = line.split()
        if len(parts) != 3:
            continue
        addr, kind, name = parts
        if kind not in ('T', 't', 'W', 'w'):
            continue
        if name.startswith('.') or name.startswith('__names') or name == '__kernel_start':
            continue
        entries.append((int(addr, 16), name))
entries.sort()

letters = []
offset = 0
offsets = []
for addr, name in entries:
    offsets.append(offset)
    letters.append(name)
    offset += len(name) + 1

print('/* made by tools/mknames.py: the names of the code, in address order */')
print('#include <eb/types.h>')
print('struct names_entry { u64 addr; u32 name; u32 pad; };')
n = len(entries)
print('__attribute__((section(".names"), used)) const struct {')
print('    u32 count; u32 letters;')
if n:
    print('    struct names_entry e[%d];' % n)
    print('    char s[%d];' % (offset if offset else 1))
print('} __names_table = {')
print('    %d, %d,' % (n, 8 + n * 16))
if n:
    print('    {')
    for (addr, name), off in zip(entries, offsets):
        print('        { 0x%016xULL, %d, 0 },' % (addr, off))
    print('    },')
    body = ''.join(x + '\\0' for x in letters)
    # in pieces, so that no one line is unreasonably long
    print('    "' + body[:0] + '"')
    step = 60
    i = 0
    while i < len(body):
        piece = body[i:i + step]
        # do not split an escape
        while piece.endswith('\\'):
            piece = body[i:i + len(piece) + 1]
        print('    "%s"' % piece)
        i += len(piece)
print('};')
