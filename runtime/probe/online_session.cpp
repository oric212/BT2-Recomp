#include "online_session.h"
#include "ee/ee_runtime.h"
#include "host/menu_host.h"
#include "host/net_lockstep.h"
#include "host/player_session.h"
#include "services/devices/pad_script.h"
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
    std::array<PadInput,2> ports{};
    ~Online() {
        if(!session)return;
        std::cerr<<std::dec<<"Online session: "<<session->fieldsPlayed()<<" fields at an input delay of "<<session->delay()<<", "
                 <<session->checksCompared()<<" drift checks alike; waited for the other player on "<<session->waits()<<" fields ("
                 <<session->waitedSeconds()<<" s)."<<std::endl;
        session.reset(); // Tells the other player, while the transport is still open.
    }
} online;

void setSwitch(const char* name,const char* value) {
    // Both the C library's copy and the process's (BT2_CARD_FOLDER is read with GetEnvironmentVariableW).
    if(_putenv_s(name,value?value:"") || (!SetEnvironmentVariableA(name,value) && value))
        throw std::runtime_error(std::string("Online session: cannot set ")+name);
}

// A player's window while connecting: what is awaited, and Cancel (or Back) to return to the launcher.
void waitInWindow(ViewerWindow& window,HostInput& input,const LockstepAddress& address) {
    MenuHost host(window,input);
    MenuScreen screen;screen.title="Online battle";
    MenuEntry status=menuAction(address.host?"Waiting for the other player":"Connecting to the host",[]{},
        address.host?"On UDP port "+std::to_string(address.port)+", which your router must forward to this PC."
                    :"At "+address.address+", port "+std::to_string(address.port)+".");
    const auto start=std::chrono::steady_clock::now();
    status.value=[start] {
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count();
        return std::to_string(seconds)+" s";
    };
    screen.entries.push_back(status);
    screen.entries.push_back(menuAction("Cancel",[&host]{host.finished=true;},"Back to the launcher."));
    host.menu.open(screen);
    host.menu.on_leave_root=[&host]{host.finished=true;};
    bool connected=false;
    host.every(50,[&] {if(online.session->connect(std::chrono::milliseconds(30))) {connected=true;host.finished=true;}});
    if(!host.run("BT2 Recomp - online battle") || !connected)std::exit(PlayerSession::launcher_exit_code);
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

void startOnlineSession(EeRuntime& runtime,ViewerWindow* window,HostInput* input) {
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
    online.transport=address.host?hostUdp(address.port):joinUdp(address.address,address.port);
    online.session=std::make_unique<Lockstep>(*online.transport,options);
    if(address.host)std::cerr<<"Online session: waiting for the other player on port "<<address.port<<std::endl;
    else std::cerr<<"Online session: joining "<<address.address<<':'<<address.port<<std::endl;
    if(window && input)waitInWindow(*window,*input,address);
    else if(!online.session->connect(address.host?std::chrono::minutes(5):std::chrono::minutes(1)))
        throw std::runtime_error(address.host?"Online session: no other player joined":"Online session: the host did not answer");
    std::cerr<<"Online session: connected as player "<<(address.host?1:2)<<", input delay "<<online.session->delay()<<" fields"<<std::endl;
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
    runtime.on_pad2_input=[&runtime,local] {online.ports=online.session->field(local(),runtime.service_request_hash);return online.ports[1];};
    runtime.on_pad_input=[] {return online.ports[0];};
}
