import unittest
import struct
from prepare_map import select_candidates, has_tail_nop_gap, has_branch_nop_gaps


class CandidateMapTests(unittest.TestCase):
    metadata = {"sections": [{"name": ".text", "flags": 6, "occupies_file_bytes": True,
                              "address": 0x1000, "size": 0x100}]}

    def row(self, start, end):
        return {"start": start, "end_exclusive": end, "body_size": end-start, "contiguous_aligned": True}

    def test_unreachable_branch_padding_and_control_rejections(self):
        metadata={'sections':[{**self.metadata['sections'][0],'file_offset':0}]}
        words=[0x10000007,0,0,0x24020001,0x10000003,0,0,0x24020002,0x03E00008,0]
        row={'start':0x1000,'end_exclusive':0x1028,'body_size':32,'contiguous_aligned':False,'body_ranges':['1000-1007','100c-1017','101c-1027']}
        data=struct.pack('<10I',*words)
        self.assertTrue(has_branch_nop_gaps(row,metadata,data))
        for i,value in [(2,1),(0,0x14000007),(0,0x10000001),(4,0x10000001),(3,0x03200008),(1,0x03E00008),(3,0x45000002)]:
            changed=words.copy();changed[i]=value
            self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<10I',*changed)))
        self.assertFalse(has_branch_nop_gaps(row,metadata,data[:-1]))
        selected,_=select_candidates({'functions':[row,self.row(0x1008,0x1010),self.row(0x1040,0x1048)]},metadata,data=data)
        self.assertEqual(selected,[{'start':0x1040,'end':0x1048}])

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

    def test_single_tail_padding_gap_and_rejections(self):
        metadata = {"sections": [{**self.metadata["sections"][0], "file_offset": 0}]}
        words = [0x14000004, 0, 0x08000800, 0x27BD0010, 0, 0x03E00008, 0x27BD0010]
        data = struct.pack("<7I", *words)
        candidate = {"start": 0x1000, "end_exclusive": 0x101C, "body_size": 24,
                     "contiguous_aligned": False, "body_ranges": ["1000-100f", "1014-101b"]}
        self.assertTrue(has_tail_nop_gap(candidate, metadata, data))
        self.assertEqual(select_candidates({"functions": [candidate]}, metadata, data=data)[0],
                         [{"start": 0x1000, "end": 0x101C}])
        for index, replacement in ((4, 1), (2, 0x0C000800), (2, 0x08000404),
                                   (0, 0x14000003), (0, 0x03200008), (0, 0x45000003),
                                   (3, 0x03E00008), (5, 0)):
            changed = words.copy()
            changed[index] = replacement
            self.assertFalse(has_tail_nop_gap(candidate, metadata, struct.pack("<7I", *changed)))
        self.assertFalse(has_tail_nop_gap(candidate, metadata, data[:-1]))
        overlapping = self.row(0x1010, 0x1018)
        selected, excluded = select_candidates({"functions": [candidate, overlapping, self.row(0x1040, 0x1048)]},
                                               metadata, data=data)
        self.assertEqual(selected, [{"start": 0x1040, "end": 0x1048}])
        self.assertEqual(len(excluded), 2)


if __name__ == "__main__":
    unittest.main()
