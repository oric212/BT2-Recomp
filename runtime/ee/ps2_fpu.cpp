// Authored R5900 COP1 single-precision semantics for generated probes.
// The EE FPU is not IEEE 754: it rounds toward zero only, has no infinities, NaNs or
// denormals (denormal operands/results become signed zero; overflow clamps to the largest
// finite magnitude) and float-to-word conversion truncates and saturates. Reached operations
// only; flag (O/U/I/D) and accumulator side effects other than ACC writes are not modeled.
#include "ps2_fpu.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <intrin.h>
#include <utility>
#pragma fenv_access(on)

namespace {
float normalize(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto exponent = bits & 0x7F800000u, sign = bits & 0x80000000u;
    if (!exponent) return std::bit_cast<float>(sign);
    if (exponent == 0x7F800000u) return std::bit_cast<float>(sign | 0x7F7FFFFFu);
    return value;
}
template <class Operation>
float chop(Operation operation) {
    // Round toward zero for the one operation, set in MXCSR directly: the C library's rounding
    // functions cost several times the operation itself.
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x6000u);
    const volatile float result = operation();
    _mm_setcsr(saved);
    return normalize(result);
}
// The same without changing MXCSR (two loads of it cost more than the operation): `rounded` is the operation
// rounded to nearest in the thread's own mode (the host's default: to nearest, no flushing; VU1 changes it only
// inside its runs), `error` the exact value minus it (or at least its sign). Where rounding went away from zero
// the result steps one unit toward it. Results below `smallest` (where the error may have underflowed: a sum's
// error is exact down to the smallest normal number, a product's only 24 binades above it) and overflows take chop.
bool chopFromNearest(float rounded, float error, std::uint32_t smallest, float& result) {
    const auto bits = std::bit_cast<std::uint32_t>(rounded);
    const auto magnitude = bits & 0x7FFFFFFFu;
    if (magnitude <= smallest || magnitude >= 0x7F800000u) return false;
    const bool outward = error != 0.0f && ((std::bit_cast<std::uint32_t>(error) ^ bits) & 0x80000000u) != 0u;
    result = std::bit_cast<float>(outward ? bits - 1u : bits);
    return true;
}
// A product through chop, out of line: inlined, its captured operands kept the fast path's values on the stack.
__declspec(noinline) float chopMul(float a, float b) {return chop([&] { return a * b; });}
float addTowardZero(float a, float b) {
    // Fast2Sum: with |a| >= |b|, b - (s - a) is the exact error of s = a + b. The magnitudes compare as integers (the
    // operands are normalized: no infinities or NaNs); std::fabs was a call under fenv_access.
    if ((std::bit_cast<std::uint32_t>(a) & 0x7FFFFFFFu) < (std::bit_cast<std::uint32_t>(b) & 0x7FFFFFFFu)) std::swap(a, b);
    const float s = a + b, error = b - (s - a);
    float result;
    if (chopFromNearest(s, error, 0x00800000u, result)) return result;
    // The rest needs no rounding mode (it took chop, a third of the helpers' time in a fight): a sum of two floats
    // up to the smallest normal number is exact (a zero sum is +0 to nearest and toward zero alike) and becomes a
    // signed zero; a sum rounded to an infinity was beyond the largest magnitude, which toward zero gives.
    return normalize(s);
}
const bool has_fma = [] {
    int registers[4];
    __cpuid(registers, 1);
    // FMA3, and the system saving the AVX registers (OSXSAVE, then XCR0's SSE and AVX state).
    if ((registers[2] & (1 << 12)) == 0 || (registers[2] & (1 << 27)) == 0) return false;
    return (_xgetbv(0) & 6u) == 6u;
}();
// The EE FPU's adder keeps a single guard bit: the operand of the smaller exponent loses its low (difference - 1)
// bits before the addition, and becomes a signed zero from a difference of 25 (PCSX2's FPU recompiler, FPU_ADD_SUB;
// the CPU opponent's sums against PCSX2, docs/plans/ee/AI_VERIFICATION_PLAN.md). Then the sum rounds toward zero.
float alignedAdd(float a, float b) {
    auto x = std::bit_cast<std::uint32_t>(a), y = std::bit_cast<std::uint32_t>(b);
    const int difference = int((x >> 23) & 0xFF) - int((y >> 23) & 0xFF);
    auto& smaller = difference > 0 ? y : x;
    const int shift = difference > 0 ? difference : -difference;
    if (shift >= 25) smaller &= 0x80000000u;
    else if (shift > 1) smaller &= 0xFFFFFFFFu << (shift - 1);
    return addTowardZero(std::bit_cast<float>(x), std::bit_cast<float>(y));
}
}

