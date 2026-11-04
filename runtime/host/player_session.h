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
#include <functional>

class PlayerSession {
public:
    static constexpr int launcher_exit_code=2; // The player chose "Quit to the launcher".
    static constexpr int title_exit_code=3;    // The player chose "Return to the title screen": the game program starts the game again.
    Settings settings;
    // An online battle (BT2_NET): the game cannot stand still or run fast on one side only, so there is no fast
    // forward, and the in-game menu's keys leave the battle for the launcher.
    bool online=false;

    PlayerSession(std::string path,ViewerWindow& window,HostInput& input,AudioMixer& mixer,FramePacer* pacer)
        :path_(std::move(path)),window_(window),input_(input),mixer_(mixer),pacer_(pacer) {
        if(auto loaded=Settings::load(path_))settings=*loaded;else throw std::runtime_error("No settings file at "+path_);
        apply();
        // BT2_PAUSE_TEST=<field>:<milliseconds>[:<picture file>]: the in-game menu opens by itself at
        // that field, closes after that time and writes the last picture it drew (to reproduce what a
        // pause at a certain moment does, without a key press).
        if(const char* test=std::getenv("BT2_PAUSE_TEST")) {
            const std::string text(test);const auto first=text.find(':'),second=text.find(':',first+1);
            if(first==std::string::npos)throw std::runtime_error("BT2_PAUSE_TEST is <field>:<milliseconds>[:<picture file>]");
            pause_test_field_=unsigned(std::stoul(text.substr(0,first)));pause_test_ms_=unsigned(std::stoul(text.substr(first+1,second==std::string::npos?second:second-first-1)));
            if(second!=std::string::npos)pause_test_picture_=text.substr(second+1);
        }
    }
    bool fastForward() const {return fast_;}
    // The picture under the in-game menu when it opens; without it, the last picture given to shown().
    // Set when pictures are shown on the GPU, which then reads the last one back only for the menu.
    std::function<DisplayFrame()> picture_source;

    // Once per field: the controller state for the game. Opens the in-game menu when its combination
    // is pressed (the game stands still meanwhile) and follows the fast forward key and button.
    PadInput padInput() {
        if(pause_test_field_ && ++fields_==pause_test_field_) {
            DisplayFrame drawn;
            takePicture();
            runOverlay(settings,path_,window_,input_,picture_,[this]{apply();},pause_test_ms_,&drawn);
            if(!pause_test_picture_.empty() && drawn.width)writePpm(pause_test_picture_,drawn);
            std::fprintf(stderr,"Pause test: menu shown at field %u over a picture of %u x %u\n",fields_,picture_.width,picture_.height);
        }
        if(!window_.focused()) {fast_switch_.stop();setFast(false);return {};}
        const auto players=assignPlayers(input_.gamepads());
        const auto& pad=players.one;
        const bool keyboard=!(settings.second_controller && players.keyboard_two); // The keyboard plays port 2 then.
        if(trigger_.update(pad,HostInput::keyDown(VK_ESCAPE),settings.overlay_start_select)) {
            if(online)std::exit(launcher_exit_code); // The session tells the other player as it ends.
            setFast(false);
            takePicture();
            const auto result=runOverlay(settings,path_,window_,input_,picture_,[this]{apply();});
            if(result!=OverlayResult::Resume)std::exit(result==OverlayResult::Launcher?launcher_exit_code:result==OverlayResult::Title?title_exit_code:0);
            trigger_.swallow();swallow_=true;
            return {};
        }
        const bool fast_button=settings.fast_forward_button<GamepadState::Count && pad.connected && pad.held(GamepadState::Button(settings.fast_forward_button));
        if(!online)setFast(fast_switch_.update((settings.fast_forward_key && HostInput::keyDown(std::uint8_t(settings.fast_forward_key))) || fast_button,settings.fast_forward_toggle));
        auto mapped=mapInput(settings.mapping,masked(pad),[keyboard](std::uint8_t key){return keyboard && HostInput::keyDown(key);});
        // What was held when the menu closed (the button that confirmed Resume) reaches the game only after its release.
        if(swallow_) {if(mapped.pressed)mapped.pressed=0;else swallow_=false;}
        return mapped;
    }
    // Once per field with "Player 2 controller" (asked before padInput): port 2's controller state, the second
    // gamepad's or, with one gamepad, the keyboard's (assignPlayers). The in-game menu and fast forward stay player 1's.
    PadInput pad2Input() {
        if(!window_.focused())return {};
        const auto players=assignPlayers(input_.gamepads());
        if(players.keyboard_two)return mapInput(settings.mapping,GamepadState{},&HostInput::keyDown);
        return mapInput(settings.mapping,players.two,[](std::uint8_t){return false;});
    }
    // Each picture shown: counted for the frame rate display; a picture given here is kept for the
    // in-game menu's background (when there is no picture_source).
    void shown(const DisplayFrame* frame=nullptr) {
        if(frame && !picture_source)picture_=*frame;
        ++pictures_;
        const auto now=std::chrono::steady_clock::now();
        const double seconds=std::chrono::duration<double>(now-window_start_).count();
        if(seconds>=0.5) {
            char text[48];std::snprintf(text,sizeof text,"%.1f fps%s",pictures_/seconds,fast_?"  >>":"");
            frame_rate_=settings.show_frame_rate || fast_?text:"";
            window_.setStatus(statusLine());
            pictures_=0;window_start_=now;
        }
    }
    // An online battle's line beside the frame rate (the connection, or a wait for the other player); `now`: shown at
    // once over the last picture, for a game that stands still meanwhile.
    void onlineStatus(std::string text,bool now) {
        if(text==online_status_ && !now)return;
        online_status_=std::move(text);
        if(now)window_.showStatusNow(statusLine());else window_.setStatus(statusLine());
    }

private:
    std::string path_;ViewerWindow& window_;HostInput& input_;AudioMixer& mixer_;FramePacer* pacer_;
    OverlayTrigger trigger_;FastForward fast_switch_;bool fast_=false,swallow_=false;
    unsigned pause_test_field_=0,pause_test_ms_=0,fields_=0;std::string pause_test_picture_;
    std::string frame_rate_,online_status_;
    std::string statusLine() const {return frame_rate_.empty() || online_status_.empty()?frame_rate_+online_status_:frame_rate_+"   "+online_status_;}
    DisplayFrame picture_;unsigned pictures_=0;std::chrono::steady_clock::time_point window_start_=std::chrono::steady_clock::now();

    void takePicture() {if(picture_source)picture_=picture_source();}
    void apply() {
        window_.setScaling(settings.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);
        window_.setLayout(settings.window_scale,settings.fullscreen);
        mixer_.master_percent=settings.master_volume;mixer_.stream_percent=settings.music_volume;mixer_.extra_percent=settings.effects_volume;
        if(!settings.show_frame_rate && !fast_){frame_rate_.clear();window_.setStatus(statusLine());}
    }
    void setFast(bool on) {
        if(on==fast_)return;
        fast_=on;if(pacer_)pacer_->free_run=on;
        if(!on && !settings.show_frame_rate){frame_rate_.clear();window_.setStatus(statusLine());} // The fast forward mark goes at once.
    }
    // The fast forward button does not reach the game.
    GamepadState masked(GamepadState pad) const {
        if(settings.fast_forward_button<GamepadState::Count)pad.buttons&=~(1u<<settings.fast_forward_button);
        return pad;
    }
};
#endif
