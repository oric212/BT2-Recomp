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

    def test_tail_jump_padding_before_another_path(self):
        metadata={'sections':[{**self.metadata['sections'][0],'file_offset':0}]}
        words=[0x14000004,0,0x08000420,0x27BD0020,0,0x0C000420,0,0x03E00008,0]
        row={'start':0x1000,'end_exclusive':0x1024,'body_size':32,'contiguous_aligned':False,'body_ranges':['1000-100f','1014-1023']}
        data=struct.pack('<9I',*words)
        self.assertTrue(has_branch_nop_gaps(row,metadata,data))
        indirect=words.copy();indirect[5]=0x0040F809
        self.assertTrue(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*indirect)))
        indirect[5]=0x00401009
        self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*indirect)))
        for index,value in [(2,0x0C000420),(2,0x03200008),(2,0x08000404),(2,0x08000800),(0,0x14000003),(4,1),(3,0x03E00008)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(row,metadata,struct.pack('<9I',*changed)))

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


class BoundedSwitchTests(unittest.TestCase):
    def test_bounded_table_and_changed_bounds_targets_instructions(self):
        metadata={'sections':[{'name':'.text','address':0x1000,'size':0x48,'file_offset':0,'flags':4,'occupies_file_bytes':True}]}
        words=[0,0,0x2623FFE0,0x2C620002,0x10400009,0x00031080,0x3C030000,0x00621821,0x8C631040,0x00600008,0,0x10000003,0,0,0x03E00008,0,0x1000,0x1038]
        candidate={'start':0x1000,'end_exclusive':0x1040,'body_size':60,'contiguous_aligned':False,'body_ranges':['1000-1033','1038-103f']}
        self.assertTrue(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*words)))
        for index,value in [(3,0x2C620000),(3,0x2C620003),(4,0x14400009),(5,0x000310C0),(8,0x8C631041),(16,0x1034),(16,0x1040),(17,0x1039),(10,0x24020001)]:
            changed=words.copy();changed[index]=value
            self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<18I',*changed)))
        self.assertFalse(has_branch_nop_gaps(candidate,metadata,struct.pack('<17I',*words[:-1])))


if __name__ == "__main__":
    unittest.main()
