// The launcher: the settings screens before the game starts (docs/plans/launcher/LAUNCHER_PLAN.md).
// Every change is applied to the window and written to the settings file at once. The disc image
// comes from the data folder and the memory cards from the saves folder next to the programs
// (host/game_folders.h); there is no file or folder dialog. The lists are read again while they are
// shown, so a file moved into a folder appears without leaving the screen.
#pragma once
#ifdef _WIN32
#include "host/game_folders.h"
#include "host/menu_host.h"
#include <algorithm>
#include <memory>
#include <vector>

enum class LauncherResult {Start,Host,Join,Quit}; // Host and Join: an online battle (docs/plans/online/ONLINE_BATTLE_PLAN.md).

// What the launcher reads from the folders and keeps in step with them.
class Launcher {
public:
    Launcher(Settings& settings,std::string settings_path,const GameFolders& folders,MenuHost& host)
        :settings_(settings),settings_path_(std::move(settings_path)),folders_(folders),host_(host) {}

    LauncherResult result=LauncherResult::Quit;
    std::function<void()> on_change; // Applies the settings to the window; set by runLauncher.

    // Opens the first screen. Also picks what there is only one of: the only disc image in data, and
    // a card of saves when none is in the slot.
    void open() {
        host_.setDiscPresent([this]{return discReady();});
        pickDefaults();
        host_.menu.open(mainScreen());
        host_.menu.on_leave_root=[]{}; // Back does nothing on the first screen.
    }
    // Called while the menu runs: the open list screen is built again when the folder changed.
    void refresh() {
        const auto discs=folders_.discImages(),cards=folders_.cards(),images=folders_.cardImages();
        const bool changed=discs!=discs_ || cards!=cards_ || images!=images_;
        discs_=discs;cards_=cards;images_=images;
        if(!changed)return;
        pickDefaults();
        // Only a list screen on top is rebuilt (the first screen's values are read as they are drawn).
        if(host_.menu.depth()!=2)return;
        if(host_.menu.screen().title==disc_title)reopen(discScreen());
        else if(host_.menu.screen().title==card_title)reopen(cardScreen());
    }
    bool discReady() const {return folders_.configured(settings_);}

private:
    enum class List {Discs,Cards};
    static constexpr const char* disc_title="Game disc";
    static constexpr const char* card_title="Memory card (slot 1)";
    Settings& settings_;std::string settings_path_;const GameFolders& folders_;MenuHost& host_;
    std::vector<std::string> discs_,cards_,images_;

    void changed() {if(on_change)on_change();settings_.save(settings_path_);}
    void pickDefaults() {
        discs_=folders_.discImages();cards_=folders_.cards();images_=folders_.cardImages();
        bool any=false;
        if(!discReady() && discs_.size()==1) {settings_.disc=discs_[0];any=true;}
        if(folders_.card(settings_).empty() && !cards_.empty()) {settings_.memory_card=cards_[0];any=true;}
        if(any)changed();
    }
    // Replaces the open list screen, keeping the highlighted entry where it can.
    void reopen(MenuScreen screen) {
        const unsigned at=host_.menu.screen().highlighted;
        host_.later([this,screen=std::move(screen),at]() mutable {
            host_.menu.close();
            screen.highlighted=std::min<unsigned>(at,unsigned(screen.entries.size())-1);
            host_.menu.open(std::move(screen));
            // open() starts at the first entry: move down to the one that was highlighted.
            while(host_.menu.screen().highlighted<at && host_.menu.screen().highlighted+1<host_.menu.screen().entries.size())host_.menu.press(MenuKey::Down);
        });
    }
    MenuEntry openList(const std::string& label,List list,const std::string& note) {
        return menuAction(label,[this,list] {
            host_.menu.open(list==List::Discs?discScreen():cardScreen());
        },note);
    }

