// Holds the run at the game's field rate by the host's clock. Every field has a deadline one period
// after the previous one; the run waits for it once per field, at the moment the field's picture is
// shown (or at the field's end when it shows none), so pictures appear at even intervals whatever
// the work before them took. A field that finishes late is not waited for, and after a long stall
// the deadlines start again from now instead of being caught up in a burst.
// The period can be nudged by a small factor, which is how the sound device's queue is kept level:
// the two clocks differ by a tiny amount, and running the fields a fraction of a percent faster or
// slower follows the device without touching the sound.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <chrono>

class FramePacer {
public:
    using Clock=std::chrono::steady_clock;
    static constexpr double field_seconds=1001.0/60000.0; // 59.94 fields per second.
    double waited_seconds=0; // Time spent waiting, for statistics: a field's work is its length minus this.
    unsigned late_fields=0;  // Fields whose deadline had passed when they asked.
    bool free_run=false;     // Fast forward: nothing is waited for; pacing starts afresh when it ends.

    FramePacer() {
        timeBeginPeriod(1);
        // A timer that wakes within a fraction of a millisecond where the system has one (Windows 10 1803 on).
        timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002 /* high resolution */,TIMER_ALL_ACCESS);
    }
    FramePacer(const FramePacer&)=delete;
    FramePacer& operator=(const FramePacer&)=delete;
    ~FramePacer() {if(timer_)CloseHandle(timer_);timeEndPeriod(1);}

    // A new field begins: its deadline is one period after the last one. `factor` scales the period
    // (within 1%).
    void fieldStarted(double factor=1.0) {
        const auto period=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(field_seconds*std::clamp(factor,0.99,1.01)));
        const auto now=Clock::now();
        if(!started_ || now-deadline_>4*period) {deadline_=now;started_=true;} // First field, or a stall: start over.
        deadline_+=period;period_=period;waited_=false;
    }
    // Before a picture is shown ahead of its deadline (the deadline is still waited for after it): waits to
    // the same moment in every field, so pictures stay evenly spaced, as early as they are usually ready.
    // The game read its input at the field's start, so this takes the time a picture sat waiting for its
    // deadline out of the delay between a button and the screen. The moment is when 99% of the last 120
    // pictures were ready, plus a millisecond; a later picture is shown when it is ready.
    // `ready`: when the picture was ready (60 fps: when its in-between picture was). Returns when it is shown.
    Clock::time_point waitToShow(Clock::time_point ready_at=Clock::now()) {
        if(free_run || !started_ || waited_)return Clock::now();
        const auto field_start=deadline_-period_;
        const auto ready=std::clamp(ready_at-field_start,Clock::duration::zero(),period_);
        ready_[ready_next_++%ready_.size()]=ready;ready_count_=std::min<std::size_t>(ready_count_+1,ready_.size());
        auto recent=ready_;std::sort(recent.begin(),recent.begin()+ready_count_);
        const auto usual=recent[std::min(ready_count_-1,ready_count_*99/100)]+std::chrono::milliseconds(1);
        waitUntil(field_start+std::min(usual,period_));
        return Clock::now();
    }
    Clock::duration period() const {return period_;}
    Clock::time_point deadline() const {return deadline_;}
    // How far ahead of their deadline pictures are shown now (statistics).
    double showAheadSeconds() const {
        if(!ready_count_)return 0;
        auto recent=ready_;std::sort(recent.begin(),recent.begin()+ready_count_);
        return std::chrono::duration<double>(period_-std::min(recent[std::min(ready_count_-1,ready_count_*99/100)]+std::chrono::milliseconds(1),period_)).count();
    }
    // Waits for the current field's deadline, once per field (a second call in the same field returns at once).
    void wait() {
        if(free_run) {started_=false;return;}
        if(!started_ || waited_)return;
        waited_=true;
        if(Clock::now()>=deadline_) {++late_fields;return;}
        waitUntil(deadline_);
    }
    // Whether this field's picture is shown: always at the game's speed; in fast forward only when a
    // field period has passed since the last one shown, so the run does not spend its time reading
    // back and drawing pictures nobody can see.
    bool pictureDue() {
        if(!free_run)return true;
        const auto now=Clock::now();
        if(now-last_picture_<std::chrono::duration<double>(field_seconds))return false;
        last_picture_=now;return true;
    }

private:
    void waitUntil(Clock::time_point target) {
        const auto begin=Clock::now();
        for(auto now=begin;now<target;now=Clock::now()) {
            const auto left=target-now;
            if(timer_ && left>std::chrono::microseconds(300)) {
                LARGE_INTEGER due;due.QuadPart=-std::chrono::duration_cast<std::chrono::nanoseconds>(left-std::chrono::microseconds(200)).count()/100;
                if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer_,INFINITE);
            } else if(!timer_ && left>std::chrono::milliseconds(2))Sleep(1);
            else YieldProcessor(); // The last fraction of a millisecond.
        }
        waited_seconds+=std::chrono::duration<double>(Clock::now()-begin).count();
    }
    HANDLE timer_=nullptr;Clock::time_point deadline_{},last_picture_{};bool started_=false,waited_=false;
    Clock::duration period_{};
    std::array<Clock::duration,120> ready_{};std::size_t ready_next_=0,ready_count_=0; // When recent pictures were ready in their fields.
};
#endif
