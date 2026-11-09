// The clipboard, for online battles' connection codes (host/connection_code.h): the host's window copies its code for
// a chat, the launcher pastes it. The clipboard is opened with the window as its owner (without one, EmptyClipboard
// leaves no owner and the text may not stay) and again a few times when another program holds it for a moment
// (clipboard history and chat programs read every change). The text is Unicode, which chat programs read.
// What the pasted text gives is decided by pure functions the tests check.
#pragma once
#include "host/connection_code.h"
#include <optional>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

namespace clipboard {
// The first connection code in `text`, as written there: a chat line such as "my code: SC07-3ZKY" gives "SC07-3ZKY".
// Words are split at spaces, line ends, quotes and the like; a code's own dashes stay in it. A sentence's last full
// stop is not part of the word.
inline std::optional<std::string> codeIn(std::string_view text) {
    const auto separator=[](char c){return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='"' || c=='\'' || c==',' || c==';' || c==':' || c=='(' || c==')' || c=='<' || c=='>';};
    for(std::size_t i=0;i<text.size();) {
        while(i<text.size() && separator(text[i]))++i;
        std::size_t end=i;
        while(end<text.size() && !separator(text[end]))++end;
        std::string_view word=text.substr(i,end-i);
        while(!word.empty() && (word.back()=='.' || word.back()=='!' || word.back()=='?'))word.remove_suffix(1); // End of a sentence.
        // A code as the host's screen writes it has a dash, and most have digits: an ordinary long word whose letters
        // happen to pass the check (one in 256) is not taken for one.
        const bool marked=word.find_first_of("-0123456789")!=std::string_view::npos;
        if(word.size()>=8 && marked && connection_code::read(word))return std::string(word);
        i=end;
    }
    return std::nullopt;
}
// What a paste gives a typed field holding `typed`: a connection code in the clipboard's text replaces the field;
// otherwise the text is added (an address: up to a ':', so an address with its port gives the address), letters in
// lower case, without what `allowed` leaves out, at most `length` in all.
inline std::string pastedInto(std::string typed,std::string_view clip,const std::string& allowed,std::size_t length) {
    if(const auto code=codeIn(clip)) {
        std::string lower;
        for(const char c:*code)lower+=c>='A' && c<='Z'?char(c-'A'+'a'):c;
        bool fits=lower.size()<=length;
        for(const char c:lower)fits=fits && allowed.find(c)!=std::string::npos;
        if(fits)return lower;
    }
    for(const char c:clip) {
        if(c==':' || typed.size()>=length)break;
        const char lower=c>='A' && c<='Z'?char(c-'A'+'a'):c;
        if(allowed.find(lower)!=std::string::npos)typed+=lower;
    }
    return typed;
}

#ifdef _WIN32
inline bool open(HWND owner) {
    for(int attempt=0;attempt<25;++attempt) {
        if(OpenClipboard(owner))return true;
        Sleep(10);
    }
    return false;
}
// Puts `text` (ASCII) on the clipboard; false when the clipboard stayed busy.
inline bool copy(HWND owner,std::string_view text) {
    if(!open(owner))return false;
    bool done=false;
    if(EmptyClipboard()) {
        if(const HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,(text.size()+1)*sizeof(wchar_t))) {
            if(auto* wide=static_cast<wchar_t*>(GlobalLock(data))) {
                for(std::size_t i=0;i<text.size();++i)wide[i]=wchar_t(static_cast<unsigned char>(text[i]));
                wide[text.size()]=0;
                GlobalUnlock(data);
                done=SetClipboardData(CF_UNICODETEXT,data)!=nullptr;
            }
            if(!done)GlobalFree(data);
        }
    }
    CloseClipboard();
    return done;
}
// The clipboard's text, its ASCII characters kept and others as '?'; empty when there is none.
inline std::string text(HWND owner) {
    std::string out;
    if(!open(owner))return out;
    if(const HANDLE data=GetClipboardData(CF_UNICODETEXT)) {
        if(const auto* wide=static_cast<const wchar_t*>(GlobalLock(data))) {
            for(const wchar_t* c=wide;*c && out.size()<4096;++c)out+=*c<128?char(*c):'?';
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return out;
}
#endif
}
