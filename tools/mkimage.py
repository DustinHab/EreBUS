#!/usr/bin/env python3
# mkimage.py -- a program linked for the machine (programs/decoder/program.ld) as the image the machine loads
# (MANUAL 18.4: "EBX2", code length, data length, zeroed room, entry offset, then the code and the data), and that
# image as a C array for the kernel to carry.
#   python3 tools/mkimage.py <program.elf> <out.img> [<out.c> <symbol>]
#   python3 tools/mkimage.py wrap <in.img> <out.c> <symbol>      an image made elsewhere (the self-build's), wrapped
import struct, sys

CODE_AT, DATA_AT = 0x1000000, 0x1100000

def image_of(elf):
    if elf[:4] != b'\x7fELF' or elf[4] != 2 or elf[5] != 1:
        sys.exit("mkimage: not a little-endian ELF64")
    e_entry, e_phoff = struct.unpack_from('<QQ', elf, 24)
    e_phentsize, e_phnum = struct.unpack_from('<HH', elf, 54)
    code = data = None
    for i in range(e_phnum):
        p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = \
            struct.unpack_from('<IIQQQQQQ', elf, e_phoff + i * e_phentsize)
        if p_type != 1:                      # PT_LOAD only
            continue
        seg = (p_vaddr, elf[p_offset:p_offset + p_filesz], p_memsz)
        if p_flags & 1:                      # PF_X: the code
            if code is not None: sys.exit("mkimage: two code segments")
            code = seg
        else:
            if data is not None: sys.exit("mkimage: two data segments")
            data = seg
    if code is None or code[0] != CODE_AT:
        sys.exit("mkimage: the code segment is not at 0x%x" % CODE_AT)
    if len(code[1]) != code[2]:
        sys.exit("mkimage: the code segment has zeroed room; the data segment is where that goes")
    if len(code[1]) > DATA_AT - CODE_AT:
        sys.exit("mkimage: more than 1 MiB of code")
    if data is None:
        data_bytes, zero = b'', 0
    else:
        if data[0] != DATA_AT:
            sys.exit("mkimage: the data segment is not at 0x%x" % DATA_AT)
        data_bytes, zero = data[1], data[2] - len(data[1])
    entry = e_entry - CODE_AT
    if not 0 <= entry < len(code[1]):
        sys.exit("mkimage: the entry is outside the code")
    return b'EBX2' + struct.pack('<IIII', len(code[1]), len(data_bytes), zero, entry) + code[1] + data_bytes

def wrap(img, symbol):
    lines = ["/* %s: a program image (MANUAL 18.4), made by tools/mkimage.py; %d bytes. */" % (symbol, len(img)),
             "#include <eb/types.h>",
             "const u8 %s[%d] = {" % (symbol, len(img))]
    for at in range(0, len(img), 24):
        lines.append("    " + ",".join(str(b) for b in img[at:at + 24]) + ",")
    lines.append("};")
    lines.append("const u32 %s_len = %d;" % (symbol, len(img)))
    return "\n".join(lines) + "\n"

if len(sys.argv) >= 2 and sys.argv[1] == "wrap":
    if len(sys.argv) != 5: sys.exit("usage: mkimage.py wrap <in.img> <out.c> <symbol>")
    img = open(sys.argv[2], 'rb').read()
    if img[:4] != b'EBX2': sys.exit("mkimage: %s is not an image" % sys.argv[2])
    open(sys.argv[3], 'w').write(wrap(img, sys.argv[4]))
    sys.exit(0)

if len(sys.argv) not in (3, 5): sys.exit("usage: mkimage.py <program.elf> <out.img> [<out.c> <symbol>]")
img = image_of(open(sys.argv[1], 'rb').read())
open(sys.argv[2], 'wb').write(img)
if len(sys.argv) == 5:
    open(sys.argv[3], 'w').write(wrap(img, sys.argv[4]))
