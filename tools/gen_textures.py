#!/usr/bin/env python3
"""Generate the original part textures used by Nuts & Bolts 64.

Every texture here is drawn procedurally by this script, so the repository
ships no ripped game art. Output is 32x32 PNGs in assets/textures/, sized for
the N64's 4 KB texture memory. png2n64.py turns them (or any replacement PNG
you drop in with the same name, at any resolution) into RGBA16 C arrays.

Paintable materials are drawn in light, mostly neutral tones because the game
tints them with the part's paint colour at runtime.
"""
import math
import os
import random
import sys

from PIL import Image, ImageDraw

S = 32
OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "textures")


def clamp(v):
    return max(0, min(255, int(v)))


def new(color):
    return Image.new("RGBA", (S, S), color)


def noise(img, amount, seed):
    rnd = random.Random(seed)
    px = img.load()
    for y in range(S):
        for x in range(S):
            r, g, b, a = px[x, y]
            n = rnd.uniform(-amount, amount)
            px[x, y] = (clamp(r + n), clamp(g + n), clamp(b + n), a)
    return img


def wood():
    img = new((214, 178, 128, 255))
    px = img.load()
    rnd = random.Random(1)
    for y in range(S):
        plank = y // 8
        off = rnd.random() * 6
        for x in range(S):
            grain = math.sin((x + off * 7) * 0.45 + math.sin(y * 0.9 + plank) * 1.6) * 14
            base = 205 - plank % 2 * 14
            px[x, y] = (clamp(base + grain), clamp(base * 0.8 + grain * 0.8), clamp(base * 0.55 + grain * 0.5), 255)
        if y % 8 == 7:
            for x in range(S):
                px[x, y] = (120, 84, 50, 255)
    for plank in range(4):
        x = (plank * 11 + 5) % S
        for yy in range(plank * 8, plank * 8 + 7):
            px[x, yy] = (140, 100, 62, 255)
    for (cx, cy) in [(3, 3), (28, 3), (3, 19), (28, 19), (16, 11), (16, 27)]:
        px[cx, cy] = (90, 70, 50, 255)
    return noise(img, 6, 11)


def metal():
    img = new((196, 200, 206, 255))
    d = ImageDraw.Draw(img)
    for y in range(S):
        shade = 196 + int(10 * math.sin(y * 0.35))
        d.line([(0, y), (S - 1, y)], fill=(shade, shade + 3, shade + 8, 255))
    d.rectangle([0, 0, S - 1, S - 1], outline=(120, 124, 132, 255))
    d.rectangle([1, 1, S - 2, S - 2], outline=(228, 230, 236, 255))
    for (cx, cy) in [(4, 4), (27, 4), (4, 27), (27, 27), (16, 4), (16, 27), (4, 16), (27, 16)]:
        d.ellipse([cx - 1, cy - 1, cx + 1, cy + 1], fill=(140, 144, 150, 255))
        img.putpixel((cx - 1, cy - 1), (245, 245, 250, 255))
    return noise(img, 5, 12)


def tire():
    img = new((44, 42, 44, 255))
    d = ImageDraw.Draw(img)
    for i in range(0, S, 8):
        d.polygon([(i, 0), (i + 4, 0), (i + 8, 16), (i + 4, 16)], fill=(26, 25, 27, 255))
        d.polygon([(i + 8, 16), (i + 4, 16), (i, 32), (i + 4, 32)], fill=(26, 25, 27, 255))
    d.line([(0, 0), (S, 0)], fill=(70, 68, 70, 255))
    return noise(img, 7, 13)


def hub():
    img = new((210, 210, 214, 255))
    d = ImageDraw.Draw(img)
    c = S / 2 - 0.5
    for r, col in [(15, (150, 152, 158)), (13, (228, 228, 232)), (8, (186, 188, 194)), (3, (120, 122, 128))]:
        d.ellipse([c - r, c - r, c + r, c + r], fill=col + (255,))
    for k in range(5):
        a = k * 2 * math.pi / 5
        x, y = c + math.cos(a) * 5.5, c + math.sin(a) * 5.5
        d.ellipse([x - 1.2, y - 1.2, x + 1.2, y + 1.2], fill=(90, 92, 98, 255))
    return noise(img, 4, 14)


def engine():
    img = new((70, 72, 78, 255))
    d = ImageDraw.Draw(img)
    for y in range(2, S, 4):
        d.line([(2, y), (S - 3, y)], fill=(150, 154, 162, 255))
        d.line([(2, y + 1), (S - 3, y + 1)], fill=(40, 42, 46, 255))
    d.rectangle([0, 0, S - 1, S - 1], outline=(30, 30, 34, 255))
    d.rectangle([12, 12, 19, 19], fill=(200, 60, 40, 255), outline=(90, 30, 20, 255))
    return noise(img, 5, 15)


