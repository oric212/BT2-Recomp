"""PS2 rounding for the float operations the recompiler writes out inline.

The recompiler emits most float arithmetic through the runtime's helpers (FPU_ADD_S, PS2_VMUL, ...), which round as
the EE and VU0 do (toward zero, no infinities or denormals: runtime/ee/ps2_fpu.cpp). A few operations it writes as
host arithmetic, rounding to nearest: the FPU's DIV.S and VU0's DIV, SQRT and RSQRT (into Q). Found by comparing the
CPU opponent's AI with PCSX2 (docs/plans/ee/AI_VERIFICATION_PLAN.md): its vectors came out one unit off. This
rewrites those statements in generated sources into calls of ps2_fpu_div (DIV.S, to nearest), ps2_vu_div
(VU0, toward zero) and ps2_fpu_sqrt. Forms an earlier version of this rewrite wrote are brought up to date too. generate_game.py applies
it before the generation manifest takes the files' hashes; the discovery loop's receipt check applies it to the
previous output before comparing, so the rewrite is not taken for an unexpected change.
"""
import re

FPU_DIV = re.compile(
    r'if \(ctx->f\[(\d+)\] == 0\.0f\) \{ ctx->fcr31 \|= 0x100000; /\* DZ flag \*/ ctx->f\[(\d+)\] = copysignf\(INFINITY, ctx->f\[(\d+)\] \* 0\.0f\); \} '
    r'else ctx->f\[\2\] = ctx->f\[\3\] / ctx->f\[\1\];')
VU_DIV = 'ctx->vu0_q = (ft != 0.0f) ? (fs / ft) : 0.0f;'
VU_SQRT = 'ctx->vu0_q = sqrtf(std::max(0.0f, ft));'
VU_RSQRT = 'ctx->vu0_q = (ft > 0.0f) ? (1.0f / sqrtf(ft)) : 0.0f;'
OLD_RSQRT_ONE = 'ctx->vu0_q = ps2_fpu_div(1.0f, ps2_fpu_sqrt(ft));'
# RSQRT divides fs by the root of ft; the recompiler's statement ignores fs. Its register and lane come from the
# instruction word: the comment before the statement in a generated function, or an interpreter handler's name.
WORD = re.compile(r'// 0x[0-9a-f]+: 0x([0-9a-f]{8}) |void h_([0-9A-F]{8})\(')


def rsqrt(match_text_before):
    words = WORD.findall(match_text_before)
    if not words:
        raise ValueError('A VU0 RSQRT without its instruction word')
    word = int(words[-1][0] or words[-1][1], 16)
    if word & 0xFE0007FF != 0x4A0003BE:
        raise ValueError(f'The word before a VU0 RSQRT is not one: {word:#010x}')
    fs, field = (word >> 11) & 31, (word >> 21) & 3
    return (f'float fs = _mm_cvtss_f32(_mm_shuffle_ps(ctx->vu0_vf[{fs}], ctx->vu0_vf[{fs}], _MM_SHUFFLE(0,0,0,{field}))); '
            'ctx->vu0_q = ps2_vu_div(fs, ps2_fpu_sqrt(ft));')


def rewrite(text):
    """The source with those operations as the PS2 rounds them."""
    text = FPU_DIV.sub(r'if (ctx->f[\1] == 0.0f) ctx->fcr31 |= 0x100000; /* DZ flag */ ctx->f[\2] = ps2_fpu_div(ctx->f[\3], ctx->f[\1]);', text)
    text = text.replace(VU_DIV, 'ctx->vu0_q = ps2_vu_div(fs, ft);').replace(VU_SQRT, 'ctx->vu0_q = ps2_fpu_sqrt(ft);')
    # Earlier forms of this rewrite (VU0's division through ps2_fpu_div, RSQRT of vf0w as 1 / root).
    text = text.replace('ctx->vu0_q = ps2_fpu_div(fs, ft);', 'ctx->vu0_q = ps2_vu_div(fs, ft);')
    text = text.replace('ctx->vu0_q = ps2_fpu_div(fs, ps2_fpu_sqrt(ft));', 'ctx->vu0_q = ps2_vu_div(fs, ps2_fpu_sqrt(ft));')
    while (at := text.find(OLD_RSQRT_ONE)) >= 0:
        text = text[:at] + rsqrt(text[:at]).replace('ps2_fpu_div', 'ps2_vu_div') + text[at + len(OLD_RSQRT_ONE):]
    while (at := text.find(VU_RSQRT)) >= 0:
        text = text[:at] + rsqrt(text[:at]) + text[at + len(VU_RSQRT):]
    return text


def rewrite_folder(folder):
    """Rewrites the generated sources of a folder in place; returns how many changed."""
    changed = 0
    for path in sorted(folder.glob('*.cpp')):
        text = path.read_bytes().decode('utf-8')  # Bytes, so line endings stay as the generator wrote them.
        new = rewrite(text)
        if new != text:
            path.write_bytes(new.encode('utf-8'))
            changed += 1
    return changed
