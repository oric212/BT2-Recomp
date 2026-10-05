#pragma once
#include "ps2_runtime.h"
#include <bit>

inline std::uint32_t GPR_U32(const R5900Context* context, unsigned index) {
    return index == 0 ? 0 : static_cast<std::uint32_t>(context->r.at(index).low);
}

inline std::int32_t GPR_S32(const R5900Context* context, unsigned index) {
    return std::bit_cast<std::int32_t>(GPR_U32(context, index));
}

inline std::uint32_t SRL32(std::uint32_t value, unsigned shift) {
    return value >> (shift & 31u);
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
inline std::int64_t GPR_S64(const R5900Context* context, unsigned index) {
    return std::bit_cast<std::int64_t>(GPR_U64(context, index));
}
inline void SET_GPR_S64(R5900Context* context, unsigned index, std::int64_t value) {
    if (index) context->r.at(index).low = std::bit_cast<std::uint64_t>(value);
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
inline void SET_GPR_VEC(R5900Context* context, unsigned index, __m128i value) {
    SET_GPR_VEC(context, index, std::bit_cast<ProbeGpr>(value));
}
inline ProbeGpr PS2_PNOR(ProbeGpr left,ProbeGpr right) {
    return {~(left.low|right.low),~(left.high|right.high)};
}
inline ProbeGpr PS2_PAND(ProbeGpr left,ProbeGpr right) {
    return {left.low&right.low,left.high&right.high};
}
inline ProbeGpr PS2_PSUBB(ProbeGpr left,ProbeGpr right) {
    auto a=std::bit_cast<std::array<std::uint8_t,16>>(left);
    const auto b=std::bit_cast<std::array<std::uint8_t,16>>(right);
    for(unsigned n=0;n<16;++n)a[n]=static_cast<std::uint8_t>(a[n]-b[n]);
    return std::bit_cast<ProbeGpr>(a);
}
inline ProbeGpr PS2_PXOR(ProbeGpr left,ProbeGpr right) {
    return {left.low^right.low,left.high^right.high};
}
inline ProbeGpr PS2_POR(ProbeGpr left,ProbeGpr right) {
    return {left.low|right.low,left.high|right.high};
}
inline ProbeGpr PS2_PCPYLD(ProbeGpr left, ProbeGpr right) {
    return {right.low, left.low};
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

#define READ16(address) runtime->read16(rdram, (address))
#define WRITE16(address, value) runtime->write16(rdram, (address), (value))
#define WRITE8(address, value) runtime->write8(rdram, (address), (value))
#define READ128(address) runtime->read128(rdram, (address))
#define FAST_READ128(address) runtime->read128(rdram, (address))
#define WRITE128(address, value) runtime->write128(rdram, (address), (value))
#define READ32(address) runtime->Load32(rdram, ctx, (address))

#define WRITE64(address, value) runtime->write64(rdram, (address), (value))
#define READ64(address) runtime->read64(rdram, (address))

#define WRITE32(address, value) runtime->Store32(rdram, ctx, (address), (value))
// Generated fast path still uses the same checked bootstrap memory API.
#define FAST_WRITE32(address, value) WRITE32(address, value)
// Optional upstream diagnostic hook only; the following store performs validation.
inline void ps2TraceGuestWrite(std::uint8_t*, std::uint32_t, unsigned, std::uint64_t,
                              std::uint64_t, const char*, R5900Context*) {}

#define FAST_READ32(address) READ32(address)

inline std::uint32_t SLL32(std::uint32_t value, unsigned amount) { return value << (amount & 31); }

inline void SET_GPR_ZE32(R5900Context* context, unsigned index, std::uint32_t value) {
    SET_GPR_U64(context, index, value);
}
#define READ8(address) runtime->read8(rdram, (address))

inline std::uint32_t SUB32(std::uint32_t left, std::uint32_t right) { return left - right; }

inline std::uint32_t SRA32(std::uint32_t value, unsigned shift) {
    return std::bit_cast<std::uint32_t>(std::bit_cast<std::int32_t>(value) >> (shift & 31u));
}
