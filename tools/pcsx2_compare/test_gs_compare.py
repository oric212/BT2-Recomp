import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gs_compare as gc  # noqa: E402
import gs_memory as gm  # noqa: E402


class GsCompareTest(unittest.TestCase):
    def test_regions_from_frame_and_z(self):
        named, (fbp, fbw) = gc.regions([{'FRAME': 0x80070, 'ZBUF': 0x310000E0}, {}])
        self.assertEqual((fbp, fbw), (0x70, 8))
        self.assertEqual((named['frame'].start, named['frame'].stop), (0x70, 0xE0))       # 448 lines: 14 page rows of 8
        self.assertEqual((named['Z buffer'].start, named['Z buffer'].stop), (0xE0, 0x150))
        self.assertEqual(len(named['the rest']), 512 - 2 * 112)

    def test_counts_changes_and_pixel_differences(self):
        words = gm.VRAM_SIZE // 4
        before = np.zeros(words, dtype='<u4')
        pcsx2, port = before.copy(), before.copy()
        index = gm.ct32_words(0x70, 8, 512, 448)
        pcsx2[index[0, 0]] = 0x10101010
        port[index[0, 0]] = 0x11101010                     # one level off in alpha
        port[index[1, 1]] = 0x00000040                     # a pixel only the port drew
        pcsx2[400 * 2048] = port[400 * 2048] = 7           # the same texture word in both
        result, _, _, _ = gc.compare(before, pcsx2, port, [{'FRAME': 0x80070, 'ZBUF': 0x310000E0}, {}])
        self.assertEqual(result['frame'], {'pcsx2_changed': 1, 'port_changed': 2, 'differ': 2})
        self.assertEqual(result['the rest'], {'pcsx2_changed': 1, 'port_changed': 1, 'differ': 0})
        self.assertEqual((result['pixels']['by 1'], result['pixels']['by more than 16'], result['pixels']['largest']), (1, 1, 64))


if __name__ == '__main__':
    unittest.main()
