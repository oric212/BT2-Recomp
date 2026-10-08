#include "ipu_device.h"
#include <cstring>
#include <stdexcept>
#include <string>

// IPU setup model (see ipu_device.h). Register semantics follow the public IPU documentation as
// adapted by PS2Recomp's sceIpuInit handling and PCSX2's IPU registers; decode is out of scope.

namespace {
constexpr std::uint32_t ipu_cmd=0x10002000,ipu_ctrl=0x10002010,ipu_bp=0x10002020,ipu_top=0x10002030,ipu_in_fifo=0x10007010;
constexpr std::uint32_t from_base=0x1000B000,to_base=0x1000B400;
}

std::optional<std::uint32_t> IpuDevice::read32(std::uint32_t address) const {
    switch(address) {
    case ipu_cmd: return command_result_;
    // Commands complete at once: BUSY (bit 31) is clear; IFC (bits 0-3) is the input FIFO quadword count.
    case ipu_ctrl: return decode_parameters|static_cast<std::uint32_t>(in_fifo_.size());
    case ipu_bp: return bit_pointer_|(static_cast<std::uint32_t>(in_fifo_.size())<<8);
    case ipu_top: return 0;
    }
    for(const auto& [base,channel]:{std::pair{from_base,&from_ipu},std::pair{to_base,&to_ipu}}) {
        if(address==base)return channel->chcr;
        if(address==base+0x10)return channel->madr;
        if(address==base+0x20)return channel->qwc;
        if(address==base+0x30)return channel->tadr;
    }
    return std::nullopt;
}

bool IpuDevice::write32(std::uint32_t address,std::uint32_t value) {
    if(address==ipu_ctrl) {
        if(value&0x40000000u) {reset();return true;} // RST.
        // Decode parameters (bits 16-26): IDP, AS, IVF, QST, MP1, PCT. Retained for decode commands.
        if(value&~decode_parameter_mask)throw std::runtime_error("Unsupported IPU control write: "+std::to_string(value));
        decode_parameters=value;return true;
    }
    if(address==ipu_cmd) {command(value);return true;}
    for(const auto& [base,channel]:{std::pair{from_base,&from_ipu},std::pair{to_base,&to_ipu}}) {
        if(address==base) {
            if(value&0x100u)throw std::runtime_error("IPU DMA start is not modelled (channel "+std::string(base==from_base?"3":"4")+")");
            channel->chcr=value;return true;
        }
        if(address==base+0x10) {channel->madr=value;return true;}
        if(address==base+0x20) {channel->qwc=value&0xFFFF;return true;}
        if(address==base+0x30) {channel->tadr=value;return true;}
    }
    return false;
}

bool IpuDevice::writeFifo(std::uint32_t address,const std::array<std::uint32_t,4>& qword) {
    if(address!=ipu_in_fifo)return false;
    if(in_fifo_.size()>=8)throw std::runtime_error("IPU input FIFO overflow");
    in_fifo_.push_back(qword);return true;
}

void IpuDevice::reset() {
    in_fifo_.clear();command_result_=0;bit_pointer_=0;decode_parameters=0;++resets;
}

void IpuDevice::take(std::uint8_t* out,unsigned qwords) {
    if(bit_pointer_ || in_fifo_.size()<qwords)throw std::runtime_error("IPU table command without its FIFO data");
    for(unsigned n=0;n<qwords;++n) {std::memcpy(out+n*16,in_fifo_.front().data(),16);in_fifo_.pop_front();}
}

void IpuDevice::command(std::uint32_t value) {
    const unsigned code=value>>28;
    switch(code) {
    case 0x0: in_fifo_.clear();bit_pointer_=value&0x7F;break; // BCLR: clear the input FIFO, set the bit pointer.
    case 0x5: { // SETIQ: bit 27 selects the non-intra table; 64 bytes from the FIFO.
        const bool nonintra=(value>>27)&1;take((nonintra?nonintra_iq:intra_iq).data(),4);iq_loaded[nonintra]=true;break;
    }
    case 0x6: {std::array<std::uint8_t,32> bytes{};take(bytes.data(),2);std::memcpy(vq_clut.data(),bytes.data(),32);vq_loaded=true;break;} // SETVQ.
    case 0x9: thresholds=value&0x01FF01FFu;break; // SETTH.
    default: throw std::runtime_error("IPU decode command is not modelled: "+std::to_string(value));
    }
    command_result_=0;++commands;
}
