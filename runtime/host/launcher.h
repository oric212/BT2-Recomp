// The launcher: the settings screens before the game starts (docs/plans/launcher/LAUNCHER_PLAN.md).
// Every change is applied to the window and written to the settings file at once.
#pragma once
#ifdef _WIN32
#include "host/folder_dialog.h"
#include "host/menu_host.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

enum class LauncherResult {Start,Quit};

// Disc images offered on the Game disc screen: the .iso files in `disc` (the folder meant for
// them), `rom` and `user` under `root`, and in the folder the player chose (`extra`, if any).
inline std::vector<std::string> discImages(const std::string& root,const std::string& extra) {
    std::vector<std::string> found;
    std::vector<std::filesystem::path> folders;
    for(const char* folder:{"disc","rom","user"})folders.push_back(std::filesystem::path(root)/folder);
    if(!extra.empty())folders.push_back(extra);
    for(const auto& folder:folders) {
        std::error_code error;
        for(std::filesystem::directory_iterator it(folder,error),end;!error && it!=end;it.increment(error)) {
            auto extension=it->path().extension().string();
            for(auto& c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
            if(it->is_regular_file(error) && extension==".iso")found.push_back(it->path().string());
        }
    }
    std::sort(found.begin(),found.end());found.erase(std::unique(found.begin(),found.end()),found.end());
    return found;
}

// Makes a new, empty card folder in user/memcards ("Card N" with the lowest number not taken, so
// no existing card is touched) and returns its name.
inline std::string newCardFolder(const std::string& root) {
    const auto cards=std::filesystem::path(root)/"user"/"memcards";
    std::error_code error;
    for(unsigned n=1;;++n) {
        const std::string name="Card "+std::to_string(n);
        if(std::filesystem::exists(cards/name,error))continue;
        std::filesystem::create_directories(cards/name,error);
        if(error)throw std::runtime_error("Cannot make the card folder "+(cards/name).string());
        return name;
    }
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
    {
        const auto discs=discImages(root,settings.disc_folder);
        if(settings.disc.empty() && discs.size()==1) {settings.disc=discs[0];settings.save(settings_path);} // The only image there is.
    }
    const auto disc_ready=[&] {std::error_code error;return !settings.disc.empty() && std::filesystem::is_regular_file(settings.disc,error);};

    MenuScreen main;main.title="BT2 Recomp";
    MenuEntry start=menuAction("Start game",[&] {
        if(disc_ready()) {result=LauncherResult::Start;host.finished=true;}
    });
    start.value=[disc_ready]{return std::string(disc_ready()?"":"choose a game disc first");};
    main.entries.push_back(start);
    // Screens that list folders are built again after the player chose a folder.
    std::function<MenuScreen()> card_screen,disc_screen;
    const auto reopen=[&](const std::function<MenuScreen()>& screen) {host.later([&host,&screen]{host.menu.press(MenuKey::Back);host.menu.open(screen());});};
    card_screen=[&] {
        // The cards in user/memcards and the card folders elsewhere that the player added; one of
        // them is in slot 1 (the game gets a new empty card when none is).
        MenuScreen screen;screen.title="Memory card (slot 1)";
        const auto cards_folder=std::filesystem::path(root)/"user"/"memcards";
        std::error_code error;std::vector<std::string> cards;
        for(std::filesystem::directory_iterator it(cards_folder,error),end;!error && it!=end;it.increment(error))if(it->is_directory(error))cards.push_back(it->path().filename().string());
        std::sort(cards.begin(),cards.end());
        const auto saves_in=[](const std::filesystem::path& card) {
            unsigned count=0;std::error_code failed;
            for(std::filesystem::directory_iterator it(card,failed),end;!failed && it!=end;it.increment(failed))if(it->is_directory(failed))++count;
            return std::to_string(count)+(count==1?" save":" saves");
        };
        const auto add=[&](const std::string& label,const std::string& value,const std::string& note) {
            MenuEntry e=menuAction(label,[&settings,value,changed]{settings.memory_card=value;changed();},note);
            e.value=[&settings,value]{return std::string(settings.memory_card==value?"in use":"");};
            screen.entries.push_back(e);
        };
        for(const auto& card:cards)add(card,card,saves_in(cards_folder/card)+". The game loads from and saves to this folder.");
        for(const auto& folder:settings.card_folders) {
            const std::filesystem::path path(folder);
            add(path.filename().string(),folder,(std::filesystem::is_directory(path,error)?saves_in(path):std::string("Folder not found"))+". In "+path.parent_path().string());
        }
        screen.entries.push_back(menuAction("New empty card",[&] {
            settings.memory_card=newCardFolder(root);changed();reopen(card_screen);
        },"Makes a new card folder in user/memcards and puts it in the slot."));
        screen.entries.push_back(menuAction("Add a card folder...",[&,cards_folder] {
            const std::string chosen=chooseFolder(host.window().handle(),"The memory card folder (it holds one folder per save)");
            if(chosen.empty())return;
            // A card of the program's own folder goes by its name, as when it is picked from the list.
            const std::filesystem::path picked(chosen);std::error_code same;
            if(std::filesystem::equivalent(picked.parent_path(),cards_folder,same))settings.memory_card=picked.filename().string();
            else {
                if(std::find(settings.card_folders.begin(),settings.card_folders.end(),chosen)==settings.card_folders.end())settings.card_folders.push_back(chosen);
                settings.memory_card=chosen;
            }
            changed();reopen(card_screen);
        },"Opens the system's folder dialog (mouse and keyboard). An empty folder is an empty card."));
        if(!settings.card_folders.empty())screen.entries.push_back(menuAction("Remove a folder from this list",[&] {
            MenuScreen removal;removal.title="Remove from the list";
            for(const auto& folder:settings.card_folders) {
                removal.entries.push_back(menuAction(std::filesystem::path(folder).filename().string(),[&,folder] {
                    auto& list=settings.card_folders;
                    list.erase(std::remove(list.begin(),list.end(),folder),list.end());
                    if(settings.memory_card==folder)settings.memory_card.clear();
                    changed();
                    host.later([&]{host.menu.press(MenuKey::Back);host.menu.press(MenuKey::Back);host.menu.open(card_screen());});
                },"Only the entry goes: the folder and its saves stay where they are. In "+std::filesystem::path(folder).parent_path().string()));
            }
            host.menu.open(std::move(removal));
        },"Takes a card folder that was added from elsewhere off this list. Nothing is deleted."));
        return screen;
    };
    MenuEntry saves=menuAction("Memory card",[&]{host.menu.open(card_screen());},"A card is a folder with one folder per save; user/memcards holds the cards listed here.");
    saves.value=[&settings]{return settings.memory_card.empty()?std::string("a new card"):std::filesystem::path(settings.memory_card).filename().string();};
    main.entries.push_back(saves);
    main.entries.push_back(menuAction("Controls",[&]{host.menu.open(menu_screens::controls(host,settings,changed));},"Keys and gamepad buttons, fast forward, the in-game menu."));
    main.entries.push_back(menuAction("Display",[&]{host.menu.open(menu_screens::display(host,settings,changed));}));
    main.entries.push_back(menuAction("Sound",[&]{host.menu.open(menu_screens::sound(settings,changed));}));
    disc_screen=[&] {
        MenuScreen screen;screen.title="Game disc";
        const auto discs=discImages(root,settings.disc_folder);
        for(const auto& path:discs) {
            MenuEntry e=menuAction(std::filesystem::path(path).filename().string(),[&settings,path,changed]{settings.disc=path;changed();},
                "In "+std::filesystem::path(path).parent_path().string());
            e.value=[&settings,path]{return std::string(settings.disc==path?"in use":"");};
            screen.entries.push_back(e);
        }
        if(discs.empty())screen.entries.push_back(menuAction("No disc image found",[]{},"Put the game's .iso file into the disc folder next to the program, or choose its folder."));
        screen.entries.push_back(menuAction("Choose a folder...",[&] {
            const std::string chosen=chooseFolder(host.window().handle(),"The folder with the game's disc image (.iso)");
            if(chosen.empty())return;
            settings.disc_folder=chosen;
            const auto there=discImages(root,chosen);
            unsigned inside=0;std::string only;
            for(const auto& path:there)if(std::filesystem::path(path).parent_path()==std::filesystem::path(chosen)) {++inside;only=path;}
            if(inside==1)settings.disc=only; // The only image in the chosen folder.
            changed();reopen(disc_screen);
        },"Opens the system's folder dialog (mouse and keyboard). Its .iso files are listed here."));
        return screen;
    };
    main.entries.push_back(menuAction("Game disc",[&]{host.menu.open(disc_screen());},"The disc image the game is read from."));
    main.entries.push_back(menuAction("Quit",[&]{result=LauncherResult::Quit;host.finished=true;}));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[]{}; // Back does nothing on the first screen.
    if(!host.run("BT2 Recomp - launcher"))return LauncherResult::Quit;
    return result;
}
#endif
