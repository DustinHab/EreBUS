#!/usr/bin/env python3
"""
mkfont.py -- builds the kernel font from GNU Unifont.

Unifont ships as a .hex file: one line per glyph, "0041:<32 hex digits>",
which is 16 bytes of 8 pixels each -- exactly our 8x16 cell. So no
rasteriser is needed, only a parser.

Only the ranges we actually want to display are taken; the whole of
Unifont would be well over a megabyte.

  usage:  mkfont.py <unifont.hex> <output.h>
"""
import sys

# (first, last) code point, both inclusive.
RANGES = [
    (0x0020, 0x02FF),  # Latin, Latin-1 and the extended blocks
    (0x2010, 0x203A),  # dashes, quotation marks, ellipsis
    (0x2190, 0x21BF),  # arrows
    (0x2500, 0x257F),  # box drawing
    (0x2580, 0x259F),  # block elements (bars, shading)
    (0x25A0, 0x25CF),  # geometric shapes
]

# Substitute for a missing code point: a filled box with a border.
MISSING = [0x00, 0x00, 0x7E, 0x42, 0x42, 0x5A, 0x5A, 0x42,
           0x42, 0x5A, 0x5A, 0x42, 0x42, 0x7E, 0x00, 0x00]


def load_hex(path):
    glyphs = {}
    with open(path, "r", encoding="ascii") as fh:
        for line in fh:
            line = line.strip()
            if not line or ":" not in line:
                continue
            code, bits = line.split(":", 1)
            # Single-width glyphs only: 32 hex digits = 16 bytes = 8x16.
            if len(bits) != 32:
                continue
            glyphs[int(code, 16)] = bytes.fromhex(bits)
    return glyphs


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mkfont.py <unifont.hex> <output.h>")

    glyphs = load_hex(sys.argv[1])

    rows, ranges, missing_count = [], [], 0
    for first, last in RANGES:
        offset = len(rows)
        for cp in range(first, last + 1):
            g = glyphs.get(cp)
            if g is None:
                g = MISSING
                missing_count += 1
            rows.append((cp, g))
        ranges.append((first, last, offset))

    out = []
    out.append("/*")
    out.append(" * font8x16.h -- GENERATED FILE, do not edit by hand.")
    out.append(" *")
    out.append(" * Source: GNU Unifont (SIL Open Font License 1.1),")
    out.append(" * produced by tools/mkfont.py.")
    out.append(" *")
    out.append(f" * {len(rows)} glyphs in {len(ranges)} ranges, "
               f"{len(rows) * 16} bytes.")
    out.append(" */")
    out.append("#ifndef EB_FONT8X16_H")
    out.append("#define EB_FONT8X16_H")
    out.append("")
    out.append("#include <eb/types.h>")
    out.append("")
    out.append("typedef struct {")
    out.append("    u32 first;   /* first code point of the range     */")
    out.append("    u32 last;    /* last one, inclusive               */")
    out.append("    u32 offset;  /* row in font_bitmap for 'first'    */")
    out.append("} font_range;")
    out.append("")
    out.append(f"#define FONT_RANGE_COUNT {len(ranges)}")
    out.append(f"#define FONT_GLYPH_COUNT {len(rows)}")
    out.append("")
    out.append("static const font_range font_ranges[FONT_RANGE_COUNT] = {")
    for first, last, offset in ranges:
        out.append(f"    {{ 0x{first:04X}, 0x{last:04X}, {offset} }},")
    out.append("};")
    out.append("")
    out.append("/* Shown in place of a code point we do not have. */")
    out.append("static const u8 font_missing[16] = {")
    out.append("    " + ", ".join(f"0x{b:02X}" for b in MISSING))
    out.append("};")
    out.append("")
    out.append("static const u8 font_bitmap[FONT_GLYPH_COUNT][16] = {")
    for cp, g in rows:
        body = ",".join(f"0x{b:02X}" for b in g)
        ch = chr(cp) if 0x21 <= cp <= 0x7E else ""
        note = f"  /* U+{cp:04X} {ch} */" if ch else f"  /* U+{cp:04X} */"
        out.append(f"    {{{body}}},{note}")
    out.append("};")
    out.append("")
    out.append("#endif /* EB_FONT8X16_H */")

    with open(sys.argv[2], "w", encoding="ascii", newline="\n") as fh:
        fh.write("\n".join(out) + "\n")

    print(f"mkfont: {len(rows)} glyphs, {len(rows) * 16} bytes, "
          f"{missing_count} missing and substituted")


if __name__ == "__main__":
    main()
