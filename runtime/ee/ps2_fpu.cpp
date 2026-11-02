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
float addTowardZero(float a, float b) {
    // Fast2Sum: with |a| >= |b|, b - (s - a) is the exact error of s = a + b.
    if (std::fabs(a) < std::fabs(b)) std::swap(a, b);
    const float s = a + b, error = b - (s - a);
    float result;
    if (chopFromNearest(s, error, 0x00800000u, result)) return result;
    return chop([&] { return a + b; });
}
const bool has_fma = [] {
    int registers[4];
    __cpuid(registers, 1);
    // FMA3, and the system saving the AVX registers (OSXSAVE, then XCR0's SSE and AVX state).
    if ((registers[2] & (1 << 12)) == 0 || (registers[2] & (1 << 27)) == 0) return false;
    return (_xgetbv(0) & 6u) == 6u;
}();
}

float ps2_fpu_add(float left, float right) {
    return addTowardZero(normalize(left), normalize(right));
}
float ps2_fpu_sub(float left, float right) {
    return addTowardZero(normalize(left), -normalize(right));
}
float ps2_fpu_sqrt(float value) {
    // SQRT.S uses |value| (a negative operand only raises the unmodeled invalid flag).
    const float a = std::bit_cast<float>(std::bit_cast<std::uint32_t>(normalize(value)) & 0x7FFFFFFFu);
    return chop([&] { return std::sqrt(a); });
}
float ps2_fpu_mul(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    if (has_fma) {
        // The product's exact error from a fused multiply-add.
        const float product = a * b;
        const float error = _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(-product)));
        float result;
        if (chopFromNearest(product, error, 0x0C800000u, result)) return result;
    }
    return chop([&] { return a * b; });
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
