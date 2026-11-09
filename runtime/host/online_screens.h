// The screens a player's window shows while an online battle connects (probe/online_session.cpp), kept as short as
// BT3-Recomp's netplay tab. No port has to be open (docs/plans/online/ONLINE_BATTLE_PLAN.md, "Without an open port"):
// the host gives its code (host/connection_code.h: its addresses as the internet sees them, IPv4 and IPv6), the
// joiner's screen shows a reply code (the joiner's addresses) to send back, and the host's game connects once the
// host copies that reply. Codes are copied by themselves; each note fits on one line and says the one thing to do.
#pragma once
#include "host/connection_code.h"
#include "host/menu_model.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace online_screens {
// The addresses the internet can reach (a home network's or a shared one are left out).
inline std::vector<connection_code::Address> reachableOnly(const std::vector<connection_code::Address>& addresses) {
    std::vector<connection_code::Address> out;
    std::copy_if(addresses.begin(),addresses.end(),std::back_inserter(out),[](const auto& a){return connection_code::reachable(a.address);});
    return out;
}
struct Hosting {
    unsigned port=0;
    std::vector<connection_code::Address> internet; // This side as the internet sees it (STUN, IPv6), empty until known.
    bool finding=false;                             // Still asking for it.
    std::string home_address;                       // This PC on the home network; empty without one.
    bool replied=false;                             // The joiner's reply code was taken: connecting to it.
    bool no_reply=false;                            // A paste found no reply code on the clipboard.
    std::string copied;                             // The code on the clipboard, put there by this screen.
};
inline std::optional<std::string> internetCode(const Hosting& h) {
    const auto reachable=reachableOnly(h.internet);
    if(reachable.empty())return std::nullopt;
    return connection_code::make(reachable);
}
inline std::optional<std::string> homeCode(const Hosting& h) {return connection_code::make(h.home_address,std::uint16_t(h.port));}
// The code to give the other player, which the screen copies by itself: the internet code once known; the home code
// when no internet code will come; none while it is still asked for.
inline std::optional<std::string> codeToGive(const Hosting& h) {
    if(h.finding)return std::nullopt;
    if(auto code=internetCode(h))return code;
    return homeCode(h);
}
inline MenuEntry line(std::string label,std::string note) {return menuAction(std::move(label),[]{},std::move(note));}
// A code as the screen writes it: a long one (several addresses) shortened, since it is pasted, not read.
inline std::string shown(const std::string& code) {return code.size()>14?code.substr(0,9)+"...":code;}
inline MenuEntry codeEntry(const std::string& label,const std::string& code,const std::string& copied,
                           const std::function<void(const std::string&)>& copy,const std::string& note) {
    return menuAction(label+shown(code)+(copied==code?"  (copied)":""),[copy,code]{copy(code);},note);
}

// `copy`: puts a code on the clipboard; `paste`: takes the reply code from it.
inline MenuScreen hosting(const Hosting& h,std::function<void()> cancel,std::function<void(const std::string&)> copy,std::function<void()> paste={}) {
    MenuScreen screen;screen.title="Waiting for the other player";
    const auto internet=h.finding?std::nullopt:internetCode(h);
    const auto home=homeCode(h);
    if(internet) {
        screen.entries.push_back(codeEntry("Code: ",*internet,h.copied,copy,h.copied==*internet?"Copied: paste it (Ctrl+V) into a chat to the other player."
                                                                                                  :"Confirm copies it, to paste into a chat to the other player."));
        screen.entries.push_back(h.replied?line("Reply code received","Connecting to the other player.")
            :menuAction("Then paste their reply code (Ctrl+V)",paste?paste:[]{},h.no_reply
                ?"No reply code on the clipboard: copy it from the chat first, then Ctrl+V here."
                :"Their screen shows it after they join. Copy it from the chat, then Ctrl+V here."));
    }
    else if(h.finding)screen.entries.push_back(line("Code: one moment","Finding your internet address."));
    else if(home)screen.entries.push_back(codeEntry("Code: ",*home,h.copied,copy,"Home network only: your internet address was not found."));
    else screen.entries.push_back(line("Code: none","This PC is not on a network."));
    if(home && internet)screen.entries.push_back(codeEntry("Home code: ",*home,h.copied,copy,"For a player on the same home network (no reply code needed)."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}

struct Joining {
    std::vector<connection_code::Address> host; // From the host's code.
    std::string reply;                          // This side's reply code, once its addresses are known.
    bool finding=false;                         // Still asking for them.
    bool late=false;                            // No answer for a while.
    std::string copied;
};
// A host on the internet needs the reply code; one on the home network does not.
inline bool needsReply(const Joining& j) {return !reachableOnly(j.host).empty();}
inline MenuScreen joining(const Joining& j,std::function<void()> cancel,std::function<void(const std::string&)> copy) {
    MenuScreen screen;screen.title="Connecting to the host";
    if(needsReply(j)) {
        if(!j.reply.empty())screen.entries.push_back(codeEntry("Reply: ",j.reply,j.copied,copy,j.copied==j.reply
            ?"Copied: paste it (Ctrl+V) into the chat to the host. The battle starts when they copy it.":"Confirm copies it, to send to the host."));
        else if(j.finding)screen.entries.push_back(line("Reply: one moment","Finding your internet address."));
        else screen.entries.push_back(line("Reply: none","Your internet address was not found: the host needs an open port."));
    }
    const std::string host=j.host.empty()?std::string("?"):j.host[0].address.find(':')!=std::string::npos?"(IPv6)":j.host[0].address+" : "+std::to_string(j.host[0].port);
    screen.entries.push_back(line("Host: "+host,
        !j.late?"The battle starts when the host answers."
        :needsReply(j)?"No answer yet: check that the host copied your reply code.":"No answer yet: check the code, and that the host is waiting."));
    screen.entries.push_back(menuAction("Cancel",std::move(cancel),"Back to the launcher."));
    return screen;
}
}
