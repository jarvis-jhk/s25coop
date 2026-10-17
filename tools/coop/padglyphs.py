#!/usr/bin/env python3
# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the pixel-art Xbox controller glyphs in data/RTTR/assets/base/pad_glyphs.

The glyphs are drawn here, from code, so the artwork is reviewable and owes no licence
to anyone: 8-bit palette BMPs like the original game's own icons, palette index 0
transparent. They are sized for the game's normal font (no glyph taller than 13 px).
The file order is the PadGlyph enum in libs/s25main/input/KeyGlyph.h; the D-pad comes
last, one glyph per combination of lit directions (bit 0 up, 1 down, 2 left, 3 right).

  python3 tools/coop/padglyphs.py [--preview out.png]
"""
import math
import os
import struct
import sys
import zlib

PALETTE = [
    (255, 0, 143),    # 0 transparent (libsiedler2's TRANSPARENT_COLOR)
    (20, 14, 8),      # 1 outline
    (58, 56, 60),     # 2 body
    (96, 94, 100),    # 3 body highlight
    (34, 32, 36),     # 4 body shadow
    (104, 212, 72),   # 5 A
    (36, 110, 30),    # 6 A dark
    (240, 76, 64),    # 7 B
    (124, 26, 24),    # 8 B dark
    (80, 150, 255),   # 9 X
    (28, 62, 140),    # 10 X dark
    (252, 208, 48),   # 11 Y
    (132, 96, 10),    # 12 Y dark
    (244, 240, 226),  # 13 white
    (118, 116, 124),  # 14 grey (inactive d-pad arm)
    (16, 16, 18),     # 15 letter shadow
]
T, O, BODY, HI, SH = 0, 1, 2, 3, 4
WHITE, GREY, LSH = 13, 14, 15

FONT5x7 = {
    "A": [" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"],
    "B": ["#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "],
    "X": ["#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"],
    "Y": ["#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "],
}
FONT3x5 = {
    "L": ["#  ", "#  ", "#  ", "#  ", "###"],
    "R": ["## ", "# #", "## ", "# #", "# #"],
    "B": ["## ", "# #", "## ", "# #", "## "],
    "T": ["###", " # ", " # ", " # ", " # "],
    "3": ["###", "  #", " ##", "  #", "###"],
}


class Img:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[T] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x] if 0 <= x < self.w and 0 <= y < self.h else T

    def text(self, s, x, y, font, color, shadow=None):
        for ch in s:
            rows = font[ch]
            for dy, row in enumerate(rows):
                for dx, c in enumerate(row):
                    if c == "#":
                        if shadow is not None and self.get(x + dx + 1, y + dy + 1) != color:
                            self.set(x + dx + 1, y + dy + 1, shadow)
                        self.set(x + dx, y + dy, color)
            x += len(rows[0]) + 1

    def outline(self):
        """Add a one-pixel outline around every opaque pixel; shapes are drawn one pixel inset."""
        edge = [(x, y) for y in range(self.h) for x in range(self.w)
                if self.px[y][x] == T and any(self.get(x + dx, y + dy) not in (T, O)
                                              for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
        for x, y in edge:
            self.px[y][x] = O


def shade(img, inside):
    """Fill `inside` with body colour, lit from the top left like the original's buttons."""
    for y in range(img.h):
        for x in range(img.w):
            if inside(x, y):
                top = not inside(x, y - 1) or not inside(x - 1, y)
                bottom = not inside(x, y + 1) or not inside(x + 1, y)
                img.set(x, y, HI if top else SH if bottom else BODY)


def disc(size):
    img = Img(size, size)
    c = (size - 1) / 2
    r = size / 2 - 1.0
    shade(img, lambda x, y: (x - c) ** 2 + (y - c) ** 2 <= r * r + 0.5)
    img.outline()
    return img


def face(letter, color, dark):
    img = disc(13)
    img.text(letter, 4, 3, FONT5x7, color, dark)
    return img


def bumper(label, right):
    img = Img(19, 10)

    def inside(x, y):
        if not (1 <= x <= 17 and 2 <= y <= 8):
            return False
        # Slanted outer top corner, like the shoulder of the real controller.
        dx = (17 - x) if right else (x - 1)
        return y >= 2 + max(0, 3 - dx) - (1 if dx > 6 else 0) or y >= 4 - min(dx, 2)
    shade(img, inside)
    img.outline()
    img.text(label, 6, 3, FONT3x5, WHITE)
    return img


def trigger(label):
    img = Img(13, 13)

    def inside(x, y):
        if not (1 <= x <= 11 and 1 <= y <= 11):
            return False
        if y <= 4:
            # Rounded top of the trigger.
            return (x - 6) ** 2 + ((y - 5) * 1.3) ** 2 <= 30
        return True
    shade(img, inside)
    img.outline()
    img.text(label, 3, 5, FONT3x5, WHITE)
    return img


def view_btn():
    img = disc(13)
    for x in range(3, 8):
        img.set(x, 3, WHITE)
        img.set(x, 7, WHITE)
    for y in range(3, 8):
        img.set(3, y, WHITE)
        img.set(7, y, WHITE)
    for x in range(5, 10):
        img.set(x, 9, WHITE)
    for y in range(5, 10):
        img.set(9, y, WHITE)
    img.set(8, 5, WHITE)
    img.set(5, 8, WHITE)
    return img


