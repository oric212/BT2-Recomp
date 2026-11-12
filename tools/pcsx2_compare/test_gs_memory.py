import struct
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gs_memory as gm  # noqa: E402


def freeze(vram, frame=0x80070):
    """A version-9 GS.bin with the given GS memory and FRAME_1."""
    data = bytearray(gm.VRAM_OFFSET + gm.VRAM_SIZE + 85)
    struct.pack_into('<I', data, 0, gm.VERSION)
    struct.pack_into('<Q', data, gm.CONTEXTS + 8 * gm.CONTEXT_REGISTERS.index('FRAME'), frame)
    struct.pack_into('<Q', data, gm.CONTEXTS + 8 * gm.CONTEXT_REGISTERS.index('TEX0'), (7 << 61) | 0x1234)
    data[gm.VRAM_OFFSET:gm.VRAM_OFFSET + gm.VRAM_SIZE] = vram
    return bytes(data)


class GsMemoryTest(unittest.TestCase):
    def test_the_whole_memory_is_one_picture(self):
        words = gm.ct32_words(0, 8, 512, 2048).ravel()
        self.assertEqual(np.unique(words).size, gm.VRAM_SIZE // 4)

    def test_ct32_layout(self):
        # The first page: block 1 is right of block 0, block 2 below it; in a block, word 2 starts the second row.
        words = gm.ct32_words(0, 1, 64, 32)
        self.assertEqual((words[0, 0], words[0, 1], words[1, 0], words[0, 8], words[8, 0]), (0, 1, 2, 64, 128))
        self.assertEqual(gm.ct32_words(0, 2, 128, 32)[0, 64], 2048)                 # the next page

    def test_setup_uploads_memory_and_writes_registers(self):
        vram = np.arange(gm.VRAM_SIZE // 4, dtype='<u4').tobytes()
        packets = gm.setup_packets(freeze(vram))
        _, contexts, _ = gm.read_freeze(freeze(vram))
        self.assertEqual(gm.frame_register(contexts[0]['FRAME']), (0x70, 8, 0))
        self.assertEqual(struct.unpack_from('<QQ', packets, 16), (8 << 48, 0x50))     # BITBLTBUF: DBW 8 (upper half)
        image = packets[16 * 5:]                                                      # after the first A+D (4 writes)
        first_tag, = struct.unpack_from('<Q', image, 0)
        self.assertEqual((first_tag & 0x7FFF, (first_tag >> 58) & 3), (32767, 2))     # IMAGE
        # The upload's first pixels are the picture's first row: words 0, 1, 4, 5 of the first block.
        self.assertEqual(struct.unpack_from('<4I', image, 16), (0, 1, 4, 5))
        # TEX0_1 is written without its CLUT load bits.
        self.assertIn(struct.pack('<QQ', 0x1234, 0x06), packets)


if __name__ == '__main__':
    unittest.main()