float ps2_fpu_add(float left, float right) {
    return alignedAdd(normalize(left), normalize(right));
}
float ps2_fpu_sub(float left, float right) {
    return alignedAdd(normalize(left), -normalize(right));
}
float ps2_vu_add(float left, float right) {
    return addTowardZero(normalize(left), normalize(right));
}
float ps2_vu_sub(float left, float right) {
    return addTowardZero(normalize(left), -normalize(right));
}
// Both use |value| (a negative operand only raises the unmodeled invalid flag).
float ps2_fpu_sqrt(float value) {
    // SQRT.S rounds to nearest, as DIV.S: a fight's quaternion unpacking (0x29A840) in PCSX2, whose division
    // rounding mode (nearest by default) covers its square root too, gave the nearest root where the chopped one
    // was a unit below (docs/plans/validation/PCSX2_COMPARISON_PLAN.md).
    const float a = std::bit_cast<float>(std::bit_cast<std::uint32_t>(normalize(value)) & 0x7FFFFFFFu);
    return normalize(std::sqrt(a));
}
float ps2_vu_sqrt(float value) {
    // VU0's SQRT and RSQRT's root round toward zero: to nearest broke the CPU opponent's distances against PCSX2
    // (docs/plans/ee/AI_VERIFICATION_PLAN.md).
    const float a = std::bit_cast<float>(std::bit_cast<std::uint32_t>(normalize(value)) & 0x7FFFFFFFu);
    return chop([&] { return std::sqrt(a); });
}
float ps2_fpu_mul(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    if (a == 0.0f || b == 0.0f) return a * b; // An exact signed zero in every rounding mode (often: the zeros of matrices).
    if (has_fma) {
        // The product's exact error from a fused multiply-add.
        const float product = a * b;
        const float error = _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(-product)));
        float result;
        if (chopFromNearest(product, error, 0x0C800000u, result)) return result;
    }
    return chopMul(a, b);
}
float ps2_fpu_div(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    // A zero divisor gives the largest magnitude, signed as the quotient (no infinities).
    if (b == 0.0f)
        return std::bit_cast<float>(((std::bit_cast<std::uint32_t>(a) ^ std::bit_cast<std::uint32_t>(b)) & 0x80000000u) | 0x7F7FFFFFu);
    // Rounded to nearest, unlike the other operations: the CPU opponent's quotients in PCSX2 (whose division
    // rounding mode is separate and nearest by default) were the nearest values, the chopped ones one unit below
    // (docs/plans/ee/AI_VERIFICATION_PLAN.md).
    return normalize(a / b);
}
float ps2_vu_div(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    if (b == 0.0f)
        return std::bit_cast<float>(((std::bit_cast<std::uint32_t>(a) ^ std::bit_cast<std::uint32_t>(b)) & 0x80000000u) | 0x7F7FFFFFu);
    // VU0's DIV rounds toward zero as its other operations (the same comparison: its quotients were the chopped ones).
    return chop([&] { return a / b; });
}
float ps2_fpu_cvt_s_w(std::int32_t value) {
    return chop([&] { return static_cast<float>(value); });
}
std::int32_t ps2_fpu_cvt_w_s(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    // Magnitudes below 2^31 truncate; larger (including clamped extremes) saturate by sign.
    if ((bits & 0x7F800000u) <= 0x4E800000u) return static_cast<std::int32_t>(normalize(value));
    return (bits & 0x80000000u) ? INT32_MIN : INT32_MAX;
}
bool ps2_fpu_c_eq(float left, float right) {
    return normalize(left) == normalize(right);
}
bool ps2_fpu_c_lt(float left, float right) {
    return normalize(left) < normalize(right);
}
bool ps2_fpu_c_le(float left, float right) {
    return normalize(left) <= normalize(right);
}