def menu_btn():
    img = disc(13)
    for y in (4, 6, 8):
        for x in range(3, 10):
            img.set(x, y, WHITE)
    return img


def guide_btn():
    # The glowing logo button: a light disc with a dark cross.
    img = disc(13)
    for y in range(img.h):
        for x in range(img.w):
            if img.px[y][x] in (BODY, HI, SH):
                img.px[y][x] = WHITE if img.px[y][x] != SH else GREY
    for i in range(1, 6):
        img.set(3 + i, 3 + i, BODY)
        img.set(9 - i, 3 + i, BODY)
    return img


def dpad(lit):
    img = Img(13, 13)

    def inside(x, y):
        return (4 <= x <= 8 and 1 <= y <= 11) or (1 <= x <= 11 and 4 <= y <= 8)
    shade(img, inside)
    img.outline()
    arms = {"up": [(6, 2), (5, 3), (6, 3), (7, 3)], "down": [(6, 10), (5, 9), (6, 9), (7, 9)],
            "left": [(2, 6), (3, 5), (3, 6), (3, 7)], "right": [(10, 6), (9, 5), (9, 6), (9, 7)]}
    for name, pts in arms.items():
        for x, y in pts:
            img.set(x, y, WHITE if name in lit else GREY)
    return img


def stick(letter, motion):
    size = 13
    img = Img(size, size)
    c = (size - 1) / 2
    r = 3.6 if motion else 5.0
    shade(img, lambda x, y: (x - c) ** 2 + (y - c) ** 2 <= r * r + 0.5)
    img.outline()
    # Concave cap: a ring of shadow inside the stick top.
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c)
            if (1.9 if motion else 2.6) <= d <= (2.4 if motion else 3.2) and img.get(x, y) == BODY:
                img.set(x, y, SH)
    m = int(c)
    if motion:
        img.text(letter, m - 1, m - 2, FONT3x5, WHITE)
        for pts in (((m, 0), (m - 1, 1), (m + 1, 1)), ((m, size - 1), (m - 1, size - 2), (m + 1, size - 2)),
                    ((0, m), (1, m - 1), (1, m + 1)), ((size - 1, m), (size - 2, m - 1), (size - 2, m + 1))):
            for x, y in pts:
                img.set(x, y, WHITE)
    else:
        img.text(letter + "3", m - 3, m - 2, FONT3x5, WHITE)
    return img


def glyphs():
    items = [
        ("a", face("A", 5, 6)), ("b", face("B", 7, 8)), ("x", face("X", 9, 10)), ("y", face("Y", 11, 12)),
        ("lb", bumper("LB", False)), ("rb", bumper("RB", True)),
        ("lt", trigger("LT")), ("rt", trigger("RT")),
        ("view", view_btn()), ("menu", menu_btn()), ("guide", guide_btn()),
        ("ls", stick("L", True)), ("rs", stick("R", True)),
        ("l3", stick("L", False)), ("r3", stick("R", False)),
    ]
    dirs = ("up", "down", "left", "right")
    for mask in range(1, 16):
        lit = {d for bit, d in enumerate(dirs) if mask & (1 << bit)}
        items.append(("dpad%d" % mask, dpad(lit)))
    return items


def write_bmp(path, img):
    stride = (img.w + 3) & ~3
    pal = b"".join(struct.pack("<BBBB", b, g, r, 0) for r, g, b in PALETTE + [(0, 0, 0)] * (256 - len(PALETTE)))
    rows = b"".join(bytes(img.px[y]) + b"\0" * (stride - img.w) for y in reversed(range(img.h)))
    off = 14 + 40 + len(pal)
    hdr = struct.pack("<2sIHHI", b"BM", off + len(rows), 0, 0, off)
    info = struct.pack("<IiiHHIIiiII", 40, img.w, img.h, 1, 8, 0, len(rows), 2835, 2835, 256, 0)
    with open(path, "wb") as f:
        f.write(hdr + info + pal + rows)


def write_png(path, w, h, rgb):
    raw = b"".join(b"\0" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def preview(path, items, scale=6):
    pad = 4
    cellw = max(i.w for _, i in items) + pad
    cellh = max(i.h for _, i in items) + pad
    cols = 8
    rowsn = (len(items) + cols - 1) // cols
    w, h = cols * cellw * scale, rowsn * cellh * scale
    bg = (122, 88, 48)  # the wood of the original menus
    rgb = bytearray(bg * (w * h))
    for n, (_, img) in enumerate(items):
        ox = (n % cols) * cellw + pad // 2
        oy = (n // cols) * cellh + pad // 2
        for y in range(img.h):
            for x in range(img.w):
                c = img.px[y][x]
                if c == T:
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        p = (((oy + y) * scale + sy) * w + (ox + x) * scale + sx) * 3
                        rgb[p:p + 3] = bytes(PALETTE[c])
    write_png(path, w, h, rgb)


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    out = os.path.join(root, "data", "RTTR", "assets", "base", "pad_glyphs")
    os.makedirs(out, exist_ok=True)
    items = glyphs()
    for n, (name, img) in enumerate(items):
        write_bmp(os.path.join(out, "%03d.%s.bmp" % (n, name)), img)
    if "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1], items)


if __name__ == "__main__":
    main()
