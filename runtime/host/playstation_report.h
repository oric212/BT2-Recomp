// Sony controllers' HID input reports as a GamepadState: the DualSense and the DualShock 4. The
// layouts follow the controllers' publicly documented reports. Both put the same three button bytes
// after the sticks:
// - the first holds the d-pad as a direction (0 up, clockwise, 8 none) in its low nibble and square,
//   cross, circle, triangle above;
// - the second L1, R1, L2, R2, share/create, options, L3, R3;
// - the third the PS button in bit 0.
// DualSense:
// - USB, report 0x01 (64 bytes): sticks at 1..4, triggers at 5 and 6, buttons at 8..10.
// - Bluetooth, report 0x31 (78 bytes): the same fields one byte later.
// - Bluetooth before the full report is enabled, report 0x01: sticks at 1..4, buttons at 5..7,
//   triggers at 8 and 9.
// DualShock 4:
// - USB (and Sony's wireless adapter), and Bluetooth before the full report is enabled, report 0x01:
//   sticks at 1..4, buttons at 5..7, triggers at 8 and 9.
// - Bluetooth, report 0x11 (78 bytes): the same fields two bytes later.
#pragma once
#include "host/gamepad.h"
#include <cstdint>
#include <optional>
#include <span>

// The sticks at `sticks` and the three button bytes at `buttons`; nothing when the report is too short.
inline std::optional<GamepadState> decodePlayStationReport(std::span<const std::uint8_t> report,std::size_t sticks,std::size_t buttons) {
    if(report.size()<buttons+3 || report.size()<sticks+4)return std::nullopt;
    GamepadState s;s.connected=true;
    s.lx=report[sticks];s.ly=report[sticks+1];s.rx=report[sticks+2];s.ry=report[sticks+3];
    const std::uint8_t b0=report[buttons],b1=report[buttons+1],b2=report[buttons+2];
    const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
    const unsigned hat=b0&15;
    set(GamepadState::Up,hat==7 || hat==0 || hat==1);set(GamepadState::Right,hat>=1 && hat<=3);
    set(GamepadState::Down,hat>=3 && hat<=5);set(GamepadState::Left,hat>=5 && hat<=7);
    set(GamepadState::West,b0&0x10);set(GamepadState::South,b0&0x20);set(GamepadState::East,b0&0x40);set(GamepadState::North,b0&0x80);
    set(GamepadState::LeftShoulder,b1&0x01);set(GamepadState::RightShoulder,b1&0x02);set(GamepadState::LeftTrigger,b1&0x04);set(GamepadState::RightTrigger,b1&0x08);
    set(GamepadState::Back,b1&0x10);set(GamepadState::Start,b1&0x20);set(GamepadState::LeftStick,b1&0x40);set(GamepadState::RightStick,b1&0x80);
    set(GamepadState::Guide,b2&0x01);
    return s;
}

// `bluetooth` tells which transport the report came over (the device's report length says so).
// Returns nothing for a report that is not an input report of the layouts above.
inline std::optional<GamepadState> parseDualSenseReport(std::span<const std::uint8_t> report,bool bluetooth) {
    if(report.empty())return std::nullopt;
    if(!bluetooth && report[0]==0x01)return decodePlayStationReport(report,1,8);
    if(bluetooth && report[0]==0x31)return decodePlayStationReport(report,2,9);
    if(bluetooth && report[0]==0x01)return decodePlayStationReport(report,1,5);
    return std::nullopt;
}

// The report ID tells the layout on either transport.
inline std::optional<GamepadState> parseDualShock4Report(std::span<const std::uint8_t> report) {
    if(report.empty())return std::nullopt;
    if(report[0]==0x01)return decodePlayStationReport(report,1,5);
    if(report[0]==0x11)return decodePlayStationReport(report,3,7);
    return std::nullopt;
}
