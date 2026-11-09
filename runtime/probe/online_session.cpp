#include "online_session.h"
#include "ee/ee_runtime.h"
#include "ee/online_view.h"
#include "host/frame_pacer.h"
#include "host/menu_host.h"
#include "host/net_lockstep.h"
#include "host/online_screens.h"
#include "host/player_session.h"
#include "host/upnp_port.h"
#include "services/devices/pad_script.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
struct Online {
    std::unique_ptr<LockstepTransport> transport;
    std::unique_ptr<Lockstep> session;
    std::unique_ptr<UpnpPort> upnp;
    std::array<PadInput,2> ports{};
    PlayerSession* player=nullptr;FramePacer* pacer=nullptr;
    struct Field {std::chrono::steady_clock::time_point at;double wait=0;};
    std::array<Field,120> recent{};unsigned fields=0; // The last fields: when each ended and how long it waited for the other player.
    std::uint64_t fight_frames=0;unsigned fight_seen=0;bool fighting=false; // The fight frames at the last check, the field a fight last ran.
    ~Online() {
        if(session)
            std::cerr<<std::dec<<"Online session: "<<session->fieldsPlayed()<<" fields at an input delay of "<<session->delay()<<", "
                     <<session->checksCompared()<<" drift checks alike; waited for the other player on "<<session->waits()<<" fields ("
                     <<session->waitedSeconds()<<" s); round trip "<<int(session->roundTripMs())<<" ms."<<std::endl;
        session.reset(); // Tells the other player, while the transport is still open.
        upnp.reset();    // Closes the port on the router.
    }
} online;

void setSwitch(const char* name,const char* value) {
    // Both the C library's copy and the process's (BT2_CARD_FOLDER is read with GetEnvironmentVariableW).
    if(_putenv_s(name,value?value:"") || (!SetEnvironmentVariableA(name,value) && value))
        throw std::runtime_error(std::string("Online session: cannot set ")+name);
}

// What the host's waiting screen shows of the router (host/online_screens.h).
online_screens::Hosting hosting(unsigned port,const std::string& home) {
    online_screens::Hosting h;h.port=port;h.home_address=home;
    if(!online.upnp)return h;
    switch(online.upnp->state()) {
    case UpnpPort::State::Asking: h.router=online_screens::RouterPort::Asking;break;
    case UpnpPort::State::Opened: h.router=online_screens::RouterPort::Opened;h.internet_address=online.upnp->externalAddress();break;
    case UpnpPort::State::Failed: break;
    }
    return h;
}

// A player's window while connecting: what is awaited, and Cancel (or Back) to return to the launcher.
void waitInWindow(ViewerWindow& window,HostInput& input,const LockstepAddress& address) {
    MenuHost host(window,input);
    const auto start=std::chrono::steady_clock::now();
    const auto cancel=[&host]{host.finished=true;};
    const std::string home=address.host?UpnpPort::localAddress():std::string();
    // The host's code goes on the clipboard by itself once it is known (the user: make the code copyable); Confirm on
    // a code, or Ctrl+C for the one to give, copies again. The screen marks the code on the clipboard.
    std::string copied,auto_copied;
    std::function<bool(const std::string&)> copy;
    const auto screen=[&]{auto h=hosting(address.port,home);h.copied=copied;return online_screens::hosting(h,cancel,copy);};
    copy=[&](const std::string& code) {
        if(!host.copy(code))return false;
        copied=code;host.later([&]{host.menu.replace(screen());});return true;
    };
    if(address.host)host.on_copy=[&]{if(const auto code=online_screens::codeToGive(hosting(address.port,home)))copy(*code);};
    host.menu.open(address.host?screen():online_screens::joining(address.address,address.port,false,cancel));
    host.menu.on_leave_root=cancel;
    bool connected=false,late=false;
    auto shown=online.upnp?online.upnp->state():UpnpPort::State::Failed;
    host.every(50,[&] {
        if(online.session->connect(std::chrono::milliseconds(30))) {connected=true;host.finished=true;return;}
        if(address.host)
            if(const auto code=online_screens::codeToGive(hosting(address.port,home));code && *code!=auto_copied && copy(*code))auto_copied=*code;
        if(address.host && online.upnp && online.upnp->state()!=shown) {
            shown=online.upnp->state();host.menu.replace(screen());
            if(shown==UpnpPort::State::Failed)std::cerr<<"Online session: UPnP: "<<online.upnp->failure()<<std::endl;
        }
        if(!address.host && !late && std::chrono::steady_clock::now()-start>=std::chrono::seconds(10)) {
            late=true;host.menu.replace(online_screens::joining(address.address,address.port,true,cancel));
        }
    });
    if(!host.run(address.host?"BT2 Recomp - hosting an online battle":"BT2 Recomp - joining an online battle") || !connected)std::exit(PlayerSession::launcher_exit_code);
}

