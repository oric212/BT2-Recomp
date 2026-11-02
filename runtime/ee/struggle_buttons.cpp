#include "ee/struggle_buttons.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace {
// The push measure and its code (main executable), found 2026-11-03 by tracing a scripted struggle.
constexpr std::uint32_t measure_address=0x217E58,measure_size=0x2D0;
constexpr std::uint64_t measure_hash=0x4665A843B217A594;
// The fighter record: its side (+0x1534), the buttons the input reader wrote this frame (+0x20) and the
// previous frame's (+0x44). The side's record in the system structure (pointer at 0x3876A0) holds the
// byte the side setter writes: nonzero when the CPU plays that side.
constexpr std::uint32_t fighter_side=0x1534,fighter_held=0x20,fighter_previous=0x44;
constexpr std::uint32_t system_pointer=0x3876A0,side_records=0xE0,side_record_size=0x1D0,side_cpu=0x1C4;
constexpr std::uint32_t cross=1u<<14,square=1u<<15; // PadScript::names order.

PS2Runtime::RecompiledFunction generated_measure=nullptr;
struct Owed {float turn=0;std::uint64_t flip=0;};
std::unordered_map<std::uint32_t,Owed> owed_by_fighter;

std::uint32_t word(std::span<const std::uint8_t> ram,std::uint32_t address) {
    address&=0x1FFFFFF;
    if(std::uint64_t(address)+4>ram.size())return 0;
    std::uint32_t value;std::memcpy(&value,ram.data()+address,4);return value;
}

void measure(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    auto& rt=*static_cast<EeRuntime*>(base);
    const std::uint32_t fighter=ctx->r[4].low;
    generated_measure(rdram,ctx,base);
    const auto ram=rt.memory();
    const std::uint32_t side=word(ram,fighter+fighter_side),system=word(ram,system_pointer)&0x1FFFFFF;
    const std::uint64_t cpu_flag=std::uint64_t(system)+side_records+side_record_size*side+side_cpu;
    if(side>1 || !system || cpu_flag>=ram.size() || ram[cpu_flag])return; // The CPU's fighter: as the game measured it.
    auto& owed=owed_by_fighter[fighter];
    if(rt.gs_display_flips>owed.flip+2)owed.turn=0; // A new struggle starts without presses owed.
    owed.flip=rt.gs_display_flips;
    ctx->f[0]=struggle_buttons::push(word(ram,fighter+fighter_held),word(ram,fighter+fighter_previous),ctx->f[0],owed.turn);
}
}

namespace struggle_buttons {
float push(std::uint32_t held,std::uint32_t previous,float sticks,float& owed) {
    const std::uint32_t pressed=held&~previous;
    owed+=press_turn*float(((pressed&cross)!=0)+((pressed&square)!=0));
    const float paid=std::min(owed,frame_turn);
    owed-=paid;
    return std::max(sticks,paid);
}

bool install(EeRuntime& runtime) {
    const auto ram=runtime.memory();
    if(ram.size()<measure_address+measure_size || nativeCodeHash(ram.subspan(measure_address,measure_size))!=measure_hash)return false;
    owed_by_fighter.clear();
    generated_measure=runtime.replaceFunction(measure_address,measure);
    return true;
}
}
