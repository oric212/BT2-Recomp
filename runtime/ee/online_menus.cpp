#include "ee/online_menus.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <cstring>

namespace {
// The pause menu's processing (0x107860, a2 the menu description; called by the fight's pause, 0x1066E8, and by itself
// for a sub-menu). Found by a word watch on the cursor (0x33F938, +0x18 of the Dueling fight's description 0x33F920).
constexpr std::uint32_t menu=0x107860,menu_size=0x674;
constexpr std::uint64_t menu_hash=0xB0F811879A49F2D1ull;
constexpr std::uint32_t entries_at=0x0,entry_count_at=0x4,choice_count_at=0x1C,entry_size=0x20,entry_id_at=0x4;
constexpr std::uint32_t return_to_main_menu=0x11;
PS2Runtime::RecompiledFunction generated_menu=nullptr;

std::uint32_t readWord(const std::uint8_t* rdram,std::uint32_t address) {std::uint32_t v;std::memcpy(&v,rdram+(address&0x1FFFFFF),4);return v;}
void writeWord(std::uint8_t* rdram,std::uint32_t address,std::uint32_t v) {std::memcpy(rdram+(address&0x1FFFFFF),&v,4);}

// The description's last entry left out when it is Return to Main Menu (once: the choices then no longer count one fewer).
void withoutMainMenu(std::uint8_t* rdram,std::uint32_t description) {
    const std::uint32_t count=readWord(rdram,description+entry_count_at),choices=readWord(rdram,description+choice_count_at);
    if(count<2 || count>16 || choices+1!=count)return;
    const std::uint32_t last=readWord(rdram,description+entries_at)+(count-1)*entry_size;
    if(readWord(rdram,last+entry_id_at)!=return_to_main_menu)return;
    // Only the entries' count: the menu draws and wraps its cursor by it. With the choices' count lowered as well the game
    // drew no menu at all (measured), so that one stays.
    writeWord(rdram,description+entry_count_at,count-1);
}

void onlineMenu(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    withoutMainMenu(rdram,GPR_U32(ctx,6));
    generated_menu(rdram,ctx,base);
}
}

bool registerOnlineMenus(EeRuntime& runtime) {
    const auto ram=runtime.memory();
    if(ram.size()<menu+menu_size || nativeCodeHash(ram.subspan(menu,menu_size))!=menu_hash)return false;
    generated_menu=runtime.replaceFunction(menu,onlineMenu);
    return true;
}
