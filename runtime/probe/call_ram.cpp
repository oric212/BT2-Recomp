#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// BT2_EE_CALL_RAM (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 5): EE RAM written whole (32 MB) before
// the N-th guest call made from one call address, the port's side of a comparison session's checkpoint at the
// same call (tools/pcsx2_compare/loaded_data.py compares the two by region). The spec:
// "<file>:<hex call address>:<call number, from 1>". Another guest-call hook installed before keeps running.
void installCallRam(EeRuntime& runtime,const std::string& spec) {
    // The file may hold a drive letter's colon: the two fields are the last two.
    const auto last=spec.rfind(':');
    const auto middle=last==std::string::npos||last==0?std::string::npos:spec.rfind(':',last-1);
    if(middle==std::string::npos||middle==1)
        throw std::runtime_error("BT2_EE_CALL_RAM is <file>:<hex call address>:<call number>");
    const std::string file=spec.substr(0,middle);
    const auto source=static_cast<std::uint32_t>(std::stoul(spec.substr(middle+1,last-middle-1),nullptr,16));
    const auto wanted=std::stoul(spec.substr(last+1));
    if(wanted==0)throw std::runtime_error("BT2_EE_CALL_RAM: calls are counted from 1");
    auto previous=std::move(runtime.on_guest_call);
    runtime.on_guest_call=[&runtime,file,source,wanted,previous,count=0ul](const R5900Context& context,std::uint32_t from,std::uint32_t target) mutable {
        if(previous)previous(context,from,target);
        if(from!=source||++count!=wanted)return;
        std::ofstream out(file,std::ios::binary);
        const auto ram=runtime.memory();
        out.write(reinterpret_cast<const char*>(ram.data()),static_cast<std::streamsize>(ram.size()));
        if(!out)throw std::runtime_error("Cannot write the call's RAM "+file);
        std::fprintf(stderr,"BT2_EE_CALL_RAM: call %lu from 0x%X at field %u written\n",count,source,runtime.vblank_deliveries);
    };
}
