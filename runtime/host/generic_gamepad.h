// A generic HID gamepad (8BitDo in D-input or Switch mode, a plain USB pad) as a GamepadState. Windows'
// HID parser gives the report's buttons by number and its axes with their logical ranges
// (hid_gamepads.h reads them); this file turns those into the neutral form, so it can be tested
// without a device.
// - Sticks: X and Y are the left stick; Z and Rz the right one, or Rx and Ry on pads without Z and Rz.
// - D-pad: the hat switch (eight directions from up, clockwise, or four), outside its range none.
// - Buttons: HID numbers them, not where they sit on the pad; a layout per maker places them, by
//   position, the way GamepadState names them. The button mapping screen changes the rest.
#pragma once
#include "host/gamepad.h"
#include <array>
#include <cstdint>
#include <optional>

struct GenericLayout {
    // GamepadState's button for HID button n+1 (Count: none).
    std::array<GamepadState::Button,16> buttons{};
};

// The button layout for a vendor and product ID.
inline GenericLayout genericLayout(std::uint16_t vendor,std::uint16_t product) {
    using B=GamepadState::Button;
    constexpr B none=GamepadState::Count;
    // 8BitDo in D-input mode: B, A, -, Y, X, -, L, R, L2, R2, select, start, home, L3, R3 (B is the bottom button).
    if(vendor==0x2DC8)return {{GamepadState::South,GamepadState::East,none,GamepadState::West,GamepadState::North,none,
        GamepadState::LeftShoulder,GamepadState::RightShoulder,GamepadState::LeftTrigger,GamepadState::RightTrigger,
        GamepadState::Back,GamepadState::Start,GamepadState::Guide,GamepadState::LeftStick,GamepadState::RightStick,none}};
    // Nintendo's Switch Pro Controller (and pads in Switch mode, which take its IDs): B, A, Y, X, L, R, ZL, ZR,
    // minus, plus, L3, R3, home, capture.
    if(vendor==0x057E && product==0x2009)return {{GamepadState::South,GamepadState::East,GamepadState::West,GamepadState::North,
        GamepadState::LeftShoulder,GamepadState::RightShoulder,GamepadState::LeftTrigger,GamepadState::RightTrigger,
        GamepadState::Back,GamepadState::Start,GamepadState::LeftStick,GamepadState::RightStick,GamepadState::Guide,none,none,none}};
    // The common PC layout: 1 left, 2 bottom, 3 right, 4 top, then L1, R1, L2, R2, select, start, L3, R3, home.
    return {{GamepadState::West,GamepadState::South,GamepadState::East,GamepadState::North,
        GamepadState::LeftShoulder,GamepadState::RightShoulder,GamepadState::LeftTrigger,GamepadState::RightTrigger,
        GamepadState::Back,GamepadState::Start,GamepadState::LeftStick,GamepadState::RightStick,GamepadState::Guide,none,none,none}};
}

// One value from the report with its logical range.
struct GenericValue {
    long value=0,min=0,max=0;
    // 0 at the range's start, 0xFF at its end; the centre when the range is empty.
    std::uint8_t axis() const {
        if(max<=min)return 0x80;
        const long long v=value<min?min:value>max?max:value;
        return std::uint8_t((v-min)*255/(max-min));
    }
};

// What one report held.
struct GenericReport {
    std::uint32_t buttons=0; // Bit n: HID button n+1.
    std::optional<GenericValue> x,y,z,rz,rx,ry,hat;
};

inline GamepadState decodeGenericReport(const GenericLayout& layout,const GenericReport& report) {
    GamepadState s;s.connected=true;
    for(unsigned n=0;n<layout.buttons.size();++n)
        if(report.buttons>>n&1 && layout.buttons[n]<GamepadState::Count)s.buttons|=1u<<layout.buttons[n];
    if(report.x)s.lx=report.x->axis();
    if(report.y)s.ly=report.y->axis();
    if(report.z && report.rz) {s.rx=report.z->axis();s.ry=report.rz->axis();}
    else if(report.rx && report.ry) {s.rx=report.rx->axis();s.ry=report.ry->axis();}
    if(report.hat) {
        const long span=report.hat->max-report.hat->min,at=report.hat->value-report.hat->min;
        // Eight directions from up, clockwise; a four-way hat's are every other one.
        long direction=-1;
        if(span==7 && at>=0 && at<=7)direction=at;
        else if(span==3 && at>=0 && at<=3)direction=at*2;
        const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
        if(direction>=0) {
            set(GamepadState::Up,direction==7 || direction<=1);set(GamepadState::Right,direction>=1 && direction<=3);
            set(GamepadState::Down,direction>=3 && direction<=5);set(GamepadState::Left,direction>=5 && direction<=7);
        }
    }
    return s;
}
