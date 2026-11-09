// The screens a player's window shows while an online battle connects (probe/online_session.cpp), kept as short as
// BT3-Recomp's netplay tab: the host's shows the code to give the other player (host/connection_code.h), copied by
// itself, the home code when it is another, and Cancel; the joiner's the host it calls, and Cancel. Each note fits on
// one line and says the one thing to do.
#pragma once
#include "host/connection_code.h"
#include "host/menu_model.h"
#include <functional>
#include <optional>
#include <string>

namespace online_screens {
enum class RouterPort {Asking,Opened,ByHand};
struct Hosting {
    unsigned port=0;
    std::string internet_address; // From the router (UPnP); empty when unknown.
    std::string home_address;     // This PC on the home network; empty without one.
    RouterPort router=RouterPort::ByHand;
    std::string copied;           // The code on the clipboard, put there by this screen.
};
inline std::optional<std::string> internetCode(const Hosting& h) {
    if(h.internet_address.empty() || !connection_code::reachable(h.internet_address))return std::nullopt;
    return connection_code::make(h.internet_address,std::uint16_t(h.port));
}
inline std::optional<std::string> homeCode(const Hosting& h) {return connection_code::make(h.home_address,std::uint16_t(h.port));}
// The code to give the other player, which the screen copies by itself: the internet code once the router gave the
// address; the home code when no internet code will come (UPnP off or failed, or the address shared); none while the
// router is still asked.
inline std::optional<std::string> codeToGive(const Hosting& h) {
    if(auto code=internetCode(h))return code;
    if(h.router==RouterPort::Asking)return std::nullopt;
    return homeCode(h);
}
inline MenuEntry line(std::string label,std::string note) {return menuAction(std::move(label),[]{},std::move(note));}

// `copy`: puts a code on the clipboard.
inline MenuScreen hosting(const Hosting& h,std::function<void()> cancel,std::function<void(const std::string&)> copy) {
    MenuScreen screen;screen.title="Waiting for the other player";
    const auto code=[&](const std::string& label,const std::string& made,const std::string& note) {
        const bool copied=h.copied==made;
        screen.entries.push_back(menuAction(label+made+(copied?"  (copied)":""),[copy,made]{copy(made);},note));
    };
    const auto internet=internetCode(h),home=homeCode(h);
    const std::string send=" into a chat to the other player.";
    if(internet)code("Code: ",*internet,h.copied==*internet?"Copied: paste it (Ctrl+V)"+send:"Confirm copies it, to paste"+send);
    else if(h.router==RouterPort::Asking)screen.entries.push_back(line("Code: one moment","Asking the router to open port "+std::to_string(h.port)+"."));
    else if(home)code("Code: ",*home,!h.internet_address.empty()
        ?"Home network only: your provider shares your internet address. Let the other player host."
        :"Home network only. For the internet: forward UDP port "+std::to_string(h.port)+" to "+h.home_address+", then host again.");
    else screen.entries.push_back(line("Code: none","This PC is not on a network."));
    if(home && (internet || h.router==RouterPort::Asking))code("Home code: ",*home,"For a player on the same home network.");
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
// `late`: no answer for a while, so the note says what to check.
inline MenuScreen joining(const std::string& address,unsigned port,bool late,std::function<void()> cancel) {
    MenuScreen screen;screen.title="Connecting to the host";
    screen.entries.push_back(line("Host: "+address+" : "+std::to_string(port),late?"No answer yet: check the code, and that the host is waiting."
                                                                              :"The battle starts when the host answers."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
}
