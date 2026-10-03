import unittest
from prepare_map import select_candidates


class CandidateMapTests(unittest.TestCase):
    metadata = {"sections": [{"name": ".text", "flags": 6, "occupies_file_bytes": True,
                              "address": 0x1000, "size": 0x100}]}

    def row(self, start, end):
        return {"start": start, "end_exclusive": end, "body_size": end-start, "contiguous_aligned": True}

    def test_overlap_group_quarantines_all_members(self):
        report = {"functions": [self.row(0x1000, 0x1020), self.row(0x1010, 0x1030),
                                self.row(0x1020, 0x1040), self.row(0x1040, 0x1048)]}
        selected, excluded = select_candidates(report, self.metadata)
        self.assertEqual(selected, [{"start": 0x1040, "end": 0x1048}])
        self.assertEqual(len(excluded), 3)

    def test_size_section_and_body_filters(self):
        invalid = self.row(0x1060, 0x1068)
        invalid["body_size"] = 4
        selected, excluded = select_candidates({"functions": [self.row(0x1000, 0x1008),
            self.row(0x1010, 0x1050), self.row(0x2000, 0x2008), invalid]}, self.metadata, max_size=16)
        self.assertEqual(len(selected), 1)
        self.assertEqual({row["reason"] for row in excluded},
                         {"oversized", "outside_file_backed_ee_text", "noncontiguous_or_invalid"})


if __name__ == "__main__":
    unittest.main()
