"""Synthetic memory-layout and generated dispatch-hook checks."""
import unittest
from prepare_runtime import build_memory, route_tail_calls, route_data_cache, route_vu_status,route_vu_control


class RuntimePreparationTests(unittest.TestCase):
    def test_vu_reset_pulse_is_not_silently_masked(self):
        source='ctx->vu0_fbrst = GPR_U32(ctx, 4) & 0x00000C0Cu;'
        text,count=route_vu_control(source)
        self.assertEqual(count,1);self.assertIn('writeVuFbrst(ctx, GPR_U32(ctx, 4))',text)
        self.assertEqual(route_vu_control(text),(text,0))
        with self.assertRaises(ValueError):route_vu_control('ctx->vu0_fbrst = 0;')
    def test_vu_status_routes_to_checked_runtime(self):
        source='SET_GPR_U32(ctx, 6, ctx->vu0_vpu_stat);'
        text,count=route_vu_status(source)
        self.assertEqual(count,1);self.assertIn('runtime->readVuStatus(ctx)',text)
        self.assertEqual(route_vu_status(text),(text,0))
        with self.assertRaises(ValueError):route_vu_status('unexpected(ctx->vu0_vpu_stat)')

    def test_checked_cache_hook_scope(self):
        source = '// 0x1234: 0xbd180040  cache       0x18, 0x40($t0)\n    ctx->pc = 0x1234u;\n    // CACHE instruction (ignored)'
        text, count = route_data_cache(source)
        self.assertEqual(count, 1)
        self.assertIn('dataCacheLine(rdram, ADD32(GPR_U32(ctx, 8), 0x40u))', text)
        self.assertEqual(route_data_cache(text), (text, 0))
        self.assertEqual(route_data_cache(source.replace('0x18,', '0x19,')), (source.replace('0x18,', '0x19,'), 0))

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
