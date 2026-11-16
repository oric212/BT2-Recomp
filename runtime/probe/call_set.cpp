#include "probe_state.h"
#include "ee/ee_runtime.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// BT2_EE_CALL_SET (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 5): registers set at a function's N-th
// entry, the port's side of a comparison session's `--set` (tools/pcsx2_compare/session.py), so the port can take
// the same route as a session that steered the game. The spec is session.py's, sets separated by ';':
// "%<hex function>:<entry, from 1>:<reg>=<value>[,<reg>=<value>...]"; registers a0 to a3 and t0 to t3, values
// decimal or 0x hex. Each set prints a line when applied (a set without its line never met its entry).
namespace {
struct CallSet {
    std::uint32_t target=0;unsigned entry=0,count=0;std::vector<std::pair<int,std::uint32_t>> values;std::string text;
};
int registerIndex(const std::string& name) {
    static const char* const names[]={"a0","a1","a2","a3","t0","t1","t2","t3"};
    for(int n=0;n<8;++n)if(name==names[n])return 4+n;
    throw std::runtime_error("BT2_EE_CALL_SET: "+name+": registers are a0 to a3 and t0 to t3");
}
CallSet parseSet(const std::string& text) {
    const auto first=text.find(':'),second=first==std::string::npos?first:text.find(':',first+1);
    if(text.empty() || text[0]!='%' || second==std::string::npos)
        throw std::runtime_error("BT2_EE_CALL_SET: a set is %FUNC:CALL:REG=VALUE[,...], not "+text);
    CallSet set;set.text=text;
    set.target=static_cast<std::uint32_t>(std::stoul(text.substr(1,first-1),nullptr,16));
    set.entry=static_cast<unsigned>(std::stoul(text.substr(first+1,second-first-1)));
    if(!set.entry)throw std::runtime_error("BT2_EE_CALL_SET: entries are counted from 1");
    std::string rest=text.substr(second+1);
    for(std::size_t at=0;at<=rest.size();) {
        const auto end=std::min(rest.find(',',at),rest.size());
        const std::string item=rest.substr(at,end-at);
        const auto equals=item.find('=');
        if(equals==std::string::npos || equals+1==item.size())throw std::runtime_error("BT2_EE_CALL_SET: "+item+" is REG=VALUE");
        set.values.emplace_back(registerIndex(item.substr(0,equals)),static_cast<std::uint32_t>(std::stoll(item.substr(equals+1),nullptr,0)));
        at=end+1;
    }
    return set;
}
}

void installCallSet(EeRuntime& runtime,const std::string& spec) {
    auto sets=std::make_shared<std::vector<CallSet>>();
    for(std::size_t at=0;at<spec.size();) {
        const auto end=std::min(spec.find(';',at),spec.size());
        if(end>at)sets->push_back(parseSet(spec.substr(at,end-at)));
        at=end+1;
    }
    auto previous=std::move(runtime.on_guest_call_edit);
    runtime.on_guest_call_edit=[&runtime,sets,previous](R5900Context& context,std::uint32_t from,std::uint32_t target) {
        if(previous)previous(context,from,target);
        for(auto& set:*sets) {
            if(set.target!=target || ++set.count!=set.entry)continue;
            for(const auto& [index,value]:set.values)
                context.r[index].low=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));
            std::fprintf(stderr,"BT2_EE_CALL_SET: %s applied at field %u (call from 0x%X)\n",set.text.c_str(),runtime.vblank_deliveries,from);
        }
    };
}
