"""Synthetic memory-layout and generated dispatch-hook checks."""
import unittest
from prepare_runtime import build_memory, route_tail_calls, route_direct_jumps, route_data_cache, route_vu_status,route_vu_control, route_indexed_cache, route_fast_halfword_reads, route_fast_byte_reads, widen_sign_branches


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

    def test_indexed_cache_preserves_explicit_tag_reads(self):
        source='// 0x1234: 0xbcd00001  cache       0x10, 0x1($a2)\nctx->pc = 0x1234u;\n// CACHE instruction (ignored)\nSET_GPR_S32(ctx, 2, (int32_t)ctx->cop0_taglo);'
        text,count,reads=route_indexed_cache(source)
        self.assertEqual((count,reads),(1,1))
        self.assertIn('indexedDataCache(rdram, ctx, 0x10u',text)
        self.assertIn('readCacheTagLo(ctx)',text)
        self.assertEqual(route_indexed_cache(text),(text,0,0))
        with self.assertRaises(ValueError):route_indexed_cache(source.replace('0x10,','0x11,'))
        with self.assertRaises(ValueError):route_indexed_cache('ctx->cop0_taglo = 0;')

    def test_fast_halfword_load_uses_checked_owned_ram(self):
        text,count=route_fast_halfword_reads('SET_GPR_ZE32(ctx, 2, (uint16_t)FAST_READ16(0x1234u));')
        self.assertEqual(count,1);self.assertIn('READ16(0x1234u)',text)
        self.assertEqual(route_fast_halfword_reads(text),(text,0))
        with self.assertRaises(ValueError):route_fast_halfword_reads('FAST_READ16(variable)')

    def test_fast_byte_load_preserves_signed_and_unsigned_casts(self):
        source='(int8_t)FAST_READ8(0x1234u); (uint8_t)FAST_READ8(0x5678u);'
        text,count=route_fast_byte_reads(source)
        self.assertEqual(count,2);self.assertEqual(text,'(int8_t)READ8(0x1234u); (uint8_t)READ8(0x5678u);')
        self.assertEqual(route_fast_byte_reads(text),(text,0))
        with self.assertRaises(ValueError):route_fast_byte_reads('FAST_READ8(variable)')

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

    def test_sign_branches_compare_64_bits(self):
        source=('const bool branch_taken_0x31a378 = (GPR_S32(ctx, 17) >= 0); '
                'const bool branch_taken_0x10 = (GPR_S32(ctx, 2) < 0); '
                'const bool branch_taken_0x14 = (GPR_U64(ctx, 3) != GPR_U64(ctx, 0)); '
                'SET_GPR_S32(ctx, 2, (int32_t)GPR_S32(ctx, 4));')
        text,count=widen_sign_branches(source)
        self.assertEqual(count,2)
        self.assertIn('= (GPR_S64(ctx, 17) >= 0);',text)
        self.assertIn('= (GPR_S64(ctx, 2) < 0);',text)
        self.assertIn('(int32_t)GPR_S32(ctx, 4)',text)

    def test_direct_jump_ends_the_path(self):
        source = ('    ctx->pc = 0x28BB60u;\n'
                  '    if (!runtime->dispatchGuestBranch(rdram, ctx, 0x28BB60u, 0x3BC5A0u, 0x0u, PS2Runtime::GuestBranchKind::DirectJump, "J")) {\n'
                  '        return;\n    }\n    ctx->pc = 0x3BC5A8u;\n')
        text, count = route_direct_jumps(source)
        self.assertEqual(count, 1)
        self.assertIn('dispatchGuestBranch(rdram, ctx, 0x28BB60u, 0x3BC5A0u, 0u, PS2Runtime::GuestBranchKind::TailJump, "generated tail jump"); return;', text)
        self.assertNotIn("DirectJump", text)
        self.assertEqual(route_direct_jumps(text), (text, 0))
        other = 'if (!runtime->dispatchGuestBranch(rdram, ctx, ctx->r[31], 0x10u, 0x0u, PS2Runtime::GuestBranchKind::Return, "JR")) {\n return;\n}'
        self.assertEqual(route_direct_jumps(other), (other, 0))


if __name__ == "__main__":
    unittest.main()
