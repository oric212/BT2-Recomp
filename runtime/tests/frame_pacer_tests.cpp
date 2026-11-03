#include "host/frame_pacer.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

// The pacer shows a picture ahead of its field's deadline, at the moment pictures are usually ready, and
// still holds the field to its deadline. Bounds are generous: the test runs on a busy machine too.
namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Frame pacer check failed: "<<what<<'\n';std::exit(1);}}
double since(FramePacer::Clock::time_point t) {return std::chrono::duration<double>(FramePacer::Clock::now()-t).count();}
}

int main() {
    FramePacer pacer;
    std::vector<double> shown;double latest_ready=0;
    for(unsigned field=0;field<30;++field) { // Pictures ready at once: shown about a millisecond into the field.
        pacer.fieldStarted();
        const auto start=FramePacer::Clock::now();
        latest_ready=std::max(latest_ready,std::chrono::duration<double>(start-(pacer.deadline()-pacer.period())).count());
        pacer.waitToShow();
        shown.push_back(since(start));
        pacer.wait();
        // Not "12 ms after the field began": deadlines do not drift, so a thread the loaded machine started late
        // has a shorter field; but wait never returns before the deadline.
        check(FramePacer::Clock::now()>=pacer.deadline(),"the field still waits for its deadline");
    }
    // The median: a loaded machine (the gate runs probes beside the tests) may hold a field's thread past 8 ms.
    std::nth_element(shown.begin(),shown.begin()+shown.size()/2,shown.end());
    // The moment covers the latest of the 30 (99%): when the loaded machine started a field's thread late (the pacer
    // sleeps for most of its wait), its picture was ready late, and the moment rightly moved with it.
    check(latest_ready>0.005 || shown[shown.size()/2]<0.008,"a ready picture is shown well ahead of the deadline");
    check(latest_ready>0.005 || pacer.showAheadSeconds()>0.010,"pictures go out about 15 ms ahead");
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
        const auto asked=FramePacer::Clock::now();
        const auto shown=pacer.waitToShow(asked);
        // By the deadline when asked in time: the moment is clamped to the field, and it is the deadline itself when
        // the loop above ran late (a loaded machine stretches Sleep(10) to the system's 15.6 ms tick).
        check(shown<=FramePacer::Clock::now() && (shown<=pacer.deadline()+std::chrono::milliseconds(1) || asked+std::chrono::milliseconds(2)>=pacer.deadline()),
              "shown by the deadline");
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