// The running program, hashed: two different builds refuse each other.
std::uint64_t programHash() {
    wchar_t path[MAX_PATH];
    const DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!length || length==MAX_PATH)throw std::runtime_error("Online session: the program's path is not found");
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Online session: the program cannot be read");
    std::uint64_t hash=lockstep_hash_start;
    std::vector<char> chunk(1<<20);
    while(in) {
        in.read(chunk.data(),std::streamsize(chunk.size()));
        hash=lockstepHash(hash,std::span(reinterpret_cast<const std::uint8_t*>(chunk.data()),std::size_t(in.gcount())));
    }
    return hash;
}

// After each field: the pace kept after a wait; in a player's game the connection line (the round trip with the frame
// rate display; always while the game runs slow for the other player).
// Measured 2026-11-04 (the online battle plan, two paced probes): the side ahead is held where the other's input just arrives, a
// wait in most fields that is only its pacing (pictures as even as without a session); at an 80 ms round trip with
// 5 ms of jitter that edge made a tenth of the host's fight pictures late, which the cushion takes away.
constexpr auto cushion=std::chrono::milliseconds(6);
void afterField() {
    const double wait=online.session->lastWaitSeconds();
    const auto now=std::chrono::steady_clock::now();
    const auto slot=online.fields++%online.recent.size();
    online.recent[slot]={now,wait};
    if(wait>0.001 && online.pacer)online.pacer->delayBy(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(wait))+cushion);
    // Outside fights (the menus, the select screens) each player is told which side is theirs: on one machine the
    // game's two-player screens show both cursors alike (the user, 2026-11-04: which fighter the other picked was unclear).
    // The log says when the line comes and goes (tool runs, which have no window, check it so).
    if(const auto frames=onlineFightFrames();frames!=online.fight_frames) {online.fight_frames=frames;online.fight_seen=online.fields;}
    const bool fighting=online.fight_frames && online.fields-online.fight_seen<60;
    if(fighting!=online.fighting || online.fields==1) {
        online.fighting=fighting;
        std::cerr<<"Online session: field "<<online.fields<<(fighting?", a fight: the side line hidden":", outside a fight: the side line shown")<<std::endl;
    }
    if(!online.player || (online.fields%30 && wait<0.25))return; // Every half second, and at once after a wait that was shown.
    // Slow: the last 120 fields took over 5% longer than the game's pace, and a tenth of that time went to waiting.
    const auto& oldest=online.recent[online.fields%online.recent.size()];
    double waited=0;for(const auto& field:online.recent)waited+=field.wait;
    const double span=std::chrono::duration<double>(now-oldest.at).count();
    const bool slow=online.fields>online.recent.size() && span>online.recent.size()*FramePacer::field_seconds*1.05 && waited>span*0.1;
    char text[96];
    const int rtt=int(online.session->roundTripMs()+0.5);
    if(slow)std::snprintf(text,sizeof text,"Connection slow (round trip %d ms): the host can raise the input delay",rtt);
    else if(!fighting)std::snprintf(text,sizeof text,"%s",online.session->host()?"Online: you are 1P, the top row and the left side; you lead the menus"
                                                                                 :"Online: you are 2P, the bottom row and the right side; 1P leads the menus");
    else if(online.player->settings.show_frame_rate)std::snprintf(text,sizeof text,"online: %d ms, delay %u",rtt,online.session->delay());
    else text[0]=0;
    online.player->onlineStatus(text,false);
}
}

void prepareOnlineSession() {
    const char* net=std::getenv("BT2_NET");
    if(!net)return;
    const auto address=parseLockstepAddress(net);
    for(const char* name:{"BT2_START_BATTLE","BT2_START_EPISODE","BT2_PAD2_SCRIPT","BT2_PAD2_LIVE","BT2_REPLAY_CALL"})
        if(std::getenv(name))throw std::runtime_error(std::string(name)+" cannot be used in an online session (BT2_NET)");
    if(const char* screen=std::getenv("BT2_START_SCREEN");screen && std::string_view(screen)!="30")
        throw std::runtime_error("BT2_START_SCREEN: an online session starts in the Dueling screen (30)");
    setSwitch("BT2_START_SCREEN","30");
    setSwitch("BT2_START_SCREEN_PLAYERS","1");
    setSwitch("BT2_UNLOCK_ROSTER","1");
    if(!std::getenv("BT2_ONLINE_VIEW"))setSwitch("BT2_ONLINE_VIEW",address.host?"1":"2");
    for(const char* name:{"BT2_CARD_FOLDER","BT2_FAST_LOADING","BT2_FAST_FORWARD","BT2_QUICK_TITLE"})setSwitch(name,nullptr);
}

