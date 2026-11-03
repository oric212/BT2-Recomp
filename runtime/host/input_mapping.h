// Which host key and which gamepad button press each PS2 button, and the mapping itself: host state
// in, the game's controller state out. No Win32 here; keys are virtual-key codes looked up through
// a caller-supplied predicate.
#pragma once
#include "host/gamepad.h"
#include "services/devices/pad_input.h"
#include "services/devices/pad_script.h"
#include <array>
#include <cstdint>
#include <functional>
#include <span>

struct InputMapping {
    // Per PS2 button, in the order of `PadScript::names`: select, l3, r3, start, up, right, down,
    // left, l2, r2, l1, r1, triangle, circle, cross, square.
    std::array<std::uint8_t,16> keys{0x08,'C','V',0x0D,0x26,0x27,0x28,0x25,'E','R','Q','W','S','X','Z','A'}; // Backspace, Enter, arrows.
    std::array<GamepadState::Button,16> buttons{GamepadState::Back,GamepadState::LeftStick,GamepadState::RightStick,GamepadState::Start,
        GamepadState::Up,GamepadState::Right,GamepadState::Down,GamepadState::Left,
        GamepadState::LeftTrigger,GamepadState::RightTrigger,GamepadState::LeftShoulder,GamepadState::RightShoulder,
        GamepadState::North,GamepadState::East,GamepadState::South,GamepadState::West};
    std::uint8_t deadzone=24; // A stick this close to its centre counts as at rest.
    unsigned stick_sensitivity=100; // Percent: how far from its centre the game sees a stick, against where it is.
    bool operator==(const InputMapping&) const=default;
};

// The controller state for the game: a PS2 button is pressed when its key or its gamepad button is;
// the sticks are the gamepad's (at rest without one).
inline PadInput mapInput(const InputMapping& mapping,const GamepadState& pad,const std::function<bool(std::uint8_t)>& key_down) {
    PadInput out;
    for(unsigned n=0;n<16;++n)
        if((mapping.keys[n] && key_down(mapping.keys[n])) || (pad.connected && pad.held(mapping.buttons[n])))out.pressed|=std::uint16_t(1u<<n);
    // A stick near its centre is at rest; beyond that its distance from the centre is scaled and
    // stops at the ends of the range.
    const auto axis=[&](std::uint8_t v) {
        if(v>=0x80-mapping.deadzone && v<=0x80+mapping.deadzone)return std::uint8_t(0x80);
        const int scaled=0x80+(int(v)-0x80)*int(mapping.stick_sensitivity)/100;
        return std::uint8_t(scaled<0?0:scaled>0xFF?0xFF:scaled);
    };
    if(pad.connected)out.sticks={axis(pad.rx),axis(pad.ry),axis(pad.lx),axis(pad.ly)};
    return out;
}

// Two players (the setting "Player 2 controller"): the connected gamepads in order, the first player's
// and the second's. With two gamepads or more the first plays port 1 with the keyboard and the second port 2;
// with one, it plays port 1 and the keyboard port 2; with none, the keyboard plays port 1.
struct PlayerPads {GamepadState one,two;bool keyboard_two=false;};
inline PlayerPads assignPlayers(std::span<const GamepadState> pads) {
    PlayerPads out;
    if(!pads.empty())out.one=pads[0];
    if(pads.size()>=2)out.two=pads[1];else out.keyboard_two=pads.size()==1;
    return out;
}
