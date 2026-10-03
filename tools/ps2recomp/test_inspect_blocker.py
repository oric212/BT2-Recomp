import unittest
from inspect_blocker import classify


class BlockerTests(unittest.TestCase):
    def test_entry_interior_and_exclusive_end(self):
        rows = [{"start": 0x100, "end": 0x120}]
        self.assertEqual(classify(0x100, rows, [], [])["map_status"], "mapped entry")
        self.assertEqual(classify(0x104, rows, [], [])["map_status"], "inside mapped function")
        self.assertEqual(classify(0x104, rows, [], [])["neutral_name"], "FUN_00000100")
        self.assertEqual(classify(0x120, rows, [], [])["map_status"], "map gap")

    def test_quarantine_evidence_is_retained(self):
        candidates = [{"start": 0x100, "end_exclusive": 0x120}]
        excluded = [{"start": 0x100, "end": 0x120, "reason": "noncontiguous_or_invalid"}]
        result = classify(0x104, [], candidates, excluded)
        self.assertEqual(result["map_status"], "map gap")
        self.assertEqual(result["analysis_candidates"], candidates)
        self.assertEqual(result["exclusions"], excluded)
