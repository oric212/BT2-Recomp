"""Render the displayed framebuffer of the owned GS memory model to a PNG (local evidence only).

Reads `<report>.gs.bin` (per 32-bit GS word: value, then known-bit mask) written by the entry
probe, and the report's DISPFB/DISPLAY registers. PSMCT32/PSMCT24 framebuffers are decoded with
the public GS page/block/pixel layout. Pixels never written by the model are shown as a magenta
checker so missing coverage is visible. Output belongs under ignored `work/`.
"""
import argparse
import json
import struct
from pathlib import Path


def ct32_word(bp, bw, x, y):
    bx, by = (x % 64) // 8, (y % 32) // 8
    block = (bx & 1) | ((by & 1) << 1) | ((bx & 2) << 1) | ((by & 2) << 2) | ((bx & 4) << 2)
    px, py = x % 8, y % 8
    pixel = (px & 1) | ((py & 1) << 1) | ((px & 6) << 1) | ((py & 6) << 3)
    return (bp * 64 + ((y // 32) * bw + x // 64) * 2048 + block * 64 + pixel) % (1024 * 1024)


def display_area(dispfb, display):
    fbp, fbw, psm = dispfb & 511, (dispfb >> 9) & 63, (dispfb >> 15) & 31
    dbx, dby = (dispfb >> 32) & 2047, (dispfb >> 43) & 2047
    magh = (display >> 23) & 15
    width = (((display >> 32) & 4095) + 1) // (magh + 1)
    height = ((display >> 44) & 2047) + 1
    return fbp, fbw, psm, dbx, dby, width, height


def render(words, fbp, fbw, psm, dbx, dby, width, height):
    if psm not in (0, 1):
        raise ValueError(f'Unsupported display PSM {psm:#x}')
    rgb = bytearray(width * height * 3)
    unknown = 0
    for y in range(height):
        for x in range(width):
            index = ct32_word(fbp * 32, fbw, dbx + x, dby + y)
            value, known = words[index]
            o = (y * width + x) * 3
            if known & 0xFFFFFF != 0xFFFFFF:
                unknown += 1
                rgb[o:o + 3] = b'\xff\x00\xff' if ((x // 8) ^ (y // 8)) & 1 else b'\x40\x00\x40'
            else:
                rgb[o:o + 3] = bytes((value & 255, (value >> 8) & 255, (value >> 16) & 255))
    return rgb, unknown


def load_words(path):
    data = Path(path).read_bytes()
    if len(data) != 8 * 1024 * 1024:
        raise ValueError('GS dump must hold 1,048,576 value/known word pairs')
    return list(struct.iter_unpack('<II', data))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--native', type=Path, required=True, help='entry-stop.json (with .gs.bin beside it)')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--circuit', type=int, choices=(1, 2), default=2)
    a = p.parse_args()
    r = json.loads(a.native.read_text(encoding='utf-8'))
    area = display_area(r[f'gs_dispfb{a.circuit}'], r[f'gs_display{a.circuit}'])
    rgb, unknown = render(load_words(str(a.native) + '.gs.bin'), *area)
    from PIL import Image
    a.output.parent.mkdir(parents=True, exist_ok=True)
    Image.frombytes('RGB', (area[5], area[6]), bytes(rgb)).save(a.output)
    print(f'Rendered {area[5]}x{area[6]} from FBP {area[0]} FBW {area[1]} PSM {area[2]}; {unknown} unwritten pixels -> {a.output}')


if __name__ == '__main__':
    main()
