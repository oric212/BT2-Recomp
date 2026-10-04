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
        const auto mcman=service.load(1,"cdrom0:\\IRX\\MCMAN.IRX;1");check(mcman.id==2 && mcman.result==2);
        check(service.residents.size()==2 && service.runtime.threads.begin()->second.waiting && service.runtime.steps==1365);
        IopContext query;check(service.runtime.import("sifman",29,query)==0);
        rejected([&]{service.attachEeTransport(2);});service.attachEeTransport(1);rejected([&]{service.attachEeTransport(1);});
        check(service.runtime.import("sifman",29,query)==1 && fresh.runtime.import("sifman",29,query)==0);
        const auto mcserv=service.load(1,"cdrom0:\\IRX\\MCSERV.IRX;1");check(mcserv.id==3 && mcserv.result==2 && service.runtime.steps==1464);
        const auto dbcman=service.load(1,"cdrom0:\\IRX\\DBCMAN.IRX;1");check(dbcman.id==4 && dbcman.result==0 && service.runtime.steps==2226);
        rejected([&]{service.load(1,"cdrom0:\\IRX\\SIO2D.IRX;1");});check(service.residents.size()==4 && service.runtime.steps==3440);
        unsigned waiting=0;for(const auto& [id,t]:service.runtime.threads)if(t.waiting)++waiting;check(waiting==6);

        NativeIopService blocked(1,bt2_compiled_iop_modules());blocked.runtime.providers.erase({"thevent",4});
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty());const auto steps=blocked.runtime.steps;
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty() && blocked.runtime.steps==steps);
        std::cout<<"Native entry completion, epoch isolation, retained worker and failed-load exclusion verified.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