    MenuScreen mainScreen() {
        MenuScreen main;main.title="BT2 Recomp";
        MenuEntry start=menuAction("Start game",[this] {
            if(discReady()) {result=LauncherResult::Start;host_.finished=true;}
        },"Disc image: the data folder. Memory cards: the saves folder.");
        start.value=[this]{return std::string(discReady()?"":"No game disc in the data folder");};
        main.entries.push_back(start);
        MenuEntry disc=openList("Game disc",List::Discs,"The disc images in the data folder next to the program.");
        disc.value=[this]{const auto d=folders_.disc(settings_);return d.empty()?std::string("none"):utf8(d.filename());};
        main.entries.push_back(disc);
        MenuEntry card=openList("Memory card",List::Cards,"The memory cards in the saves folder next to the program.");
        card.value=[this]{const auto c=folders_.card(settings_);return c.empty()?std::string("a new card"):utf8(c.filename());};
        main.entries.push_back(card);
        const std::function<void()> apply=[this]{changed();};
        main.entries.push_back(menuAction("Controls",[this,apply]{host_.menu.open(menu_screens::controls(host_,settings_,apply));},"Keys and gamepad buttons, fast forward, the in-game menu."));
        main.entries.push_back(menuAction("Display",[this,apply]{host_.menu.open(menu_screens::display(host_,settings_,apply));},"Window size, fullscreen, scaling, frame rate."));
        main.entries.push_back(menuAction("Enhancements",[this,apply]{host_.menu.open(menu_screens::enhancements(settings_,apply));},"HD textures, super-sampling, the HD outline, widescreen, 60 fps battles, fast loading, beam struggles by buttons."));
        main.entries.push_back(menuAction("Sound",[this,apply]{host_.menu.open(menu_screens::sound(settings_,apply));},"Master, music and effects volume."));
        main.entries.push_back(menuAction("Online battle",[this]{host_.menu.open(onlineScreen());},"1P vs 2P with a player on another PC: one hosts, the other joins with the host's code."));
        main.entries.back().available=[this]{return discReady();};
        main.entries.push_back(menuAction("Quit",[this]{result=LauncherResult::Quit;host_.finished=true;}));
        return main;
    }
    // Both PCs start fresh into the game's Dueling screen with every fighter unlocked; no save is used, and each player
    // keeps their own picture settings and enhancements. Hosting is one press; joining is one press, the code pasted or
    // typed, and Enter (the user asked for simple navigation, 2026-11-04); the rest waits under Options.
    MenuScreen onlineScreen() {
        MenuScreen screen;screen.title="Online battle";
        const auto start=[this](LauncherResult how){if(discReady()) {result=how;host_.finished=true;}};
        screen.entries.push_back(menuAction("Host a battle",[start]{start(LauncherResult::Host);},
            "Your code is copied by itself: paste it into a chat, then copy the reply code they send back."));
        screen.entries.push_back(menuAction("Join with a code",[this,start] {
            // A code already on the clipboard (copied from the chat) is filled in; Ctrl+V pastes one.
            const auto clip=host_.clipboardText();
            const bool has_code=clipboard::codeIn(clip).has_value();
            host_.type(has_code?"The host's code, pasted from the clipboard: Enter joins":"The host's code: type it or paste it (Ctrl+V), then Enter",
                       has_code?clipboard::pastedInto("",clip,Settings::online_address_characters,253):settings_.online_address,
                       Settings::online_address_characters,253,
                [this,start](std::string text) {
                    if(text.empty())return;
                    settings_.online_address=std::move(text);changed();
                    start(LauncherResult::Join);
                });
        },"Copy the host's code first: it is filled in, and Enter joins. Then send back the reply code shown."));
        screen.entries.back().value=[this]{return settings_.online_address.empty()?std::string():"last: "+settings_.online_address;};
        screen.entries.push_back(menuAction("Options",[this]{host_.menu.open(onlineOptions());},"Only if needed: the port, the input delay, the router (UPnP)."));
        return screen;
    }
    MenuScreen onlineOptions() {
        MenuScreen screen;screen.title="Online battle options";
        MenuEntry port;port.label="Port";
        port.value=[this]{return std::to_string(settings_.online_port);};
        port.confirm=[this] {
            host_.type("The UDP port, 1 to 65535 (Enter accepts, Escape cancels)",std::to_string(settings_.online_port),"0123456789",5,[this](std::string text) {
                const unsigned value=text.empty()?0u:unsigned(std::stoul(text));
                if(value>=1 && value<=65535) {settings_.online_port=value;changed();}
            });
        };
        port.note="The port hosted (codes carry it); joining by address, the host's port. Confirm, then type it.";
        screen.entries.push_back(port);
        MenuEntry delay=menu_screens::number("Input delay",settings_.online_delay,0,8,1,"",[this]{changed();},
            "The host's: fields from a press to the game playing it (60 a second). Auto: chosen from the connection. Raise it if the game halts often.");
        delay.value=[this]{const unsigned d=settings_.online_delay;return d?std::to_string(d)+(d==1?" field":" fields"):std::string("Auto");};
        screen.entries.push_back(delay);
        screen.entries.push_back(menu_screens::toggle("Open the port by UPnP",settings_.online_upnp,[this]{changed();},
            "When hosting: asks the router to open the port. Off, or a router without UPnP: forward the UDP port to this PC by hand."));
        return screen;
    }
    MenuScreen discScreen() {
        MenuScreen screen;screen.title=disc_title;screen.browser=true;
        for(const auto& name:discs_) {
            MenuEntry e=menuAction(name,[this,name]{settings_.disc=name;changed();},"The game is read from this image.");
            e.icon=MenuIcon::Disc;
            e.value=[this,name]{return std::string(settings_.disc==name?"in use":"");};
            screen.entries.push_back(e);
        }
        if(discs_.empty()) {
            MenuEntry none=menuAction("No disc image yet",[]{},"Put the game's .iso into the data folder; it appears here.");
            none.icon=MenuIcon::NoDisc;screen.entries.push_back(none);
        }
        return screen;
    }
    MenuScreen cardScreen() {
        MenuScreen screen;screen.title=card_title;screen.browser=true;
        const auto saves_in=[](const std::filesystem::path& card) {
            unsigned count=0;std::error_code failed;
            for(std::filesystem::directory_iterator it(card,failed),end;!failed && it!=end;it.increment(failed))if(it->is_directory(failed))++count;
            return std::to_string(count)+(count==1?" save":" saves");
        };
        for(const auto& name:cards_) {
            MenuEntry e=menuAction(name,[this,name]{settings_.memory_card=name;changed();},
                saves_in(folders_.saves/pathFromUtf8(name))+" on this card. The game loads from and saves to it.");
            e.icon=MenuIcon::Card;
            e.value=[this,name]{return std::string(settings_.memory_card==name?"in use":"");};
            screen.entries.push_back(e);
        }
        for(const auto& name:images_) {
            MenuEntry e=menuAction(name,[]{},"A card image of another program; the port uses card folders (one folder per save) and cannot read it yet.");
            e.enabled=false;e.value=[]{return std::string("unsupported");};e.icon=MenuIcon::CardImage;
            screen.entries.push_back(e);
        }
        MenuEntry fresh=menuAction("New empty card",[this] {
            settings_.memory_card=folders_.newCard();changed();refresh();
        },"Makes a new card in the saves folder and puts it in the slot.");
        fresh.icon=MenuIcon::NewCard;screen.entries.push_back(fresh);
        return screen;
    }
};

inline LauncherResult runLauncher(Settings& settings,const std::string& settings_path,const GameFolders& folders,ViewerWindow& window,HostInput& input) {
    MenuHost host(window,input);
    host.setLogo(folders.logo());
    const auto apply=[&] {
        window.setScaling(settings.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);
        window.setLayout(settings.window_scale,settings.fullscreen);
    };
    apply();
    Launcher launcher(settings,settings_path,folders,host);
    launcher.on_change=apply;
    launcher.open();
    host.every(500,[&]{launcher.refresh();});
    if(!host.run("BT2 Recomp - launcher"))return LauncherResult::Quit;
    return launcher.result;
}
#endif
