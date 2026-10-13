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
}

float ps2_fpu_add(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    return chop([&] { return a + b; });
}
float ps2_fpu_sub(float left, float right) {
    const float a = normalize(left), b = normalize(right);
    return chop([&] { return a - b; });
}
float ps2_fpu_sqrt(float value) {
    // SQRT.S uses |value| (a negative operand only raises the unmodeled invalid flag).
    const float a = std::bit_cast<float>(std::bit_cast<std::uint32_t>(normalize(value)) & 0x7FFFFFFFu);
    return chop([&] { return std::sqrt(a); });
}
float ps2_fpu_mul(float left, float right) {
    const float a = normalize(left), b = normalize(right);
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
