#include "game_starter.h"
#include <charconv>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

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
// The story map's queries (STORY_MODE_PLAN.md): has the player reached a marker, and which; in a
// chase, has the player caught the enemy.
constexpr std::uint32_t marker_reached=0x20C870,marker_number=0x20C8E8,chase_caught=0x20C8A0;
constexpr unsigned map_frames_before_marker=120;
PS2Runtime::RecompiledFunction original_marker_reached=nullptr,original_marker_number=nullptr,original_chase_caught=nullptr;
unsigned chase_queries=0;
std::uint32_t configured_marker=0;
unsigned marker_queries=0;
bool marker_fired=false;
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

void returnValue(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime,std::uint32_t value,std::uint32_t function) {
    setRegister(context,2,value);
    const auto return_address=static_cast<std::uint32_t>(context->r[31].low);
    context->pc=return_address;
    runtime->dispatchGuestBranch(rdram,context,return_address,function,0,PS2Runtime::GuestBranchKind::Return,"JR $ra");
}

// After the map has run a while, the player is reported at the chosen marker once; the map's exit test
// then runs that marker's block of the scene script, which names the next scene, as when the player
// flies there. Before and after, the game's own queries answer.
void reachMarker(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(marker_fired || ++marker_queries<map_frames_before_marker) {original_marker_reached(rdram,context,runtime);return;}
    marker_fired=true;
    returnValue(rdram,context,runtime,1,marker_reached);
}
void reachedMarker(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(!marker_fired || marker_queries==~0u) {original_marker_number(rdram,context,runtime);return;}
    marker_queries=~0u; // Answered once.
    returnValue(rdram,context,runtime,configured_marker,marker_number);
}

// In a chase the map's exit test asks only whether the enemy is caught (a byte of the map object);
// once it is, the map goes to the scene the chase's script named (command 1013). After the map has
// run a while the enemy is reported caught, as when the player catches it.
void catchEnemy(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(++chase_queries!=map_frames_before_marker) {original_chase_caught(rdram,context,runtime);return;}
    returnValue(rdram,context,runtime,1,chase_caught);
}

// The scenes a run goes to: every next scene the story sets (0x2ACE58, from a marker, an event or a
// chase), in order, for the report.
constexpr std::uint32_t next_scene=0x2ACE58;
PS2Runtime::RecompiledFunction original_next_scene=nullptr;
std::string scenes_visited;
void nextScene(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(!scenes_visited.empty())scenes_visited+=',';
    scenes_visited+=std::to_string(static_cast<std::int32_t>(context->r[4].low));
    original_next_scene(rdram,context,runtime);
}

// Confirmations (STORY_MODE_PLAN.md): story mode's waits for a button test port 0's pressed buttons
// (0x3B0C34) for cross in a function called once per frame. Every advance_calls calls of such a
// function, one call runs with cross shown as pressed, as when the player presses it; the game's own
// test decides whether the press counts, and the pad state is restored after the call. Functions with
// a phase argument are pressed only in the phase that tests the button. Cursor moves use the repeating
// buttons (0x3B0C44), as the screens read them:
// - the yes/no dialogue keeps its default (No) except on the save flow's "Exit saving?", where left
//   first moves its cursor to Yes: no tool run saves;
// - the pre-battle screen (state at +364 of the structure at 0x3879D0) lists the team's slots (+416,
//   20 bytes each, -1 when empty; +616 slots, +624 filled, the cursor at +360); while slots are empty
//   and the cursor is on a filled one, down moves it on; cross on an empty slot picks a fighter.
// A screen pressed more than its limit since the last part ended is stuck: the run ends.
constexpr std::uint32_t pressed_buttons=0x3B0C34,repeated_buttons=0x3B0C44,cross=0x4000,triangle=0x1000,left=0x80,down=0x40;
constexpr unsigned advance_calls=30;
constexpr std::uint32_t any_phase=~0u;
constexpr std::uint32_t yes_no_dialogue=0x29C640,dialogue_pointer=0x387DA0,dialogue_cursor=88;
constexpr std::uint32_t save_flow_pointer=0x387A00,save_flow_state=20,exit_saving=21;
constexpr std::uint32_t prebattle_input=0x255580,prebattle_pointer=0x3879D0,prebattle_state=364,prebattle_cursor=360,
                        team_slots=416,team_slot_size=20,team_size=616,team_filled=624;
