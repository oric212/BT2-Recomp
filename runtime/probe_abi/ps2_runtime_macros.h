#pragma once
#include "ps2_runtime.h"

inline std::uint32_t GPR_U32(const R5900Context* context, unsigned index) {
    return index == 0 ? 0 : static_cast<std::uint32_t>(context->r.at(index).low);
}

inline std::uint32_t ADD32(std::uint32_t left, std::uint32_t right) {
    return left + right;
}

inline void SET_GPR_S32(R5900Context* context, unsigned index, std::int32_t value) {
    if (index != 0) context->r.at(index).low = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
}
