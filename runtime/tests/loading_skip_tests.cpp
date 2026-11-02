// Fast loading's rule (runtime/host/loading_skip.h): when a silent loading stretch starts, lasts and ends.
#include "host/loading_skip.h"
#include <cstdlib>
#include <iostream>
#include <source_location>

namespace {
void check(bool ok,std::source_location at=std::source_location::current()) {
    if(!ok) {std::cerr<<"loading skip: check failed at line "<<at.line()<<'\n';std::exit(1);}
}
}

int main() {
    {
        LoadingSkip skip;
        check(!skip.update({0,true,false}));                 // Nothing loads: paced.
        check(skip.update({3,true,false}));                  // A silent load starts a stretch.
        for(unsigned n=0;n<LoadingSkip::gap;++n)check(skip.update({3,true,false})); // It lasts through a gap...
        check(!skip.update({3,true,false}));                 // ...and ends after it.
        check(skip.update({4,true,false}) && skip.fields==LoadingSkip::gap+2);
    }
    {
        LoadingSkip skip;
        check(!skip.update({1,false,false}));                // A load with sound: paced.
        check(skip.update({2,true,false}));
        check(!skip.update({2,false,false}));                // Sound ends a stretch at once,
        check(!skip.update({2,true,false}));                 // and a new one needs a new load.
        check(skip.update({5,true,false}));
        check(!skip.update({6,true,true}));                  // A held button ends it too, even while loading.
        check(!skip.update({6,true,false}));
    }
    std::cout<<"Loading skip checks passed\n";
}
