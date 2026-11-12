// The in-game menu (docs/plans/launcher/LAUNCHER_PLAN.md, Overlay): shown over the dimmed game
// picture while the game stands still. Changes take effect and are saved at once.
#pragma once
#ifdef _WIN32
#include "host/menu_host.h"

enum class OverlayResult {Resume,Title,Launcher,Quit};

// `applied` runs after every change, after the settings were saved. `close_after_ms` and `last_drawn`
// serve the pause test: the menu closes by itself and hands back the last picture it drew.
inline OverlayResult runOverlay(Settings& settings,const std::string& settings_path,ViewerWindow& window,HostInput& input,
                                const DisplayFrame& picture,const std::function<void()>& applied,unsigned close_after_ms=0,DisplayFrame* last_drawn=nullptr);

// An online battle's menu: both games stand still while it is open (`keep_alive`, run every 100 ms, keeps the session
// going and throws when it ends), and it only resumes or ends the battle; Launcher: end it (the other player is told).
inline OverlayResult runOnlineOverlay(ViewerWindow& window,HostInput& input,const DisplayFrame& picture,const std::function<void()>& keep_alive,unsigned close_after_ms=0) {
    MenuHost host(window,input);host.close_after_ms=close_after_ms; // The pause test closes it by itself.
    OverlayResult result=OverlayResult::Resume;
    MenuScreen main;main.title="Paused (online battle)";
    main.entries.push_back(menuAction("Resume",[&]{host.finished=true;},"The other player's game waits meanwhile."));
    main.entries.push_back(menuAction("End online battle",[&] {
        MenuScreen screen;screen.title="End the online battle?";
        screen.entries.push_back(menuAction("No, go back",[&host]{host.menu.close();}));
        screen.entries.push_back(menuAction("Yes",[&host,&result]{result=OverlayResult::Launcher;host.finished=true;},"The other player is told, and you return to the launcher."));
        host.menu.open(std::move(screen));
    },"Leaves the battle and returns to the launcher; the other player is told."));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[&]{host.finished=true;}; // Back resumes.
    host.every(100,keep_alive);
    if(!host.run("BT2 Recomp - paused",&picture))return OverlayResult::Launcher; // The window closed: leave the battle.
    return result;
}

inline OverlayResult runOverlay(Settings& settings,const std::string& settings_path,ViewerWindow& window,HostInput& input,
                                const DisplayFrame& picture,const std::function<void()>& applied,unsigned close_after_ms,DisplayFrame* last_drawn) {
    MenuHost host(window,input);host.close_after_ms=close_after_ms;
    OverlayResult result=OverlayResult::Resume;
    const std::function<void()> changed=[&] {settings.save(settings_path);applied();};
    const auto confirmed=[&](const char* title,OverlayResult chosen) {
        MenuScreen screen;screen.title=title;
        screen.entries.push_back(menuAction("No, go back",[&host]{host.menu.close();}));
        screen.entries.push_back(menuAction("Yes",[&host,&result,chosen]{result=chosen;host.finished=true;},"Progress that the game has not saved is lost."));
        host.menu.open(std::move(screen));
    };
    MenuScreen main;main.title="Paused";
    main.entries.push_back(menuAction("Resume",[&]{host.finished=true;}));
    // The settings as in the launcher (menu_screens::settingsHub), listed over the game.
    main.entries.push_back(menuAction("Settings",[&]{host.menu.open(menu_screens::settingsHub(host,settings,changed,false));},"Graphics, controls, sound, fast loading."));
    main.entries.push_back(menuAction("Return to the title screen",[&]{confirmed("Return to the title screen?",OverlayResult::Title);},"Starts the game again; the logos and films pass in fast forward."));
    main.entries.push_back(menuAction("Quit to the launcher",[&]{confirmed("Quit to the launcher?",OverlayResult::Launcher);}));
    main.entries.push_back(menuAction("Quit game",[&]{confirmed("Quit the game?",OverlayResult::Quit);}));
    host.menu.open(std::move(main));
    host.menu.on_leave_root=[&]{host.finished=true;}; // Back resumes.
    const bool open=host.run("BT2 Recomp - paused",&picture);
    if(last_drawn)*last_drawn=host.last_drawn;
    if(!open)return OverlayResult::Quit;
    return result;
}
#endif
