// The screens a player's window shows while an online battle connects (probe/online_session.cpp): the host's, with
// the codes to give the other player (host/connection_code.h) and the router's port, and the joiner's. Short lines;
// the details are in the notes.
#pragma once
#include "host/connection_code.h"
#include "host/menu_model.h"
#include <functional>
#include <string>

namespace online_screens {
enum class RouterPort {Asking,Opened,ByHand};
struct Hosting {
    unsigned port=0;
    std::string internet_address; // From the router (UPnP); empty when unknown.
    std::string home_address;     // This PC on the home network; empty without one.
    RouterPort router=RouterPort::ByHand;
    std::string router_note;      // Why the port is to be forwarded by hand.
};
inline MenuEntry line(std::string label,std::string note) {return menuAction(std::move(label),[]{},std::move(note));}

// `copy`: puts a code on the clipboard.
inline MenuScreen hosting(const Hosting& h,std::function<void()> cancel,std::function<void(const std::string&)> copy) {
    MenuScreen screen;screen.title="Online battle: hosting";
    const std::string port=std::to_string(h.port);
    screen.entries.push_back(line("Waiting for the other player","They choose Join a battle in the launcher and enter the code below."));
    const auto code=[&](const std::string& label,const std::string& address,const std::string& note) {
        const auto made=connection_code::make(address,std::uint16_t(h.port));
        if(!made)return false;
        screen.entries.push_back(menuAction(label+*made,[copy,text=*made]{copy(text);},note));
        return true;
    };
    if(!h.internet_address.empty() && !connection_code::reachable(h.internet_address))
        screen.entries.push_back(line("Code: none, the address is shared",
            "The router's internet address ("+h.internet_address+") is shared by your provider (carrier NAT): other players cannot reach it."));
    else if(!code("Code: ",h.internet_address,"Give this code to the other player. Confirm copies it, to paste into a chat."))
        screen.entries.push_back(line(h.router==RouterPort::Asking?"Code: when the router answers":"Code: needs your internet address",
            "Without UPnP: give them your internet address (your router's page or a \"what is my IP\" site) and port "+port+"."));
    code("Home code: ",h.home_address,"For a player on the same home network. Confirm copies it.");
    switch(h.router) {
    case RouterPort::Asking: screen.entries.push_back(line("Port "+port+": asking the router","By UPnP; it takes a few seconds."));break;
    case RouterPort::Opened: screen.entries.push_back(line("Port "+port+": opened","The router opened it (UPnP); it is closed again when the battle ends."));break;
    case RouterPort::ByHand: screen.entries.push_back(line("Port "+port+": forward it by hand",
        "On your router: UDP port "+port+" to "+(h.home_address.empty()?std::string("this PC"):h.home_address)+". "+h.router_note));break;
    }
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
// `late`: no answer for a while, so the note says what to check.
inline MenuScreen joining(const std::string& address,unsigned port,bool late,std::function<void()> cancel) {
    MenuScreen screen;screen.title="Online battle: joining";
    screen.entries.push_back(line("Connecting to the host",late?"No answer yet: check the code, that the host is waiting, and that the host's port is open."
                                                              :"Waiting for the host's answer."));
    screen.entries.push_back(line("Host: "+address+" : "+std::to_string(port),"From the code (or the address and port) entered in the launcher."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
}
