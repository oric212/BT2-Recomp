#include "ee/online_lock_on.h"
#include <iostream>
#include <stdexcept>

// Online battles start each round locked on (ee/online_lock_on.h): the lock is set on a round's first fighting frame only.
namespace {
void check(bool value,const char* what) {if(!value)throw std::runtime_error(what);}
}

int main() {
    try {
        using namespace online_lock_on;
        constexpr std::uint32_t intro=0x100004,ready=0x200004,fight=0x400004;
        bool was=false;
        check(atUpdate(intro,was)==intro && !was,"the intro is left alone");
        check(atUpdate(ready,was)==ready && !was,"so is the ready phase");
        check(atUpdate(fight,was)==(fight|locked_on) && was,"the first fighting frame locks on");
        check(atUpdate(fight,was)==fight,"later frames keep the game's own rules (a lost lock stays lost)");
        check(atUpdate(fight|locked_on,was)==(fight|locked_on),"a lock already set stays");
        check(atUpdate(ready,was)==ready && !was && atUpdate(fight,was)==(fight|locked_on),"the next round locks on again");
        std::cout<<"Online lock-on checks passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
