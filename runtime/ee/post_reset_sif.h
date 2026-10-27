#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

// The original's SIF handshake after the IOP reset (a capture): the DMAC status and SIF0 control the
// EE reads, the SIF registers, the two INIT_CMD packets and the IOP's SET_SREG reply. The general SIF
// (services/sif_system.cpp) answers the handshake; these are diagnostics and the evidence it is
// checked against (post_reset_sif_tests.cpp).
class PostResetSif {
public:
    static constexpr std::array<std::uint32_t,34> evidence={
        0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640,
        1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640,
        0x1E640,0x1E6C0,0,0x1E730,0x1E640,0x1E6C0,0x3A94C0,0x1E730,
        0,0,0x80000001,0,0,1,0,1};
    void supply(std::span<const std::uint8_t> captured) {
        if(supplied || captured.size()!=sizeof(evidence) || std::memcmp(captured.data(),evidence.data(),sizeof(evidence)))
            throw std::runtime_error("Invalid fresh SIF transport evidence");
        supplied=true;
    }
    bool supplied=false,rpc_initialized=false;
    unsigned submissions=0,sreg_responses=0; // INIT_CMD packets and SET_SREG replies after the reset.
    std::uint32_t ee_receive_address=0;
};

// The CD-init and S-command replies after the reset (a capture), checked against the native disc
// service's answers; the counters count its requests after the reset (diagnostics).
class PostResetCd {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        std::array<std::uint32_t,85> expected{};
        for(unsigned n : {0u,16u}) {
            expected[n+2]=0x80000008;expected[n+5]=0x203A97C0;
            expected[n+7]=0x3A87A8;expected[n+8]=0x80000009;
        }
        expected[25]=0x43388;expected[26]=0x441B8;
        expected[32]=0x1000;expected[33]=0x3695C0;expected[34]=0x80000008;
        expected[39]=0x3A87A8;expected[40]=0x8000000A;
        expected[48]=1;expected[49]=expected[50]=0x225;
        expected[54]=0x80000008;expected[57]=0x203A97C0;expected[59]=0x369F40;expected[60]=0x80000009;
        expected[61]=0x434F0;expected[62]=0x439A8;
        expected[68]=0x400;expected[69]=0x3695C0;expected[70]=0x80000008;expected[75]=0x369F40;expected[76]=0x8000000A;expected[84]=1;
        if(supplied || (evidence.size()!=208 && evidence.size()!=340) || std::memcmp(evidence.data(),expected.data(),evidence.size()))
            throw std::runtime_error("Fresh CD evidence differs");
        supplied=true;scmd_supplied=evidence.size()==340;
    }
    bool supplied=false;
    bool scmd_supplied=false;
    unsigned binds=0,calls=0,scmd_binds=0,modes=0;
    unsigned operations() const {return binds+calls+scmd_binds+modes;}
};

// LOADFILE's bind and version reply after the reset (a capture), checked against the native module
// service's answers; the counters count those requests (diagnostics).
class PostResetLoadfile {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        std::array<std::uint32_t,33> expected{};
        expected[2]=0x80000008;expected[5]=0x203A97C0;expected[7]=0x3ACC00;
        expected[8]=0x80000009;expected[9]=0x22568;expected[10]=0x225B0;
        expected[16]=0x400;expected[17]=0x3ACA00;expected[18]=0x80000008;
        expected[23]=0x3ACC00;expected[24]=0x8000000A;expected[32]=0x30303033;
        if(supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("LOADFILE evidence differs");
        supplied=true;
    }
    bool supplied=false;
    unsigned binds=0,versions=0;
    unsigned operations() const {return binds+versions;}
};
