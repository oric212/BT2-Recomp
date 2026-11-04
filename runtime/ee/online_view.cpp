#include "ee/online_view.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <cstring>

namespace {
constexpr std::uint32_t view_setup=0x25BB38,view_setup_size=0x19C;
constexpr std::uint64_t view_setup_hash=0x2407C3A7003BECDEull;
// In the view object: the scissor's bounds (x0, x1, y0, y1), and the split flag: with it set, mode 1 is the left half
// and any other mode the right half; without it the view is the full screen whatever the mode.
constexpr std::uint32_t scissor_x0=0x200,scissor_y0=0x208,split=0x258;
PS2Runtime::RecompiledFunction generated_view_setup=nullptr;
// The fights' radars (0x1172C8, through 0x117488 from 0x111138; it builds their packets through 0x116BE8): drawn where
// each half had its radar, two of them over a full-screen view; left out in a session (the user's choice). Found by a
// store watch on the radar's GS packet in the display list; skipping it leaves the fight's audio byte-identical.
constexpr std::uint32_t radars=0x1172C8,radars_size=0x1BC;
constexpr std::uint64_t radars_hash=0x0CBCEBA35F6139BEull;
std::uint64_t fight_frames=0; // The radars' calls: one per fight frame.
void noRadars(std::uint8_t*,R5900Context* ctx,PS2Runtime*) {++fight_frames;ctx->pc=GPR_U32(ctx,31);}

// view_setup(a0 view object, a1 mode): a half is set up as the full screen (the split flag cleared for the call), then
// marked by its player in the scissor's top row.
void onlineViewSetup(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    const std::uint32_t view=GPR_U32(ctx,4)&0x1FFFFFF,mode=GPR_U32(ctx,5)&0xFF;
    std::uint32_t flag;std::memcpy(&flag,rdram+view+split,4);
    if(!flag) {generated_view_setup(rdram,ctx,base);return;}
    const std::uint32_t zero=0,player=mode==1?1:2;
    std::memcpy(rdram+view+split,&zero,4);
    generated_view_setup(rdram,ctx,base);
    std::memcpy(rdram+view+split,&flag,4);
    std::memcpy(rdram+view+scissor_x0,&zero,4); // The right half's left edge would stay.
    std::memcpy(rdram+view+scissor_y0,&player,4);
}
}

std::uint64_t onlineFightFrames() {return fight_frames;}

bool registerOnlineView(EeRuntime& runtime) {
    const auto ram=runtime.memory();
    if(ram.size()<view_setup+view_setup_size || nativeCodeHash(ram.subspan(view_setup,view_setup_size))!=view_setup_hash)return false;
    if(nativeCodeHash(ram.subspan(radars,radars_size))!=radars_hash)return false;
    generated_view_setup=runtime.replaceFunction(view_setup,onlineViewSetup);
    runtime.replaceFunction(radars,noRadars);
    return true;
}
