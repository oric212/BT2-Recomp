"""Synthetic memory-layout and generated dispatch-hook checks."""
import unittest
from prepare_runtime import build_memory, route_tail_calls


class RuntimePreparationTests(unittest.TestCase):
    def load(self, address=4, memory_size=8, file_size=4, offset=0):
        return {"virtual_address": address, "memory_size": memory_size,
                "file_size": file_size, "file_offset": offset}

    def test_file_payload_and_zero_fill(self):
        image = build_memory(b"test", {"load_segments": [self.load()]}, size=16)
        self.assertEqual(image, bytes(4) + b"test" + bytes(8))

    def test_unsafe_loads_rejected(self):
        for loads in ([self.load(address=12)], [self.load(file_size=9)],
                      [self.load(offset=1)], [self.load(), self.load(address=8)]):
            with self.subTest(loads=loads), self.assertRaises(ValueError):
                build_memory(b"test", {"load_segments": loads}, size=16)

    def test_tail_hook_does_not_replace_declarations_or_other_calls(self):
        source = "void FUN_00001234_0x1234(uint8_t*);\nFUN_00001234_0x1234(rdram, ctx, runtime); return;\nother(rdram, ctx, runtime);"
        text, count = route_tail_calls(source)
        self.assertEqual(count, 1)
        self.assertIn("void FUN_00001234_0x1234(uint8_t*);", text)
        self.assertIn("0x1234u", text)
        self.assertIn("GuestBranchKind::TailJump", text)
        self.assertIn("other(rdram, ctx, runtime);", text)
        self.assertEqual(route_tail_calls(text), (text, 0))


if __name__ == "__main__":
    unittest.main()
