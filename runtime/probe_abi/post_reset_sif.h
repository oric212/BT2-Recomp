#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

// A separate diagnostic transport lifetime. It owns no old RPC services.
class PostResetSif {
public:
    void supply(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,34> expected={
            0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640,
            1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640,
            0x1E640,0x1E6C0,0,0x1E730,0x1E640,0x1E6C0,0x3A94C0,0x1E730,
            0,0,0x80000001,0,0,1,0,1};
        if(supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("Invalid fresh SIF transport evidence");
        supplied=true;
    }
    void advance(unsigned expected) {
        if(!supplied || phase!=expected)throw std::runtime_error("Unsupported fresh SIF order");
        ++phase;
    }
    std::uint32_t status() {advance(0);return 0x800044;}
    std::uint32_t channel() {advance(1);return 0x84;}
    void chain() {advance(2);}
    void handler() {advance(3);}
    void enable() {advance(4);}
    std::uint32_t get(std::uint32_t reg) {
        if(phase==5 && reg==0x80000000) {advance(5);return 0;}
        if(phase==6 && reg==4) {advance(6);return 0x70000;}
        if(phase==7 && reg==2) {advance(7);return 0x1E640;}
        if(phase==11 && reg==0x80000002) {advance(11);return 0;}
        throw std::runtime_error("Unsupported fresh SIF query");
    }
    void set(std::uint32_t reg,std::uint32_t value) {
        if(phase==8 && reg==0x80000000 && value==0x1E640) {advance(8);return;}
        if(phase==9 && reg==0x80000001 && value==0x3A9598) {advance(9);return;}
        if(phase==13 && reg==0x80000002 && value==1) {advance(13);rpc_initialized=true;return;}
        throw std::runtime_error("Unsupported fresh SIF register mutation");
    }
    // INIT_CMD opt 0 installs EE receive ownership; opt 1 requests RPC init.
    // IDs are authored nonzero logical tokens, not captured BIOS DMA IDs.
    std::uint32_t submit(std::span<const std::uint8_t> packet) {
        const unsigned opt=phase==10?0u:1u;
        if(!supplied || (phase!=10 && phase!=12) || packet.size()!=(opt?16u:20u))
            throw std::runtime_error("Unsupported fresh INIT_CMD submission");
        std::array<std::uint32_t,5> words{};std::memcpy(words.data(),packet.data(),packet.size());
        if(words[0]!=packet.size() || words[1]!=0 || words[2]!=0x80000002 || words[3]!=opt
            || (!opt && words[4]!=0x3A94C0))throw std::runtime_error("Fresh INIT_CMD payload differs");
        advance(opt?12:10);++submissions;
        if(!opt)ee_receive_address=0x3A94C0;
        else ++sreg_responses;
        return 0x10000u+submissions;
    }
    bool supplied=false,rpc_initialized=false;
    unsigned phase=0,submissions=0,sreg_responses=0;
    std::uint32_t ee_receive_address=0;
};