// The item shop (a town's shop keeper; script command 1008): its step (0x247638; state at +0x1288 of
// the structure at gp-0x4498) shows messages that take cross once their text is shown (states 2, 21
// and 22); its main menu (0x246860) opens a list on cross and closes the shop on triangle, and the
// lists go back on triangle. The starter presses cross on the messages, cross once in the main menu
// (into the first list), and triangle after that until the shop closes.
constexpr std::uint32_t shop_step=0x247638,shop_menu=0x246860,shop_pointer=0x387958,shop_state=0x1288; // gp-0x4498.
std::uint32_t shop_last_state=0;
bool shop_menu_entered=false;
struct Confirmation {std::uint32_t function,phase;unsigned limit;PS2Runtime::RecompiledFunction original;unsigned calls,presses,since_part;};
Confirmation confirmations[]={
    {0x2A7400,2,400},             // The scene script's message command (13004), in its update phase, once the text is shown.
    {prebattle_input,any_phase,40}, // The pre-battle screen's input step.
    {0x2581F0,any_phase,20},        // The after-battle results' input step.
    {yes_no_dialogue,any_phase,20}, // The yes/no dialogue (after a won story battle: "Save game data?", then "Exit saving?").
    {shop_step,any_phase,40},       // The item shop's step.
    {shop_menu,any_phase,20},       // The item shop's main menu.
    {0x2A6F98,2,20},                // The "GET!!" banner of an item a town gives (script command 1011), in its update phase.
};
std::uint32_t stuck_function=0;
void endRun(PS2Runtime* runtime,unsigned flips_from_now) {
    auto& ee=*static_cast<EeRuntime*>(runtime);
    if(!ee.frame_limit || ee.frame_limit>ee.gs_display_flips+flips_from_now)ee.frame_limit=ee.gs_display_flips+flips_from_now;
}

// The end of a part: the story loop's after-battle step (0x2AC4A0) runs the results and, after a win,
// the save flow, through 0x2582E8, whose result is nonzero when the story goes on (cleared) and zero
// when the part is reset for another try (lost). The run ends 300 flips after the step.
constexpr std::uint32_t after_battle=0x2AC4A0,battle_results=0x2582E8;
constexpr unsigned flips_after_part=300;
PS2Runtime::RecompiledFunction original_after_battle=nullptr,original_results=nullptr;
unsigned parts_completed=0,battles_cleared=0,battles_lost=0;
void results(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    original_results(rdram,context,runtime);
    ++(context->r[2].low?battles_cleared:battles_lost);
}
void afterBattle(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    original_after_battle(rdram,context,runtime);
    ++parts_completed;
    for(auto& point:confirmations)point.since_part=0;
    endRun(runtime,flips_after_part);
}
void press(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime,PS2Runtime::RecompiledFunction original,
           std::uint32_t address,std::uint32_t button) {
    const std::uint32_t buttons=read32(rdram,address);
    write32(rdram,address,buttons|button);
    original(rdram,context,runtime);
    write32(rdram,address,buttons);
}
template<std::size_t index> void confirm(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    auto& point=confirmations[index];
    const bool phase=point.phase==any_phase || static_cast<std::uint32_t>(context->r[4].low)==point.phase;
    if(!phase || ++point.calls<advance_calls) {point.original(rdram,context,runtime);return;}
    point.calls=0;
    ++point.presses;
    if(++point.since_part>point.limit && !stuck_function) {stuck_function=point.function;endRun(runtime,1);}
    if(point.function==yes_no_dialogue && read32(rdram,read32(rdram,save_flow_pointer)+save_flow_state)==exit_saving
       && read32(rdram,read32(rdram,dialogue_pointer)+dialogue_cursor)!=0) {
        press(rdram,context,runtime,point.original,repeated_buttons,left);return;
    }
    if(point.function==shop_step) {
        shop_last_state=read32(rdram,read32(rdram,shop_pointer)+shop_state);
        const bool message=shop_last_state==2 || shop_last_state==21 || shop_last_state==22;
        press(rdram,context,runtime,point.original,pressed_buttons,message?cross:triangle);return;
    }
    if(point.function==shop_menu) {
        const bool into_list=!shop_menu_entered;
        shop_menu_entered=true;
        press(rdram,context,runtime,point.original,pressed_buttons,into_list?cross:triangle);return;
    }
    if(point.function==prebattle_input) {
        const std::uint32_t screen=read32(rdram,prebattle_pointer);
        const std::uint32_t cursor=read32(rdram,screen+prebattle_cursor);
        const bool filled=static_cast<std::int32_t>(read32(rdram,screen+team_slots+cursor*team_slot_size))>=0;
        if(read32(rdram,screen+prebattle_state)==0 && filled && read32(rdram,screen+team_filled)!=read32(rdram,screen+team_size)) {
            press(rdram,context,runtime,point.original,repeated_buttons,down);return;
        }
    }
    press(rdram,context,runtime,point.original,pressed_buttons,cross);
}
// Towns (the scenes optional markers lead to; STORY_MODE_PLAN.md): each frame the town step
// (0x2A99B8) goes through the town's characters (map objects 3 to 11). The first one talks when the
// player is in reach (bit 0 of 0x20C950) and presses cross; the others run their block when the
// player touches them (bit k of 0x20C980), which can start a fight. Either runs the character's block
// of the scene script, while the script state's running block (0x3B2664) is nonzero. The starter goes
// through the characters in turn: after the town has run a while, the current one is reported in
// reach (with cross pressed every advance_calls calls) or touched until its block has run, or for
// at most town_frames_per_character calls if it does not answer; after the last one the run ends.
constexpr std::uint32_t town_step=0x2A99B8,talk_reach=0x20C950,touched=0x20C980,running_block=0x3B2664;
constexpr std::uint32_t town_characters=9;
constexpr unsigned town_frames_before=120,town_frames_per_character=150;
PS2Runtime::RecompiledFunction original_town_step=nullptr,original_talk_reach=nullptr,original_touched=nullptr;
std::uint32_t town_character=0;
unsigned town_calls=0,town_blocks=0;
bool town_reporting=false,town_block_ran=false;
void townStep(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    town_reporting=false;
    if(read32(rdram,running_block)) {town_block_ran=true;original_town_step(rdram,context,runtime);return;}
    if(town_block_ran) {town_block_ran=false;++town_blocks;++town_character;town_calls=0;}
    if(town_character<town_characters && ++town_calls>town_frames_before+town_frames_per_character) {++town_character;town_calls=0;}
    if(town_character>=town_characters) {endRun(runtime,flips_after_part);original_town_step(rdram,context,runtime);return;}
    town_reporting=town_calls>town_frames_before;
    if(town_reporting && town_character==0 && town_calls%advance_calls==0) {
        press(rdram,context,runtime,original_town_step,pressed_buttons,cross);return;
    }
    original_town_step(rdram,context,runtime);
}
void talkReach(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(!town_reporting || town_character!=0) {original_talk_reach(rdram,context,runtime);return;}
    returnValue(rdram,context,runtime,1,talk_reach);
}
void touchedCharacters(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    if(!town_reporting || town_character==0) {original_touched(rdram,context,runtime);return;}
    returnValue(rdram,context,runtime,1u<<town_character,touched);
}

