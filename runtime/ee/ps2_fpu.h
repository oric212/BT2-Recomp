#pragma once
#include <cstdint>

// R5900 COP1 single-precision operations; see ps2_fpu.cpp for the modeled semantics.
float ps2_fpu_add(float left, float right);
float ps2_fpu_sub(float left, float right);
float ps2_fpu_mul(float left, float right);
float ps2_fpu_div(float left, float right); // DIV.S: to nearest (generated code, tools/ps2recomp/float_rewrite.py).
float ps2_vu_div(float left, float right);  // VU0's DIV and RSQRT: toward zero.
// VU0 macro-mode add and subtract: toward zero without the EE FPU's operand alignment (ps2_fpu.cpp).
float ps2_vu_add(float left, float right);
float ps2_vu_sub(float left, float right);
float ps2_fpu_sqrt(float value);
float ps2_fpu_cvt_s_w(std::int32_t value);
std::int32_t ps2_fpu_cvt_w_s(float value);
bool ps2_fpu_c_eq(float left, float right);
bool ps2_fpu_c_lt(float left, float right);
bool ps2_fpu_c_le(float left, float right);
