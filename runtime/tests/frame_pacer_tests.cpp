#include "host/frame_pacer.h"
#include <cstdlib>
#include <iostream>

// The pacer shows a picture ahead of its field's deadline, at the moment pictures are usually ready, and
// still holds the field to its deadline. Bounds are generous: the test runs on a busy machine too.
namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Frame pacer check failed: "<<what<<'\n';std::exit(1);}}
double since(FramePacer::Clock::time_point t) {return std::chrono::duration<double>(FramePacer::Clock::now()-t).count();}
}

int main() {
    FramePacer pacer;
    for(unsigned field=0;field<30;++field) { // Pictures ready at once: shown about a millisecond into the field.
        pacer.fieldStarted();
        const auto start=FramePacer::Clock::now();
        pacer.waitToShow();
        check(since(start)<0.008,"a ready picture is shown well ahead of the deadline");
        pacer.wait();
        check(since(start)>0.012,"the field still waits for its deadline");
    }
    check(pacer.showAheadSeconds()>0.010,"pictures go out about 15 ms ahead");
    // Pictures ready late in their fields: the moment moves with them, so they stay evenly spaced.
    for(unsigned field=0;field<130;++field) {
        pacer.fieldStarted();
        Sleep(10);
        pacer.waitToShow();
        pacer.wait();
    }
    check(pacer.showAheadSeconds()<0.007,"the moment follows when pictures are ready");
    // A picture ready earlier than the call (60 fps: its in-between picture): the moment it is shown is
    // returned, and the period puts the next picture a field after it.
    pacer.fieldStarted();
    {
        const auto shown=pacer.waitToShow(FramePacer::Clock::now());
        check(shown<=FramePacer::Clock::now() && pacer.deadline()-shown>FramePacer::Clock::duration::zero(),"shown before the deadline");
        check(pacer.period()>std::chrono::milliseconds(16) && pacer.period()<std::chrono::milliseconds(17),"a field's period");
        pacer.wait();
    }
    // Fast forward waits for nothing.
    pacer.free_run=true;pacer.fieldStarted();
    const auto start=FramePacer::Clock::now();
    pacer.waitToShow();pacer.wait();
    check(since(start)<0.005,"fast forward does not wait");
    std::cout<<"Frame pacer tests passed.\n";
    return 0;
}
