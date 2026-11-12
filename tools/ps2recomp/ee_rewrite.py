"""Generated statements that differ from what the EE does, rewritten after each generation.

**Rounding.** The recompiler emits most float arithmetic through the runtime's helpers (FPU_ADD_S, PS2_VMUL, ...),
which round as the EE and VU0 do (toward zero, no infinities or denormals: runtime/ee/ps2_fpu.cpp). A few operations
it writes as host arithmetic, rounding to nearest: the FPU's DIV.S and VU0's DIV, SQRT and RSQRT (into Q). Found by
comparing the CPU opponent's AI with PCSX2 (docs/plans/ee/AI_VERIFICATION_PLAN.md): its vectors came out one unit
off. Those statements become calls of ps2_fpu_div (DIV.S, to nearest), ps2_vu_div and ps2_vu_sqrt (VU0, toward
zero); SQRT.S goes through ps2_fpu_sqrt (to nearest, as PCSX2's divide rounding covers it: the PCSX2 comparison
sessions). Forms an earlier version of this rewrite wrote are brought up to date too.

**MOVN and MOVZ** copy a register's low 64 bits and keep the upper 64 of the destination, as every non-MMI
instruction does; the recompiler wrote them as 128-bit copies. Found by the PCSX2 comparison sessions
(docs/plans/validation/PCSX2_COMPARISON_PLAN.md): v0's upper half differed after the AI's calls.

**SQRT.S** takes the root of the register in its ft field; the recompiler read the fs field, which is 0 in the
game's words, so 110 of its 300 SQRT.S sites took the root of f0 instead (the others have ft = f0). Found by the
same sessions: the fight's quaternion unpacking (0x29A840) differed from PCSX2.

generate_game.py applies the rewrite before the generation manifest takes the files' hashes; the discovery loop's
receipt check applies it to the previous output before comparing, so the rewrite is not taken for an unexpected
change.
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
# MOVZ (funct 0x0A, rt == 0) and MOVN (0x0B, rt != 0): the condition, the destination and the source.
MOVE_IF = re.compile(r'if \(GPR_U64\(ctx, (\d+)\) (==|!=) 0\) SET_GPR_VEC\(ctx, (\d+), GPR_VEC\(ctx, (\d+)\)\);')
# SQRT.S (COP1, format S, funct 4): the destination and the register the statement reads.
FPU_SQRT = re.compile(r'ctx->f\[(\d+)\] = FPU_SQRT_S\(ctx->f\[(\d+)\]\);')
WORD = re.compile(r'// 0x[0-9a-f]+: 0x([0-9a-f]{1,8}) |void h_([0-9A-F]{8})\(')


def word_before(text, at, what):
    words = WORD.findall(text, max(0, at - 800), at)  # The word is a few lines before.
    if not words:
        raise ValueError(f'A {what} without its instruction word')
    return int(words[-1][0] or words[-1][1], 16)


def fpu_sqrt(text):
    """SQRT.S fd, ft: the EE takes the root of ft (bits 16-20); the recompiler read fs (bits 11-15)."""
    out, at = [], 0
    for match in FPU_SQRT.finditer(text):
        word = word_before(text, match.start(), 'SQRT.S')
        fd, source = int(match.group(1)), int(match.group(2))
        ft, fs = (word >> 16) & 31, (word >> 11) & 31
        if word & 0xFFE0003F != 0x46000004 or (word >> 6) & 31 != fd or source not in (fs, ft):
            raise ValueError(f'The word before a SQRT.S is not it: {word:#010x}')
        out += [text[at:match.start()], f'ctx->f[{fd}] = FPU_SQRT_S(ctx->f[{ft}]);']
        at = match.end()
    return ''.join(out) + text[at:]


def rsqrt(match_text_before):
    words = WORD.findall(match_text_before)
    if not words:
        raise ValueError('A VU0 RSQRT without its instruction word')
    word = int(words[-1][0] or words[-1][1], 16)
    if word & 0xFE0007FF != 0x4A0003BE:
        raise ValueError(f'The word before a VU0 RSQRT is not one: {word:#010x}')
    fs, field = (word >> 11) & 31, (word >> 21) & 3
    return (f'float fs = _mm_cvtss_f32(_mm_shuffle_ps(ctx->vu0_vf[{fs}], ctx->vu0_vf[{fs}], _MM_SHUFFLE(0,0,0,{field}))); '
            'ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));')


def move_if(text):
    """MOVN and MOVZ as 64-bit copies; each checked against its instruction word."""
    out, at = [], 0
    for match in MOVE_IF.finditer(text):
        word = word_before(text, match.start(), 'conditional move')
        rt, rd, rs, funct = int(match.group(1)), int(match.group(3)), int(match.group(4)), word & 63
        expected = 0x0A if match.group(2) == '==' else 0x0B
        if word >> 26 != 0 or funct != expected or (word >> 16) & 31 != rt or (word >> 11) & 31 != rd or (word >> 21) & 31 != rs:
            raise ValueError(f'The word before a conditional move is not it: {word:#010x}')
        out += [text[at:match.start()], f'if (GPR_U64(ctx, {rt}) {match.group(2)} 0) SET_GPR_U64(ctx, {rd}, GPR_U64(ctx, {rs}));']
        at = match.end()
    return ''.join(out) + text[at:]


def rewrite(text):
    """The source with those statements as the EE does them."""
    text = fpu_sqrt(move_if(text))
    text = FPU_DIV.sub(r'if (ctx->f[\1] == 0.0f) ctx->fcr31 |= 0x100000; /* DZ flag */ ctx->f[\2] = ps2_fpu_div(ctx->f[\3], ctx->f[\1]);', text)
    text = text.replace(VU_DIV, 'ctx->vu0_q = ps2_vu_div(fs, ft);').replace(VU_SQRT, 'ctx->vu0_q = ps2_vu_sqrt(ft);')
    # Earlier forms of this rewrite (VU0's division through ps2_fpu_div, RSQRT of vf0w as 1 / root, VU0's roots
    # through the FPU's ps2_fpu_sqrt).
    text = text.replace('ctx->vu0_q = ps2_fpu_div(fs, ft);', 'ctx->vu0_q = ps2_vu_div(fs, ft);')
    text = text.replace('ctx->vu0_q = ps2_fpu_div(fs, ps2_fpu_sqrt(ft));', 'ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));')
    text = text.replace('ctx->vu0_q = ps2_fpu_sqrt(ft);', 'ctx->vu0_q = ps2_vu_sqrt(ft);')
    text = text.replace('ctx->vu0_q = ps2_vu_div(fs, ps2_fpu_sqrt(ft));', 'ctx->vu0_q = ps2_vu_div(fs, ps2_vu_sqrt(ft));')
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