def fuel():
    img = new((230, 230, 230, 255))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 11, S - 1, 20], fill=(250, 250, 250, 255))
    d.rectangle([0, 10, S - 1, 10], fill=(150, 150, 150, 255))
    d.rectangle([0, 21, S - 1, 21], fill=(150, 150, 150, 255))
    # droplet emblem
    d.polygon([(16, 12), (12, 17), (16, 20), (20, 17)], fill=(40, 40, 40, 255))
    d.line([(4, 3), (27, 28)], fill=(190, 190, 190, 255), width=2)
    d.line([(27, 3), (4, 28)], fill=(190, 190, 190, 255), width=2)
    return noise(img, 4, 16)


def seat():
    img = new((215, 215, 215, 255))
    d = ImageDraw.Draw(img)
    for x in (0, 10, 21, 31):
        d.line([(x, 0), (x, S - 1)], fill=(150, 150, 150, 255))
    for x in range(1, S - 1):
        if x % 10 in (1, 2, 8, 9):
            for y in range(0, S, 4):
                img.putpixel((x, y), (175, 175, 175, 255))
    for y in range(S):
        for x in range(S):
            r, g, b, a = img.getpixel((x, y))
            sh = 12 * math.cos((x % 10.5) / 10.5 * math.pi * 2)
            img.putpixel((x, y), (clamp(r + sh), clamp(g + sh), clamp(b + sh), 255))
    return noise(img, 4, 17)


def canvas():
    img = new((236, 232, 220, 255))
    d = ImageDraw.Draw(img)
    for x in range(0, S, 8):
        d.line([(x, 0), (x, S - 1)], fill=(180, 176, 166, 255))
    for y in range(S):
        for x in range(S):
            if (x + y) % 2 == 0:
                r, g, b, a = img.getpixel((x, y))
                img.putpixel((x, y), (clamp(r - 8), clamp(g - 8), clamp(b - 8), 255))
    return noise(img, 3, 18)


def glass():
    img = new((150, 200, 230, 255))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, S - 1, S - 1], outline=(70, 90, 110, 255))
    d.line([(4, 22), (22, 4)], fill=(230, 245, 255, 255), width=3)
    d.line([(10, 26), (26, 10)], fill=(200, 230, 250, 255), width=1)
    return noise(img, 3, 19)


def honeycomb():
    img = new((150, 90, 10, 255))
    d = ImageDraw.Draw(img)
    r = 5.0
    w = math.sqrt(3) * r
    for row in range(-1, 6):
        for col in range(-1, 6):
            cx = col * w + (row % 2) * w / 2
            cy = row * 1.5 * r
            pts = [(cx + r * math.cos(math.pi / 6 + k * math.pi / 3), cy + r * math.sin(math.pi / 6 + k * math.pi / 3)) for k in range(6)]
            d.polygon(pts, fill=(240, 184, 40, 255), outline=(170, 100, 10, 255))
    return noise(img, 6, 20)


def jiggy():
    img = new((236, 196, 60, 255))
    d = ImageDraw.Draw(img)
    d.line([(16, 0), (16, 9)], fill=(160, 110, 20, 255))
    d.ellipse([11, 9, 21, 19], outline=(160, 110, 20, 255))
    d.line([(16, 19), (16, 31)], fill=(160, 110, 20, 255))
    d.line([(0, 16), (9, 16)], fill=(160, 110, 20, 255))
    d.line([(22, 16), (31, 16)], fill=(160, 110, 20, 255))
    d.line([(3, 3), (8, 8)], fill=(255, 240, 160, 255))
    return noise(img, 6, 21)


