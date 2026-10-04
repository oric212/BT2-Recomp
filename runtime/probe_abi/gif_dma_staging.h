#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

class GifPacketMismatch : public std::runtime_error {
public:
    GifPacketMismatch(std::span<const std::uint8_t> expected,std::span<const std::uint8_t> actual)
        : std::runtime_error("GIF source packet differs from reference"),
          expected(expected.begin(),expected.end()),actual(actual.begin(),actual.end()) {}
    std::vector<std::uint8_t> expected,actual;
};

// First observed normal-mode GIF request only. No transfer is accepted here.
class GifDmaStaging {
public:
    void supply(std::span<const std::uint8_t> expected_packet) {
        if(supplied_ || expected_packet.size()!=240)throw std::runtime_error("Invalid/repeated GIF packet evidence");
        expected_.assign(expected_packet.begin(),expected_packet.end());supplied_=true;
    }
    std::uint32_t readIdle() {
        if(!supplied_ || phase_!=0)throw std::runtime_error("Unsupported/repeated GIF idle read");
        phase_=1;++idle_reads;return 0;
    }
    void setCount(std::uint32_t value) {
        if(phase_!=1 || value!=15)throw std::runtime_error("Unsupported GIF count/order");
        qwc=value;phase_=2;++staging_writes;
    }
    void setAddress(std::uint32_t value,std::uint32_t expected_address,std::span<const std::uint8_t> ram) {
        if(phase_!=2 || value!=expected_address || value%16 || value>ram.size()
            || expected_.size()>ram.size()-value)
            throw std::runtime_error("GIF source address/bounds/packet differs");
        auto actual=ram.subspan(value,expected_.size());
        unsigned padding_differences=0;
        bool matches=std::equal(expected_.begin(),expected_.begin()+16,actual.begin());
        for(std::size_t offset=16;offset<expected_.size();offset+=16) {
            std::uint64_t expected_value,expected_reg,actual_value,actual_reg;
            std::memcpy(&expected_value,expected_.data()+offset,8);
            std::memcpy(&expected_reg,expected_.data()+offset+8,8);
            std::memcpy(&actual_value,actual.data()+offset,8);
            std::memcpy(&actual_reg,actual.data()+offset+8,8);
            // Pinned GS register definitions: only bit 0 exists in these three.
            // The generated constructor preserves previous stack padding bits.
            const std::uint64_t mask=(expected_reg==0x1A || expected_reg==0x46 || expected_reg==0x45) ? 1 : UINT64_MAX;
            matches=matches && expected_reg==actual_reg && (expected_value&mask)==(actual_value&mask);
            if(mask==1 && expected_value!=actual_value)++padding_differences;
        }
        if(!matches)throw GifPacketMismatch(expected_,actual);
        reserved_difference_words=padding_differences;
        madr=value;phase_=3;++staging_writes;packet_verified=true;
    }
    bool ready() const {return phase_==3 && packet_verified;}
    unsigned idle_reads=0,staging_writes=0,reserved_difference_words=0;
    std::uint32_t qwc=0,madr=0;
    bool packet_verified=false;
private:
    bool supplied_=false;
    unsigned phase_=0;
    std::vector<std::uint8_t> expected_;
};
