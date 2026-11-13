// The player's programs (docs/plans/launcher/LAUNCHER_PLAN.md). Built twice from this file:
//   bt2-game      starts the game with the stored settings; while the game is not set up (no
//                 settings file, or no disc image of the data folder chosen) it shows the launcher first.
//   bt2-launcher  always shows the launcher (BT2_LAUNCHER_ALWAYS).
// Next to the programs: `data` (the disc image) and `saves` (the memory cards), made on the first start
// and never emptied (host/game_folders.h), and `assets` (the launcher's logo, from the build).
// Both then run the runtime program (bt2-entry-probe, next to this one) as a player's run: window,
// sound, live input, the GPU renderer, the settings file. It needs only the player's disc image.
// When the player chooses "Quit to the launcher" in the game, the launcher comes back.
#include "host/game_folders.h"
#include "host/launcher.h"
#include "host/player_session.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
namespace fs=std::filesystem;

fs::path programFolder() {
    char path[MAX_PATH]{};GetModuleFileNameA(nullptr,path,MAX_PATH);
    return fs::path(path).parent_path();
}
// Where `user` (settings, the last run's messages) lives and the runtime runs: a developer's repository above the
// program (it has work/bt2-generation), else the program's own folder (a release).
fs::path findRoot(const fs::path& program) {
    fs::path from=program;
    for(unsigned up=0;up<8;++up,from=from.parent_path()) {
        std::error_code error;
        if(fs::is_directory(from/"work"/"bt2-generation",error))return from;
        if(!from.has_parent_path() || from.parent_path()==from)break;
    }
    return program;
}
std::string inQuotes(const fs::path& path) {return "\""+path.string()+"\"";}

// Whether a process shows a top-level window yet.
bool showsWindow(DWORD process_id) {
    struct Look {DWORD id;bool found=false;} look{process_id};
    EnumWindows([](HWND window,LPARAM data)->BOOL {
        auto& l=*reinterpret_cast<Look*>(data);
        DWORD owner=0;GetWindowThreadProcessId(window,&owner);
        if(owner==l.id && IsWindowVisible(window) && !GetWindow(window,GW_OWNER)) {l.found=true;return FALSE;}
        return TRUE;
    },reinterpret_cast<LPARAM>(&look));
    return look.found;
}

