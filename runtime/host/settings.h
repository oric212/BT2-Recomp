// The player's settings (docs/plans/launcher/LAUNCHER_PLAN.md): a typed record and its text form,
// one "key = value" per line. A file with an unknown key or a value out of range is refused by name;
// a missing key keeps its default, so older files stay readable.
#pragma once
#include "host/input_mapping.h"
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

struct Settings {
    std::string disc;               // Path of the game's disc image.
    bool integer_scaling=true;      // Off: the picture is stretched to the window.
    bool fullscreen=false;
    unsigned window_scale=2;        // Window size as a multiple of the game's picture, 1..4.
    bool show_frame_rate=false;
    unsigned master_volume=100,music_volume=100,effects_volume=100; // Percent.
    bool hd_textures=false;
    InputMapping mapping;           // Keys and gamepad buttons of the PS2 buttons.
    // Fast forward: a key (virtual-key code, 0 for none) and a gamepad button (Count for none), held
    // or pressed once to switch.
    unsigned fast_forward_key=0x09; // Tab.
    unsigned fast_forward_button=GamepadState::Count;
    bool fast_forward_toggle=false;
    // What opens the in-game menu on a gamepad besides the DualSense's PS button and Escape:
    // the two stick buttons together, or Start with Select.
    bool overlay_start_select=false;
    std::string memory_card;        // The card folder in slot 1, a name under user/memcards; empty: no card.
    bool operator==(const Settings&) const=default;

    std::string text() const {
        std::ostringstream out;
        out<<"disc = "<<disc<<"\ninteger_scaling = "<<integer_scaling<<"\nfullscreen = "<<fullscreen<<"\nwindow_scale = "<<window_scale
           <<"\nshow_frame_rate = "<<show_frame_rate<<"\nmaster_volume = "<<master_volume<<"\nmusic_volume = "<<music_volume
           <<"\neffects_volume = "<<effects_volume<<"\nhd_textures = "<<hd_textures<<"\nfast_forward_key = "<<fast_forward_key
           <<"\nfast_forward_button = "<<buttonName(fast_forward_button)<<"\nfast_forward_toggle = "<<fast_forward_toggle
           <<"\noverlay_start_select = "<<overlay_start_select<<"\nmemory_card = "<<memory_card<<"\n";
        for(unsigned n=0;n<16;++n)out<<"key_"<<PadScript::names[n]<<" = "<<unsigned(mapping.keys[n])<<"\n";
        for(unsigned n=0;n<16;++n)out<<"button_"<<PadScript::names[n]<<" = "<<buttonName(mapping.buttons[n])<<"\n";
        return out.str();
    }
    static Settings parse(const std::string& text) {
        Settings s;
        std::istringstream in(text);std::string line;
        while(std::getline(in,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            if(line.empty())continue;
            if(line.size()>2 && line.ends_with(" ="))line+=' '; // "key =" with nothing after it is an empty value.
            const auto at=line.find(" = ");
            if(at==std::string::npos)throw std::runtime_error("Settings line is not \"key = value\": "+line);
            const std::string key=line.substr(0,at),value=line.substr(at+3);
            const auto number=[&](unsigned low,unsigned high) {
                if(value.empty() || value.size()>4 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Setting "+key+" is not a number: "+value);
                const unsigned v=unsigned(std::stoul(value));
                if(v<low || v>high)throw std::runtime_error("Setting "+key+" is out of range: "+value);
                return v;
            };
            const auto button=[&](bool none_allowed) {
                for(unsigned b=0;b<GamepadState::Count;++b)if(value==GamepadState::names[b])return b;
                if(none_allowed && value=="none")return unsigned(GamepadState::Count);
                throw std::runtime_error("Setting "+key+" names no gamepad button: "+value);
            };
            if(key=="disc")s.disc=value;
            else if(key=="integer_scaling")s.integer_scaling=number(0,1);
            else if(key=="fullscreen")s.fullscreen=number(0,1);
            else if(key=="window_scale")s.window_scale=number(1,4);
            else if(key=="show_frame_rate")s.show_frame_rate=number(0,1);
            else if(key=="master_volume")s.master_volume=number(0,100);
            else if(key=="music_volume")s.music_volume=number(0,100);
            else if(key=="effects_volume")s.effects_volume=number(0,100);
            else if(key=="hd_textures")s.hd_textures=number(0,1);
            else if(key=="fast_forward_key")s.fast_forward_key=number(0,255);
            else if(key=="fast_forward_button")s.fast_forward_button=button(true);
            else if(key=="fast_forward_toggle")s.fast_forward_toggle=number(0,1);
            else if(key=="overlay_start_select")s.overlay_start_select=number(0,1);
            else if(key=="memory_card") {
                if(value.find_first_of("/\\:*?\"<>|")!=std::string::npos || value==".." || value==".")throw std::runtime_error("Setting memory_card is not a folder name: "+value);
                s.memory_card=value;
            }
            else {
                const bool is_key=key.starts_with("key_"),is_button=key.starts_with("button_");
                unsigned index=16;
                if(is_key || is_button)for(unsigned n=0;n<16;++n)if(key.substr(is_key?4:7)==PadScript::names[n])index=n;
                if(index==16)throw std::runtime_error("Unknown setting: "+key);
                if(is_key)s.mapping.keys[index]=std::uint8_t(number(0,255));
                else s.mapping.buttons[index]=GamepadState::Button(button(false));
            }
        }
        return s;
    }
    // The settings in `path`, or nothing when the file does not exist.
    static std::optional<Settings> load(const std::string& path) {
        std::ifstream in(path,std::ios::binary);
        if(!in)return std::nullopt;
        std::ostringstream all;all<<in.rdbuf();
        return parse(all.str());
    }
    void save(const std::string& path) const {
        std::ofstream out(path,std::ios::binary);
        out<<text();
        if(!out)throw std::runtime_error("Cannot write the settings to "+path);
    }
    static std::string buttonName(unsigned button) {return button<GamepadState::Count?std::string(GamepadState::names[button]):"none";}
};
