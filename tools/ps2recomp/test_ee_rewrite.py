import unittest

from ee_rewrite import rewrite

FPU_DIV = ('    if (ctx->f[2] == 0.0f) { ctx->fcr31 |= 0x100000; /* DZ flag */ ctx->f[0] = copysignf(INFINITY, ctx->f[1] * 0.0f); } '
           'else ctx->f[0] = ctx->f[1] / ctx->f[2];')
RSQRT = ('    // 0x129af8: 0x4a6503be  vrsqrt      $Q, {numerator}, $vf5x\n'
         '    ctx->pc = 0x129af8u;\n'
         '    {{ float ft = 1.0f; ctx->vu0_q = (ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f; }}\n')


MOVN = ('    // 0x100cc0: 0x3100b  movn        $v0, $zero, $v1\n'
        '    ctx->pc = 0x100cc0u;\n'
        '    if (GPR_U64(ctx, 3) != 0) SET_GPR_VEC(ctx, 2, GPR_VEC(ctx, 0));\n')


class EeRewriteTests(unittest.TestCase):
    def test_conditional_moves_keep_the_upper_half(self):
        # movn v0, zero, v1 (0x0003100B) and movz v0, a0, v1 (0x0083100A): 64-bit copies, as on the EE.
        self.assertIn('if (GPR_U64(ctx, 3) != 0) SET_GPR_U64(ctx, 2, GPR_U64(ctx, 0));', rewrite(MOVN))
        movz = MOVN.replace('0x3100b  movn        $v0, $zero', '0x83100a  movz        $v0, $a0').replace(
            '!= 0) SET_GPR_VEC(ctx, 2, GPR_VEC(ctx, 0))', '== 0) SET_GPR_VEC(ctx, 2, GPR_VEC(ctx, 4))')
        self.assertIn('if (GPR_U64(ctx, 3) == 0) SET_GPR_U64(ctx, 2, GPR_U64(ctx, 4));', rewrite(movz))
        handler = 'void h_0003100B(uint8_t* rdram) {\n    if (GPR_U64(ctx, 3) != 0) SET_GPR_VEC(ctx, 2, GPR_VEC(ctx, 0));\n'
        self.assertIn('SET_GPR_U64(ctx, 2, GPR_U64(ctx, 0));', rewrite(handler))

    def test_fpu_square_root_reads_ft(self):
        # 0x460C0044 is sqrt.s f1, f12 (ft 12, fs 0, fd 1); the recompiler read f0.
        text = ('    // 0x29a920: 0x460c0044  c1          0xC0044\n    ctx->pc = 0x29a920u;\n'
                '    ctx->f[1] = FPU_SQRT_S(ctx->f[0]);\n')
        self.assertIn('ctx->f[1] = FPU_SQRT_S(ctx->f[12]);', rewrite(text))
        self.assertEqual(rewrite(rewrite(text)), rewrite(text))
        handler = 'void h_460C0044(uint8_t* rdram) {\n    ctx->f[1] = FPU_SQRT_S(ctx->f[0]);\n'
        self.assertIn('FPU_SQRT_S(ctx->f[12])', rewrite(handler))
        with self.assertRaises(ValueError):
            rewrite(text.replace('ctx->f[1] =', 'ctx->f[2] ='))   # Not the word's destination.

    def test_a_conditional_move_not_matching_its_word_stops(self):
        with self.assertRaises(ValueError):
            rewrite(MOVN.replace('0x3100b', '0x3100a'))          # The word says movz, the statement movn.
        with self.assertRaises(ValueError):
            rewrite('    if (GPR_U64(ctx, 3) != 0) SET_GPR_VEC(ctx, 2, GPR_VEC(ctx, 0));\n')

    def test_fpu_divide_rounds_as_the_ee(self):
        self.assertEqual(rewrite(FPU_DIV), '    if (ctx->f[2] == 0.0f) ctx->fcr31 |= 0x100000; /* DZ flag */ ctx->f[0] = ps2_fpu_div(ctx->f[1], ctx->f[2]);')

    def test_vu0_q_operations(self):
        self.assertEqual(rewrite('ctx->vu0_q = (ft != 0.0f) ? (fs / ft) : 0.0f;'), 'ctx->vu0_q = ps2_vu_div(fs, ft);')
        self.assertEqual(rewrite('ctx->vu0_q = sqrtf(std::max(0.0f, ft));'), 'ctx->vu0_q = ps2_vu_sqrt(ft);')
        self.assertIn('ctx->vu0_vf[0], ctx->vu0_vf[0], _MM_SHUFFLE(0,0,0,3))); ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));',
                      rewrite(RSQRT.format(numerator='$vf0w')))

    def test_rsqrt_divides_its_own_numerator(self):
        # 0x4A6503BE is vrsqrt Q, vf0w, vf5x; with fs vf3 lane y (fs bits 11-15, fsf bits 21-22) the word is 0x4A251BBE.
        text = RSQRT.format(numerator='$vf3y').replace('0x4a6503be', '0x4a251bbe')
        self.assertIn('_mm_shuffle_ps(ctx->vu0_vf[3], ctx->vu0_vf[3], _MM_SHUFFLE(0,0,0,1))); ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));', rewrite(text))
        handler = 'void h_4A6503BE(uint8_t* rdram) {\n    { float ft = 1.0f; ctx->vu0_q = (ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f; }\n'
        self.assertIn('ctx->vu0_vf[0], ctx->vu0_vf[0], _MM_SHUFFLE(0,0,0,3)', rewrite(handler))

    def test_rsqrt_without_its_word_stops(self):
        with self.assertRaises(ValueError):
            rewrite('{ float ft = 1.0f; ctx->vu0_q = (ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f; }')

    def test_earlier_forms_are_brought_up_to_date(self):
        self.assertEqual(rewrite('ctx->vu0_q = ps2_fpu_div(fs, ft);'), 'ctx->vu0_q = ps2_vu_div(fs, ft);')
        old = RSQRT.format(numerator='$vf0w').replace('ctx->vu0_q = (ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f;', 'ctx->vu0_q = ps2_fpu_div(1.0f, ps2_fpu_sqrt(ft));')
        self.assertEqual(rewrite(old), rewrite(RSQRT.format(numerator='$vf0w')))
        # VU0's roots once went through the FPU's ps2_fpu_sqrt (now SQRT.S's, to nearest).
        self.assertEqual(rewrite('ctx->vu0_q = ps2_fpu_sqrt(ft);'), 'ctx->vu0_q = ps2_vu_sqrt(ft);')
        self.assertEqual(rewrite('ctx->vu0_q = ps2_vu_div(fs, ps2_fpu_sqrt(ft));'), 'ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));')

    def test_rewritten_text_is_unchanged_by_a_second_rewrite(self):
        once = rewrite(FPU_DIV + '\n' + RSQRT.format(numerator='$vf0w'))
        self.assertEqual(rewrite(once), once)


if __name__ == '__main__':
    unittest.main()
