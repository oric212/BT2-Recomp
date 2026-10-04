import unittest
from expand_startup import select_target, render


class StartupExpansionTests(unittest.TestCase):
    def test_only_exact_unique_main_entries_are_selected(self):
        rows = [{'Start': '0x1000', 'End': '0x1080'}]
        self.assertEqual(select_target(0x1000, rows), 0x1000)
        for target in (None, '0x1000', 0, 0x1001, 0x1004, 0x80001000):
            with self.subTest(target=target), self.assertRaises(ValueError):
                select_target(target, rows)
        with self.assertRaises(ValueError):
            select_target(0x1000, rows * 2)

    def test_neutral_bindings_and_source_selection(self):
        cmake, header = render([0x1000, 0x2000])
        self.assertIn('${BT2_GENERATED_DIR}/../bt2-runtime/FUN_00001000_0x1000.cpp', cmake)
        self.assertIn('runtime.registerFunction(0x00002000, FUN_00002000_0x2000);', header)
        self.assertNotIn('game_data', cmake)
