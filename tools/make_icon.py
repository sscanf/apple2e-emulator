"""Build the macOS app icon (assets/icon/AppIcon.icns) from the monitor artwork.

    uvx --with pillow python tools/make_icon.py

The icon is the Apple Monitor II with "][" on its screen, striped in the six
colours of the 1977 Apple badge (the colours only, not the logo).
"""
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SIZE = 1024

bezel = Image.open(ROOT / "assets/monitor/monitor2.png").convert("RGBA")
scale = (SIZE - 64) / bezel.width
bezel = bezel.resize((round(bezel.width * scale), round(bezel.height * scale)), Image.LANCZOS)
left = (SIZE - bezel.width) // 2
top = (SIZE - bezel.height) // 2

icon = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(icon)

# The glass behind the bezel's opening (tube opening 41,40 608x432 of 690x512)
gx, gy, gw, gh = (round(v * scale * bezel.width / (690 * scale)) for v in (41, 40, 608, 432))
glass = (left + gx - 4, top + gy - 4, left + gx + gw + 4, top + gy + gh + 4)
draw.rounded_rectangle(glass, radius=40, fill=(11, 32, 26, 255))

# "][" filled with the badge's stripes, centred on the glass
stripes = [(97, 187, 70), (253, 184, 39), (245, 130, 31), (224, 58, 62), (150, 61, 151), (0, 157, 220)]
glyph = ["###.###", "..#.#..", "..#.#..", "..#.#..", "..#.#..", "###.###"]
cell = 62
width, height = len(glyph[0]) * cell, len(glyph) * cell
ox = (glass[0] + glass[2]) // 2 - width // 2
oy = (glass[1] + glass[3]) // 2 - height // 2
mask = Image.new("L", (SIZE, SIZE), 0)
mask_draw = ImageDraw.Draw(mask)
for row, line in enumerate(glyph):
    for col, ch in enumerate(line):
        if ch == "#":
            x, y = ox + col * cell, oy + row * cell
            mask_draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=255)
colours = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
colours_draw = ImageDraw.Draw(colours)
band = height / len(stripes)
for i, colour in enumerate(stripes):
    colours_draw.rectangle((ox, round(oy + i * band), ox + width, round(oy + (i + 1) * band) - 1), fill=colour + (255,))
icon.paste(colours, (0, 0), mask)

icon.alpha_composite(bezel, (left, top))

with tempfile.TemporaryDirectory() as tmp:
    iconset = Path(tmp) / "AppIcon.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        icon.resize((size, size), Image.LANCZOS).save(iconset / f"icon_{size}x{size}.png")
        icon.resize((size * 2, size * 2), Image.LANCZOS).save(iconset / f"icon_{size}x{size}@2x.png")
    out = ROOT / "assets/icon/AppIcon.icns"
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out)], check=True)
    icon.resize((256, 256), Image.LANCZOS).save(ROOT / "assets/icon/icon.png")
    print("wrote", out)
