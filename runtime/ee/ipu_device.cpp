#include "ipu_device.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

// IPU register and command model (see ipu_device.h). Register semantics follow the public IPU
// documentation; commands complete synchronously once their input is available.

namespace {
constexpr std::uint32_t ipu_cmd=0x10002000,ipu_ctrl=0x10002010,ipu_bp=0x10002020,ipu_top=0x10002030,ipu_in_fifo=0x10007010;
constexpr std::uint32_t from_base=0x1000B000,to_base=0x1000B400;
std::string hex(std::uint32_t value) {char text[12];std::snprintf(text,sizeof text,"0x%X",value);return text;}
}

std::optional<std::uint32_t> IpuDevice::read32(std::uint32_t address) {
    switch(address) {
    case ipu_cmd: return command_result_;
    case ipu_cmd+4: return busy()?0x80000000u:0u;              // IPU_CMD BUSY (bit 63).
    // IFC (bits 0-3) is the input FIFO quadword count; BUSY is bit 31.
    case ipu_ctrl: return decode_parameters|input.fifoQwords()|(busy()?0x80000000u:0u);
    case ipu_bp: return input.bpRegister();
    case ipu_ctrl+4: case ipu_bp+4: return 0u;                    // Upper words of IPU_CTRL/IPU_BP.
    case ipu_top: return input.available(32)?input.peek(32):0u;  // BSTOP: the next 32 bits.
    case ipu_top+4: return input.available(32)?0u:0x80000000u;  // IPU_TOP BUSY until 32 bits are present.
    }
    if(address==from_base)return from_ipu.chcr;
    if(address==from_base+0x10)return from_ipu.madr;
    if(address==from_base+0x20)return from_ipu.qwc;
    if(address==to_base)return input.channel.chcr;
    if(address==to_base+0x10)return input.channel.madr;
    if(address==to_base+0x20)return input.channel.qwc;
    if(address==to_base+0x30)return input.channel.tadr;
    return std::nullopt;
}

bool IpuDevice::write32(std::uint32_t address,std::uint32_t value) {
    if(address==ipu_ctrl) {
        if(value&0x40000000u) {reset();return true;} // RST.
        // Decode parameters (bits 16-26): IDP, AS, IVF, QST, MP1, PCT. Retained for decode commands.
        if(value&~decode_parameter_mask)throw std::runtime_error("Unsupported IPU control write: "+hex(value));
        decode_parameters=value;return true;
    }
    if(address==ipu_cmd) {command(value);return true;}
    if(address==from_base) {
        if(value&0x100u)throw std::runtime_error("fromIPU DMA start is not modelled (CHCR "+hex(value)+", MADR "+hex(from_ipu.madr)+", QWC "+hex(from_ipu.qwc)+")");
        from_ipu.chcr=value;return true;
    }
    if(address==from_base+0x10) {from_ipu.madr=value;return true;}
    if(address==from_base+0x20) {from_ipu.qwc=value&0xFFFF;return true;}
    auto& to=input.channel;
    if(address==to_base) {
        // Clearing STR stops a running transfer; MADR/QWC/TADR and the FIFO contents are kept, and a later
        // start continues from them (a chain start with QWC > 0 first finishes the current tag's data).
        if((to.chcr&0x100u) && (value&0x100u))throw std::runtime_error("toIPU CHCR start "+hex(value)+" while the transfer runs");
        if(value&0x100u) {input.start(value,ram);retry();} else to.chcr=(to.chcr&0xFFFF0000u)|(value&0xFFFFu);
        return true;
    }
    if(to.chcr&0x100u && (address==to_base+0x10 || address==to_base+0x20 || address==to_base+0x30))
        throw std::runtime_error("toIPU register write while the transfer runs");
    if(address==to_base+0x10) {to.madr=value;return true;}
    if(address==to_base+0x20) {to.qwc=value&0xFFFF;return true;}
    if(address==to_base+0x30) {to.tadr=value;return true;}
    return false;
}

bool IpuDevice::writeFifo(std::uint32_t address,const std::array<std::uint32_t,4>& qword) {
    if(address!=ipu_in_fifo)return false;
    std::array<std::uint8_t,16> bytes;std::memcpy(bytes.data(),qword.data(),16);
    input.push(bytes);retry();return true;
}

void IpuDevice::reset() {
    input.clear(0);command_result_=0;pending_=0;decode_parameters=0;++resets;
}

void IpuDevice::command(std::uint32_t value) {
    if(busy())throw std::runtime_error("IPU command "+hex(value)+" while "+hex(pending_)+" is pending");
    if(execute(value))++commands; else pending_=value;
}

bool IpuDevice::execute(std::uint32_t value) {
    const unsigned code=value>>28;
    switch(code) {
    case 0x0: input.clear(value&0x7F);break; // BCLR: clear the input FIFO, set the bit pointer.
    case 0x4: { // FDEC: skip FB bits, then IPU_CMD holds the next 32 bits (not consumed).
        const unsigned fb=value&0x3F;
        if(!input.available(fb+32))return false;
        input.skip(fb);command_result_=input.peek(32);return true;
    }
    case 0x5: { // SETIQ: bit 27 selects the non-intra table; 64 bytes from the input.
        const bool nonintra=(value>>27)&1;
        if(!input.takeQwords((nonintra?nonintra_iq:intra_iq).data(),4))return false;
        iq_loaded[nonintra]=true;break;
    }
    case 0x6: {std::array<std::uint8_t,32> bytes{};if(!input.takeQwords(bytes.data(),2))return false;std::memcpy(vq_clut.data(),bytes.data(),32);vq_loaded=true;break;} // SETVQ.
    case 0x9: thresholds=value&0x01FF01FFu;break; // SETTH.
    default: {
        static const char* names[16]={"BCLR","IDEC","BDEC","VDEC","FDEC","SETIQ","SETVQ","CSC","PACK","SETTH","?","?","?","?","?","?"};
        throw std::runtime_error(std::string("IPU decode command is not modelled: ")+names[code]+" "+hex(value));
    }
    }
    command_result_=0;return true;
}
