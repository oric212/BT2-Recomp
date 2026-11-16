// Host input for the game and the menus (Win32): the keyboard, the XInput pads (Xbox controllers) and
// the pads read directly over HID: DualSense, DualShock 4 and generic HID gamepads (hid_gamepads.h).
// `gamepad()` returns the first XInput pad when one is connected, otherwise the first HID pad.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#include "host/gamepad.h"
#include "host/hid_gamepads.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class HostInput {
public:
    // The first connected XInput pad, otherwise the first HID pad, otherwise a disconnected state.
    GamepadState gamepad() {
        const auto pads=gamepads();
        return pads.empty()?GamepadState{}:pads.front();
    }
    // Every connected gamepad: the XInput pads in their slots' order, then the HID pads in the order they were found.
    std::vector<GamepadState> gamepads() {
        std::vector<GamepadState> pads;
        unsigned slots=0;
        for(DWORD user=0;user<XUSER_MAX_COUNT;++user) {
            XINPUT_STATE x{};
            if(XInputGetState(user,&x)!=ERROR_SUCCESS)continue;
            pads.push_back(xinputState(x.Gamepad));slots|=1u<<user;
        }
        xinput_slots_=slots;
        auto hid=hid_.connected();
        std::erase_if(hid,[&pads](const HidGamepads::Pad& pad) {
            const auto state=pad.reader->state();
            if(state.connected)pads.push_back(state);
            return !state.connected; // Gone since connected() looked.
        });
        {std::lock_guard lock(seen_mutex_);hid_listed_=hid;}
        if(timing_ && !hid.empty()) {
            const double age=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-hid.front().reader->received()).count();
            ++reads_;age_sum_+=age;age_max_=std::max(age_max_,age);if(age>8)++old_reads_;
        }
        return pads;
    }
    // Sampling between fields (press_latch.h): from the first call on, a thread reads the connected XInput pads and
    // `keys` (virtual-key codes) about every millisecond; takeSeen gives what was held at any moment since its last call,
    // in gamepads()' order (the pads connected at its last call; each HID pad's own reader sees each report).
    void sampleBetweenFields(std::vector<std::uint8_t> keys) {
        {std::lock_guard lock(seen_mutex_);watched_keys_=std::move(keys);}
        if(!sampler_.joinable())sampler_=std::thread([this]{sample();});
    }
    struct Seen {std::vector<std::uint32_t> pads;std::array<bool,256> keys{};};
    Seen takeSeen() {
        Seen out;
        std::lock_guard lock(seen_mutex_);
        for(DWORD user=0;user<XUSER_MAX_COUNT;++user)if(xinput_slots_>>user&1) {out.pads.push_back(seen_pads_[user]);seen_pads_[user]=0;}
        for(const auto& pad:hid_listed_)out.pads.push_back(pad.reader->takeSeen());
        out.keys=seen_keys_;seen_keys_={};
        return out;
    }
    ~HostInput() {
        stop_sampling_=true;
        if(sampler_.joinable())sampler_.join();
        if(!timing_ || !reads_)return;
        for(const auto& pad:hid_.all()) {
            const auto t=pad.reader->timing();
            std::fprintf(stderr,"%s timing: %u reports, %u had waited in the queue, longest gap %.1f ms\n",pad.reader->name(),t.reports,t.waited,t.longest_gap_ms);
        }
        std::fprintf(stderr,"First HID pad: %u game reads, newest report %.2f ms old on average, %.1f ms at most, %u over 8 ms\n",reads_,age_sum_/reads_,age_max_,old_reads_);
    }
    // Whether a key (virtual-key code) is down right now.
    static bool keyDown(std::uint8_t key) {return (GetAsyncKeyState(key)&0x8000)!=0;}

private:
    static GamepadState xinputState(const XINPUT_GAMEPAD& g) {
        GamepadState s;s.connected=true;
        const auto set=[&](GamepadState::Button b,bool on){if(on)s.buttons|=1u<<b;};
        set(GamepadState::South,g.wButtons&XINPUT_GAMEPAD_A);set(GamepadState::East,g.wButtons&XINPUT_GAMEPAD_B);
        set(GamepadState::West,g.wButtons&XINPUT_GAMEPAD_X);set(GamepadState::North,g.wButtons&XINPUT_GAMEPAD_Y);
        set(GamepadState::LeftShoulder,g.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);set(GamepadState::RightShoulder,g.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
        set(GamepadState::LeftTrigger,g.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD);set(GamepadState::RightTrigger,g.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
        set(GamepadState::Back,g.wButtons&XINPUT_GAMEPAD_BACK);set(GamepadState::Start,g.wButtons&XINPUT_GAMEPAD_START);
        set(GamepadState::LeftStick,g.wButtons&XINPUT_GAMEPAD_LEFT_THUMB);set(GamepadState::RightStick,g.wButtons&XINPUT_GAMEPAD_RIGHT_THUMB);
        set(GamepadState::Up,g.wButtons&XINPUT_GAMEPAD_DPAD_UP);set(GamepadState::Down,g.wButtons&XINPUT_GAMEPAD_DPAD_DOWN);
        set(GamepadState::Left,g.wButtons&XINPUT_GAMEPAD_DPAD_LEFT);set(GamepadState::Right,g.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT);
        // XInput axes are signed with up positive; the neutral form is 0..255 with up at 0.
        const auto axis=[](int v){return std::uint8_t((v+32768)>>8);};
        s.lx=axis(g.sThumbLX);s.ly=axis(-1-g.sThumbLY);s.rx=axis(g.sThumbRX);s.ry=axis(-1-g.sThumbRY);
        return s;
    }
    // The sampler's loop: a high-resolution timer of 1 ms where Windows has one, else Sleep(1).
    void sample() {
        HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        while(!stop_sampling_) {
            const unsigned slots=xinput_slots_;
            std::array<std::uint32_t,XUSER_MAX_COUNT> pads{};
            for(DWORD user=0;user<XUSER_MAX_COUNT;++user)if(slots>>user&1) {
                XINPUT_STATE x{};if(XInputGetState(user,&x)==ERROR_SUCCESS)pads[user]=xinputState(x.Gamepad).buttons;
            }
            {
                std::lock_guard lock(seen_mutex_);
                for(DWORD user=0;user<XUSER_MAX_COUNT;++user)seen_pads_[user]|=pads[user];
                for(const auto key:watched_keys_)if(keyDown(key))seen_keys_[key]=true;
            }
            if(timer) {LARGE_INTEGER due{};due.QuadPart=-10000;SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);WaitForSingleObject(timer,20);}
            else Sleep(1);
        }
        if(timer)CloseHandle(timer);
    }
    std::atomic<unsigned> xinput_slots_{0}; // The XInput slots connected at gamepads()' last call.
    std::mutex seen_mutex_;std::array<std::uint32_t,XUSER_MAX_COUNT> seen_pads_{};std::array<bool,256> seen_keys_{};
    std::vector<std::uint8_t> watched_keys_;
    std::atomic<bool> stop_sampling_{false};
    std::thread sampler_;
    std::vector<HidGamepads::Pad> hid_listed_; // The HID pads at gamepads()' last call (under seen_mutex_).
    HidGamepads hid_;
    const bool timing_=std::getenv("BT2_PAD_TIMING")!=nullptr;
    unsigned reads_=0,old_reads_=0;double age_sum_=0,age_max_=0;
};
#endif
