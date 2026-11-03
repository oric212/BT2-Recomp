"""The programs' icons (assets/icons/): drawn here, no game artwork.

    python tools/build/make_icons.py [--preview <png>]

bt2-game.ico: a glossy orange ball with two red stars (Tenkaichi 2). bt2-launcher.ico: the same ball on a dark
rounded tile with an orange ring. Each holds 16, 24, 32, 48, 64, 128 and 256 pixel images, each drawn at four times
its size and reduced, so small sizes stay smooth.
"""
import argparse
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
SIZES = [16, 24, 32, 48, 64, 128, 256]


def star(cx, cy, r, points=5, inner=0.45, turn=-math.pi / 2):
    out = []
    for n in range(points * 2):
        radius = r if n % 2 == 0 else r * inner
        a = turn + n * math.pi / points
        out.append((cx + radius * math.cos(a), cy + radius * math.sin(a)))
    return out


def ball(size, scale=1.0, shadow=True):
    """The orange ball, `scale` of the image, centred, on a transparent background."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    r = size * 0.46 * scale
    c = size / 2
    if shadow:
        sh = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        ImageDraw.Draw(sh).ellipse((c - r, c - r + size * 0.03, c + r, c + r + size * 0.03), fill=(0, 0, 0, 110))
        img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(size * 0.025)))
    # The sphere: a radial gradient from a light upper left to a deep orange rim.
    body = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    px = body.load()
    lx, ly = c - r * 0.35, c - r * 0.4
    for y in range(int(c - r) - 1, int(c + r) + 2):
        for x in range(int(c - r) - 1, int(c + r) + 2):
            d = math.hypot(x + 0.5 - c, y + 0.5 - c)
            if d > r:
                continue
            t = min(1.0, math.hypot(x + 0.5 - lx, y + 0.5 - ly) / (r * 1.55))
            red = int(255 - 25 * t)
            green = int(205 - 120 * t)
            blue = int(80 - 70 * t)
            edge = max(0.0, (d - r * 0.86) / (r * 0.14))  # darker towards the rim
            red, green, blue = int(red * (1 - 0.25 * edge)), int(green * (1 - 0.35 * edge)), int(blue * (1 - 0.4 * edge))
            px[x, y] = (red, max(0, green), max(0, blue), 255)
    img = Image.alpha_composite(img, body)
    d = ImageDraw.Draw(img)
    # Two red stars, side by side and a little lower than the middle.
    for dx, dy in ((-0.34, 0.06), (0.30, -0.12)):
        sx, sy, sr = c + dx * r, c + dy * r, r * 0.27
        d.polygon(star(sx, sy, sr), fill=(200, 24, 24, 255))
        d.polygon(star(sx - sr * 0.08, sy - sr * 0.08, sr * 0.55), fill=(236, 60, 44, 255))
    # The gloss: a soft white highlight at the upper left and a thin rim light below.
    gloss = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    g = ImageDraw.Draw(gloss)
    g.ellipse((c - r * 0.56, c - r * 0.74, c - r * 0.12, c - r * 0.38), fill=(255, 255, 255, 130))
    g.arc((c - r * 0.9, c - r * 0.9, c + r * 0.9, c + r * 0.9), 25, 155, fill=(255, 240, 200, 90), width=max(1, int(r * 0.06)))
    img = Image.alpha_composite(img, gloss.filter(ImageFilter.GaussianBlur(size * 0.012)))
    return img


def tile(size):
    """The launcher: the ball on a dark blue rounded tile inside an orange ring."""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = size * 0.03
    d.rounded_rectangle((m, m, size - m, size - m), radius=size * 0.22, fill=(22, 30, 58, 255))
    # A faint vertical shade on the tile.
    shade = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    s = ImageDraw.Draw(shade)
    for y in range(size):
        s.line((0, y, size, y), fill=(60, 90, 160, int(70 * (1 - y / size))))
    mask = Image.new('L', (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((m, m, size - m, size - m), radius=size * 0.22, fill=255)
    img.paste(Image.alpha_composite(img, shade), (0, 0), mask)
    d = ImageDraw.Draw(img)
    ring = size * 0.12
    d.ellipse((ring, ring, size - ring, size - ring), outline=(255, 150, 30, 255), width=max(1, int(size * 0.035)))
    return Image.alpha_composite(img, ball(size, 0.72, shadow=False))


def images(draw):
    return [draw(s * 4).resize((s, s), Image.LANCZOS) for s in SIZES]


def save_ico(path, frames):
    path.parent.mkdir(parents=True, exist_ok=True)
    frames[-1].save(path, format='ICO', sizes=[(f.width, f.height) for f in frames], append_images=frames[:-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--preview', type=Path, help='also write both icons at every size side by side to this PNG')
    args = parser.parse_args()
    game, launcher = images(ball), images(tile)
    save_ico(ROOT / 'assets/icons/bt2-game.ico', game)
    save_ico(ROOT / 'assets/icons/bt2-launcher.ico', launcher)
    if args.preview:
        width = sum(SIZES) + 10 * len(SIZES)
        sheet = Image.new('RGBA', (width, 2 * 256 + 30), (235, 235, 235, 255))
        x = 5
        for g, l in zip(game, launcher):
            sheet.alpha_composite(g, (x, 256 - g.height))
            sheet.alpha_composite(l, (x, 2 * 256 + 20 - l.height))
            x += g.width + 10
        sheet.save(args.preview)


if __name__ == '__main__':
    main()
