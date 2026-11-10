#include "ee/online_view.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <array>
#include <cstring>

namespace {
constexpr std::uint32_t view_setup=0x25BB38,view_setup_size=0x19C;
constexpr std::uint64_t view_setup_hash=0x2407C3A7003BECDEull;
// In the view object: the scissor's bounds (x0, x1, y0, y1), and the split flag: with it set, mode 1 is the left half
// and any other mode the right half; without it the view is the full screen whatever the mode.
constexpr std::uint32_t scissor_x0=0x200,scissor_y0=0x208,split=0x258;
PS2Runtime::RecompiledFunction generated_view_setup=nullptr;
// The fights' radars (0x1172C8, through 0x117488 from 0x111138): with the split screen's bit (bit 0x2 of the 64-bit word
// +0x4D8 of the object at gp-0x4750) one per player, each at the bottom right of its half; without it player 1's alone.
// Each player sees only their own, at the bottom right of the screen as in a single-screen fight (the user, 2026-11-10:
// the opponent was too easy to lose):
// - Player 2's game leaves out player 1's packet (0x116BE8 builds radar a0's; found by a store watch on the radar's GS
//   packet in the display list); player 2's half already has its radar at the bottom right.
// - Player 1's game draws the radars as a single-screen fight (the bit cleared for the call), with player 1's radar
//   moved by half the screen for the call. Player i's radar data is at *(gp-0x4728) + 0x1390 + i*0x3E40; its seven
//   sprite objects hold their x (0 to 512 across the screen) in the low half of the words at +4, +8, +0x20, +0x30,
//   +0x40 and +0x50; a single-screen fight gives player 1 the positions the split screen gives player 2, 0x100 more
//   (compared in the memory of both fights).
// Drawing only: leaving the radars out altogether kept the fight's audio byte-identical, and the drift checks of an online
// pair stay alike.
constexpr std::uint32_t radars=0x1172C8,radars_size=0x1BC,radar_packet=0x116BE8,radar_packet_size=0xF8;
constexpr std::uint64_t radars_hash=0x0CBCEBA35F6139BEull,radar_packet_hash=0xFA2AF122DF0EAF79ull;
constexpr std::uint32_t system_pointer_gp=0x4750,screen_mode=0x4D8;constexpr std::uint64_t split_screen=0x2;
constexpr std::uint32_t hud_pointer_gp=0x4728,radar_data=0x1390,half_screen=0x100;
constexpr std::array<std::uint32_t,7> radar_sprites{0x3720,0x37E0,0x38A0,0x3960,0x39C0,0x3A80,0x3AE0};
constexpr std::array<std::uint32_t,6> sprite_x{0x4,0x8,0x20,0x30,0x40,0x50};
PS2Runtime::RecompiledFunction generated_radars=nullptr,generated_radar_packet=nullptr;
unsigned local_player=1;
std::uint64_t fight_frames=0; // The radars' calls: one per fight frame.
std::uint8_t* guestWord(std::uint8_t* rdram,std::uint32_t address) {return rdram+(address&0x1FFFFFF);}
std::uint32_t readWord(std::uint8_t* rdram,std::uint32_t address) {std::uint32_t v;std::memcpy(&v,guestWord(rdram,address),4);return v;}
void addToWord(std::uint8_t* rdram,std::uint32_t address,std::uint32_t add) {const std::uint32_t v=readWord(rdram,address)+add;std::memcpy(guestWord(rdram,address),&v,4);}
void moveRadar(std::uint8_t* rdram,std::uint32_t gp,std::uint32_t by) {
    const std::uint32_t data=readWord(rdram,gp-hud_pointer_gp)+radar_data;
    for(const auto sprite:radar_sprites)for(const auto x:sprite_x)addToWord(rdram,data+sprite+x,by);
}
void onlineRadars(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    ++fight_frames;
    if(local_player==2) {generated_radars(rdram,ctx,base);return;}
    const std::uint32_t gp=GPR_U32(ctx,28);
    std::uint8_t* const mode=guestWord(rdram,readWord(rdram,gp-system_pointer_gp)+screen_mode);
    std::uint64_t word;std::memcpy(&word,mode,8);
    if(!(word&split_screen)) {generated_radars(rdram,ctx,base);return;}
    const std::uint64_t single=word&~split_screen;
    std::memcpy(mode,&single,8);moveRadar(rdram,gp,half_screen);
    generated_radars(rdram,ctx,base);
    std::memcpy(mode,&word,8);moveRadar(rdram,gp,0u-half_screen);
}
void onlineRadarPacket(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    if(local_player==2 && GPR_U32(ctx,4)==0) {ctx->pc=GPR_U32(ctx,31);return;}
    generated_radar_packet(rdram,ctx,base);
}

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

bool registerOnlineView(EeRuntime& runtime,unsigned player) {
    const auto ram=runtime.memory();
    local_player=player;
    if(ram.size()<view_setup+view_setup_size || nativeCodeHash(ram.subspan(view_setup,view_setup_size))!=view_setup_hash)return false;
    if(nativeCodeHash(ram.subspan(radars,radars_size))!=radars_hash)return false;
    if(nativeCodeHash(ram.subspan(radar_packet,radar_packet_size))!=radar_packet_hash)return false;
    generated_view_setup=runtime.replaceFunction(view_setup,onlineViewSetup);
    generated_radars=runtime.replaceFunction(radars,onlineRadars);
    generated_radar_packet=runtime.replaceFunction(radar_packet,onlineRadarPacket);
    return true;
}
