#include "ee/online_lock_on.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <array>
#include <cstring>

namespace {
// 0x2115D0(a0 the fighter): called once a frame per fighter by 0x1F71B0 right after the phase bits are set.
constexpr std::uint32_t update=0x2115D0,update_size=0xB8;
constexpr std::uint64_t update_hash=0xF68C42D5F64F87B0ull;
// A fighter's flag words at +0x2B0 (word 0 first); the setter 0x2113C0 also marks the bits it touched in the words at +0x2F4.
constexpr std::uint32_t flags_at=0x2B0,touched_at=0x2F4;
PS2Runtime::RecompiledFunction generated_update=nullptr;

struct Fighter {std::uint32_t address=0;bool fighting=false;};
std::array<Fighter,8> fighters{}; // The fight's fighter objects (two in 1P vs 2P), each with its last phase.

bool& wasFighting(std::uint32_t address) {
    for(auto& f:fighters)if(f.address==address)return f.fighting;
    for(auto& f:fighters)if(!f.address) {f.address=address;return f.fighting;}
    fighters[0]={address,false}; // More objects than slots: start over (not seen; a fighter then misses one round's lock).
    return fighters[0].fighting;
}
std::uint32_t readWord(const std::uint8_t* rdram,std::uint32_t address) {std::uint32_t v;std::memcpy(&v,rdram+(address&0x1FFFFFF),4);return v;}
void writeWord(std::uint8_t* rdram,std::uint32_t address,std::uint32_t v) {std::memcpy(rdram+(address&0x1FFFFFF),&v,4);}

void onlineUpdate(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    const std::uint32_t fighter=GPR_U32(ctx,4)&0x1FFFFFF;
    const std::uint32_t flags=readWord(rdram,fighter+flags_at);
    const std::uint32_t now=online_lock_on::atUpdate(flags,wasFighting(fighter));
    if(now!=flags) {
        writeWord(rdram,fighter+flags_at,now);
        writeWord(rdram,fighter+touched_at,readWord(rdram,fighter+touched_at)|online_lock_on::locked_on);
    }
    generated_update(rdram,ctx,base);
}
}

std::uint32_t online_lock_on::atUpdate(std::uint32_t flags,bool& was_fighting) {
    const bool now=(flags&fighting)!=0,starts=now && !was_fighting;
    was_fighting=now;
    return starts?flags|locked_on:flags;
}

bool registerOnlineLockOn(EeRuntime& runtime) {
    const auto ram=runtime.memory();
    if(ram.size()<update+update_size || nativeCodeHash(ram.subspan(update,update_size))!=update_hash)return false;
    fighters={};
    generated_update=runtime.replaceFunction(update,onlineUpdate);
    return true;
}
