#include "game_starter.h"
#include <charconv>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

// Established on BT2's code (WHOLE_GAME_COVERAGE_PLAN.md, step 6); names stay neutral.
constexpr std::uint32_t overlay_entry=0x3B7FA8;                       // Runs the title and the menus.
constexpr std::uint32_t progress_pointer=0x3876A4,system_pointer=0x3876A0;
constexpr std::uint32_t progress_flags=0x18,progress_mode=0x1C,first_run_flag=0x40;
constexpr std::uint32_t archive_mount=0x100950,archive_wait=0x100990; // Mount archive n; wait until it is mounted.
constexpr std::uint32_t common_files=0x102450;                      // Loads files 15401, 15402 and 15404 once.
constexpr std::uint32_t setup_clear=0x101A10,side_control=0x101AB0,side_record=0x101A88,fighter_set=0x101AF8,
                        stage_choice=0x24AF50,battle_set=0x101A60;   // The setters the demo picker calls.
constexpr std::uint32_t character_table=116,setup_flags=1240;        // In the system structure.
constexpr std::uint32_t variants_per_character=9;
constexpr std::uint32_t characters=130; // 0 to 129 fight; from 130 the battle load reads past the character table (sweep of 2026-10-18).
constexpr std::uint32_t modes=64;       // Entries of the entry's mode table.

BattleStart configured;
std::uint32_t configured_mode=0;
std::uint32_t configured_episode[2]{};
constexpr std::uint32_t episode_number=48,story_flag=0x200; // In the progress structure.
PS2Runtime::RecompiledFunction original_entry=nullptr;
bool started=false;

std::uint32_t read32(const std::uint8_t* rdram,std::uint32_t address) {
    std::uint32_t value;std::memcpy(&value,rdram+address,4);return value;
}
void write32(std::uint8_t* rdram,std::uint32_t address,std::uint32_t value) {std::memcpy(rdram+address,&value,4);}
void setRegister(R5900Context* context,int index,std::uint32_t value) {
    context->r[index].low=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));
}

// A call as a JAL from the entry makes it: the arguments in a0-a3 and t0-t3; returns v0.
std::uint32_t call(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime,std::uint32_t target,
                   std::initializer_list<std::uint32_t> arguments={}) {
    int index=4;
    for(const auto argument:arguments)setRegister(context,index++,argument);
    setRegister(context,31,overlay_entry);
    runtime->dispatchGuestBranch(rdram,context,target,overlay_entry,overlay_entry,PS2Runtime::GuestBranchKind::DirectCall,"game starter");
    return static_cast<std::uint32_t>(context->r[2].low);
}

// Of the entry's first-run work, the second archive is mounted and the common files (the character
// table among them) are loaded; the memory card check, the logos and the films are left out.
void firstRun(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    const std::uint32_t progress=read32(rdram,progress_pointer);
    if(!(read32(rdram,progress+progress_flags)&first_run_flag))return;
    call(rdram,context,runtime,archive_mount,{1});
    call(rdram,context,runtime,archive_wait,{1});
    call(rdram,context,runtime,common_files);
    write32(rdram,progress+progress_flags,read32(rdram,progress+progress_flags)&~first_run_flag);
}

void startBattle(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(started) {original_entry(rdram,context,runtime);return;}
    started=true;
    const auto return_address=static_cast<std::uint32_t>(context->r[31].low);
    const auto stack=static_cast<std::uint32_t>(context->r[29].low);
    firstRun(rdram,context,runtime);
    call(rdram,context,runtime,setup_clear);
    const std::uint32_t system=read32(rdram,system_pointer),progress=read32(rdram,progress_pointer);
    std::memset(rdram+system+setup_flags,0,8);
    for(std::uint32_t side=0;side<2;++side)call(rdram,context,runtime,side_control,{side,1,1,0}); // CPU on both sides.
    const std::uint32_t table=read32(rdram,system+character_table);
    for(std::uint32_t side=0;side<2;++side) {
        const std::uint32_t record=call(rdram,context,runtime,side_record,{side});
        const std::uint32_t character=configured.character[side],values=table+character*16;
        // The fighter setter's stack arguments: six flags, a doubleword 0 and three of the character's values.
        const std::uint32_t frame=(stack-96)&~15u;
        std::memset(rdram+frame,0,80);
        for(std::uint32_t slot=0;slot<6;++slot)write32(rdram,frame+slot*8,1);
        write32(rdram,frame+56,read32(rdram,values));
        write32(rdram,frame+64,read32(rdram,values));
        write32(rdram,frame+72,read32(rdram,values+4));
        setRegister(context,29,frame);
        call(rdram,context,runtime,fighter_set,{record+8,character*variants_per_character+configured.variant[side],0,2,0,0,1,1});
        setRegister(context,29,stack);
    }
    const std::uint32_t stage=call(rdram,context,runtime,stage_choice,{configured.stage});
    // The settings the versus menus leave (kind 0: a battle to the end, without the demo's time limit
    // and its return to the title; then the music, whose file is 15903 + it, and the stage); the demo
    // picker passes kind 6 with 5 and 3.
    call(rdram,context,runtime,battle_set,{0,configured.music,0,1,4,stage});
    write32(rdram,progress+progress_flags,read32(rdram,progress+progress_flags)|0x4); // Set before both the demo's and the menus' battles.
    setRegister(context,2,0);
    setRegister(context,29,stack);
    setRegister(context,31,return_address);
    context->pc=return_address;
    runtime->dispatchGuestBranch(rdram,context,return_address,overlay_entry,0,PS2Runtime::GuestBranchKind::Return,"JR $ra");
}

