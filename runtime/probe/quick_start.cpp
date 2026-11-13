#include "quick_start.h"
#include "ee/ee_runtime.h"
#include "ee/native_functions.h"
#include "ee/ps2_runtime_macros.h"
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <iostream>

namespace {
// The game's code the presses rely on (found 2026-11-12/13 in boots with a card holding a save and with an empty card,
// calls traced from the card check to the main menu; docs/plans/launcher/LAUNCHER_PLAN.md, "Quick start"). All are
// checked by hash when the first logo starts (the overlay that holds most of them is loaded by then).
struct Code {std::uint32_t address,size;std::uint64_t hash;};
constexpr Code logo_player{0x3BC000,0x310,0xCB6F74A10CC786B7ull};   // A logo (a2 non-zero: Start or Cross skips it).
constexpr Code title_input{0x3B9710,0x644,0x4972A0E6A8917F85ull};   // The title's buttons, each frame of its menu.
constexpr Code title_states{0x3B9EF0,0x10F4,0xAE4758E38413AD91ull}; // The title's states (its questions among them).
constexpr Code title_menu{0x3B84B8,0x7BC,0xC1546BE40D7C8BDF};        // Builds the title's menu (with or without Continue).
constexpr Code message_box{0x29C640,0x180,0x5C4D259D9EA12663ull};   // A question's Yes and No.
constexpr Code main_menu{0x3B4250,0x19C,0x633BB2C284BB71D0ull};     // The main menu.
constexpr Code checked_code[]{logo_player,title_input,title_states,title_menu,message_box,main_menu};
// The title (pointer at 0x4213B0): its flags (+0xCC, 0x8: the menu is shown), the menu's cursor (+0xD0) and number of
// entries (+0xD4), its state (+0xE8: 0 the menu; 0x12 "Do you wish to create a save file on the memory card?", which
// New Game asks when no save is loaded). The progress structure (pointer at 0x3876A4): its flags (+0x18). The title
// builds its menu (0x3B84B8) with three entries and the cursor on the third, Continue, when the flags hold 0x8 or
// 0x800 (a save loaded); with two, New Game first, otherwise.
constexpr std::uint32_t title_pointer=0x4213B0,title_flags=0xCC,title_menu_shown=0x8,title_cursor=0xD0,title_entries=0xD4,title_state=0xE8;
constexpr std::uint32_t menu_state=0,create_save_question=0x12;
// After Yes, the states the game passes while it checks the card, creates the save file and says "Save successful"
// (0x13 to 0x18), then on to the main menu (0xD, 0xF) (watched on an empty card, 2026-11-13). Nothing is pressed in
// them; any other state (another question, an error, back to the menu) stops quick start.
constexpr std::uint32_t saving_states[]{0x13,0x14,0x16,0x17,0x18,0xD,0xF};
// The title's last states before the main menu; after them its state returns to 0 (the title closing), which before
// them would mean the menu again (the question answered No).
constexpr std::uint32_t leaving_states[]{0xD,0xF};
bool leaving=false;
constexpr std::uint32_t continue_entry=2,entries_with_continue=3,new_game_entry=0,entries_without_save=2;
constexpr std::uint32_t progress_pointer=0x3876A4,progress_flags=0x18,progress_loaded=0x808;
// The message box (pointer at 0x387DA0, gp-0x4050): +0x70 non-zero while it takes an answer, +0x58 its cursor (0 Yes,
// 1 No, which the game preselects; Left and Right move it, Cross answers).
constexpr std::uint32_t box_pointer=0x387DA0,box_answering=0x70,box_cursor=0x58,box_yes=0,box_no=1;
constexpr std::uint16_t start=1u<<3,left=1u<<7,cross=1u<<14; // PadScript::names order (PadInput::pressed).
// A press lasts 4 fields and is let go for 4: the game's frame (one or two fields) sees each as a new press.
constexpr unsigned press_fields=4,rhythm_fields=8;
// Limits in fields: the card stage before the first logo, and each later stage. Measured: the card stage ends at field
// 586 with a save (331 with an empty card), the main menu follows about 740 fields later, a new save about 1,000. A
// question at the card stage (no card: "Continue anyway?") waits for the player: past the limit it is theirs.
constexpr unsigned card_stage_limit=1200,stage_limit=3600;

// Passing: the logos, film and title, by Start. Continuing: Cross on Continue. NewGame: Cross on New Game (no save).
// CreatingSave: Yes to the question to create a save file, then the game writes it (its messages are shown).
enum class Stage {CardStage,Passing,Continuing,NewGame,CreatingSave,Done,Stopped};
Stage stage=Stage::CardStage;
quick_start::Hooks hooks;
EeRuntime* runtime_=nullptr;
unsigned fields=0,stage_fields=0,presses=0;
bool save_created=false;
std::string ending;
PS2Runtime::RecompiledFunction original_logo=nullptr,original_title=nullptr,original_menu=nullptr;

std::uint32_t word(std::uint32_t address) {std::uint32_t v;std::memcpy(&v,runtime_->memory().data()+(address&0x1FFFFFF),4);return v;}
std::string hex(std::uint32_t v) {char t[16];std::snprintf(t,sizeof t,"0x%X",v);return t;}
bool codeIs(const Code& code) {
    const auto ram=runtime_->memory();
    return ram.size()>=code.address+code.size && nativeCodeHash(ram.subspan(code.address,code.size))==code.hash;
}
bool underWay() {return stage!=Stage::Done && stage!=Stage::Stopped;}
std::uint32_t progressFlags() {return word(word(progress_pointer)+progress_flags);}
void enter(Stage next) {
    stage=next;stage_fields=0;
    if(hooks.fast)hooks.fast(underWay());
    // The card's messages while a save is made are the player's to see.
    if(hooks.cover)hooks.cover(stage==Stage::Passing || stage==Stage::Continuing || stage==Stage::NewGame);
}
void stop(const std::string& why) {
    if(!underWay())return;
    ending="Quick start stopped at field "+std::to_string(fields)+": "+why+"; the player has the game from here.";
    std::cerr<<ending<<std::endl;
    enter(Stage::Stopped);
}

// The first logo: the card stage is over. The game's code is checked before any button is pressed.
void logoPlayer(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    if(stage==Stage::CardStage) {
        bool expected=true;
        for(const auto& code:checked_code)expected=expected && codeIs(code);
        if(!expected)stop("the game's title code is not the expected code");
        else {std::cerr<<"Quick start: the card stage done at field "<<fields<<" (progress flags "<<hex(progressFlags())<<"); passing the logos"<<std::endl;enter(Stage::Passing);}
    }
    original_logo(rdram,ctx,base);
}

// The title, each frame of its menu: the menu the game built says what to press.
void titleInput(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    if(stage==Stage::Passing) {
        const std::uint32_t title=word(title_pointer);
        if(title && (word(title+title_flags)&title_menu_shown)) {
            const std::uint32_t cursor=word(title+title_cursor),entries=word(title+title_entries),flags=progressFlags();
            if(cursor==continue_entry && entries==entries_with_continue && (flags&progress_loaded))enter(Stage::Continuing);
            else if(cursor==new_game_entry && entries==entries_without_save && !(flags&progress_loaded)) {
                std::cerr<<"Quick start: no save on the card; New Game with a save file created"<<std::endl;enter(Stage::NewGame);
            } else stop("the title's menu is not as the game builds it (entry "+std::to_string(cursor)+" of "+std::to_string(entries)+", progress flags "+hex(flags)+")");
        }
    }
    original_title(rdram,ctx,base);
}

void mainMenu(std::uint8_t* rdram,R5900Context* ctx,PS2Runtime* base) {
    if(stage==Stage::Continuing || stage==Stage::CreatingSave) {
        ending="Quick start: the main menu at field "+std::to_string(fields)+(save_created?", a save file created":", the save loaded")+" ("
              +std::to_string(presses)+" presses; progress flags "+hex(progressFlags())+").";
        std::cerr<<ending<<std::endl;
        enter(Stage::Done);
    } else if(underWay()) stop("the main menu came at an unexpected stage");
    original_menu(rdram,ctx,base);
}

// Whether the title stands in its menu with the cursor where the stage presses Cross (a Cross anywhere else could answer
// a question the game asks next).
bool menuReady(std::uint32_t entry) {
    const std::uint32_t title=word(title_pointer);
    return title && word(title+title_state)==menu_state && (word(title+title_flags)&title_menu_shown) && word(title+title_cursor)==entry;
}

// Once per field (EeRuntime::pad_presses): the presses for this field, and the limits.
std::uint16_t pressesFor() {
    ++fields;
    if(!underWay())return 0;
    ++stage_fields;
    if(runtime_->padButtonsHeld()) {stop("the player pressed a button");return 0;}
    if(stage==Stage::CardStage) {
        if(stage_fields>card_stage_limit)stop("the card stage is still running (the game asks the player a question about the card)");
        return 0;
    }
    if(stage_fields>stage_limit) {stop("the next screen did not come");return 0;}
    std::uint16_t button=0;
    if(stage==Stage::Passing)button=start;
    else if(stage==Stage::Continuing) {if(menuReady(continue_entry))button=cross;}
    else if(stage==Stage::NewGame) {
        const std::uint32_t title=word(title_pointer);
        if(word(title+title_state)==create_save_question) {enter(Stage::CreatingSave);return 0;}
        if(menuReady(new_game_entry))button=cross;
    } else if(stage==Stage::CreatingSave) {
        const std::uint32_t box=word(box_pointer),state=word(word(title_pointer)+title_state);
        if(state==create_save_question) {
            if(box && word(box+box_answering)) {
                const std::uint32_t cursor=word(box+box_cursor);
                button=cursor==box_no?left:cursor==box_yes?cross:0;
            }
        } else if(std::find(std::begin(saving_states),std::end(saving_states),state)!=std::end(saving_states)) {
            save_created=true; // The game writes it.
            leaving=leaving || state==leaving_states[0] || state==leaving_states[1];
        } else if(state==menu_state && leaving) {} // The title closing for the main menu.
        else {stop("the game went on to title state "+hex(state)+" while creating the save (a question or a message quick start does not answer)");return 0;}
    }
    if(!button || stage_fields%rhythm_fields>=press_fields)return 0;
    if(stage_fields%rhythm_fields==0)++presses;
    return button;
}
}

namespace quick_start {
void install(EeRuntime& runtime,Hooks given) {
    runtime_=&runtime;hooks=std::move(given);
    original_logo=runtime.replaceFunction(logo_player.address,logoPlayer);
    original_title=runtime.replaceFunction(title_input.address,titleInput);
    original_menu=runtime.replaceFunction(main_menu.address,mainMenu);
    runtime.pad_presses=pressesFor;
    enter(Stage::CardStage);
}
bool running() {return underWay();}
std::string summary() {return ending.empty()?std::string("Quick start: still under way at field "+std::to_string(fields)+"."):ending;}
}