// The story's battles: the pre-battle setup (0x255EA0) sets side 0 to the player (the side setter's
// third argument 0) and side 1 to the CPU (1); the starter passes 1 for both, as the battle starter
// does, so a story battle can be won without input.
PS2Runtime::RecompiledFunction original_side_control=nullptr;
void cpuSide(std::uint8_t* rdram,R5900Context* context,PS2Runtime* runtime) {
    setRegister(context,6,1);
    original_side_control(rdram,context,runtime);
}
template<std::size_t... index> void installConfirmations(EeRuntime& runtime,std::index_sequence<index...>) {
    ((confirmations[index].original=runtime.replaceFunction(confirmations[index].function,confirm<index>)),...);
    original_after_battle=runtime.replaceFunction(after_battle,afterBattle);
    original_results=runtime.replaceFunction(battle_results,results);
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
    std::string_view fields[3];std::size_t count=0;
    while(true) {
        const auto comma=text.find(',');
        if(count==3)throw std::runtime_error("BT2_START_EPISODE: too many fields");
        fields[count++]=text.substr(0,comma);
        if(comma==std::string_view::npos)break;
        text.remove_prefix(comma+1);
    }
    start.episode=number(fields[0],"BT2_START_EPISODE");
    if(count>1)start.part=number(fields[1],"BT2_START_EPISODE");
    if(count>2) {
        if(!fields[2].starts_with("marker:"))throw std::runtime_error("BT2_START_EPISODE: the third field is marker:<number>");
        start.marker=number(fields[2].substr(7),"BT2_START_EPISODE");
        if(!start.marker || start.marker>25)throw std::runtime_error("BT2_START_EPISODE: markers are 1 to 25");
    }
    return start;
}

void installEpisodeStarter(EeRuntime& runtime,const EpisodeStart& start) {
    configured_episode[0]=start.episode;configured_episode[1]=start.part;
    original_entry=runtime.replaceFunction(overlay_entry,startEpisode);
    installConfirmations(runtime,std::make_index_sequence<std::size(confirmations)>{});
    original_side_control=runtime.replaceFunction(side_control,cpuSide);
    original_chase_caught=runtime.replaceFunction(chase_caught,catchEnemy);
    original_next_scene=runtime.replaceFunction(next_scene,nextScene);
    original_town_step=runtime.replaceFunction(town_step,townStep);
    original_talk_reach=runtime.replaceFunction(talk_reach,talkReach);
    original_touched=runtime.replaceFunction(touched,touchedCharacters);
    if(start.marker) {
        configured_marker=start.marker;
        original_marker_reached=runtime.replaceFunction(marker_reached,reachMarker);
        original_marker_number=runtime.replaceFunction(marker_number,reachedMarker);
    }
}

std::string gameStarterReport() {
    std::string text="\"starter_scenes\":["+scenes_visited+"],\"starter_town_blocks\":"+std::to_string(town_blocks)+",\"starter_shop_state\":"+std::to_string(shop_last_state)+",\"starter_town_done\":"+(town_character>=town_characters?"true":"false")+",\"starter_chase_caught\":"+std::string(chase_queries>=map_frames_before_marker?"true":"false")+",\"starter_parts_completed\":"+std::to_string(parts_completed)+",\"starter_battles_cleared\":"+std::to_string(battles_cleared)
        +",\"starter_battles_lost\":"+std::to_string(battles_lost)+",\"starter_stuck\":"+std::to_string(stuck_function)+",\"starter_confirmations\":[";
    for(const auto& point:confirmations) {
        if(text.back()!='[')text+=',';
        text+="["+std::to_string(point.function)+","+std::to_string(point.presses)+"]";
    }
    return text+"]";
}
