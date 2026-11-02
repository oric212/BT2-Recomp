"""The beam struggle prompt for the setting "Beam struggles by buttons" (runtime/ee/struggle_buttons.h).

In a beam struggle the game shows an analog stick turning (six 64 x 64 frames). With the setting on, the port draws
these frames as the one face button that pushes, Square (Xbox art: X), pressed and released in turn, so the prompt
says "mash". The art is drawn here from simple shapes (the project's own) and written as DDS (BC3, 128 x 128) under the
frames' texture names into assets/struggle-buttons/<playstation|xbox>/, which the HD texture loader reads as
overrides (gs/gpu/hd_texture_pack.h). The files are kept in the repository; run this again only to change the art.

Usage: python tools/graphics/textures/make_struggle_button_art.py [--preview <png>]
"""
import argparse
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'assets' / 'struggle-buttons'
SIZE = 128

# The six frames of the turning stick (texture-palette-format), found 2026-11-03 in a scripted struggle
# (docs/plans/input/BEAM_STRUGGLE_PLAN.md). Even frames show the button pressed, odd frames released.
FRAMES = [
    '658ad09def435ebd-2576f11447071541-00001994',
    '891250f8aecfec1b-2deefa0ca5cb0527-00001994',
    '9a2db293ac7c3b26-dbafe9ac748d80f7-00001994',
    'cf3735dad773325-a70434ad74007420-00001994',
    'd0953ceb78590d2d-28268e4833ec4904-00001994',
    'e934beb3810b920e-67e28298d4cf5049-00001994',
]

SQUARE_PINK = (236, 140, 190)
XBOX_BLUE = (60, 130, 230)


def symbol(draw, kind, cx, cy, r, colour):
    w = max(3, r // 5)
    if kind == 'square':
        s = r * 0.55
        draw.rectangle([cx - s, cy - s, cx + s, cy + s], outline=colour, width=w)
    elif kind == 'letter-x':
        s = r * 0.5
        draw.line([cx - s, cy - s, cx + s, cy + s], fill=(255, 255, 255), width=w)
        draw.line([cx - s, cy + s, cx + s, cy - s], fill=(255, 255, 255), width=w)


def button(draw, kind, colour, face, cx, cy, r, pressed):
    """A round face button seen slightly from above; pressed, it sits lower and darker."""
    drop = r // 5 if pressed else 0
    draw.ellipse([cx - r, cy - r + r // 3, cx + r, cy + r + r // 3], fill=(25, 25, 30, 255))  # Socket.
    top = (40, 40, 48) if face is None else tuple(c // 2 for c in face)
    if pressed:
        top = tuple(int(c * 0.7) for c in top)
    draw.ellipse([cx - r, cy - r + drop, cx + r, cy + r + drop], fill=top + (255,), outline=(90, 90, 100, 255), width=2)
    symbol(draw, kind, cx, cy + drop, r, colour if face is None else (255, 255, 255))


def frame(layout, pressed):
    image = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    kind, colour, face = layout
    button(ImageDraw.Draw(image), kind, colour, face, SIZE * 0.5, SIZE * 0.44, int(SIZE * 0.3), pressed)
    return image


LAYOUTS = {
    'playstation': ('square', SQUARE_PINK, None),
    'xbox': ('letter-x', None, XBOX_BLUE),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--preview', type=Path, help='Also write all frames side by side as a PNG.')
    args = parser.parse_args()
    previews = []
    for art, layout in LAYOUTS.items():
        folder = OUT / art
        folder.mkdir(parents=True, exist_ok=True)
        for n, name in enumerate(FRAMES):
            image = frame(layout, n % 2 == 0)
            image.save(folder / f'{name}.dds', pixel_format='DXT5')
            previews.append(image)
    if args.preview:
        sheet = Image.new('RGBA', (SIZE * len(FRAMES), SIZE * len(LAYOUTS)), (70, 70, 70, 255))
        for i, image in enumerate(previews):
            sheet.alpha_composite(image, ((i % len(FRAMES)) * SIZE, (i // len(FRAMES)) * SIZE))
        sheet.save(args.preview)
    print(f'{len(previews)} frames written to {OUT}')


if __name__ == '__main__':
    main()
