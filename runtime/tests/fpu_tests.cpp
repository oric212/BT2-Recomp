// The EE FPU's add, subtract and multiply (runtime/ee/ps2_fpu.cpp) against a reference that rounds toward zero by
// setting MXCSR around each operation, then clamps and flushes as the EE does: special values, values near the
// smallest normal number and overflow, and random operands of every sign and exponent.
#include "ee/ps2_fpu.h"
#include <bit>
#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <random>
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
unsigned failures = 0;
void compare(std::uint32_t x, std::uint32_t y) {
    const float a = std::bit_cast<float>(x), b = std::bit_cast<float>(y);
    const auto same = [](float p, float q) { return std::bit_cast<std::uint32_t>(p) == std::bit_cast<std::uint32_t>(q); };
    const bool add = same(ps2_fpu_add(a, b), reference(a, b, [](float p, float q) { return p + q; }));
    const bool sub = same(ps2_fpu_sub(a, b), reference(a, b, [](float p, float q) { return p - q; }));
    const bool mul = same(ps2_fpu_mul(a, b), reference(a, b, [](float p, float q) { return p * q; }));
    if ((!add || !sub || !mul) && failures++ < 10)
        std::cerr << "FPU differs for " << std::hex << x << ' ' << y << std::dec << (add ? "" : " add") << (sub ? "" : " sub") << (mul ? "" : " mul") << '\n';
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
    if (failures) {
        std::cerr << failures << " FPU results differ\n";
        return 1;
    }
    std::cout << "FPU checks passed\n";
}
