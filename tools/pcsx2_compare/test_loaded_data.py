import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import loaded_data  # noqa: E402


def ram(size, changes):
    data = bytearray(size)
    for address, value in changes:
        data[address:address + 4] = value.to_bytes(4, 'little')
    return bytes(data)


class LoadedDataTest(unittest.TestCase):
    def test_equal(self):
        self.assertEqual(loaded_data.differing_ranges(bytes(64), bytes(64)), [])

    def test_ranges_merge_across_small_gaps(self):
        a = bytes(1024)
        b = ram(1024, [(0x10, 1), (0x20, 2), (0x200, 3)])
        self.assertEqual(loaded_data.differing_ranges(a, b, gap=64), [(0x10, 0x24, 2), (0x200, 0x204, 1)])

    def test_classify_splits_at_region_edges(self):
        regions = [(0x100, 0x200, 'stubs', '')]
        ranges = loaded_data.classify([(0xF0, 0x210, 5)], regions)
        self.assertEqual([(r['start'], r['end'], r['region']) for r in ranges],
                         [(0xF0, 0x100, None), (0x100, 0x200, 'stubs'), (0x200, 0x210, None)])

    def test_compare_lists_findings_outside_regions(self):
        a = bytes(0x400)
        b = ram(0x400, [(0x10, 1), (0x300, 2)])
        result = loaded_data.compare(a, b, [(0x300, 0x400, 'timing', '')])
        self.assertEqual(result['known'], {'timing': {'ranges': 1, 'bytes': 4}})
        self.assertEqual([(f['start'], f['end']) for f in result['findings']], [(0x10, 0x14)])

    def test_session_patches_cover_calls_and_entries(self):
        import json
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            session = Path(folder)
            (session / 'checkpoint').mkdir()
            (session / 'session.json').write_text(json.dumps({'sites': [
                {'call': '0x102ef8', 'target': '0x102aa0'},
                {'call': '0x101a60', 'target': '0x101a60'},
                {'call': '0x200000', 'target': '0x300000', 'entry': True}]}), encoding='utf-8')
            patches = loaded_data.session_patches(session / 'checkpoint')
        self.assertEqual([(s, e) for s, e, _, _ in patches],
                         [(0x102EF8, 0x102EFC), (0x101A60, 0x101A68), (0x200000, 0x200008)])


if __name__ == '__main__':
    unittest.main()
