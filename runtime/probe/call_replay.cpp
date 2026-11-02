#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// BT2_REPLAY_CALL (docs/plans/ee/AI_VERIFICATION_PLAN.md): one guest function run on an EE RAM captured from
// PCSX2, to compare what it leaves with PCSX2's own RAM after the same call. The spec is
// "<RAM in>|<hex function>|<hex a0>|<hex a1>|<hex sp>|<hex gp>|<RAM out>"; the call starts from main's prepared
// context with those registers and returns to a sentinel; the RAM after it is written and v0 printed.
void replayCall(EeRuntime& runtime,const R5900Context& prepared,const std::string& spec) {
    std::vector<std::string> field;
    for(std::size_t at=0;;) {
        const auto end=spec.find('|',at);
        field.push_back(spec.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(field.size()!=7)throw std::runtime_error("BT2_REPLAY_CALL is <RAM in>|<function>|<a0>|<a1>|<sp>|<gp>|<RAM out>");
    const auto hex=[](const std::string& text){return static_cast<std::uint32_t>(std::stoul(text,nullptr,16));};
    auto ram=runtime.memory();
    std::ifstream in(field[0],std::ios::binary);
    if(!in.read(reinterpret_cast<char*>(ram.data()),std::streamsize(ram.size())))throw std::runtime_error("BT2_REPLAY_CALL: cannot read "+field[0]);
    constexpr std::uint32_t sentinel=0xFFFFFFFC;
    R5900Context call=prepared;
    call.r[4].low=hex(field[2]);call.r[5].low=hex(field[3]);call.r[29].low=hex(field[4]);call.r[28].low=hex(field[5]);
    call.r[31].low=sentinel;
    runtime.expected_return=sentinel;
    runtime.callFunction(hex(field[1]),call);
    if(static_cast<std::uint32_t>(call.pc)!=sentinel)throw std::runtime_error("BT2_REPLAY_CALL: the function did not return to the sentinel");
    std::ofstream out(field[6],std::ios::binary);
    out.write(reinterpret_cast<const char*>(ram.data()),std::streamsize(ram.size()));
    std::cout<<std::hex<<"Replayed call: v0 "<<static_cast<std::uint32_t>(call.r[2].low)<<std::dec<<std::endl;
}
