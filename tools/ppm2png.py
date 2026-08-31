#!/usr/bin/env python3
"""
ppm2png.py -- converts a QEMU screen dump into a PNG.

QEMU's "screendump" writes a PPM. That is uncompressed and huge; as a
PNG it can be viewed and passed around.
"""
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("ppm2png: python3-pil is missing (apt install python3-pil)")


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: ppm2png.py <input.ppm> <output.png>")

    src, dst = sys.argv[1], sys.argv[2]
    try:
        img = Image.open(src)
    except FileNotFoundError:
        sys.exit(f"ppm2png: {src} does not exist -- did QEMU run at all?")

    img.save(dst)
    print(f"ppm2png: {dst} ({img.width}x{img.height})")


if __name__ == "__main__":
    main()
