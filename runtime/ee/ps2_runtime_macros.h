#pragma once
#include "ps2_runtime.h"
#include "ps2_fpu.h"
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

// HI:LO as one 64-bit accumulator (low words of each) and its 32-bit sign-extended halves, for the
// R5900 multiply-accumulate instructions (MADD/MADDU/MSUB and pipeline-1 forms).
inline std::uint64_t Ps2HiLoToU64(std::uint64_t hi, std::uint64_t lo) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(hi)) << 32) | static_cast<std::uint32_t>(lo);
}
inline std::uint64_t Ps2SignExt32ToU64(std::uint32_t value) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));
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
inline ProbeGpr PS2_PSUBW(ProbeGpr left,ProbeGpr right) {
    auto words=std::bit_cast<std::array<std::uint32_t,4>>(left);
    const auto subtract=std::bit_cast<std::array<std::uint32_t,4>>(right);
    for(unsigned lane=0;lane<4;++lane)words[lane]-=subtract[lane];
    return std::bit_cast<ProbeGpr>(words);
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
// R5900 COP1: round-toward-zero, no Inf/NaN/denormals, saturating conversion (ps2_fpu.cpp).
inline float FPU_ADD_S(float left, float right) { return ps2_fpu_add(left, right); }
inline float FPU_MUL_S(float left, float right) { return ps2_fpu_mul(left, right); }
inline float FPU_CVT_S_W(std::int32_t value) { return ps2_fpu_cvt_s_w(value); }
inline std::int32_t FPU_CVT_W_S(float value) { return ps2_fpu_cvt_w_s(value); }
inline float FPU_MOV_S(float value) {return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value));}
inline bool FPU_C_EQ_S(float left,float right) { return ps2_fpu_c_eq(left, right); }
inline float FPU_SUB_S(float left, float right) { return ps2_fpu_sub(left, right); }
inline float FPU_SQRT_S(float value) { return ps2_fpu_sqrt(value); }
// NEG/ABS only change the sign bit, as on the EE FPU.
inline float FPU_NEG_S(float value) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) ^ 0x80000000u); }
inline float FPU_ABS_S(float value) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) & 0x7FFFFFFFu); }
inline bool FPU_C_OLT_S(float left, float right) { return ps2_fpu_c_lt(left, right); }
inline bool FPU_C_OLE_S(float left, float right) { return ps2_fpu_c_le(left, right); }
inline void FPU_SET_ACC(R5900Context* context, float value) { context->f_acc = value; }

#define READ16(address) runtime->read16(rdram, (address))
#define WRITE16(address, value) runtime->write16(rdram, (address), (value))
#define WRITE8(address, value) runtime->write8(rdram, (address), (value))
#define FAST_WRITE8(address, value) WRITE8(address, value)
#define READ128(address) runtime->read128(rdram, (address))
#define FAST_READ128(address) runtime->read128(rdram, (address))
#define WRITE128(address, value) runtime->write128(rdram, (address), (value))
#define READ32(address) runtime->Load32(rdram, ctx, (address))

#define WRITE64(address, value) runtime->guestWrite64(rdram, ctx, (address), (value))
#define READ64(address) runtime->guestRead64(rdram, ctx, (address))
#define FAST_READ64(address) runtime->read64(rdram, (address))

#define WRITE32(address, value) runtime->Store32(rdram, ctx, (address), (value))
// Generated fast path still uses the same checked bootstrap memory API.
#define FAST_WRITE32(address, value) WRITE32(address, value)
// Optional upstream diagnostic hook only; the following store performs validation.
inline void ps2TraceGuestWrite(std::uint8_t*, std::uint32_t, unsigned, std::uint64_t,
                              std::uint64_t, const char*, R5900Context*) {}

#define FAST_READ32(address) READ32(address)
// Remaining fast-path widths use the same checked memory API as their plain forms.
#define FAST_WRITE16(address, value) WRITE16(address, value)
#define FAST_READ16(address) READ16(address)
#define FAST_WRITE64(address, value) WRITE64(address, value)
#define FAST_WRITE128(address, value) WRITE128(address, value)
#define FAST_READ8(address) READ8(address)

inline std::uint32_t SLL32(std::uint32_t value, unsigned amount) { return value << (amount & 31); }

inline void SET_GPR_ZE32(R5900Context* context, unsigned index, std::uint32_t value) {
    SET_GPR_U64(context, index, value);
}
#define READ8(address) runtime->read8(rdram, (address))

inline std::uint32_t SUB32(std::uint32_t left, std::uint32_t right) { return left - right; }

inline std::uint32_t SRA32(std::uint32_t value, unsigned shift) {
    return std::bit_cast<std::uint32_t>(std::bit_cast<std::int32_t>(value) >> (shift & 31u));
}