def checker():
    img = new((240, 240, 240, 255))
    d = ImageDraw.Draw(img)
    for y in range(0, S, 8):
        for x in range(0, S, 8):
            if (x // 8 + y // 8) % 2:
                d.rectangle([x, y, x + 7, y + 7], fill=(40, 40, 44, 255))
    return noise(img, 3, 22)


def floater():
    img = new((240, 240, 240, 255))
    d = ImageDraw.Draw(img)
    for x in range(0, S, 16):
        d.rectangle([x, 0, x + 7, S - 1], fill=(220, 50, 40, 255))
    d.line([(0, 6), (S, 6)], fill=(255, 255, 255, 255))
    return noise(img, 4, 23)


def jet():
    img = new((60, 60, 66, 255))
    d = ImageDraw.Draw(img)
    c = S / 2 - 0.5
    for r, col in [(15, (110, 112, 120)), (12, (70, 70, 76)), (9, (255, 160, 40)), (6, (255, 230, 120)), (3, (255, 255, 230))]:
        d.ellipse([c - r, c - r, c + r, c + r], fill=col + (255,))
    return noise(img, 4, 24)


def hazard():
    img = new((240, 200, 30, 255))
    d = ImageDraw.Draw(img)
    for i in range(-S, S * 2, 10):
        d.polygon([(i, 0), (i + 5, 0), (i + 5 - S, S), (i - S, S)], fill=(30, 30, 30, 255))
    return noise(img, 4, 25)


def lamp():
    img = new((250, 240, 190, 255))
    d = ImageDraw.Draw(img)
    c = S / 2 - 0.5
    for r, col in [(15, (120, 120, 120)), (13, (255, 250, 210)), (8, (255, 255, 240)), (3, (255, 255, 255))]:
        d.ellipse([c - r, c - r, c + r, c + r], fill=col + (255,))
    d.arc([4, 4, 27, 27], 200, 250, fill=(255, 255, 255, 255), width=2)
    return img


def balloon():
    img = new((235, 235, 235, 255))
    d = ImageDraw.Draw(img)
    d.ellipse([6, 4, 14, 10], fill=(255, 255, 255, 255))
    for y in range(S):
        for x in range(S):
            r, g, b, a = img.getpixel((x, y))
            sh = -26 * (y / S)
            img.putpixel((x, y), (clamp(r + sh), clamp(g + sh), clamp(b + sh), 255))
    return img


def plain():
    return noise(new((235, 235, 235, 255)), 4, 26)


def skull():
    img = new((235, 230, 215, 255))
    d = ImageDraw.Draw(img)
    d.ellipse([7, 5, 24, 22], fill=(250, 248, 238, 255), outline=(150, 140, 120, 255))
    d.rectangle([11, 20, 20, 27], fill=(250, 248, 238, 255), outline=(150, 140, 120, 255))
    d.ellipse([10, 11, 14, 15], fill=(40, 30, 30, 255))
    d.ellipse([17, 11, 21, 15], fill=(40, 30, 30, 255))
    for x in (13, 15, 17):
        d.line([(x, 22), (x, 26)], fill=(120, 110, 100, 255))
    return img


def note():
    img = new((250, 200, 40, 255))
    d = ImageDraw.Draw(img)
    d.ellipse([7, 19, 15, 26], fill=(230, 120, 20, 255))
    d.ellipse([18, 16, 26, 23], fill=(230, 120, 20, 255))
    d.line([(14, 22), (14, 6)], fill=(230, 120, 20, 255), width=2)
    d.line([(25, 19), (25, 4)], fill=(230, 120, 20, 255), width=2)
    d.line([(14, 6), (25, 4)], fill=(230, 120, 20, 255), width=3)
    return img


def grille():
    img = new((160, 162, 170, 255))
    d = ImageDraw.Draw(img)
    for x in range(3, S, 5):
        d.rectangle([x, 3, x + 2, S - 4], fill=(30, 30, 34, 255))
    d.rectangle([0, 0, S - 1, S - 1], outline=(220, 222, 228, 255))
    return noise(img, 4, 27)


TEXTURES = [
    ("wood", wood), ("metal", metal), ("tire", tire), ("hub", hub),
    ("engine", engine), ("fuel", fuel), ("seat", seat), ("canvas", canvas),
    ("glass", glass), ("honeycomb", honeycomb), ("jiggy", jiggy), ("checker", checker),
    ("floater", floater), ("jet", jet), ("hazard", hazard), ("lamp", lamp),
    ("balloon", balloon), ("plain", plain), ("skull", skull), ("note", note),
    ("grille", grille),
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else OUT
    os.makedirs(out, exist_ok=True)
    for name, fn in TEXTURES:
        fn().save(os.path.join(out, name + ".png"))
    sheet = Image.new("RGBA", (S * 7 * 4, S * 3 * 4), (0, 0, 0, 255))
    for i, (name, fn) in enumerate(TEXTURES):
        tile = fn().resize((S * 4, S * 4), Image.NEAREST)
        sheet.paste(tile, ((i % 7) * S * 4, (i // 7) * S * 4))
    sheet.save(os.path.join(out, "..", "texture_sheet.png"))
    print("wrote %d textures to %s" % (len(TEXTURES), os.path.abspath(out)))


if __name__ == "__main__":
    main()
