#!/usr/bin/env python3
"""
mkfont.py -- erzeugt den Zeichensatz des Kernels aus GNU Unifont.

Unifont liegt als .hex vor: pro Zeile "0041:<32 Hexziffern>", also 16 Bytes
zu je 8 Pixeln -- genau unser 8x16-Raster. Damit brauchen wir gar keinen
Rasterizer, nur einen Parser.

Es werden nur die Bereiche uebernommen, die wir wirklich anzeigen wollen;
das vollstaendige Unifont waere ueber 1 MB gross.

  Aufruf:  mkfont.py <unifont.hex> <ausgabe.h>
"""
import sys

# (erster, letzter) Codepunkt, jeweils einschliesslich.
RANGES = [
    (0x0020, 0x02FF),  # Latein, Latein-1, erweitert -- deckt Deutsch ab
    (0x2010, 0x203A),  # Gedankenstriche, Anfuehrungszeichen, Auslassung
    (0x2190, 0x21BF),  # Pfeile
    (0x2500, 0x257F),  # Rahmenzeichen
    (0x2580, 0x259F),  # Blockelemente (Balken, Raster)
    (0x25A0, 0x25CF),  # geometrische Formen
]

# Ersatzzeichen, wenn ein Codepunkt fehlt: gefuelltes Kaestchen mit Rand.
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
            # Nur einfachbreite Glyphen: 32 Hexziffern = 16 Bytes = 8x16.
            if len(bits) != 32:
                continue
            glyphs[int(code, 16)] = bytes.fromhex(bits)
    return glyphs


def main():
    if len(sys.argv) != 3:
        sys.exit("Aufruf: mkfont.py <unifont.hex> <ausgabe.h>")

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
    out.append(" * font8x16.h -- ERZEUGTE DATEI, nicht von Hand aendern.")
    out.append(" *")
    out.append(" * Quelle: GNU Unifont (SIL Open Font License 1.1),")
    out.append(" * erzeugt von tools/mkfont.py.")
    out.append(" *")
    out.append(f" * {len(rows)} Zeichen in {len(ranges)} Bereichen, "
               f"{len(rows) * 16} Bytes.")
    out.append(" */")
    out.append("#ifndef EB_FONT8X16_H")
    out.append("#define EB_FONT8X16_H")
    out.append("")
    out.append("#include <eb/types.h>")
    out.append("")
    out.append("typedef struct {")
    out.append("    u32 first;   /* erster Codepunkt des Bereichs      */")
    out.append("    u32 last;    /* letzter, einschliesslich           */")
    out.append("    u32 offset;  /* Zeile in font_bitmap fuer 'first'  */")
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
    out.append("/* Ersatzdarstellung fuer unbekannte Zeichen. */")
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

    print(f"mkfont: {len(rows)} Zeichen, {len(rows) * 16} Bytes, "
          f"{missing_count} fehlten und wurden ersetzt")


if __name__ == "__main__":
    main()
