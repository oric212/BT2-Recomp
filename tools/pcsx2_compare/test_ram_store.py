"""Tests of the changed-page storage of checkpoint RAM (ram_store.py)."""
import random
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ram_store  # noqa: E402

SIZE = 64 * ram_store.PAGE


class RamStoreTest(unittest.TestCase):
    def test_round_trip_and_keyframes(self):
        rng = random.Random(7)
        ram = bytearray(rng.randbytes(SIZE))
        states = []
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            writer = ram_store.Writer()
            for k in range(ram_store.KEYFRAME + 5):
                for _ in range(rng.randrange(0, 4)):
                    ram[rng.randrange(SIZE)] ^= 0xFF
                folder = root / f'cp-{k}'
                folder.mkdir()
                writer.store(folder, bytes(ram))
                states.append(bytes(ram))
            self.assertTrue((root / 'cp-0/ee-ram.bin').exists())
            self.assertTrue((root / f'cp-{ram_store.KEYFRAME}/ee-ram.bin').exists())
            self.assertTrue((root / 'cp-1/ram-pages.json').exists())
            self.assertLess((root / 'cp-1/ram-pages.bin').stat().st_size, 4 * ram_store.PAGE)
            ram_store._cache.clear()
            for k in reversed(range(len(states))):            # Out of order: walks back through the chain.
                self.assertEqual(ram_store.read(root / f'cp-{k}'), states[k], k)
            out = ram_store.materialize(root / 'cp-3', root / 'in.bin')
            self.assertEqual(out, root / 'in.bin')
            self.assertEqual(out.read_bytes(), states[3])
            self.assertEqual(ram_store.materialize(root / 'cp-0', root / 'in.bin'), root / 'cp-0/ee-ram.bin')


if __name__ == '__main__':
    unittest.main()
