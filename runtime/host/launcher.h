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
    MenuEntry saves;saves.label="Saves";saves.value=[]{return std::string("not available yet");};saves.enabled=false;
    main.entries.push_back(saves);
    main.entries.push_back(menuAction("Controls",[&]{host.menu.open(menu_screens::controls(host,settings,changed));},"Keyboard keys and gamepad buttons, fast forward, the in-game menu."));
    main.entries.push_back(menuAction("Display",[&]{host.menu.open(menu_screens::display(settings,changed));}));
    main.entries.push_back(menuAction("Sound",[&]{host.menu.open(menu_screens::sound(settings,changed));}));
    main.entries.push_back(menuAction("Game disc",[&] {
        MenuScreen screen;screen.title="Game disc";
        for(const auto& path:discs) {
            MenuEntry e=menuAction(std::filesystem::path(path).filename().string(),[&settings,path,changed]{settings.disc=path;changed();},path);
            e.value=[&settings,path]{return std::string(settings.disc==path?"selected":"");};
            screen.entries.push_back(e);
        }
        if(discs.empty())screen.entries.push_back(menuAction("No disc image found",[]{},"Put the game's .iso file into the rom or the user folder and open this screen again."));
        host.menu.open(std::move(screen));
    },"The disc image the game is read from."));
    main.entries.push_back(menuAction("Quit",[&]{result=LauncherResult::Quit;host.finished=true;}));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[]{}; // Back does nothing on the first screen.
    if(!host.run("BT2 Recomp - launcher"))return LauncherResult::Quit;
    return result;
}
#endif
