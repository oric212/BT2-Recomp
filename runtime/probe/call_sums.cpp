#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstring>
#include <fstream>
#include <algorithm>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// BT2_EE_CALL_SUMS (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 9): the port's side of the input delay
// measured in PCSX2 by the comparison sessions' logging sites (tools/pcsx2_compare/context_block.py). At every
// guest call made from one of the given call addresses it writes one line: the call address, the field (VBlanks
// delivered), the display flips, the word at an address, and checksums of address ranges computed as the PCSX2 stub does (each 32-bit word added,
// the sum rotated left by one). A range in VU1 data memory is given at the EE address the EE sees it
// (0x1100C000 to 0x11010000). The spec: "<file>:<hex call address>[,<hex call address>...]:<hex word>[:<start>-<end>...]".
namespace {
struct CallSums {
    std::ofstream out;
    std::vector<std::uint32_t> sources;std::uint32_t word=0;
    struct Range {std::uint32_t start,end;};
    std::vector<Range> ranges;
};

constexpr std::uint32_t vu1_data=0x1100C000,vu1_data_size=0x4000,ram_size=0x2000000;

bool inside(std::uint32_t start,std::uint32_t end) {
    return (start>=vu1_data && end<=vu1_data+vu1_data_size) || end<=ram_size;
}

std::uint32_t checksum(std::span<const std::uint8_t> bytes) {
    std::uint32_t total=0;
    for(std::size_t at=0;at+4<=bytes.size();at+=4) {
        std::uint32_t word;std::memcpy(&word,bytes.data()+at,4);
        total+=word;total=(total<<1)|(total>>31);
    }
    return total;
}

std::span<const std::uint8_t> bytesOf(EeRuntime& runtime,std::uint32_t start,std::uint32_t end) {
    const auto& vu1=runtime.vif1.vu.data;
    static_assert(sizeof vu1==vu1_data_size);
    if(start>=vu1_data && end<=vu1_data+vu1.size())
        return {reinterpret_cast<const std::uint8_t*>(vu1.data())+(start-vu1_data),end-start};
    const auto ram=runtime.memory();
    if(end<=ram.size())return {ram.data()+start,end-start};
    throw std::runtime_error("BT2_EE_CALL_SUMS: a range outside EE RAM and VU1 data memory");
}
}

void installCallSums(EeRuntime& runtime,const std::string& spec) {
    std::vector<std::string> part;
    for(std::size_t at=0;;) {
        // The file may hold a drive letter's colon: split from the right after it.
        const auto end=spec.find(':',at==0&&spec.size()>2&&spec[1]==':'?2:at);
        part.push_back(spec.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(part.size()<3)throw std::runtime_error("BT2_EE_CALL_SUMS is <file>:<hex call address>[,...]:<hex word>[:<start>-<end>...]");
    auto sums=std::make_shared<CallSums>();
    sums->out.open(part[0]);
    if(!sums->out)throw std::runtime_error("Cannot write the call sums "+part[0]);
    for(std::size_t at=0;at<part[1].size();) {
        const auto end=std::min(part[1].find(',',at),part[1].size());
        sums->sources.push_back(static_cast<std::uint32_t>(std::stoul(part[1].substr(at,end-at),nullptr,16)));
        at=end+1;
    }
    sums->word=static_cast<std::uint32_t>(std::stoul(part[2],nullptr,16));
    for(std::size_t k=3;k<part.size();++k) {
        const auto dash=part[k].find('-');
        if(dash==std::string::npos)throw std::runtime_error("BT2_EE_CALL_SUMS: a range is <start>-<end>");
        const CallSums::Range range{static_cast<std::uint32_t>(std::stoul(part[k].substr(0,dash),nullptr,16)),
                                    static_cast<std::uint32_t>(std::stoul(part[k].substr(dash+1),nullptr,16))};
        if(range.start%4||range.end%4||range.end<range.start)throw std::runtime_error("BT2_EE_CALL_SUMS: ranges are 4-byte aligned");
        if(!inside(range.start,range.end))throw std::runtime_error("BT2_EE_CALL_SUMS: a range outside EE RAM and VU1 data memory");
        sums->ranges.push_back(range);
    }
    if(sums->word+4>ram_size)throw std::runtime_error("BT2_EE_CALL_SUMS: the word lies outside EE RAM");
    runtime.on_guest_call=[&runtime,sums](const R5900Context&,std::uint32_t source,std::uint32_t) {
        if(std::find(sums->sources.begin(),sums->sources.end(),source)==sums->sources.end())return;
        std::uint32_t word;std::memcpy(&word,runtime.memory().data()+sums->word,4);
        sums->out<<std::hex<<source<<std::dec<<' '<<runtime.vblank_deliveries<<' '<<runtime.gs_display_flips<<' '<<word;
        for(const auto& range:sums->ranges)sums->out<<' '<<checksum(bytesOf(runtime,range.start,range.end));
        sums->out<<'\n';
    };
}
