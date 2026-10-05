#include "module_service.h"
#include "embedded_native.h"
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
        const auto sio2d=service.load(1,"cdrom0:\\IRX\\SIO2D.IRX;1");check(sio2d.id==5 && sio2d.result==0 && service.runtime.steps==3459);
        const auto ds2ud=service.load(1,"cdrom0:\\IRX\\DS2U_D.IRX;1");check(ds2ud.id==6 && ds2ud.result==0 && service.residents.size()==6 && service.runtime.steps==7491);
        const auto libsd=service.load(1,"cdrom0:\\IRX\\LIBSD.IRX;1");check(libsd.id==7 && libsd.result==0 && service.runtime.steps==7564);
        const auto sdrdrv=service.load(1,"cdrom0:\\IRX\\SDRDRV.IRX;1");check(sdrdrv.id==8 && sdrdrv.result==2 && service.runtime.steps==7797);
        const auto modhsyn=service.load(1,"cdrom0:\\IRX\\MODHSYN.IRX;1");check(modhsyn.id==9 && modhsyn.result==0 && service.runtime.steps==7817);
        const auto modsesq2=service.load(1,"cdrom0:\\IRX\\MODSESQ2.IRX;1");check(modsesq2.id==10 && modsesq2.result==0 && service.runtime.steps==7856);
        const auto cdvdstm=service.load(1,"cdrom0:\\IRX\\CDVDSTM.IRX;1");check(cdvdstm.id==11 && cdvdstm.result==2 && service.runtime.steps==7906);
        const auto sounds=service.load(1,"cdrom0:\\IRX\\SOUNDS.IRX;1");check(sounds.id==12 && sounds.result==0 && service.runtime.steps==7978);
        unsigned waiting=0;for(const auto& [id,t]:service.runtime.threads)if(t.waiting)++waiting;check(waiting==10);

        const auto cri=service.load(1,"cdrom0:\\IRX\\CRI_ADXI.IRX;1");
        check(cri.id==13 && cri.result==2 && service.residents.size()==13 && service.runtime.steps==30839);
        rejected([&]{service.load(1,"cdrom0:\\IRX\\CRI_ADXI.IRX;1");});check(service.runtime.steps==30839);
        check(service.runtime.spu2_reset_status_reads==2 && service.runtime.spu2_pending_key_off==std::array<std::uint32_t,2>{0xFFFFFF,0xFFFFFF});
        check(service.runtime.spu2_io_transfer_completed && service.runtime.spu2_sound_ram.size()==8 && service.runtime.native_export_results.at("libsd:4")==0);
        check(service.runtime.interrupts.size()==4 && service.runtime.abi_read_only_words.size()==3);
        rejected([&]{service.allocateBootHeap(2,0x360);});rejected([&]{service.allocateBootHeap(1,0x361);});
        check(!service.boot_heap_allocations && service.runtime.steps==30839);
        const auto heap=service.allocateBootHeap(1,0x360);check(heap==0xCE500 && service.boot_heap_allocations==1 && service.residents.size()==13);
        service.runtime.check(heap,0x400);rejected([&]{service.runtime.check(heap+0x400,1);});
        check(!service.runtime.providers.contains({"sysmem",4}));const auto heap_steps=service.runtime.steps;
        rejected([&]{service.allocateBootHeap(1,0x360);});check(service.runtime.steps==heap_steps);
        rejected([&]{service.loadBootBuffer(2,heap);});rejected([&]{service.loadBootBuffer(1,heap);});
        check(service.runtime.steps==heap_steps && !service.buffer_loads);
        std::copy(iop_embedded_file.begin(),iop_embedded_file.end(),service.runtime.ram.begin()+heap);
        rejected([&]{service.loadBootBuffer(1,heap+4);});
        const auto embedded=service.loadBootBuffer(1,heap);
        check(embedded.id==14 && embedded.result==1 && service.buffer_loads==1 && service.residents.size()==13);
        rejected([&]{service.runtime.check(0xCE930,4);});service.runtime.check(heap,864);
        const auto embedded_steps=service.runtime.steps;
        rejected([&]{service.loadBootBuffer(1,heap);});check(service.runtime.steps==embedded_steps);
        rejected([&]{service.freeBootHeap(2,heap);});rejected([&]{service.freeBootHeap(1,heap+4);});
        check(service.freeBootHeap(1,heap)==0 && service.boot_heap_frees==1 && !service.runtime.providers.contains({"sysmem",5}));
        rejected([&]{service.runtime.check(heap,1);});rejected([&]{service.freeBootHeap(1,heap);});
        const auto before_reuse=service.runtime.steps;
        rejected([&]{service.allocateBootHeap(2,0x81800);});rejected([&]{service.allocateBootHeap(1,0x81801);});
        check(service.runtime.steps==before_reuse);
        for(auto [lo,hi]:service.runtime.ranges)check(!(lo<0x14FD00 && 0xCE500<hi));
        service.runtime.map(heap+0x400,std::vector<std::uint8_t>(4));
        const auto conflict_ram=service.runtime.ram;const auto conflict_ranges=service.runtime.ranges;
        rejected([&]{service.allocateBootHeap(1,0x81800);});
        check(service.runtime.steps==before_reuse && service.runtime.ram==conflict_ram && service.runtime.ranges==conflict_ranges);
        std::erase(service.runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{heap+0x400,heap+0x404});
        check(service.allocateBootHeap(1,0x81800)==heap && service.boot_heap_allocations==2);
        service.runtime.check(heap,0x81800);rejected([&]{service.runtime.check(heap+0x81800,1);});
        const auto second_steps=service.runtime.steps;rejected([&]{service.allocateBootHeap(1,0x81800);});
        check(service.runtime.steps==second_steps && !service.runtime.providers.contains({"sysmem",4}));
        rejected([&]{service.runtime.map(heap+0x400,std::vector<std::uint8_t>(4));});
        NativeIopService blocked(1,bt2_compiled_iop_modules());blocked.runtime.providers.erase({"thevent",4});
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty());const auto steps=blocked.runtime.steps;
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty() && blocked.runtime.steps==steps);
        std::cout<<"Native entry completion, epoch isolation, retained worker and failed-load exclusion verified.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
