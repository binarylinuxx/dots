"""Draws runix-m3.png, blxshell's default wallpaper.

Material 3 Expressive shapes (a scalloped cookie, a clover, a soft burst, a
pill and dots) in tonal greens from the Runix palette, on a flat
near-black surface. The scheme blxshell builds from it with matugen comes out green too.

    python3 generate.py [output.png] [width] [height]
"""

import math
import sys

from PIL import Image, ImageDraw

WIDTH = int(sys.argv[2]) if len(sys.argv) > 2 else 3840
HEIGHT = int(sys.argv[3]) if len(sys.argv) > 3 else 2160
OUTPUT = sys.argv[1] if len(sys.argv) > 1 else "runix-m3.png"
SCALE = 2  # drawn at twice the size, then reduced: smooth edges

SURFACE = (14, 17, 16)
CONTAINER = (16, 52, 34)
PRIMARY_DARK = (22, 88, 55)
PRIMARY = (0, 163, 90)
PRIMARY_LIGHT = (126, 214, 160)
TERTIARY = (63, 181, 165)
TERTIARY_DARK = (24, 74, 70)


def polar(cx, cy, radius, wave, rotation=0.0, steps=720):
    """A closed outline whose radius follows `wave(angle)`."""
    points = []
    for step in range(steps):
        angle = 2 * math.pi * step / steps
        r = radius * wave(angle)
        points.append((cx + r * math.cos(angle + rotation), cy + r * math.sin(angle + rotation)))
    return points


def cookie(lobes, depth):
    return lambda angle: 1 + depth * math.cos(lobes * angle)


def main():
    w, h = WIDTH * SCALE, HEIGHT * SCALE
    unit = h / 2160

    image = Image.new("RGB", (w, h), SURFACE)

    draw = ImageDraw.Draw(image)

    # The big scalloped cookie: nine lobes, Material's "cookie9sided".
    draw.polygon(polar(0.30 * w, 0.62 * h, 520 * unit, cookie(9, 0.075), 0.2), fill=CONTAINER)
    draw.polygon(polar(0.30 * w, 0.62 * h, 380 * unit, cookie(9, 0.075), 0.2), fill=PRIMARY_DARK)

    # A four-leaf clover, upper right.
    draw.polygon(polar(0.76 * w, 0.30 * h, 360 * unit, cookie(4, 0.20), math.pi / 4), fill=TERTIARY_DARK)

    # A soft twelve-point burst, small and bright.
    draw.polygon(polar(0.60 * w, 0.70 * h, 150 * unit, cookie(12, 0.09)), fill=PRIMARY)

    # A pill, tilted, crossing toward the clover.
    pill = Image.new("L", (w, h), 0)
    pill_draw = ImageDraw.Draw(pill)
    length, thickness = 900 * unit, 220 * unit
    cx, cy = 0.62 * w, 0.40 * h
    pill_draw.rounded_rectangle(
        [cx - length / 2, cy - thickness / 2, cx + length / 2, cy + thickness / 2],
        radius=thickness / 2, fill=255)
    pill = pill.rotate(-28, center=(cx, cy), resample=Image.BICUBIC)
    image.paste(Image.new("RGB", (w, h), PRIMARY_LIGHT), mask=pill)
    draw = ImageDraw.Draw(image)

    # A few dots, like the ones Material puts beside its shapes.
    for x, y, r, colour in [
        (0.47 * w, 0.22 * h, 46, TERTIARY),
        (0.86 * w, 0.66 * h, 70, PRIMARY_DARK),
        (0.90 * w, 0.78 * h, 30, PRIMARY_LIGHT),
        (0.12 * w, 0.20 * h, 38, TERTIARY_DARK),
    ]:
        r *= unit
        draw.ellipse([x - r, y - r, x + r, y + r], fill=colour)

    image.resize((WIDTH, HEIGHT), Image.LANCZOS).save(OUTPUT, optimize=True)


main()
