// A game run for a player rather than for a test: the settings file decides the window, the
// volumes and the controls; the in-game menu and fast forward are available. The game program
// (runtime/game/game_main.cpp) starts the runtime with BT2_PLAYER_SETTINGS set to the settings file;
// automated runs never set it.
#pragma once
#ifdef _WIN32
#include "host/frame_pacer.h"
#include "host/overlay.h"
#include "services/audio_mixer.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

class PlayerSession {
public:
    static constexpr int launcher_exit_code=2; // The player chose "Quit to the launcher".
    Settings settings;

    PlayerSession(std::string path,ViewerWindow& window,HostInput& input,AudioMixer& mixer,FramePacer* pacer)
        :path_(std::move(path)),window_(window),input_(input),mixer_(mixer),pacer_(pacer) {
        if(auto loaded=Settings::load(path_))settings=*loaded;else throw std::runtime_error("No settings file at "+path_);
        apply();
    }
    bool fastForward() const {return fast_;}

    // Once per field: the controller state for the game. Opens the in-game menu when its combination
    // is pressed (the game stands still meanwhile) and follows the fast forward key and button.
    PadInput padInput() {
        if(!window_.focused())return {};
        const auto pad=input_.gamepad();
        if(trigger_.update(pad,HostInput::keyDown(VK_ESCAPE),settings.overlay_start_select)) {
            setFast(false);
            const auto result=runOverlay(settings,path_,window_,input_,picture_,[this]{apply();});
            if(result!=OverlayResult::Resume)std::exit(result==OverlayResult::Launcher?launcher_exit_code:0);
            trigger_.swallow();swallow_=true;
            return {};
        }
        const bool fast_button=settings.fast_forward_button<GamepadState::Count && pad.connected && pad.held(GamepadState::Button(settings.fast_forward_button));
        setFast(fast_switch_.update((settings.fast_forward_key && HostInput::keyDown(std::uint8_t(settings.fast_forward_key))) || fast_button,settings.fast_forward_toggle));
        auto mapped=mapInput(settings.mapping,masked(pad),&HostInput::keyDown);
        // What was held when the menu closed (the button that confirmed Resume) reaches the game only after its release.
        if(swallow_) {if(mapped.pressed)mapped.pressed=0;else swallow_=false;}
        return mapped;
    }
    // Each picture shown: kept for the in-game menu's background, counted for the frame rate display.
    void shown(const DisplayFrame& frame) {
        picture_=frame;++pictures_;
        const auto now=std::chrono::steady_clock::now();
        const double seconds=std::chrono::duration<double>(now-window_start_).count();
        if(seconds>=0.5) {
            char text[48];std::snprintf(text,sizeof text,"%.1f fps%s",pictures_/seconds,fast_?"  >>":"");
            window_.setStatus(settings.show_frame_rate || fast_?text:"");
            pictures_=0;window_start_=now;
        }
    }

private:
    std::string path_;ViewerWindow& window_;HostInput& input_;AudioMixer& mixer_;FramePacer* pacer_;
    OverlayTrigger trigger_;FastForward fast_switch_;bool fast_=false,swallow_=false;
    DisplayFrame picture_;unsigned pictures_=0;std::chrono::steady_clock::time_point window_start_=std::chrono::steady_clock::now();

    void apply() {
        window_.setScaling(settings.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);
        window_.setLayout(settings.window_scale,settings.fullscreen);
        mixer_.master_percent=settings.master_volume;mixer_.stream_percent=settings.music_volume;mixer_.extra_percent=settings.effects_volume;
        if(!settings.show_frame_rate && !fast_)window_.setStatus("");
    }
    void setFast(bool on) {fast_=on;if(pacer_)pacer_->free_run=on;}
    // The fast forward button does not reach the game.
    GamepadState masked(GamepadState pad) const {
        if(settings.fast_forward_button<GamepadState::Count)pad.buttons&=~(1u<<settings.fast_forward_button);
        return pad;
    }
};
#endif