// Runs the game; returns the runtime's exit code. Its messages go to `log`.
// `online`: BT2_NET for an online battle (probe/online_session.h), empty for a game of one's own.
// `launcher`: the launcher's window, kept up (and answering) until the game's window opens in its place, then hidden:
// the player sees one window from the launcher to the game (the user, 2026-11-12: reaching an online battle opened two
// windows). Before, the launcher's window closed first and the game's opened elsewhere a moment later.
DWORD runGame(const fs::path& root,const fs::path& settings_path,const Settings& settings,const GameFolders& folders,const fs::path& log,
              const std::string& online,ViewerWindow* launcher) {
    const fs::path runtime=programFolder()/"bt2-entry-probe.exe";
    std::error_code error;
    if(!fs::is_regular_file(runtime,error))throw std::runtime_error("The runtime program is missing: "+runtime.string());
    // STOP_REPORT DISC_IMAGE, as the runtime's command line is defined.
    const std::string command=inQuotes(runtime)+" "+inQuotes(root/"user"/"last-run.json")+" "+inQuotes(folders.disc(settings));
    for(const char* name:{"BT2_VIEWER","BT2_AUDIO_DEVICE","BT2_HOST_INPUT"})SetEnvironmentVariableA(name,"1");
    SetEnvironmentVariableA("BT2_GPU_GS","on");
    SetEnvironmentVariableA("BT2_PLAYER_SETTINGS",settings_path.string().c_str());
    SetEnvironmentVariableA("BT2_NET",online.empty()?nullptr:online.c_str());
    SetEnvironmentVariableA("BT2_NET_DELAY",online.empty()?nullptr:std::to_string(settings.online_delay).c_str()); // 0: chosen from the round trip.
    SetEnvironmentVariableA("BT2_NET_UPNP",online.empty() || !settings.online_upnp?nullptr:"1");
    // The picture settings the renderer takes at its start: HD textures (when a pack is there) and super-sampling.
    const fs::path pack=settings.hd_textures?folders.texturePack():fs::path{};
    SetEnvironmentVariableW(L"BT2_HD_TEXTURES",pack.empty()?nullptr:pack.wstring().c_str());
    SetEnvironmentVariableA("BT2_HD_BUTTONS",settings.playstation_buttons?nullptr:"xbox");
    SetEnvironmentVariableA("BT2_SUPER_SAMPLING",std::to_string(settings.super_sampling).c_str());
    SetEnvironmentVariableA("BT2_GAME_PATCHES",settings.widescreen?"widescreen":nullptr); // ee/game_patches.h
    SetEnvironmentVariableA("BT2_SIXTY_FPS",settings.sixty_fps?"1":nullptr); // gs/gpu/in_between_frames.h
    SetEnvironmentVariableA("BT2_NATIVE_PASSES",settings.hd_outline?nullptr:"-ink-edges"); // gs/gpu/native/native_passes.h
    SetEnvironmentVariableA("BT2_DOF_STRENGTH",settings.light_depth_of_field?"25":nullptr); // gs/gpu/native/dof_weight.cpp
    SetEnvironmentVariableA("BT2_FAST_LOADING",settings.fast_loading?"1":nullptr); // host/loading_skip.h
    // Quick start (probe/quick_start.h): not for an online battle; the return to the title (BT2_QUICK_TITLE) keeps the title.
    SetEnvironmentVariableA("BT2_QUICK_START",settings.quick_start && online.empty()?"1":nullptr);
    SetEnvironmentVariableA("BT2_VARIABLE_REFRESH",settings.variable_refresh?"1":nullptr); // gs/gpu/gpu_window_presenter.cpp
    SetEnvironmentVariableA("BT2_LOW_INPUT_LAG",settings.low_input_lag?"1":nullptr); // host/early_picture.h
    SetEnvironmentVariableA("BT2_STRUGGLE_BUTTONS",settings.struggle_buttons?"1":nullptr); // ee/struggle_buttons.h
    // Its prompt: the port's own art over the turning stick, in the chosen button style.
    const fs::path prompt=folders.assets/"struggle-buttons"/(settings.playstation_buttons?"playstation":"xbox");
    SetEnvironmentVariableW(L"BT2_TEXTURE_OVERRIDES",settings.struggle_buttons?prompt.wstring().c_str():nullptr);
    // The memory card folder in slot 1, when the settings name one of the saves folder.
    const fs::path card=folders.card(settings);
    SetEnvironmentVariableW(L"BT2_CARD_FOLDER",card.empty()?nullptr:card.wstring().c_str());
    for(const char* name:{"BT2_FRAME_LIMIT","BT2_PAD_SCRIPT","BT2_PAD_RECORD","BT2_VIEWER_SCALE","BT2_GPU_GS_SNAPSHOT"})SetEnvironmentVariableA(name,nullptr);
    const auto at=launcher?launcher->position():std::nullopt;
    SetEnvironmentVariableA("BT2_WINDOW_AT",at?(std::to_string(at->x)+","+std::to_string(at->y)).c_str():nullptr); // host/viewer_window.h

    SECURITY_ATTRIBUTES inherit{sizeof inherit,nullptr,TRUE};
    const HANDLE out=CreateFileA(log.string().c_str(),GENERIC_WRITE,FILE_SHARE_READ,&inherit,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(out==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write "+log.string());
    if(settings.hd_textures) { // Which pack the run uses, first in its messages.
        const std::string line=pack.empty()?"HD textures: on, but no pack in "+utf8(folders.textures)+" (no folder there holds \"replacements\")\r\n"
                                           :"HD textures: the pack "+utf8(pack)+"\r\n";
        DWORD written=0;WriteFile(out,line.data(),DWORD(line.size()),&written,nullptr);
    }
    STARTUPINFOA startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=out;
    PROCESS_INFORMATION process{};
    std::string mutable_command=command;
    if(!CreateProcessA(nullptr,mutable_command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.string().c_str(),&startup,&process)) {
        CloseHandle(out);throw std::runtime_error("The runtime program could not be started.");
    }
    bool handed=!launcher || !launcher->opened();
    for(;;) {
        if(MsgWaitForMultipleObjects(1,&process.hProcess,FALSE,handed?INFINITE:50,QS_ALLINPUT)==WAIT_OBJECT_0)break;
        if(launcher)launcher->pump(); // Hidden or not, the launcher's window keeps answering.
        if(!handed && showsWindow(process.dwProcessId)) {launcher->setVisible(false);handed=true;}
    }
    if(launcher)launcher->setVisible(false); // Also when the game ended before its window opened.
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(out);
    return code;
}
// The end of the runtime's messages, for the player when a run stops by itself.
std::string lastLines(const fs::path& log,unsigned count) {
    std::ifstream in(log);std::vector<std::string> lines;std::string line;
    while(std::getline(in,line))if(!line.empty() && line.rfind("[INFO]",0)!=0)lines.push_back(line.substr(0,300));
    std::string text;
    for(std::size_t n=lines.size()>count?lines.size()-count:0;n<lines.size();++n)text+=lines[n]+"\n";
    return text;
}
// Why an online battle ended: the runtime's "Online session: ..." stop, without its prefix; empty for other stops.
std::string onlineReason(const fs::path& log) {
    std::ifstream in(log);std::string line,reason;
    for(const std::string_view stop:{"failed: Online session: ","ended: Online session: "}) {
        in.clear();in.seekg(0);
        while(std::getline(in,line))if(const auto at=line.find(stop);at!=std::string::npos)reason=line.substr(at+stop.size());
        if(!reason.empty())break;
    }
    if(!reason.empty())reason[0]=char(std::toupper(static_cast<unsigned char>(reason[0])));
    return reason.empty()?reason:reason+".";
}
// How many places of untranslated code the session ran through the interpreter (its log lines).
unsigned interpretedPlaces(const fs::path& log) {
    std::ifstream in(log);std::string line;unsigned count=0;
    while(std::getline(in,line))if(line.find("runs interpreted")!=std::string::npos)++count;
    return count;
}
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR arguments,int) {
    try {
        if(std::string(arguments)=="--preview-paused") {
            // The in-game menu over a stand-in picture, without starting the game: for looking at it and trying its screens.
            Settings preview;DisplayFrame picture;picture.width=640;picture.height=448;picture.bgra.resize(640*448);
            for(unsigned y=0;y<448;++y)for(unsigned x=0;x<640;++x)picture.bgra[y*640+x]=((x*255/640)<<16)|((y*255/448)<<8)|0x60;
            ViewerWindow window;HostInput input;
            const auto file=(fs::temp_directory_path()/"bt2-preview-settings").string();
            runOverlay(preview,file,window,input,picture,[&]{window.setScaling(preview.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);window.setLayout(preview.window_scale,preview.fullscreen);});
            return 0;
        }
        const fs::path root=findRoot(programFolder()),user=root/"user",settings_path=user/"settings",log=user/"last-run.log";
        fs::create_directories(user);
        const GameFolders folders(programFolder());
        {
            std::error_code error;
            const bool first_start=!fs::exists(folders.saves,error);
            folders.create();
            if(first_start)folders.copyCardsFrom(user/"memcards");
        }
        Settings settings;
        if(auto stored=Settings::load(settings_path.string()))settings=*stored;
#ifdef BT2_LAUNCHER_ALWAYS
        bool launcher=true;
#else
        std::error_code error;
        bool launcher=!fs::is_regular_file(settings_path,error) || !folders.configured(settings);
#endif
        LauncherResult chosen=LauncherResult::Start; // Host or Join: the next run is an online battle.
        std::unique_ptr<ViewerWindow> window; // The launcher's, kept while the game runs (runGame).
        bool back_from_online=false;std::string online_notice; // The launcher opens on its online page, saying how it ended.
        for(;;) {
            if(launcher) {
                if(!window || window->closed())window=std::make_unique<ViewerWindow>();
                else window->setVisible(true);
                HostInput input;
                settings.save(settings_path.string());
                chosen=runLauncher(settings,settings_path.string(),folders,*window,input,back_from_online,std::move(online_notice));
                back_from_online=false;online_notice.clear();
                if(chosen==LauncherResult::Quit)return 0;
            }
            if(!folders.configured(settings)) {launcher=true;continue;} // No disc image of the data folder: choose one.
            if(folders.card(settings).empty()) {
                // The game always has a card to save to: the first card of the saves folder, or a new
                // empty one when there is none. Existing cards are never replaced.
                const auto cards=folders.cards();
                settings.memory_card=cards.empty()?folders.newCard():cards.front();settings.save(settings_path.string());
            }
            const bool online=chosen==LauncherResult::Host || chosen==LauncherResult::Join;
            // Joining: the host's code (host/connection_code.h) gives the address and port, else the address and the port setting.
            const bool host_code=!connection_code::readAll(settings.online_address).empty();
            const std::string net=chosen==LauncherResult::Host?"host:"+std::to_string(settings.online_port)
                :chosen!=LauncherResult::Join?std::string()
                :host_code?"join:"+settings.online_address:"join:"+settings.online_address+":"+std::to_string(settings.online_port);
            chosen=LauncherResult::Start;
            const DWORD code=runGame(root,settings_path,settings,folders,log,net,window.get());
            SetEnvironmentVariableA("BT2_QUICK_TITLE",nullptr);
            if(auto stored=Settings::load(settings_path.string()))settings=*stored; // The in-game menu may have changed them.
            if(online) { // Back to the launcher's online page; why it ended, when it did not end here, at its top.
                back_from_online=true;
                if(code!=0 && code!=DWORD(PlayerSession::launcher_exit_code)) {
                    online_notice=onlineReason(log);
                    if(online_notice.empty())online_notice="The game stopped: "+lastLines(log,1).substr(0,80)+" (all in user\\last-run.log)";
                    while(!online_notice.empty() && (online_notice.back()=='\n' || online_notice.back()==' '))online_notice.pop_back();
                }
                launcher=true;continue;
            }
            if(code==DWORD(PlayerSession::launcher_exit_code)) {launcher=true;continue;}
            // "Return to the title screen": the game again, its startup in fast forward up to the title.
            if(code==DWORD(PlayerSession::title_exit_code)) {launcher=false;SetEnvironmentVariableA("BT2_QUICK_TITLE","1");continue;}
            // Code that was never translated ran interpreted (slower), or stopped the game: the discovery
            // loop builds it in.
            const std::string discover="double-click\n"+(root/"work"/"scripts"/"discover.cmd").string()+"\nand play again.\n";
            const unsigned interpreted=interpretedPlaces(log);
            if(code!=0) {
                const std::string last=lastLines(log,6);
                const std::string hint=last.find("Branch target")!=std::string::npos||interpreted?"\nThe game reached code that is not translated yet. To build it in, "+discover:"";
                MessageBoxA(nullptr,("The game stopped.\n\n"+last+hint+"\nThe full messages are in "+log.string()).c_str(),"BT2 Recomp",MB_OK|MB_ICONINFORMATION);
            } else if(interpreted) {
                MessageBoxA(nullptr,("The game ran "+std::to_string(interpreted)+" place(s) of code that is not translated yet, more slowly than the rest.\nTo build it in, "+discover).c_str(),"BT2 Recomp",MB_OK|MB_ICONINFORMATION);
            }
            return int(code);
        }
    } catch(const std::exception& error) {
        MessageBoxA(nullptr,error.what(),"BT2 Recomp",MB_OK|MB_ICONERROR);
        return 1;
    }
}
