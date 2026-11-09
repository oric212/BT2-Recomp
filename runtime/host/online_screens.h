// The screens a player's window shows while an online battle connects (probe/online_session.cpp), kept as short as
// BT3-Recomp's netplay tab. No port has to be open (docs/plans/online/ONLINE_BATTLE_PLAN.md, "Without an open port"):
// the host gives its code (host/connection_code.h, its address as the internet sees it), the joiner's screen shows a
// reply code (the joiner's address) to send back, and the host's game connects once the host copies that reply. Codes
// are copied by themselves; each note fits on one line and says the one thing to do.
#pragma once
#include "host/connection_code.h"
#include "host/menu_model.h"
#include <functional>
#include <optional>
#include <string>

namespace online_screens {
struct Hosting {
    unsigned port=0;
    std::string internet_address; // This side as the internet sees it (STUN), empty until known.
    unsigned internet_port=0;
    bool finding=false;           // Still asking for it.
    std::string home_address;     // This PC on the home network; empty without one.
    bool replied=false;           // The joiner's reply code was copied: connecting to it.
    std::string copied;           // The code on the clipboard, put there by this screen.
};
inline std::optional<std::string> internetCode(const Hosting& h) {
    if(h.internet_address.empty() || !connection_code::reachable(h.internet_address))return std::nullopt;
    return connection_code::make(h.internet_address,std::uint16_t(h.internet_port));
}
inline std::optional<std::string> homeCode(const Hosting& h) {return connection_code::make(h.home_address,std::uint16_t(h.port));}
// The code to give the other player, which the screen copies by itself: the internet code once known; the home code
// when no internet code will come; none while it is still asked for.
inline std::optional<std::string> codeToGive(const Hosting& h) {
    if(auto code=internetCode(h))return code;
    if(h.finding)return std::nullopt;
    return homeCode(h);
}
inline MenuEntry line(std::string label,std::string note) {return menuAction(std::move(label),[]{},std::move(note));}
inline MenuEntry codeEntry(const std::string& label,const std::string& code,const std::string& copied,
                           const std::function<void(const std::string&)>& copy,const std::string& note) {
    return menuAction(label+code+(copied==code?"  (copied)":""),[copy,code]{copy(code);},note);
}

// `copy`: puts a code on the clipboard.
inline MenuScreen hosting(const Hosting& h,std::function<void()> cancel,std::function<void(const std::string&)> copy) {
    MenuScreen screen;screen.title="Waiting for the other player";
    const auto internet=internetCode(h),home=homeCode(h);
    if(internet) {
        screen.entries.push_back(codeEntry("Code: ",*internet,h.copied,copy,h.copied==*internet?"Copied: paste it (Ctrl+V) into a chat to the other player."
                                                                                                  :"Confirm copies it, to paste into a chat to the other player."));
        screen.entries.push_back(h.replied?line("Reply code received","Connecting to the other player.")
            :line("Then copy their reply code","Their screen shows it after they join. Copy it from the chat (Ctrl+C): the game connects."));
    }
    else if(h.finding)screen.entries.push_back(line("Code: one moment","Finding your internet address."));
    else if(home)screen.entries.push_back(codeEntry("Code: ",*home,h.copied,copy,"Home network only: your internet address was not found."));
    else screen.entries.push_back(line("Code: none","This PC is not on a network."));
    if(home && internet)screen.entries.push_back(codeEntry("Home code: ",*home,h.copied,copy,"For a player on the same home network (no reply code needed)."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}

struct Joining {
    std::string address;unsigned port=0; // The host, from its code.
    std::string reply;                   // This side's reply code, once its internet address is known.
    bool finding=false;                  // Still asking for it.
    bool late=false;                     // No answer for a while.
    std::string copied;
};
// A host on the internet needs the reply code; one on the home network does not.
inline bool needsReply(const Joining& j) {return connection_code::reachable(j.address);}
inline MenuScreen joining(const Joining& j,std::function<void()> cancel,std::function<void(const std::string&)> copy) {
    MenuScreen screen;screen.title="Connecting to the host";
    if(needsReply(j)) {
        if(!j.reply.empty())screen.entries.push_back(codeEntry("Reply: ",j.reply,j.copied,copy,j.copied==j.reply
            ?"Copied: paste it (Ctrl+V) into the chat to the host. The battle starts when they copy it.":"Confirm copies it, to send to the host."));
        else if(j.finding)screen.entries.push_back(line("Reply: one moment","Finding your internet address."));
        else screen.entries.push_back(line("Reply: none","Your internet address was not found: the host needs an open port."));
    }
    screen.entries.push_back(line("Host: "+j.address+" : "+std::to_string(j.port),
        !j.late?"The battle starts when the host answers."
        :needsReply(j)?"No answer yet: check that the host copied your reply code.":"No answer yet: check the code, and that the host is waiting."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
}
