#!/usr/bin/env python3
"""Convert the PNG/JPG artwork to the raw CT32 streams embedded in the ELF.

The output is a 640x448 ARGB word stream (bytes R,G,B,A per pixel) that the
build embeds with bin2c.  Run from the project root:
    python3 tools/mkbg.py
"""
from PIL import Image

ITEMS = [
    ("assets/controller_ps2.png", "assets/controller.raw"),
    ("assets/splash.jpg",         "assets/splash.raw"),
]
W, H = 640, 448

for src, dst in ITEMS:
    im = Image.open(src).convert("RGB").resize((W, H), Image.LANCZOS)
    px = im.load()
    buf = bytearray(W * H * 4)
    i = 0
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            buf[i] = r
            buf[i + 1] = g
            buf[i + 2] = b
            buf[i + 3] = 0x80
            i += 4
    open(dst, "wb").write(buf)
    print("wrote", dst, len(buf), "bytes")
