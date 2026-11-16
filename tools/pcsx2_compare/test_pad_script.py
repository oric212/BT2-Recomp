"""session.py's --pad-script: the port's pad script syntax as changes to the controller's raw block."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import session  # noqa: E402


class PadScriptTest(unittest.TestCase):
    def test_steps_become_presses(self):
        # Each step holds to the next; a release step presses nothing; buttons are the controller's bits.
        presses = session.pad_presses('10:start,12:-,20:cross+up,25:-')
        self.assertEqual(presses, [(10, 2, 0x0008), (20, 5, 0x4000 | 0x0010)])

    def test_order_does_not_matter(self):
        self.assertEqual(session.pad_presses('20:-,10:circle'), [(10, 10, 0x2000)])

    def test_refusals(self):
        for text in ('10:cross', '10:jump,20:-', '10:lx=00,20:-', 'x:cross,20:-', '10:cross,10:-'):
            with self.assertRaises(ValueError, msg=text):
                session.pad_presses(text)

    def test_block_words(self):
        # Start: only the buttons word (no pressure); Cross: the buttons word and its pressure byte (index 6, byte
        # 40 = word 10's low byte), in both receive buffers.
        start = session.block_presses([(5, 3, 0x0008)])
        self.assertEqual(start, [(5, 3, 0x3B0B00 + 28, 0xFFFFFFF7, 0), (5, 3, 0x3B0B80 + 28, 0xFFFFFFF7, 0)])
        cross = session.block_presses([(7, 2, 0x4000)])
        self.assertEqual(cross[:2], [(7, 2, 0x3B0B00 + 28, 0xFFFFBFFF, 0), (7, 2, 0x3B0B00 + 40, 0xFFFFFFFF, 0xFF)])
        self.assertEqual(len(cross), 4)

    def test_the_site(self):
        site = session.pad_site('100:start,104:-')
        self.assertEqual((site['name'], site['call'], site['entry'], site['pad']), ('pad', session.PAD_FUNCTION, True, True))
        self.assertEqual(len(site['presses']), 2)


if __name__ == '__main__':
    unittest.main()
