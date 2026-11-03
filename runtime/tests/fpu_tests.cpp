// The EE FPU's add, subtract, multiply and divide (runtime/ee/ps2_fpu.cpp) against a reference that rounds toward zero by
// setting MXCSR around each operation, then clamps and flushes as the EE does: special values, values near the
// smallest normal number and overflow, and random operands of every sign and exponent.
#include "ee/ps2_fpu.h"
#include <bit>
#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <random>
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
float reference(float left, float right, Operation operation) {
    const float a = normalize(left), b = normalize(right);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(saved | 0x6000u);
    const volatile float result = operation(a, b);
    _mm_setcsr(saved);
    return normalize(result);
}
// The EE adder's operands: the one of the smaller exponent keeps only (24 - difference + 1) significant bits, or
// none from a difference of 25 (written separately from ps2_fpu.cpp's: by the significand, not by a word mask).
std::pair<float, float> aligned(float a, float b) {
    const auto x = std::bit_cast<std::uint32_t>(normalize(a)), y = std::bit_cast<std::uint32_t>(normalize(b));
    const unsigned ex = (x >> 23) & 0xFF, ey = (y >> 23) & 0xFF;
    const auto cut = [](std::uint32_t word, unsigned difference) {
        if (difference >= 25) return word & 0x80000000u;
        const std::uint32_t dropped = difference > 1 ? (1u << (difference - 1)) - 1 : 0;
        return word & ~dropped;
    };
    if (ex > ey) return {std::bit_cast<float>(x), std::bit_cast<float>(cut(y, ex - ey))};
    return {std::bit_cast<float>(cut(x, ey - ex)), std::bit_cast<float>(y)};
}
unsigned failures = 0;
void compare(std::uint32_t x, std::uint32_t y) {
    const float a = std::bit_cast<float>(x), b = std::bit_cast<float>(y);
    const auto same = [](float p, float q) { return std::bit_cast<std::uint32_t>(p) == std::bit_cast<std::uint32_t>(q); };
    const auto [sa, sb] = aligned(a, b);
    const auto [da, db] = aligned(a, -b);
    const bool add = same(ps2_fpu_add(a, b), reference(sa, sb, [](float p, float q) { return p + q; }));
    const bool sub = same(ps2_fpu_sub(a, b), reference(da, db, [](float p, float q) { return p + q; }));
    // VU0's add and subtract: toward zero without the alignment.
    const bool vu_add = same(ps2_vu_add(a, b), reference(a, b, [](float p, float q) { return p + q; }))
                     && same(ps2_vu_sub(a, b), reference(a, b, [](float p, float q) { return p - q; }));
    const bool mul = same(ps2_fpu_mul(a, b), reference(a, b, [](float p, float q) { return p * q; }));
    // A zero divisor (after flushing) gives the largest magnitude, signed as the quotient.
    const bool zero = !(std::bit_cast<std::uint32_t>(normalize(b)) & 0x7FFFFFFFu);
    // Division rounds to nearest (ps2_fpu_div), then flushes and clamps as the others.
    const float quotient = zero ? std::bit_cast<float>(((x ^ y) & 0x80000000u) | 0x7F7FFFFFu) : normalize(normalize(a) / normalize(b));
    const bool div = same(ps2_fpu_div(a, b), quotient);
    // VU0's division rounds toward zero.
    const float chopped = zero ? quotient : reference(a, b, [](float p, float q) { return p / q; });
    const bool vu_div = same(ps2_vu_div(a, b), chopped);
    if ((!add || !sub || !mul || !div || !vu_div || !vu_add) && failures++ < 10)
        std::cerr << "FPU differs for " << std::hex << x << ' ' << y << std::dec << (add ? "" : " add") << (sub ? "" : " sub") << (mul ? "" : " mul") << (div ? "" : " div") << (vu_div ? "" : " vu div") << (vu_add ? "" : " vu add") << '\n';
}
}

int main() {
    const std::uint32_t special[] = {0, 0x80000000u, 1, 0x807FFFFFu, 0x00800000u, 0x80800000u, 0x00800001u, 0x00FFFFFFu, 0x01000000u,
                                     0x3F800000u, 0xBF800000u, 0x3F800001u, 0x3FFFFFFFu, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x7F000000u,
                                     0x7F800000u, 0xFF800000u, 0x7FC00000u, 0x1F800000u, 0x20000000u, 0x5F800000u, 0x33800000u};
    for (const auto x : special)
        for (const auto y : special) compare(x, y);
    std::mt19937 random(12345);
    for (unsigned n = 0; n < 4000000; ++n) {
        std::uint32_t x = random(), y = random();
        // Half of the pairs with exponents close together (cancellation, products near 1).
        if (n & 1) y = (y & 0x807FFFFFu) | ((x & 0x7F800000u) + ((random() % 16u) << 23) - (8u << 23)) % 0x80000000u;
        compare(x, y);
    }
    // Sums at the bottom of the range (results below the smallest normal number and exact cancellations, x + -x)
    // and at the top (overflow): ps2_fpu.cpp gives these without changing the rounding mode.
    for (unsigned n = 0; n < 400000; ++n) {
        const std::uint32_t low = (random() & 0x807FFFFFu) | ((1u + random() % 3u) << 23), low2 = (random() & 0x807FFFFFu) | ((1u + random() % 3u) << 23);
        const std::uint32_t high = (random() & 0x807FFFFFu) | ((0xFCu + random() % 3u) << 23), high2 = (random() & 0x807FFFFFu) | ((0xFCu + random() % 3u) << 23);
        compare(low, low2);compare(low, low ^ 0x80000000u);compare(high, high2);compare(high, high ^ 0x80000000u);
    }
    if (failures) {
        std::cerr << failures << " FPU results differ\n";
        return 1;
    }
    std::cout << "FPU checks passed\n";
}
