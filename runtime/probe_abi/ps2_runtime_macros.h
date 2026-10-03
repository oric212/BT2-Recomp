#pragma once
#include "ps2_runtime.h"
#include <bit>

inline std::uint32_t GPR_U32(const R5900Context* context, unsigned index) {
    return index == 0 ? 0 : static_cast<std::uint32_t>(context->r.at(index).low);
}

inline std::uint32_t ADD32(std::uint32_t left, std::uint32_t right) {
    return left + right;
}

inline void SET_GPR_S32(R5900Context* context, unsigned index, std::int32_t value) {
    if (index != 0) context->r.at(index).low = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
}

inline std::uint64_t GPR_U64(const R5900Context* context, unsigned index) {
    return index == 0 ? 0 : context->r.at(index).low;
}
inline ProbeGpr GPR_VEC(const R5900Context* context, unsigned index) {
    return index == 0 ? ProbeGpr{} : context->r.at(index);
}
inline void SET_GPR_U64(R5900Context* context, unsigned index, std::uint64_t value) {
    if (index) context->r.at(index).low = value;
}
inline void SET_GPR_U32(R5900Context* context, unsigned index, std::uint32_t value) {
    SET_GPR_U64(context, index, value);
}
inline void SET_GPR_VEC(R5900Context* context, unsigned index, ProbeGpr value) {
    if (index) context->r.at(index) = value;
}
inline ProbeGpr ps2_paddu32(ProbeGpr left, ProbeGpr right) {
    auto a = std::bit_cast<std::array<std::uint32_t, 4>>(left);
    const auto b = std::bit_cast<std::array<std::uint32_t, 4>>(right);
    for (unsigned lane = 0; lane < 4; ++lane) {
        const auto sum = static_cast<std::uint64_t>(a[lane]) + b[lane];
        a[lane] = sum > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(sum);
    }
    return std::bit_cast<ProbeGpr>(a);
}
inline float FPU_ADD_S(float left, float right) {
    // Entry only adds two freshly cleared registers. Broader EE FPU behavior is unsupported.
    if (left != 0.0f || right != 0.0f) throw std::runtime_error("Unsupported bootstrap FPU operands");
    return left + right;
}
inline void FPU_SET_ACC(R5900Context* context, float value) { context->f_acc = value; }

#define WRITE8(address, value) runtime->write8(rdram, (address), (value))
#define WRITE128(address, value) runtime->write128(rdram, (address), (value))
#define READ32(address) runtime->read32(rdram, (address))
