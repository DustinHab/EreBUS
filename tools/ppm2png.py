#!/usr/bin/env python3
"""
ppm2png.py -- wandelt den Bildschirmabzug von QEMU in ein PNG.

QEMU schreibt mit "screendump" ein PPM. Das ist unkomprimiert und
riesig; als PNG laesst es sich ansehen und weitergeben.
"""
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("ppm2png: python3-pil fehlt (apt install python3-pil)")


def main():
    if len(sys.argv) != 3:
        sys.exit("Aufruf: ppm2png.py <eingabe.ppm> <ausgabe.png>")

    src, dst = sys.argv[1], sys.argv[2]
    try:
        img = Image.open(src)
    except FileNotFoundError:
        sys.exit(f"ppm2png: {src} nicht vorhanden -- lief QEMU ueberhaupt?")

    img.save(dst)
    print(f"ppm2png: {dst} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
