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

    def test_copied_kernel_target_is_separate_from_elf_map(self):
        result = classify(0x80076000, [{"start": 0x36B328, "end": 0x36BA68}], [], [])
        self.assertEqual(result["map_status"], "map gap")
        self.assertTrue(result["requires_kernel_payload_mapping"])
        self.assertEqual(result["mapped_ranges"], [])
        self.assertFalse(classify(0x36B328, [], [], [])["requires_kernel_payload_mapping"])