void startOnlineSession(EeRuntime& runtime,ViewerWindow* window,HostInput* input,PlayerSession* player,FramePacer* pacer) {
    const char* net=std::getenv("BT2_NET");
    if(!net)return;
    const auto address=parseLockstepAddress(net);
    LockstepOptions options;
    options.host=address.host;
    if(const char* delay=std::getenv("BT2_NET_DELAY"))options.delay=unsigned(std::stoul(delay));
    options.program=programHash();
    // Settings that change the fight must be alike on both machines (graphics settings and drawing patches need not be).
    if(const char* buttons=std::getenv("BT2_STRUGGLE_BUTTONS");buttons && std::string_view(buttons)=="1")options.rules|=1;
    options.rule_names="Beam struggles by buttons";
    // A field waiting for the other player: after a quarter of a second the window says so over the game's last
    // picture, and it keeps answering (closing it leaves the battle).
    if(player && window)options.waiting=[window](double seconds) {
        if(seconds>=0.25) {
            char text[64];std::snprintf(text,sizeof text,"Waiting for the other player... %.0f s",seconds);
            online.player->onlineStatus(text,true);
        } else window->pump();
        if(window->closed())std::exit(0);
    };
    online.player=player;online.pacer=pacer;
    online.transport=address.host?hostUdp(address.port):joinUdp(address.address,address.port);
    // BT2_NET_SIM=latency=<ms>,jitter=<ms>,loss=<percent>: a worse network on this side's receiving end (tests on one PC).
    if(const char* simulation=std::getenv("BT2_NET_SIM"))online.transport=simulateNetwork(std::move(online.transport),parseNetworkSimulation(simulation));
    if(address.host && std::getenv("BT2_NET_UPNP") && std::string_view(std::getenv("BT2_NET_UPNP"))=="1")online.upnp=std::make_unique<UpnpPort>(address.port);
    online.session=std::make_unique<Lockstep>(*online.transport,options);
    if(address.host)std::cerr<<"Online session: waiting for the other player on port "<<address.port<<std::endl;
    else std::cerr<<"Online session: joining "<<address.address<<':'<<address.port<<std::endl;
    if(window && input)waitInWindow(*window,*input,address);
    else if(!online.session->connect(address.host?std::chrono::minutes(5):std::chrono::minutes(1)))
        throw std::runtime_error(address.host?"Online session: no other player joined":"Online session: the host did not answer");
    std::cerr<<"Online session: connected as player "<<(address.host?1:2)<<", input delay "<<online.session->delay()<<" fields, round trip "
             <<int(online.session->roundTripMs())<<" ms"<<(online.upnp?", UPnP: "+std::string(online.upnp->state()==UpnpPort::State::Opened?"opened":online.upnp->failure()):std::string())<<std::endl;
    // Leaving tells the other player before anything else ends (the renderer's ending can take a while).
    std::atexit([]{if(online.session)online.session->leave();});
    // The local controller: the player's (live), or BT2_PAD_SCRIPT counted in fields as DualShock2 counts them.
    std::function<PadInput()> local=runtime.on_pad_input;
    if(!local) {
        struct Scripted {PadScript script;PadInput held;unsigned field=0;};
        const char* text=std::getenv("BT2_PAD_SCRIPT");
        auto scripted=std::make_shared<Scripted>(Scripted{text?PadScript::parse(text):PadScript{}});
        local=[scripted] {
            if(const auto input=scripted->script.at(++scripted->field))scripted->held=*input;
            return scripted->held;
        };
    }
    runtime.hash_service_requests=true;
    // Port 2 is asked first each field (pad_service.cpp): the exchange happens there.
    runtime.on_pad2_input=[&runtime,local] {
        online.ports=online.session->field(local(),runtime.service_request_hash);
        afterField();
        return online.ports[1];
    };
    runtime.on_pad_input=[] {return online.ports[0];};
}
