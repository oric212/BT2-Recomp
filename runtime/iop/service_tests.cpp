#include "module_service.h"
#include <iostream>
void check(bool value){if(!value)throw std::runtime_error("Native IOP service test differs");}
template<class F> void rejected(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}check(failed);}
int main() {
    try {
        constexpr auto path="cdrom0:\\IRX\\SIO2MAN.IRX;1";
        NativeIopService service(1,bt2_compiled_iop_modules());
        rejected([&]{service.load(2,path);});rejected([&]{service.load(1,"missing");});
        const std::array<std::uint8_t,1> arguments{0};rejected([&]{service.load(1,path,arguments);});
        check(service.runtime.steps==0 && service.residents.empty());
        const auto loaded=service.load(1,path);check(loaded.id==1 && loaded.result==0 && service.residents.size()==1);
        check(service.runtime.steps==151 && service.runtime.threads.begin()->second.waiting);
        rejected([&]{service.load(1,path);});check(service.runtime.steps==151 && service.residents.size()==1);
        NativeIopService fresh(2,bt2_compiled_iop_modules());rejected([&]{fresh.load(1,path);});
        fresh.load(2,path);service.runtime.events.begin()->second.bits=4;
        check(fresh.runtime.events.begin()->second.bits==0 && fresh.runtime.threads.begin()->second.waiting);
        service.runtime.events.begin()->second.bits=0;
        rejected([&]{service.load(1,"cdrom0:\\IRX\\MCMAN.IRX;1");});
        check(service.residents.size()==1 && service.runtime.threads.begin()->second.waiting && service.runtime.steps==1338);
        NativeIopService blocked(1,bt2_compiled_iop_modules());blocked.runtime.providers.erase({"thevent",4});
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty());const auto steps=blocked.runtime.steps;
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty() && blocked.runtime.steps==steps);
        std::cout<<"Native entry completion, epoch isolation, retained worker and failed-load exclusion verified.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
