"""Tests of the input delay analysis (input_delay.py) on made-up logs."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import input_delay as d  # noqa: E402

FIELD = d.FIELD


def logs(press_at=None, lists_at=None, vu1_at=None, unlike=None, frames=12):
    pad, end = [], []
    for k in range(frames):
        start = 2 * k * FIELD
        pad.append({'call': k + 1, 'cycles': int(start + 0.1 * FIELD), 'word1': 0x4000 if k == press_at else 0})
        end.append({'call': k + 1, 'cycles': int(start + 2 * FIELD + 1000),
                    'sum1': 1 if (lists_at is not None and k >= lists_at) or k == unlike else 0,
                    'sum2': 1 if vu1_at is not None and k >= vu1_at else 0, 'sum3': 0})
    return pad, end


class InputDelayTest(unittest.TestCase):
    def test_frames_and_fields(self):
        result = d.analyse(logs(), logs(press_at=4, lists_at=4, vu1_at=5))
        self.assertEqual(result['press_frame'], 5)
        self.assertTrue(result['alike_before'])
        self.assertEqual(result['display_lists']['frames_after_read'], 0)
        self.assertEqual(result['display_lists']['fields_after_read_field_start'], 2.0)
        self.assertEqual(result['vu1_memory']['frames_after_read'], 1)
        self.assertEqual(result['vu1_memory']['fields_after_read_field_start'], 4.0)
        self.assertEqual(result['fields_per_frame'], {'min': 2.0, 'max': 2.0})
        self.assertEqual(result['read_after_field_start'], 0.1)

    def test_unlike_runs_are_reported(self):
        result = d.analyse(logs(), logs(press_at=4, lists_at=4, unlike=2))
        self.assertFalse(result['alike_before'])
        self.assertEqual(result['first_unlike_before'], 3)

    def test_no_press(self):
        with self.assertRaises(ValueError):
            d.analyse(logs(), logs())


if __name__ == '__main__':
    unittest.main()
