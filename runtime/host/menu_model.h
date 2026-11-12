// A menu as data and the effect of presses on it; no drawing and no host calls. Used by the launcher
// and the in-game overlay (docs/plans/launcher/LAUNCHER_PLAN.md). A menu is a stack of screens; a
// screen is a title and entries. Entries act through small callbacks, so the menu owns no settings.
#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class MenuKey {Up,Down,Left,Right,Confirm,Back};
// What an entry of a browser screen is drawn as.
enum class MenuIcon {None,Disc,NoDisc,Card,CardImage,NewCard};

struct MenuEntry {
    std::string label;
    std::function<std::string()> value;          // Shown beside the label; empty for none.
    std::function<void()> confirm;               // Cross / Enter.
    std::function<void(int)> adjust;             // Left (-1) and right (+1).
    std::string note;                            // A line of explanation shown while highlighted.
    bool enabled=true;                           // A disabled entry is shown but cannot be highlighted.
    std::function<bool()> available;             // Also disabled while this returns false (it depends on another setting).
    MenuIcon icon=MenuIcon::None;                // Its picture on a browser screen.
    std::string section;                         // On a panel screen: a heading shown above this entry (it starts a group).
    // A big card (on a first-look screen): consecutive cards make a row, Left and Right move along it, Up and Down leave
    // it; `card_text` is its lines under the label ('\n' between them).
    bool card=false;std::string card_text;
    bool usable() const {return enabled && (!available || available());}
};

struct MenuScreen {
    std::string title;
    std::vector<MenuEntry> entries;
    unsigned highlighted=0;
    // Drawn as the console's browser (a row of icons on grey) instead of a list; every arrow moves
    // through the row.
    bool browser=false;
    // Drawn as a panel (after BT3-Recomp's netplay panel): the title with a status pill at its right, the entries in groups
    // under their headings (MenuEntry::section), each with its value at the right of its line.
    bool panel=false;
    std::function<std::string()> status; // The pill's text; empty for none. In the first screen's look: the heading under the logo.
    // Drawn in the first screen's look (black, the ring, the logo, the list beside it) though not the first screen, with
    // Back (the user, 2026-11-12: the online pages as the launcher's first screen, not a panel of another style).
    bool first_look=false;
};

class Menu {
public:
    std::function<void()> on_leave_root; // Back on the first screen.

    void open(MenuScreen screen) {
        if(screen.entries.empty())throw std::runtime_error("Menu screen without entries: "+screen.title);
        screen.highlighted=0;
        while(screen.highlighted<screen.entries.size() && !screen.entries[screen.highlighted].usable())++screen.highlighted;
        if(screen.highlighted==screen.entries.size())throw std::runtime_error("Menu screen without an enabled entry: "+screen.title);
        stack_.push_back(std::move(screen));
    }
    void close() {if(stack_.size()>1)stack_.pop_back();else if(on_leave_root)on_leave_root();}
    // The top screen built again (what it shows changed), the same entry highlighted where it still can be.
    void replace(MenuScreen screen) {
        if(stack_.empty())return open(std::move(screen));
        const unsigned at=stack_.back().highlighted;
        stack_.pop_back();open(std::move(screen));
        auto& s=stack_.back();
        if(at<s.entries.size() && s.entries[at].usable())s.highlighted=at;
    }
    bool empty() const {return stack_.empty();}
    std::size_t depth() const {return stack_.size();}
    const MenuScreen& screen() const {return stack_.back();}

    void press(MenuKey key) {
        if(stack_.empty())return;
        auto& s=stack_.back();
        switch(key) {
        case MenuKey::Up: case MenuKey::Down: {
            // To the next enabled entry, around the ends.
            // A row of cards is one stop: entered at its first card, left past its last.
            const auto count=unsigned(s.entries.size());
            const unsigned from=s.highlighted;const bool from_card=s.entries[from].card;
            do s.highlighted=(s.highlighted+(key==MenuKey::Down?1:count-1))%count;
            while(s.highlighted!=from && (!s.entries[s.highlighted].usable() || (from_card && s.entries[s.highlighted].card)));
            if(!from_card)while(s.entries[s.highlighted].card && s.highlighted>0 && s.entries[s.highlighted-1].card && s.entries[s.highlighted-1].usable())--s.highlighted;
            break;
        }
        case MenuKey::Left: case MenuKey::Right:
            if(s.browser) {press(key==MenuKey::Right?MenuKey::Down:MenuKey::Up);break;}
            if(s.entries[s.highlighted].card) { // Along the row of cards.
                const unsigned next=key==MenuKey::Right?s.highlighted+1:s.highlighted-1;
                if(next<s.entries.size() && s.entries[next].card && s.entries[next].usable())s.highlighted=next;
                break;
            }
            if(const auto adjust=s.entries[s.highlighted].adjust)adjust(key==MenuKey::Right?1:-1); // A copy: the callback may change the stack.
            break;
        case MenuKey::Confirm: {
            const auto entry=s.entries[s.highlighted];
            if(entry.confirm)entry.confirm();else if(entry.adjust)entry.adjust(1);
            break;
        }
        case MenuKey::Back: close();break;
        }
    }

private:
    std::vector<MenuScreen> stack_;
};

// Entry builders for the common kinds.
inline MenuEntry menuAction(std::string label,std::function<void()> action,std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.confirm=std::move(action);e.note=std::move(note);return e;
}
inline MenuEntry menuToggle(std::string label,bool& value,std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.note=std::move(note);
    e.value=[&value]{return std::string(value?"On":"Off");};e.adjust=[&value](int){value=!value;};return e;
}
// A number from `low` to `high` in steps of `step`; it stops at the ends.
inline MenuEntry menuNumber(std::string label,unsigned& value,unsigned low,unsigned high,unsigned step,std::string unit={},std::string note={}) {
    MenuEntry e;e.label=std::move(label);e.note=std::move(note);
    e.value=[&value,unit]{return std::to_string(value)+unit;};
    e.adjust=[&value,low,high,step](int direction) {
        if(direction>0)value=value+step>high?high:value+step;
        else value=value<low+step?low:value-step;
    };
    e.confirm=[]{}; // Confirm does nothing on a number.
    return e;
}
