#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "pad_input.h"

// Deterministic controller input for repeatable probe runs: a list of "FIELD:buttons" steps, each
// holding its buttons from that logical IOP field until the next step. Buttons are joined with '+';
// '-' releases everything. Example: BT2_PAD_SCRIPT="700:left,706:-,720:cross,726:-". A stick position
// joins the buttons as "lx=HH" (lx, ly, rx, ry; two hex digits, 00 left or up); a stick a step does not
// name is at rest (80). Example: "900:square+lx=00".
class PadScript {
public:
    // Wire button bits (active-low in the reply): byte 3 then byte 4 of a poll reply.
    static constexpr std::array<std::string_view,16> names{"select","l3","r3","start","up","right","down","left",
        "l2","r2","l1","r1","triangle","circle","cross","square"};
    static constexpr std::array<std::string_view,4> stick_names{"rx","ry","lx","ly"}; // PadInput::sticks order.
    static PadScript parse(std::string_view text) {
        PadScript script;unsigned previous=0;
        while(!text.empty()) {
            const auto comma=text.find(',');const auto step=text.substr(0,comma);
            text=comma==std::string_view::npos?std::string_view{}:text.substr(comma+1);
            const auto colon=step.find(':');
            if(colon==std::string_view::npos || !colon)throw std::runtime_error("Pad script step needs FIELD:buttons: "+std::string(step));
            unsigned field=0;
            for(char c:step.substr(0,colon)) {
                if(c<'0' || c>'9' || field>100000000)throw std::runtime_error("Invalid pad script field: "+std::string(step));
                field=field*10+unsigned(c-'0');
            }
            if(!field || field<=previous)throw std::runtime_error("Pad script fields must start at 1 and increase: "+std::string(step));
            PadInput input;auto buttons=step.substr(colon+1);
            if(buttons!="-") {
                while(!buttons.empty()) {
                    const auto plus=buttons.find('+');const auto name=buttons.substr(0,plus);
                    buttons=plus==std::string_view::npos?std::string_view{}:buttons.substr(plus+1);
                    if(name.size()==5 && name[2]=='=') {
                        const auto digit=[](char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;};
                        unsigned stick=0;while(stick<stick_names.size() && stick_names[stick]!=name.substr(0,2))++stick;
                        if(stick==stick_names.size() || digit(name[3])<0 || digit(name[4])<0)throw std::runtime_error("Invalid stick position: "+std::string(name));
                        input.sticks[stick]=std::uint8_t(digit(name[3])<<4|digit(name[4]));continue;
                    }
                    unsigned bit=0;while(bit<names.size() && names[bit]!=name)++bit;
                    if(bit==names.size())throw std::runtime_error("Unknown pad button: "+std::string(name));
                    input.pressed|=std::uint16_t(1u<<bit);
                }
            }
            script.steps_.emplace_back(field,input);previous=field;
        }
        return script;
    }
    // The input held from this field, if a step begins there.
    std::optional<PadInput> at(unsigned field) const {
        for(const auto& [start,input]:steps_)if(start==field)return input;
        return std::nullopt;
    }
    // One step as parse reads it: what a recording of live input writes.
    static std::string step(unsigned field,const PadInput& input) {
        static constexpr char hex[]="0123456789abcdef";
        std::string text=std::to_string(field)+':';bool any=false;
        for(unsigned bit=0;bit<names.size();++bit)if(input.pressed>>bit&1) {text+=any?"+":"";text+=names[bit];any=true;}
        for(unsigned n=0;n<stick_names.size();++n)if(input.sticks[n]!=0x80) {
            text+=any?"+":"";text+=stick_names[n];text+='=';text+=hex[input.sticks[n]>>4];text+=hex[input.sticks[n]&15];any=true;
        }
        return any?text:text+'-';
    }
    bool empty() const {return steps_.empty();}
    std::size_t size() const {return steps_.size();}
private:
    std::vector<std::pair<unsigned,PadInput>> steps_;
};
