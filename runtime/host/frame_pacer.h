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
        deadline_+=period;waited_=false;
    }
    // Waits for the current field's deadline, once per field (a second call in the same field returns at once).
    void wait() {
        if(free_run) {started_=false;return;}
        if(!started_ || waited_)return;
        waited_=true;
        const auto begin=Clock::now();
        if(begin>=deadline_) {++late_fields;return;}
        for(auto now=begin;now<deadline_;now=Clock::now()) {
            const auto left=deadline_-now;
            if(timer_ && left>std::chrono::microseconds(300)) {
                LARGE_INTEGER due;due.QuadPart=-std::chrono::duration_cast<std::chrono::nanoseconds>(left-std::chrono::microseconds(200)).count()/100;
                if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer_,INFINITE);
            } else if(!timer_ && left>std::chrono::milliseconds(2))Sleep(1);
            else YieldProcessor(); // The last fraction of a millisecond.
        }
        waited_seconds+=std::chrono::duration<double>(Clock::now()-begin).count();
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
    HANDLE timer_=nullptr;Clock::time_point deadline_{},last_picture_{};bool started_=false,waited_=false;
};
#endif
