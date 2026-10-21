#!/usr/bin/env python3
"""Draw Resources/icon_60x60.png and icon_30x30.png in the UNA launcher style:
a flat teal glyph on transparency. The glyph is an interval ring (a timer
drained to 7/8) around a bold "7"."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FONT = os.path.join(ROOT, "Software/Apps/LVGL-GUI/assets/ttf/Poppins-SemiBold.ttf")
TEAL = (0, 128, 128, 255)


def icon(size):
    s = 16                      # supersample
    N = size * s
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pad = N * 0.06
    w = N * 0.13
    box = [pad, pad, N - pad, N - pad]
    # PIL angles: 0 = 3 o'clock, clockwise. Ring from 12 o'clock, 315 degrees.
    d.arc(box, start=-90, end=-90 + 315, fill=TEAL, width=int(w))
    # Rounded ends
    import math
    r = (N - 2 * pad - w) / 2
    cx = cy = N / 2
    for a in (-90, -90 + 315):
        x = cx + r * math.cos(math.radians(a)); y = cy + r * math.sin(math.radians(a))
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=TEAL)
    font = ImageFont.truetype(FONT, int(N * 0.62))
    bb = d.textbbox((0, 0), "7", font=font)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    d.text((cx - tw / 2 - bb[0], cy - th / 2 - bb[1]), "7", font=font, fill=TEAL)
    return im.resize((size, size), Image.LANCZOS)

out = os.path.join(ROOT, "Resources")
for sz in (60, 30):
    icon(sz).save(os.path.join(out, f"icon_{sz}x{sz}.png"))
icon(256).save(os.path.join(out, "icon.png"))   # store listing
print("icons written to", out)
