// The launcher's and the in-game menu's logic without a window: the settings file, the menu model
// driven by presses, presses from held buttons, the combination that opens the in-game menu and
// fast forward.
#include "host/menu_input.h"
#include "host/menu_model.h"
#include "host/settings.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Menu check failed: "<<what<<'\n';std::exit(1);}}
template<class F> bool refused(F&& f) {try{f();}catch(const std::runtime_error&){return true;}return false;}
unsigned bit(MenuKey key) {return 1u<<unsigned(key);}
}

int main() {
    {
        // Settings: the text form reads back equal, also with every field changed; older files with
        // fewer keys keep the defaults; what is not a setting is refused by name.
        Settings s;check(Settings::parse(s.text())==s,"defaults read back");
        s.disc="D:\\games\\a disc = image.iso";s.integer_scaling=false;s.fullscreen=true;s.window_scale=3;s.show_frame_rate=true;
        s.master_volume=80;s.music_volume=0;s.effects_volume=55;s.fast_forward_key='F';s.fast_forward_button=GamepadState::RightTrigger;
        s.fast_forward_toggle=true;s.overlay_start_select=true;s.mapping.keys[14]='K';s.mapping.buttons[14]=GamepadState::East;
        check(Settings::parse(s.text())==s,"changed settings read back");
        check(Settings::parse("window_scale = 4\r\ndisc =\r\n").window_scale==4,"a short file with CRLF lines and an empty value");
        check(Settings::parse("fast_forward_button = none\n").fast_forward_button==GamepadState::Count,"no fast forward button");
        check(refused([]{Settings::parse("speed = 2\n");}),"an unknown key is refused");
        check(refused([]{Settings::parse("window_scale = 9\n");}),"a value out of range is refused");
        check(refused([]{Settings::parse("master_volume = loud\n");}),"a value that is not a number is refused");
        check(refused([]{Settings::parse("button_cross = elbow\n");}),"an unknown gamepad button is refused");
        check(refused([]{Settings::parse("button_cross = none\n");}),"a PS2 button needs a gamepad button");
        check(refused([]{Settings::parse("key_elbow = 65\n");}),"an unknown PS2 button is refused");
        check(refused([]{Settings::parse("window_scale 2\n");}),"a line without = is refused");
        const auto path=(std::filesystem::temp_directory_path()/"bt2-menu-tests-settings").string();
        std::filesystem::remove(path);
        check(!Settings::load(path),"no file, no settings");
        s.save(path);check(Settings::load(path)==s,"saved and loaded");
        std::filesystem::remove(path);
    }
    {
        // The menu model: moving around the ends and over disabled entries, entering and leaving
        // screens, toggles, numbers that stop at their ends.
        bool flag=false;unsigned volume=95;unsigned started=0,left_root=0;
        Menu menu;menu.on_leave_root=[&]{++left_root;};
        MenuScreen main;main.title="Main";
        main.entries.push_back(menuAction("Start",[&]{++started;}));
        MenuEntry off;off.label="Unavailable";off.enabled=false;main.entries.push_back(off);
        main.entries.push_back(menuAction("Sound",[&] {
            MenuScreen sound;sound.title="Sound";
            sound.entries.push_back(menuNumber("Volume",volume,0,100,10,"%"));
            sound.entries.push_back(menuToggle("Mute",flag));
            menu.open(std::move(sound));
        }));
        menu.open(std::move(main));
        check(menu.screen().highlighted==0,"the first entry is highlighted");
        menu.press(MenuKey::Confirm);check(started==1,"confirm runs the entry");
        menu.press(MenuKey::Down);check(menu.screen().highlighted==2,"a disabled entry is skipped");
        menu.press(MenuKey::Down);check(menu.screen().highlighted==0,"down wraps around");
        menu.press(MenuKey::Up);check(menu.screen().highlighted==2,"up wraps around");
        menu.press(MenuKey::Confirm);check(menu.depth()==2 && menu.screen().title=="Sound","an entry opens a screen");
        check(menu.screen().entries[0].value()=="95%","a number shows its unit");
        menu.press(MenuKey::Right);check(volume==100,"a number stops at its upper end");
        menu.press(MenuKey::Left);menu.press(MenuKey::Left);check(volume==80,"left lowers it by its step");
        volume=5;menu.press(MenuKey::Left);check(volume==0,"and stops at its lower end");
        menu.press(MenuKey::Confirm);check(volume==0,"confirm does nothing on a number");
        menu.press(MenuKey::Down);menu.press(MenuKey::Confirm);check(flag,"confirm switches a toggle");
        menu.press(MenuKey::Left);check(!flag && menu.screen().entries[1].value()=="Off","so do left and right");
        menu.press(MenuKey::Back);check(menu.depth()==1 && menu.screen().highlighted==2,"back returns to the entry it came from");
        menu.press(MenuKey::Back);check(left_root==1 && menu.depth()==1,"back on the first screen is reported, the screen stays");
        check(refused([&]{menu.open(MenuScreen{"Empty",{},0});}),"a screen without entries is refused");
        MenuScreen dead;dead.title="Dead";dead.entries.push_back(off);
        check(refused([&]{menu.open(dead);}),"a screen without an enabled entry is refused");
    }
    {
        // Presses from held buttons: one when a button goes down, repeats for directions only.
        MenuInput input;
        check(input.step(bit(MenuKey::Down),0)==std::vector<MenuKey>{MenuKey::Down},"a press when the button goes down");
        check(input.step(bit(MenuKey::Down),399).empty(),"nothing while it is held at first");
        check(input.step(bit(MenuKey::Down),400).size()==1 && input.step(bit(MenuKey::Down),489).empty() && input.step(bit(MenuKey::Down),490).size()==1,"then it repeats");
        check(input.step(0,500).empty() && input.step(bit(MenuKey::Down),501).size()==1,"released and pressed again");
        check(input.step(bit(MenuKey::Confirm),600).size()==1 && input.step(bit(MenuKey::Confirm),5000).empty(),"confirm does not repeat");
        MenuInput opened;opened.swallow(bit(MenuKey::Confirm),0);
        check(opened.step(bit(MenuKey::Confirm),10).empty() && opened.step(0,20).empty() && opened.step(bit(MenuKey::Confirm),30).size()==1,"a button held when the menu opens counts after its release");
        GamepadState pad;check(menuButtons(pad)==0,"no gamepad, no buttons");
        pad.connected=true;pad.buttons=1u<<GamepadState::South|1u<<GamepadState::Up;pad.lx=0xFF;
        check(menuButtons(pad)==(bit(MenuKey::Confirm)|bit(MenuKey::Up)|bit(MenuKey::Right)),"direction buttons, the stick and south");
    }
    {
        // The in-game menu's combination: once per press, by the chosen pair of buttons.
        OverlayTrigger trigger;GamepadState pad;pad.connected=true;
        pad.buttons=1u<<GamepadState::LeftStick;check(!trigger.update(pad,false,false),"one stick button alone does nothing");
        pad.buttons|=1u<<GamepadState::RightStick;check(trigger.update(pad,false,false),"both open the menu");
        check(!trigger.update(pad,false,false),"once");
        pad.buttons=0;check(!trigger.update(pad,false,false),"released");
        pad.buttons=1u<<GamepadState::Start|1u<<GamepadState::Back;check(!trigger.update(pad,false,false) && trigger.update(pad,false,true),"Start with Select only when chosen");
        pad.buttons=0;trigger.update(pad,false,true);
        check(trigger.update(pad,true,false),"Escape opens it");
        trigger.update(pad,false,false);pad.buttons=1u<<GamepadState::Guide;check(trigger.update(pad,false,false),"so does the guide button");
        OverlayTrigger closed;closed.swallow();check(!closed.update(pad,false,false),"what closed the menu does not open it again");
        // Fast forward: held, or switched by each press.
        FastForward held;check(held.update(true,false) && !held.update(false,false),"fast while held");
        FastForward toggled;
        check(toggled.update(true,true) && toggled.update(true,true) && toggled.update(false,true),"a press switches it on and it stays");
        check(!toggled.update(true,true) && !toggled.update(false,true),"the next press switches it off");
    }
    std::cout<<"Menu checks passed.\n";
}