void startScreen(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(!started) {
        started=true;
        const auto return_address=context->r[31],stack=context->r[29],argument=context->r[4];
        firstRun(rdram,context,runtime);
        write32(rdram,read32(rdram,progress_pointer)+progress_mode,configured_mode); // As the main menu writes it.
        context->r[31]=return_address;context->r[29]=stack;context->r[4]=argument;
        context->pc=overlay_entry;
    }
    original_entry(rdram,context,runtime);
}

void startEpisode(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(started) {original_entry(rdram,context,runtime);return;}
    started=true;
    const auto return_address=static_cast<std::uint32_t>(context->r[31].low);
    const auto stack=context->r[29];
    firstRun(rdram,context,runtime);
    // As the story screen leaves it after an episode's opening: the episode and its part, and the flag
    // that makes main run the story routine (0x2A8A38) instead of the versus battle routine.
    const std::uint32_t progress=read32(rdram,progress_pointer);
    write32(rdram,progress+episode_number,configured_episode[0]);
    write32(rdram,progress+episode_number+4,configured_episode[1]);
    write32(rdram,progress+progress_flags,read32(rdram,progress+progress_flags)|story_flag);
    setRegister(context,2,0);
    context->r[29]=stack;
    setRegister(context,31,return_address);
    context->pc=return_address;
    runtime->dispatchGuestBranch(rdram,context,return_address,overlay_entry,0,PS2Runtime::GuestBranchKind::Return,"JR $ra");
}

std::uint32_t number(std::string_view text,const char* name) {
    std::uint32_t value=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || error!=std::errc{} || end!=text.data()+text.size())
        throw std::runtime_error(std::string(name)+": not a number: "+std::string(text));
    return value;
}

} // namespace

BattleStart parseBattleStart(std::string_view text) {
    BattleStart start;
    std::string_view fields[4];
    std::size_t count=0;
    while(true) {
        const auto comma=text.find(',');
        if(count==4)throw std::runtime_error("BT2_START_BATTLE: too many fields");
        fields[count++]=text.substr(0,comma);
        if(comma==std::string_view::npos)break;
        text.remove_prefix(comma+1);
    }
    if(count<3)throw std::runtime_error("BT2_START_BATTLE needs <character>[:<variant>],<character>[:<variant>],<stage>[,<music>]");
    for(int side=0;side<2;++side) {
        const auto colon=fields[side].find(':');
        start.character[side]=number(fields[side].substr(0,colon),"BT2_START_BATTLE");
        if(colon!=std::string_view::npos)start.variant[side]=number(fields[side].substr(colon+1),"BT2_START_BATTLE");
        if(start.variant[side]>=variants_per_character)throw std::runtime_error("BT2_START_BATTLE: variant above 8");
        if(start.character[side]>=characters)throw std::runtime_error("BT2_START_BATTLE: character above 129");
    }
    start.stage=number(fields[2],"BT2_START_BATTLE");
    if(count==4)start.music=number(fields[3],"BT2_START_BATTLE");
    return start;
}

void installBattleStarter(EeRuntime& runtime,const BattleStart& start) {
    configured=start;
    original_entry=runtime.replaceFunction(overlay_entry,startBattle);
}

std::uint32_t parseScreenStart(std::string_view text) {
    const std::uint32_t mode=number(text,"BT2_START_SCREEN");
    if(!mode || mode>modes)throw std::runtime_error("BT2_START_SCREEN: mode outside 1 to 64");
    return mode;
}

void installScreenStarter(EeRuntime& runtime,std::uint32_t mode) {
    configured_mode=mode;
    original_entry=runtime.replaceFunction(overlay_entry,startScreen);
}

EpisodeStart parseEpisodeStart(std::string_view text) {
    EpisodeStart start;
    const auto comma=text.find(',');
    start.episode=number(text.substr(0,comma),"BT2_START_EPISODE");
    if(comma!=std::string_view::npos)start.part=number(text.substr(comma+1),"BT2_START_EPISODE");
    return start;
}

void installEpisodeStarter(EeRuntime& runtime,const EpisodeStart& start) {
    configured_episode[0]=start.episode;configured_episode[1]=start.part;
    original_entry=runtime.replaceFunction(overlay_entry,startEpisode);
}
