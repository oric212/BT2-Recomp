// The launcher: the settings screens before the game starts (docs/plans/launcher/LAUNCHER_PLAN.md).
// Every change is applied to the window and written to the settings file at once.
#pragma once
#ifdef _WIN32
#include "host/menu_host.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

enum class LauncherResult {Start,Quit};

// Disc images offered on the Game disc screen: the .iso files in `rom` and `user` under `root`.
inline std::vector<std::string> discImages(const std::string& root) {
    std::vector<std::string> found;
    for(const char* folder:{"rom","user"}) {
        std::error_code error;
        for(std::filesystem::directory_iterator it(std::filesystem::path(root)/folder,error),end;!error && it!=end;it.increment(error)) {
            auto extension=it->path().extension().string();
            for(auto& c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
            if(it->is_regular_file(error) && extension==".iso")found.push_back(it->path().string());
        }
    }
    std::sort(found.begin(),found.end());
    return found;
}

inline LauncherResult runLauncher(Settings& settings,const std::string& settings_path,const std::string& root,ViewerWindow& window,HostInput& input) {
    MenuHost host(window,input);
    LauncherResult result=LauncherResult::Quit;
    const auto apply=[&] {
        window.setScaling(settings.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);
        window.setLayout(settings.window_scale,settings.fullscreen);
    };
    const std::function<void()> changed=[&] {apply();settings.save(settings_path);};
    apply();
    const auto discs=discImages(root);
    if(settings.disc.empty() && discs.size()==1) {settings.disc=discs[0];settings.save(settings_path);} // The only image there is.
    const auto disc_ready=[&] {std::error_code error;return !settings.disc.empty() && std::filesystem::is_regular_file(settings.disc,error);};

    MenuScreen main;main.title="BT2 Recomp";
    MenuEntry start=menuAction("Start game",[&] {
        if(disc_ready()) {result=LauncherResult::Start;host.finished=true;}
    });
    start.value=[disc_ready]{return std::string(disc_ready()?"":"choose a game disc first");};
    main.entries.push_back(start);
    MenuEntry saves=menuAction("Memory card",[&] {
        // The card folders under user/memcards; one of them, or none, is in slot 1.
        MenuScreen screen;screen.title="Memory card (slot 1)";
        const auto cards_folder=std::filesystem::path(root)/"user"/"memcards";
        std::error_code error;std::vector<std::string> cards;
        for(std::filesystem::directory_iterator it(cards_folder,error),end;!error && it!=end;it.increment(error))if(it->is_directory(error))cards.push_back(it->path().filename().string());
        std::sort(cards.begin(),cards.end());
        MenuEntry none=menuAction("No card",[&settings,changed]{settings.memory_card.clear();changed();});
        none.value=[&settings]{return std::string(settings.memory_card.empty()?"in use":"");};
        screen.entries.push_back(none);
        for(const auto& card:cards) {
            unsigned count=0;
            for(std::filesystem::directory_iterator it(cards_folder/card,error),end;!error && it!=end;it.increment(error))if(it->is_directory(error))++count;
            MenuEntry e=menuAction(card,[&settings,card,changed]{settings.memory_card=card;changed();},
                std::to_string(count)+(count==1?" save":" saves")+". Loading works; the game stops if it tries to save (not built yet).");
            e.value=[&settings,card]{return std::string(settings.memory_card==card?"in use":"");};
            screen.entries.push_back(e);
        }
        host.menu.open(std::move(screen));
    },"A card is a folder in user/memcards with one folder per save.");
    saves.value=[&settings]{return settings.memory_card.empty()?std::string("no card"):settings.memory_card;};
    main.entries.push_back(saves);
    main.entries.push_back(menuAction("Controls",[&]{host.menu.open(menu_screens::controls(host,settings,changed));},"Keys and gamepad buttons, fast forward, the in-game menu."));
    main.entries.push_back(menuAction("Display",[&]{host.menu.open(menu_screens::display(settings,changed));}));
    main.entries.push_back(menuAction("Sound",[&]{host.menu.open(menu_screens::sound(settings,changed));}));
    main.entries.push_back(menuAction("Game disc",[&] {
        MenuScreen screen;screen.title="Game disc";
        for(const auto& path:discs) {
            MenuEntry e=menuAction(std::filesystem::path(path).filename().string(),[&settings,path,changed]{settings.disc=path;changed();},
                "In "+std::filesystem::path(path).parent_path().string());
            e.value=[&settings,path]{return std::string(settings.disc==path?"in use":"");};
            screen.entries.push_back(e);
        }
        if(discs.empty())screen.entries.push_back(menuAction("No disc image found",[]{},"Put the game's .iso file into the rom or the user folder."));
        host.menu.open(std::move(screen));
    },"The disc image the game is read from."));
    main.entries.push_back(menuAction("Quit",[&]{result=LauncherResult::Quit;host.finished=true;}));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[]{}; // Back does nothing on the first screen.
    if(!host.run("BT2 Recomp - launcher"))return LauncherResult::Quit;
    return result;
}
#endif
