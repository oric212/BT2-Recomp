import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).with_name('gif_dump_draws.py')


def giftag(nloop, nreg, regs, flg=0, eop=True):
    lo = nloop | (int(eop) << 15) | (flg << 58) | ((nreg & 15) << 60)
    hi = 0
    for n, reg in enumerate(regs):
        hi |= reg << (4 * n)
    return struct.pack('<QQ', lo, hi)


def ad(reg, value):
    return struct.pack('<QQ', value, reg)


def transfer(data):
    return b'T' + struct.pack('<I', len(data)) + data


class GifDumpDrawsTest(unittest.TestCase):
    def run_tool(self, dump, *args):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'dump.bin'
            path.write_bytes(dump)
            return subprocess.run([sys.executable, str(TOOL), str(path), *args], capture_output=True, text=True, check=True).stdout

    def test_sprite_copy_and_display(self):
        offset = (2048 << 4) | ((2048 << 4) << 32)
        state = [ad(0x4C, (0xE00 // 32) | (8 << 16)), ad(0x18, offset), ad(0x40, 511 << 16 | (447 << 48)),
                 ad(0x06, 0x1C00 | (8 << 14) | (0x1B << 20) | (9 << 26) | (9 << 30)), ad(0x00, 6 | (1 << 4) | (1 << 8))]
        vertices = []
        for x, y, u, v in ((-1, 0, 0, 0), (511, 448, 512, 448)):
            vertices += [ad(0x03, (u << 4) | ((v << 4) << 16)), ad(0x05, ((2048 + x) << 4) | (((2048 + y) << 4) << 16))]
        draw = giftag(len(state), 1, [0xE]) + b''.join(state)
        kick = giftag(len(vertices), 1, [0xE]) + b''.join(vertices)
        copy = [ad(0x50, (0x2A00 << 32) | (4 << 48)), ad(0x51, 0), ad(0x52, 256 | (224 << 32)), ad(0x53, 2)]
        copy_packet = giftag(len(copy), 1, [0xE]) + b''.join(copy)
        dump = (transfer(draw) + transfer(kick) + transfer(copy_packet)
                + b'P' + struct.pack('<IQ', 7, (0xE00 // 32) | (8 << 9)) + b'F' + struct.pack('<I', 800))
        out = self.run_tool(dump)
        self.assertIn('flip 800', out)
        self.assertIn('DISPFB1: fbp 0xe00 w8 CT32', out)
        self.assertIn('draw sprite x2 ctx0 rect -1.00,0.00..511.00,448.00 uv 0.00,0.00..512.00,448.00 fst until 2:0', out)
        self.assertIn('FRAME 0xe00 w8 CT32', out)
        self.assertIn('TEX 0x1c00 w8 T8H 512x512', out)
        self.assertIn('copy local->local src 0x0 w0 CT32 at 0,0 -> dst 0x2a00 w4 CT32 at 0,0 size 256x224', out)

    def test_touching_y_filters_draws(self):
        state = [ad(0x4C, 0xE00 // 32), ad(0x00, 6)]
        vertices = [ad(0x05, (10 << 4) | ((10 << 4) << 16)), ad(0x05, (20 << 4) | ((20 << 4) << 16))]
        packet = giftag(len(state + vertices), 1, [0xE]) + b''.join(state + vertices)
        dump = transfer(packet) + b'F' + struct.pack('<I', 1)
        self.assertIn('draw sprite', self.run_tool(dump))
        self.assertNotIn('draw sprite', self.run_tool(dump, '--touching-y', '447'))

    def test_area_counts_triangles_and_sprites(self):
        # A sprite of 10 x 10 pixels, then a strip of two triangles over a 4 x 2 rectangle, then a triangle half outside
        # the scissor (x 0..63): 32 x 32 of its 64 x 32 counted.
        def xy(x, y):
            return ad(0x05, (x << 4) | ((y << 4) << 16))
        writes = [ad(0x40, 63 << 16 | (63 << 48)), ad(0x4C, 0xE00 // 32), ad(0x00, 6), xy(10, 10), xy(20, 20),
                  ad(0x00, 4), xy(0, 0), xy(4, 0), xy(0, 2), xy(4, 2),
                  ad(0x00, 3), xy(32, 0), xy(96, 0), xy(32, 64)]
        dump = transfer(giftag(len(writes), 1, [0xE]) + b''.join(writes)) + b'F' + struct.pack('<I', 1)
        out = self.run_tool(dump, '--area')
        self.assertRegex(out, r'draw sprite x2 .* area 100\n')
        self.assertRegex(out, r'draw tristrip x4 .* area 8\n')
        self.assertRegex(out, r'draw tri x3 .* area 1536')
        self.assertNotIn('area', self.run_tool(dump))

    def test_registers_and_upload_words(self):
        # A host->local upload of two qwords (a palette's first eight words), then a sprite with its registers.
        upload = [ad(0x50, (0x3E8C << 32)), ad(0x51, 0), ad(0x52, 8 | (1 << 32)), ad(0x53, 0)]
        words = struct.pack('<8I', *range(0x80000000, 0x80000008))
        state = [ad(0x4C, 0xE00 // 32), ad(0x46, 1), ad(0x01, 0x80505050), ad(0x00, 6 | (1 << 6))]
        vertices = [ad(0x05, (10 << 4) | ((10 << 4) << 16)), ad(0x05, (20 << 4) | ((20 << 4) << 16))]
        dump = transfer(giftag(len(upload), 1, [0xE]) + b''.join(upload) + giftag(2, 0, [], flg=2) + words
                        + giftag(len(state + vertices), 1, [0xE]) + b''.join(state + vertices)) + b'F' + struct.pack('<I', 1)
        out = self.run_tool(dump, '--registers')
        self.assertIn('data 80000000 80000001 80000002 80000003 80000004 80000005 80000006 80000007', out)
        self.assertIn('PRIM 0x46 RGBAQ 0x80505050', out)
        self.assertIn('COLCLAMP 0x1', out)
        self.assertNotIn('RGBAQ', self.run_tool(dump))


if __name__ == '__main__':
    unittest.main()
