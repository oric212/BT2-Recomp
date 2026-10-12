import unittest
from expand_startup import select_target, render, neighbours


class StartupExpansionTests(unittest.TestCase):
    def test_only_exact_unique_main_entries_are_selected(self):
        rows = [{'Start': '0x1000', 'End': '0x1080'}]
        self.assertEqual(select_target(0x1000, rows), 0x1000)
        for target in (None, '0x1000', 0, 0x1001, 0x1004, 0x80001000):
            with self.subTest(target=target), self.assertRaises(ValueError):
                select_target(target, rows)
        with self.assertRaises(ValueError):
            select_target(0x1000, rows * 2)

    def test_neighbours_need_a_span_a_unique_row_and_a_generated_source(self):
        rows = [{'Start': '0x1000', 'End': '0x1080'}, {'Start': '0x1080', 'End': '0x1100'}, {'Start': '0x1080', 'End': '0x1100'}]
        self.assertEqual(neighbours(0x1000, rows, set(), 0), [])
        # 0x1080 is duplicated and no generated source exists for these synthetic addresses.
        self.assertEqual(neighbours(0x1000, rows, set(), 0x1000), [])

    def test_neutral_bindings_and_source_selection(self):
        cmake, header = render([0x1000, 0x2000])
        self.assertIn('${BT2_GENERATED_DIR}/../bt2-runtime/FUN_00001000_0x1000.cpp', cmake)
        # Self-contained header: declarations of the selected functions, then a table registered in order.
        self.assertIn('void FUN_00002000_0x2000(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime);', header)
        self.assertLess(header.index('{0x00001000, FUN_00001000_0x1000},'), header.index('{0x00002000, FUN_00002000_0x2000},'))
        self.assertIn('runtime.registerFunction(entry.address, entry.function);', header)
        self.assertNotIn('game_data', cmake)
