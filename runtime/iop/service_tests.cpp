#include "module_service.h"
#include "embedded_native.h"
#include <iostream>
#include <fstream>
void check(bool value){if(!value)throw std::runtime_error("Native IOP service test differs");}
template<class F> void rejected(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}check(failed);}
int main(int argc,char** argv) {
    if(argc==4 && (std::string(argv[1])=="--disc-read" || std::string(argv[1])=="--cri-create")) {
        try {
            NativeIopService service(1,bt2_compiled_iop_modules());std::vector<std::uint8_t> ee(32*1024*1024),request(300);
            if(std::string(argv[1])=="--cri-create") {
                for(const auto* name:{"SIO2MAN","MCMAN","MCSERV","DBCMAN","SIO2D","DS2U_D","LIBSD","SDRDRV","MODHSYN","MODSESQ2","CDVDSTM","SOUNDS","CRI_ADXI"})
                    {service.load(1,std::string("cdrom0:\\IRX\\")+name+".IRX;1");if(std::string(name)=="MCMAN")service.attachEeTransport(1);}
                service.allocateBootHeap(1,0x360);std::copy(iop_embedded_file.begin(),iop_embedded_file.end(),service.runtime.ram.begin()+0xCE500);
                service.loadBootBuffer(1,0xCE500);service.freeBootHeap(1,0xCE500);service.allocateBootHeap(1,0x81800);
            }
            std::memcpy(request.data()+36,"\\DATA\\ZS2US.DIR;1",17);const unsigned dest=0x3A8600;std::memcpy(request.data()+292,&dest,4);
            service.callDiscSearch(1,request,ee,argv[2]);service.callDiscReady(1);
            const std::array<unsigned,6> read={0xE2A,2,0x39DE80,0x100,0x3694C0,0x369580};
            auto input=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(read.data()),sizeof(read));
            const auto prior=service.runtime.ram;const auto prior_steps=service.runtime.steps;
            rejected([&]{service.callDiscRead(2,input,ee,argv[2]);});
            rejected([&]{service.callDiscRead(1,input,ee,"missing-owned-disc-test.iso");});
            check(service.runtime.ram==prior && service.runtime.steps==prior_steps && !service.disc_read_callbacks && !service.disc_read_alarm_sets);
            const auto result=service.callDiscRead(1,input,ee,argv[2]);
            check(result==4096 && service.disc_read_calls==1 && service.disc_read_callbacks==1 && service.disc_read_dma_submissions==4 && service.disc_read_dma_polls==3 && service.disc_read_alarm_sets==1 && service.disc_read_alarm_cancels==1 && !service.disc_read_alarm.deadline);
            unsigned progress;std::memcpy(&progress,ee.data()+0x369580,4);check(progress==4096);
            const auto complete=service.runtime.ram;const auto complete_steps=service.runtime.steps;
            rejected([&]{service.callDiscRead(1,input,ee,argv[2]);});check(service.runtime.ram==complete && service.runtime.steps==complete_steps);

            std::fill(request.begin(),request.end(),0);std::memcpy(request.data()+36,"\\DATA\\ZS2US_1.AFS;1",19);std::memcpy(request.data()+292,&dest,4);
            check(service.callDiscSearch(1,request,ee,argv[2])==1 && service.disc_search_calls==2 && service.disc_dma_submissions==2);
            check(service.runtime.read(0x1A6498,4)==0x1FC26 && service.runtime.read(0x1A649C,4)==0x419B9800);
            const auto archive_state=service.runtime.ram;rejected([&]{service.callDiscSearch(1,request,ee,argv[2]);});check(service.runtime.ram==archive_state);
            const std::array<std::string,4> rest={"\\DATA\\ZS2US_2.AFS;1","\\DATA\\ZS2USOP.SFD;1","\\DATA\\ZS2USED.SFD;1","\\DATA\\LOGO_O.SFD;1"};
            for(const auto& name:rest){std::fill(request.begin(),request.end(),0);std::memcpy(request.data()+36,name.c_str(),name.size()+1);std::memcpy(request.data()+292,&dest,4);check(service.callDiscSearch(1,request,ee,argv[2])==1);}
            check(service.disc_search_calls==6 && service.disc_dma_submissions==6);
            const auto batch_state=service.runtime.ram;rejected([&]{service.callDiscSearch(1,request,ee,argv[2]);});check(service.runtime.ram==batch_state);
            if(std::string(argv[1])=="--cri-create") {
                check(service.allocateBootHeap(1,0x8D0)==0x14FD00);
                const std::array<unsigned,4> args={0,0x365BC0,0x14FD40,0x880};
                auto input=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(args.data()),sizeof(args));
                const auto prior=service.runtime.ram;const auto steps=service.runtime.steps;
                rejected([&]{service.callCriCreate(2,input);});rejected([&]{service.callCriCreate(1,input.first(12));});
                check(service.runtime.ram==prior && service.runtime.steps==steps);
                const auto response=service.callCriCreate(1,input);unsigned slot;std::memcpy(&slot,response.data(),4);check(slot==0x1B58C0 && service.cri_create_calls==1);
                const auto completed=service.runtime.ram;rejected([&]{service.callCriCreate(1,input);});check(service.runtime.ram==completed);
                check(service.callCriControl(1).empty() && service.cri_control_calls==1);
                const auto controlled=service.runtime.ram;rejected([&]{service.callCriControl(1);});check(service.runtime.ram==controlled);
                const std::array<unsigned,3> configuration={0xBB80,0xFFFF,0xFFFF};
                const auto configuration_bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(configuration.data()),12);
                rejected([&]{service.callCriControl(2,0x40C,configuration_bytes);});
                rejected([&]{service.callCriControl(1,0x40C,configuration_bytes.first(8));});
                auto wrong_configuration=configuration;wrong_configuration[0]=44100;
                rejected([&]{service.callCriControl(1,0x40C,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(wrong_configuration.data()),12));});
                rejected([&]{service.callCriControl(1,0x422,configuration_bytes);});
                check(service.runtime.ram==controlled);
                const auto initialized=service.callCriControl(1,0x40C,configuration_bytes);
                check(initialized.size()==12);
                const std::array<unsigned,3> initialized_words={0,1,0};
                check(std::equal(initialized.begin(),initialized.end(),reinterpret_cast<const std::uint8_t*>(initialized_words.data())));
                const auto& child=service.runtime.threads.at(0x2000E);
                check(service.cri_control_calls==2 && child.entry==0x1AC890 && child.waiting && !child.ready
                    && child.wait_semaphore==service.runtime.read(0x1B4A84,4) && child.context.pc==0x1B363C
                    && child.context.get(31)==0x1AC8D0 && !service.runtime.current_thread && service.runtime.spu2_dma_idle_reads==1
                    && service.runtime.spu2_configuration.at(0x1F9001B0)==1);
                const auto& d=service.runtime.spu2_auto_dma;
                check(d.configured && d.pending && d.program_stage==4 && d.readbacks==1 && d.source==0x1B5FC0
                    && d.madr==d.source && d.bcr==0x200010 && d.chcr==0x1000201 && d.initial_half==std::vector<std::uint8_t>(2048));
                check(service.runtime.native_export_results.at("libsd:18")==2048);
                check(service.runtime.cpu_interrupts_enabled && !service.runtime.in_interrupt
                    && service.runtime.spu2_configuration.at(0x1F90076C)==0x7FFF && service.runtime.spu2_configuration.at(0x1F90076E)==0x7FFF);
                rejected([&]{service.callCriControl(1,0x40C,configuration_bytes);});
                const auto before_fourth=service.runtime.ram;const auto before_fourth_steps=service.runtime.steps;
                rejected([&]{service.allocateBootHeap(2,0x41040);});rejected([&]{service.allocateBootHeap(1,0x41000);});
                check(service.runtime.ram==before_fourth && service.runtime.steps==before_fourth_steps);
                service.runtime.map(0x150600,std::vector<std::uint8_t>(4));
                const auto conflicting_fourth=service.runtime.ram;
                rejected([&]{service.allocateBootHeap(1,0x41040);});
                check(service.runtime.ram==conflicting_fourth && service.runtime.steps==before_fourth_steps && service.boot_heap_allocations==3);
                std::erase(service.runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{0x150600,0x150604});
                check(service.allocateBootHeap(1,0x41040)==0x150600 && service.boot_heap_allocations==4);
                for(unsigned index=0;index<16;++index) {
                    const std::array<unsigned,3> stream={1,0x150640+index*0x4100,0x100};
                    auto bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(stream.data()),12);
                    const auto before_stream=service.runtime.ram;const auto before_steps=service.runtime.steps;
                    rejected([&]{service.callCriControl(2,0x422,bytes);});
                    auto wrong=stream;wrong[1]+=4;
                    rejected([&]{service.callCriControl(1,0x422,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(wrong.data()),12));});
                    check(service.runtime.ram==before_stream && service.runtime.steps==before_steps);
                    const auto streamed=service.callCriControl(1,0x422,bytes);
                    unsigned stream_handle=0;check(streamed.size()==4);std::memcpy(&stream_handle,streamed.data(),4);
                    check(stream_handle==0x1BBA40+index*0x30 && service.cri_control_calls==3+index*2);
                    const std::array<unsigned,4> attach={0x364B08+index*0x30,stream_handle,1,0x365900+index*0x14};
                    const auto attached=service.callCriControl(1,0x400,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(attach.data()),16));
                    unsigned attachment=0;check(attached.size()==4);std::memcpy(&attachment,attached.data(),4);
                    check(attachment==0x1BC580+index*0x14 && service.cri_control_calls==4+index*2);
                }
                check(service.runtime.spu2_auto_dma.pending && service.runtime.spu2_auto_dma.chcr==0x1000201
                    && child.waiting && !child.ready && service.cri_control_calls==34);
                const auto batch_done=service.runtime.ram;rejected([&]{service.callCriControl(1,0x422,configuration_bytes);});
                check(service.runtime.ram==batch_done);
                const auto before_fifth=service.runtime.ram;const auto fifth_steps=service.runtime.steps;
                rejected([&]{service.allocateBootHeap(2,0x8D0);});rejected([&]{service.allocateBootHeap(1,0x8D4);});
                check(service.runtime.ram==before_fifth && service.runtime.steps==fifth_steps);
                service.runtime.map(0x191700,std::vector<std::uint8_t>(4));const auto collision=service.runtime.ram;
                rejected([&]{service.allocateBootHeap(1,0x8D0);});check(service.runtime.ram==collision && service.runtime.steps==fifth_steps);
                std::erase(service.runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{0x191700,0x191704});
                check(service.allocateBootHeap(1,0x8D0)==0x191700);
                const std::array<unsigned,4> second_args={1,0x35FD00,0x191740,0x880};
                const auto second_bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(second_args.data()),16);
                const auto second_slot=service.callCriCreate(1,second_bytes);unsigned second_pointer=0;std::memcpy(&second_pointer,second_slot.data(),4);
                check(second_pointer==0x1B5904 && service.cri_create_calls==2);
                const auto before_sixth=service.runtime.ram;const auto sixth_steps=service.runtime.steps;
                rejected([&]{service.allocateBootHeap(2,0x4040);});rejected([&]{service.allocateBootHeap(1,0x4000);});
                check(service.runtime.ram==before_sixth && service.runtime.steps==sixth_steps);
                service.runtime.map(0x192000,std::vector<std::uint8_t>(4));const auto sixth_collision=service.runtime.ram;
                rejected([&]{service.allocateBootHeap(1,0x4040);});check(service.runtime.ram==sixth_collision && service.runtime.steps==sixth_steps);
                std::erase(service.runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{0x192000,0x192004});
                check(service.allocateBootHeap(1,0x4040)==0x192000 && service.boot_heap_allocations==6);
                const std::array<unsigned,3> setup={0x10,0x192000,0x4040};
                check(service.callCriControl(1,0x40A,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(setup.data()),12)).empty());
                for(unsigned index=0;index<5;++index) {
                    const std::array<unsigned,4> pair={2,0,0x1BBA40+index*0x60,0x1BBA70+index*0x60};
                    const auto bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(pair.data()),16);
                    const auto prior=service.runtime.ram;const auto steps=service.runtime.steps;
                    rejected([&]{service.callCriControl(2,0x408,bytes);});
                    auto wrong=pair;wrong[2]+=4;rejected([&]{service.callCriControl(1,0x408,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(wrong.data()),16));});
                    check(service.runtime.ram==prior && service.runtime.steps==steps);
                    const auto result=service.callCriControl(1,0x408,bytes);unsigned pointer=0;std::memcpy(&pointer,result.data(),4);
                    check(pointer==0x1B5DA0+index*0x1C);
                }
                check(service.cri_control_calls==40 && service.runtime.spu2_auto_dma.pending && child.waiting && !child.ready);
                std::ofstream out(argv[3],std::ios::binary);out.write(reinterpret_cast<const char*>(service.runtime.ram.data()),service.runtime.ram.size());check(bool(out));
                std::cout<<"Both CRI channel constructors, sixteen first-channel pairs and five second-channel pairs complete; initial AutoDMA half remains pending.\n";return 0;
            }
            std::ofstream output(argv[3],std::ios::binary);output.write(reinterpret_cast<const char*>(ee.data()+0x39DE80),4096);if(!output)throw std::runtime_error("Read validation write failed");
            std::cout<<"Original IOP sector result "<<result<<" callbacks "<<service.disc_read_callbacks<<" DMA "<<service.disc_read_dma_submissions<<" polls "<<service.disc_read_dma_polls<<"\n";return 0;
        }catch(const std::exception& e){std::cerr<<"Owned disc read stopped: "<<e.what()<<"\n";return 1;}
    }
    if(argc!=1)return 1;
    try {
        constexpr auto path="cdrom0:\\IRX\\SIO2MAN.IRX;1";
        NativeIopService service(1,bt2_compiled_iop_modules());
        rejected([&]{service.load(2,path);});rejected([&]{service.load(1,"missing");});
        const std::array<std::uint8_t,1> arguments{0};rejected([&]{service.load(1,path,arguments);});
        check(service.runtime.steps==0 && service.residents.empty());
        const auto loaded=service.load(1,path);check(loaded.id==1 && loaded.result==0 && service.residents.size()==1);
        check(service.runtime.steps==151 && service.runtime.threads.begin()->second.waiting);
        rejected([&]{service.load(1,path);});check(service.runtime.steps==151 && service.residents.size()==1);
        std::vector<std::uint8_t> disc_request(300),disc_ee(32*1024*1024);
        const auto before_disc_steps=service.runtime.steps;
        rejected([&]{service.callDiscSearch(2,disc_request,disc_ee,"missing.iso");});
        rejected([&]{service.callDiscSearch(1,disc_request,disc_ee,"missing.iso");});
        rejected([&]{service.callDiscSearch(1,std::span<const std::uint8_t>(disc_request.data(),299),disc_ee,"missing.iso");});
        check(service.runtime.steps==before_disc_steps && !service.disc_search_calls && !service.disc_dma_submissions);
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
        const auto rpc_server=service.runtime.rpc_servers.at(0x80000701);
        const auto worker_id=service.runtime.rpc_queues.at(rpc_server.queue).thread;
        const auto worker_cpu=service.runtime.threads.at(worker_id).context;
        const auto before_rpc=service.runtime.ram;const auto before_rpc_steps=service.runtime.steps;
        std::array<std::uint32_t,16> payload={0x3A5C40,0x370000,0x370000,0x370000,0x370000,0x370000,0x370000};
        const auto input=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(payload.data()),64);
        rejected([&]{service.callSoundRpc(2,0xE620,input,16);});rejected([&]{service.callSoundRpc(1,0xE621,input,16);});
        rejected([&]{service.callSoundRpc(1,0xE620,input.first(60),16);});rejected([&]{service.callSoundRpc(1,0xE620,input,20);});
        check(service.runtime.ram==before_rpc && service.runtime.steps==before_rpc_steps);
        check(service.callSoundRpc(1,0xE620,input,16)==std::vector<std::uint8_t>(16));
        check(service.sound_rpc_requests==1 && !service.runtime.current_thread && service.runtime.steps==before_rpc_steps+71);
        const auto& retained=service.runtime.threads.at(worker_id);
        check(retained.waiting && retained.wait_rpc==rpc_server.queue && retained.context.r==worker_cpu.r && retained.context.pc==worker_cpu.pc);
        const auto after_rpc=service.runtime.ram;const auto after_rpc_steps=service.runtime.steps;
        rejected([&]{service.callSoundRpc(1,0xE620,input,16);});check(service.runtime.ram==after_rpc && service.runtime.steps==after_rpc_steps);
        std::array<std::uint32_t,64> sounds_payload={0,1,0x4000};
        const auto sounds_input=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(sounds_payload.data()),256);
        const auto sounds_before=service.runtime.ram;const auto sounds_steps=service.runtime.steps;
        rejected([&]{service.callSoundRpc(1,16,sounds_input,64,0x02000005);});
        rejected([&]{service.callSoundRpc(1,17,sounds_input,64,0x02000004);});
        rejected([&]{service.callSoundRpc(2,16,sounds_input,64,0x02000004);});
        check(service.runtime.ram==sounds_before && service.runtime.steps==sounds_steps && !service.sounds_rpc_requests);
        const auto sounds_server=service.runtime.rpc_servers.at(0x02000004);const auto sounds_worker=service.runtime.rpc_queues.at(sounds_server.queue).thread;
        const auto sounds_cpu=service.runtime.threads.at(sounds_worker).context;
        check(service.callSoundRpc(1,16,sounds_input,64,0x02000004)==std::vector<std::uint8_t>(64));
        check(service.sounds_rpc_requests==1 && service.runtime.steps==sounds_steps+54 && !service.runtime.current_thread);
        check(service.runtime.threads.at(sounds_worker).waiting && service.runtime.threads.at(sounds_worker).context.r==sounds_cpu.r);
        const auto sounds_after=service.runtime.ram;
        rejected([&]{service.callSoundRpc(1,16,sounds_input,64,0x02000004);});check(service.runtime.ram==sounds_after);
        std::array<std::uint8_t,144> dbc_input{};
        const auto dbc_before=service.runtime.ram;const auto dbc_steps=service.runtime.steps;
        rejected([&]{service.callDbcRpc(2,0x80001363,dbc_input,144);});
        rejected([&]{service.callDbcRpc(1,0x80001364,dbc_input,144);});
        rejected([&]{service.callDbcRpc(1,0x80001363,dbc_input,140);});
        check(service.runtime.ram==dbc_before && service.runtime.steps==dbc_steps && !service.dbc_rpc_requests);
        const auto dbc_server=service.runtime.rpc_servers.at(0x80001300);
        const auto dbc_worker=service.runtime.rpc_queues.at(dbc_server.queue).thread;
        const auto dbc_cpu=service.runtime.threads.at(dbc_worker).context;
        std::vector<std::uint8_t> dbc_expected(144);dbc_expected[0]=0x10;dbc_expected[1]=3;
        check(service.callDbcRpc(1,0x80001363,dbc_input,144)==dbc_expected);
        check(service.dbc_rpc_requests==1 && service.runtime.steps==dbc_steps+54 && !service.runtime.current_thread);
        check(service.runtime.threads.at(dbc_worker).waiting && service.runtime.threads.at(dbc_worker).context.r==dbc_cpu.r);
        const auto dbc_after=service.runtime.ram;
        rejected([&]{service.callDbcRpc(1,0x80001363,dbc_input,144);});check(service.runtime.ram==dbc_after);
        std::array<std::uint32_t,36> dbc_setup={0x310,0x3A27C0};
        const auto dbc_setup_input=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(dbc_setup.data()),144);
        auto bad_setup=dbc_setup;bad_setup[1]+=4;
        rejected([&]{service.callDbcRpc(1,0x80001304,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bad_setup.data()),144),144);});
        check(service.runtime.ram==dbc_after && service.dbc_rpc_requests==1);
        std::vector<std::uint8_t> short_endpoint(128);
        rejected([&]{service.callDbcRpc(1,0x80001304,dbc_setup_input,144,short_endpoint);});
        check(service.runtime.ram==dbc_after && !service.dbc_dma_submissions && !service.runtime.providers.contains({"sifman",7}));
        std::vector<std::uint8_t> ee_memory(32*1024*1024,0xA5);
        const auto source=std::vector<std::uint8_t>(service.runtime.ram.begin()+0x62778,service.runtime.ram.begin()+0x627F8);
        std::vector<std::uint8_t> setup_expected(144);setup_expected[0]=1;std::memcpy(setup_expected.data()+4,dbc_setup.data()+1,4);
        check(service.callDbcRpc(1,0x80001304,dbc_setup_input,144,ee_memory)==setup_expected);
        check(service.dbc_rpc_requests==2 && service.dbc_dma_submissions==1 && service.dbc_dma_polls==1 && !service.runtime.current_thread);
        check(std::equal(source.begin(),source.end(),ee_memory.begin()+0x3A27C0));
        check(std::all_of(ee_memory.begin(),ee_memory.begin()+0x3A27C0,[](auto b){return b==0xA5;}) && std::all_of(ee_memory.begin()+0x3A2840,ee_memory.end(),[](auto b){return b==0xA5;}));
        check(service.runtime.threads.at(dbc_worker).waiting && service.runtime.threads.at(dbc_worker).context.r==dbc_cpu.r);
        check(!service.runtime.providers.contains({"sifman",7}) && !service.runtime.providers.contains({"sifman",8}));
        const auto dbc_setup_after=service.runtime.ram;const auto dbc_setup_steps=service.runtime.steps;
        rejected([&]{service.callDbcRpc(1,0x80001304,dbc_setup_input,144,ee_memory);});
        check(service.runtime.ram==dbc_setup_after && service.runtime.steps==dbc_setup_steps);
        for(unsigned slot=0;slot<2;++slot) {
            std::array<std::uint32_t,36> input{};input[0]=3;input[1]=1;input[2]=slot;
            input[10]=0x3B0B00+slot*0x180;input[11]=input[10]+0x80;
            const auto bytes=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(input.data()),144);
            const auto reply=service.callDbcRpc(1,0x80001301,bytes,144,ee_memory);
            input[9]=slot;check(reply.size()==144 && std::memcmp(reply.data(),input.data(),144)==0);
            check(service.dbc_rpc_requests==3+slot && service.dbc_dma_submissions==1 && service.dbc_dma_polls==1);
            check(std::equal(source.begin(),source.end(),service.dbc_dma_payload.begin()));
        }
        std::array<std::uint8_t,48> mc_input{};
        const auto mc_before=service.runtime.ram;
        rejected([&]{service.callMcRpc(2,mc_input);});check(service.runtime.ram==mc_before);
        auto bad_mc=mc_input;bad_mc[0]=1;
        rejected([&]{service.callMcRpc(1,bad_mc);});check(service.runtime.ram==mc_before);
        const std::array<std::uint32_t,3> mc_expected={0,0x210,0x226};
        const auto mc_reply=service.callMcRpc(1,mc_input);
        check(mc_reply.size()==12 && std::memcmp(mc_reply.data(),mc_expected.data(),12)==0 && service.mc_rpc_requests==1);
        const auto mc_after=service.runtime.ram;
        rejected([&]{service.callMcRpc(1,mc_input);});check(service.runtime.ram==mc_after);
        std::array<std::uint32_t,12> mc_query_input{};mc_query_input[7]=0x3A7480;
        const auto query_before=service.runtime.ram;
        rejected([&]{service.callMcQuery(2,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(mc_query_input.data()),48));});
        auto bad_query=mc_query_input;bad_query[1]=1;
        rejected([&]{service.callMcQuery(1,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(bad_query.data()),48));});
        check(service.runtime.ram==query_before && !service.mc_query_requests);
        rejected([&]{service.callMcQuery(1,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(mc_query_input.data()),48),std::span<std::uint8_t>(ee_memory.data(),1024));});
        check(service.runtime.ram==query_before && !service.mc_query_requests);
        std::fill_n(ee_memory.begin()+0x3A6D18,4,0);
        const auto query_reply=service.callMcQuery(1,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(mc_query_input.data()),48),ee_memory);
        check(query_reply.size()==4 && service.runtime.current_thread==0 && service.mc_query_requests==1);
        check(service.runtime.timed_waits==5 && service.runtime.timed_wakes==5 && service.runtime.timed_dispatches==1
            && service.runtime.logical_time_us==1900 && service.runtime.card_alarm_fires==4 && service.runtime.card_alarm_sets==5);
        check(service.runtime.sio2_card_fifo_packets==5 && service.mc_dma_submissions==1 && service.mc_dma_polls==1);
        check(std::equal(service.mc_dma_payload.begin(),service.mc_dma_payload.end(),ee_memory.begin()+0x3A7480));
        check(service.runtime.pending_ee_bind.server_id==0x80000704 && service.runtime.pending_ee_bind.client==0xA1CA0);
        const auto& helper=service.runtime.threads.at(service.runtime.pending_ee_bind.thread);
        check(helper.waiting && !helper.ready && helper.wait_ee_bind && helper.context.pc==0xA1244);
        check(!service.runtime.providers.contains({"sifcmd",15}) && !service.runtime.providers.contains({"sifman",7}) && !service.runtime.providers.contains({"sifman",8}));
        const auto& dma=service.runtime.dma_channels.at(11);
        check(dma.slice_known && dma.madr==0x320B0 && dma.bcr==0x10024 && dma.chcr==0x201);
        check(service.runtime.sio2_card_sends==6 && service.runtime.sio2_card_receives==6);
        const auto& receive_dma=service.runtime.dma_channels.at(12);
        check(receive_dma.madr==0x326E0 && receive_dma.bcr==0x10024 && receive_dma.chcr==0x40000200);
        check(std::all_of(service.runtime.ram.begin()+0x32650,service.runtime.ram.begin()+0x32656,[](auto v){return v==0xFF;}));
        check(std::all_of(service.runtime.ram.begin()+0x32656,service.runtime.ram.begin()+0x326E0,[](auto v){return v==0;}));
        const auto failed_query=service.runtime.ram;const auto failed_steps=service.runtime.steps;
        rejected([&]{service.callMcQuery(1,std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(mc_query_input.data()),48));});
        check(service.runtime.ram==failed_query && service.runtime.steps==failed_steps);

        const auto bind_before=service.runtime.ram;
        rejected([&]{service.completeEeSoundBind(2,ee_memory);});
        rejected([&]{service.completeEeSoundBind(1,ee_memory);});
        check(service.runtime.ram==bind_before && !service.ee_sound_bind_returns);
        auto store_ee=[&](unsigned address,unsigned value){std::memcpy(ee_memory.data()+address,&value,4);};
        // Synthetic owned registration fixture; full generated registration is compared separately.
        store_ee(0x3A6D00,3);store_ee(0x3A6D08,0x3A6D18);store_ee(0x3A6D18,0x80000704);
        store_ee(0x3A6D1C,0x3153F0);store_ee(0x3A6D20,0x3A6CC0);
        const auto packet_id=service.runtime.read(0x1FF418,4);
        service.runtime.write(0x1FF418,3,4);
        rejected([&]{service.completeEeSoundBind(1,ee_memory);});
        check(!service.ee_sound_bind_returns);service.runtime.write(0x1FF418,packet_id,4);
        service.completeEeSoundBind(1,ee_memory);
        check(service.ee_sound_bind_returns==1 && !service.runtime.pending_ee_bind.thread
            && helper.waiting && !helper.ready && !helper.wait_ee_bind && helper.context.pc==0xA1304
            && helper.context.get(31)==0xA0F24 && service.runtime.current_thread==0
            && service.runtime.read(0xA1CA0,4)==0 && service.runtime.read(0xA1CB4,4)==0x3A6CC0
            && service.runtime.read(0xA1CC4,4)==0x3A6D18
            && !service.runtime.providers.contains({"sifcmd",15}) && !service.runtime.providers.contains({"thbase",24}));
        const auto bind_complete=service.runtime.ram;
        rejected([&]{service.completeEeSoundBind(1,ee_memory);});check(service.runtime.ram==bind_complete);
        NativeIopService blocked(1,bt2_compiled_iop_modules());blocked.runtime.providers.erase({"thevent",4});
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty());const auto steps=blocked.runtime.steps;
        rejected([&]{blocked.load(1,path);});check(blocked.residents.empty() && blocked.runtime.steps==steps);
        std::cout<<"Native entry completion, epoch isolation, retained worker and failed-load exclusion verified.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
