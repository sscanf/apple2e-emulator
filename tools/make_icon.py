"""Build the macOS app icon (assets/icon/AppIcon.icns) from the monitor artwork.

    uvx --with pillow python tools/make_icon.py

The icon is the Apple Monitor II with a green prompt on its screen.
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

# "]" prompt and a block cursor, in big phosphor dots
green = (51, 255, 102, 255)
dot = 26
px, py = glass[0] + 110, glass[1] + 150
bracket = ["###", "..#", "..#", "..#", "..#", "..#", "###"]
for row, line in enumerate(bracket):
    for col, ch in enumerate(line):
        if ch == "#":
            x, y = px + col * dot, py + row * dot
            draw.rectangle((x, y, x + dot - 5, y + dot - 5), fill=green)
cx = px + 5 * dot
draw.rectangle((cx, py, cx + 4 * dot - 5, py + 7 * dot - 5), fill=green)

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
