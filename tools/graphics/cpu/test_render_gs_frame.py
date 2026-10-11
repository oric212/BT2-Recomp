import unittest

from render_gs_frame import ct32_word, display_area, render


class RenderGsFrameTests(unittest.TestCase):
    def test_ct32_layout(self):
        # Block order within a 64x32 page and pixel order within an 8x8 block.
        self.assertEqual(ct32_word(0, 1, 0, 0), 0)
        self.assertEqual(ct32_word(0, 1, 1, 0), 1)
        self.assertEqual(ct32_word(0, 1, 0, 1), 2)
        self.assertEqual(ct32_word(0, 1, 2, 0), 4)
        self.assertEqual(ct32_word(0, 1, 8, 0), 64)
        self.assertEqual(ct32_word(0, 1, 16, 0), 4 * 64)
        self.assertEqual(ct32_word(0, 1, 0, 8), 2 * 64)
        self.assertEqual(ct32_word(0, 2, 64, 0), 2048)
        self.assertEqual(ct32_word(0, 2, 0, 32), 2 * 2048)
        self.assertEqual(len({ct32_word(0, 1, x, y) for x in range(64) for y in range(32)}), 2048)

    def test_display_area(self):
        # 512x448: DISPFB FBW 8, DISPLAY DW 2559 with MAGH 4, DH 447.
        dispfb = 8 << 9
        display = (4 << 23) | (2559 << 32) | (447 << 44)
        self.assertEqual(display_area(dispfb, display), (0, 8, 0, 0, 0, 512, 448))

    def test_render_marks_unwritten(self):
        words = [(0, 0)] * (1024 * 1024)
        words[ct32_word(0, 1, 1, 0)] = (0x00332211, 0xFFFFFFFF)
        rgb, unknown = render(words, 0, 1, 0, 0, 0, 2, 1)
        self.assertEqual(unknown, 1)
        self.assertEqual(bytes(rgb[3:6]), b'\x11\x22\x33')


if __name__ == '__main__':
    unittest.main()
