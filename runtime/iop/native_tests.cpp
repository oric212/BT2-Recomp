#include "native_runtime.h"
#include "library_provider.h"
#include "kernel_provider.h"
#include "ioman_provider.h"
#include "vblank_provider.h"
#include "cdvd_provider.h"
#include <iostream>
#include <source_location>
void bt2_iop_synthetic(IopRuntime&,IopContext&);
void check(bool v,const std::source_location where=std::source_location::current()) {if(!v)throw std::runtime_error("IOP synthetic result differs at line "+std::to_string(where.line()));}
int main() {
    try {
        {
            auto rejects=[](auto action){bool rejected=false;try{action();}catch(const std::runtime_error&){rejected=true;}check(rejected);};
            IopRuntime d;d.executing_pc=0x1A3B1C;
            rejects([&]{d.read(0xBF40200F,1);});
            d.disc_type_known=true;rejects([&]{d.read(0xBF40200F,4);});
            d.executing_pc=0x1A3A34;rejects([&]{d.read(0xBF40200F,1);});
            d.executing_pc=0x1A3B1C;check(d.read(0xBF40200F,1)==0x14);
            rejects([&]{d.read(0xBF40200F,1);});
            d.executing_pc=0x1A3A34;check(d.read(0xBF40200F,1)==0x14);
            d.executing_pc=0x1A3A38;check(d.read(0xBF402005,1)==0x4C);
            rejects([&]{d.read(0xBF402005,1);});
            d.disc_type_known=false;d.disc_type_reads=0;d.executing_pc=0x1A3B1C;rejects([&]{d.read(0xBF40200F,1);});
        }
        IopRuntime rt;rt.map(0x20000,std::vector<std::uint8_t>(32));
        auto context=[](unsigned offset){IopContext c;c.pc=0x10000+offset;c.set(31,IopRuntime::sentinel);c.set(8,IopRuntime::sentinel);return c;};
        auto c=context(0);bt2_iop_synthetic(rt,c);check(c.get(2)==7 && c.get(3)==9 && !c.get(0));
        c=context(0xFF8);c.set(1,1);bt2_iop_synthetic(rt,c);check(c.get(2)==7 && c.get(3)==9);
        c=context(0xFF8);bt2_iop_synthetic(rt,c);check(c.get(2)==99 && c.get(3)==9);
        rt.write(0x20000,0x1234,4);c=context(0x40);c.set(2,5);c.set(4,0x20000);bt2_iop_synthetic(rt,c);check(c.get(3)==6 && c.get(5)==0x1235);
        c=context(0x80);c.set(4,0x20000);bt2_iop_synthetic(rt,c);check(c.get(2)==9 && c.get(3)==9);
        c=context(0xA0);c.set(4,static_cast<std::uint32_t>(-7));c.set(5,3);bt2_iop_synthetic(rt,c);check(c.get(2)==static_cast<std::uint32_t>(-21) && c.get(3)==UINT32_MAX && c.get(6)==static_cast<std::uint32_t>(-2) && c.get(7)==UINT32_MAX);
        for(unsigned n=0;n<16;++n)rt.write(0x20000+n,n+1,1);
        for(unsigned offset=0;offset<4;++offset) {
            c=context(0xC0);c.set(4,0x20000+offset);bt2_iop_synthetic(rt,c);
            check(c.get(2)==((offset+1)|((offset+2)<<8)|((offset+3)<<16)|((offset+4)<<24)));
            auto before=rt.ram;c=context(0x100);c.set(4,0x20008+offset);c.set(2,0x88776655);bt2_iop_synthetic(rt,c);
            std::uint32_t value=0;std::memcpy(&value,rt.ram.data()+0x20008+offset,4);check(value==0x88776655);
            for(unsigned n=0;n<IopRuntime::ram_size;++n)if(n<0x20008+offset || n>=0x2000C+offset)check(rt.ram[n]==before[n]);
        }
        c=context(0x140);bool rejected=false;try{bt2_iop_synthetic(rt,c);}catch(const std::runtime_error&){rejected=true;}check(rejected && c.pc==0x10180);
        rt.providers[{"synthetic",4}]=[](IopRuntime&,IopContext&){return 17u;};c=context(0x140);bt2_iop_synthetic(rt,c);check(c.get(2)==17);
        rt.write(0x20000,99,4);c=context(0x160);c.set(4,0x20000);bt2_iop_synthetic(rt,c);check(c.get(3)==99);
        rejected=false;try{rt.read(0x1F808280,4);}catch(const std::runtime_error&){rejected=true;}check(rejected && rt.blocked_address==0x1F808280);
        check(iop_div(0x80000000,UINT32_MAX,true)==std::make_pair(0x80000000u,0u));
        c=context(0x1F0);rejected=false;try{bt2_iop_synthetic(rt,c);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        check(iop_sub(0x80000000,0x80000000)==0);
        rejected=false;try{iop_sub(0,0x80000000);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        rejected=false;try{rt.read(0x20000,8);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        rt.map(0x10000,std::vector<std::uint8_t>(0x1020));rt.execute=bt2_iop_synthetic;
        rt.libraries.emplace("authored",IopRuntime::Library{0,1,{0x10300},0x18000});
        c=context(0);c.set(28,0x28000);c.set(31,0x12340);
        check(rt.import("authored",0,c)==17 && c.get(6)==0x18000 && c.get(28)==0x28000 && c.get(31)==0x12340 && c.pc==0x10000);
        IopRuntime libraries;libraries.map(0x30000,std::vector<std::uint8_t>(64));
        libraries.export_bounds[0x30000]=3;
        libraries.write(0x30000,0x41C00000,4);libraries.write(0x30008,0x101,2);
        for(unsigned n=0;n<4;++n)libraries.write(0x3000C+n,"test"[n],1);
        libraries.write(0x30014,0x30030,4);libraries.write(0x3001C,0x30034,4);
        install_iop_library_provider(libraries);IopContext caller;caller.set(4,0x30000);
        check(libraries.import("loadcore",6,caller)==0 && libraries.library_head==0x30000);
        check(libraries.libraries.at("test").slots==std::vector<std::uint32_t>({0x30030,0,0x30034}));
        check(libraries.read(0x30000,4)==0 && libraries.libraries.at("test").version==0x101);
        rejected=false;try{libraries.import("loadcore",6,caller);}catch(const std::runtime_error&){rejected=true;}check(rejected && libraries.libraries.size()==1);
        caller.set(4,0x30004);rejected=false;try{libraries.import("loadcore",6,caller);}catch(const std::runtime_error&){rejected=true;}check(rejected && libraries.libraries.size()==1);
        IopRuntime kernel;kernel.map(0x10000,std::vector<std::uint8_t>(0x1000));kernel.map(0x20000,std::vector<std::uint8_t>(0x100));
        install_iop_kernel_providers(kernel);IopContext host;host.set(4,0x20040);host.set(28,0x18000);
        kernel.write(0x20040,2,4);const auto event_id=kernel.import("thevent",4,host);
        kernel.write(0x20040,0x02000000,4);kernel.write(0x20048,0x101C0,4);kernel.write(0x2004C,0x800,4);kernel.write(0x20050,16,4);
        const auto thread_id=kernel.import("thbase",4,host);check(thread_id!=event_id);host.set(4,thread_id);host.set(5,event_id);kernel.execute=bt2_iop_synthetic;
        check(kernel.import("thbase",6,host)==0);auto& worker=kernel.threads.at(thread_id);
        check(worker.waiting && !worker.ready && worker.context.pc==0x10200 && worker.wait_bits==4 && worker.wait_mode==0x11);
        check(worker.context.get(28)==0x18000 && kernel.events.at(event_id).bits==0 && kernel.current_thread==0);
        const auto parked=worker.context;rejected=false;try{kernel.import("thbase",6,host);}catch(const std::runtime_error&){rejected=true;}check(rejected && worker.context.pc==parked.pc);
        kernel.events.at(event_id).bits=12;kernel.current_thread=thread_id;bt2_iop_synthetic(kernel,worker.context);kernel.current_thread=0;
        check(!worker.waiting && worker.context.pc==IopRuntime::sentinel && worker.context.get(3)==9 && kernel.read(0x20000,4)==12 && kernel.events.at(event_id).bits==0);
        kernel.write(0xBF808268,0x3BC,4);check(kernel.sio2_control_known && kernel.sio2_control==0x3BC && kernel.read(0xBF808268,4)==0x3BC);
        kernel.write(0xBF808240,0xFFC00505,4);check(kernel.read(0xBF808240,4)==0xFFC00505);
        rejected=false;try{kernel.write(0xBF808268,0x3BD,4);}catch(const std::runtime_error&){rejected=true;}check(rejected && kernel.sio2_control==0x3BC);
        host.set(4,0x20010);check(kernel.import("intrman",17,host)==0 && kernel.read(0x20010,4)==1 && !kernel.cpu_interrupts_enabled);
        host.set(4,1);check(kernel.import("intrman",18,host)==0 && kernel.cpu_interrupts_enabled);
        host.set(4,11);host.set(5,3);kernel.import("dmacman",33,host);kernel.import("dmacman",34,host);check(kernel.dma_channels.at(11).priority==3 && kernel.dma_channels.at(11).enabled);
        host.set(4,0x20080);host.set(5,0xA7);host.set(6,12);auto prior=kernel.ram;
        check(kernel.import("sysclib",14,host)==0x20080);
        for(unsigned n=0;n<kernel.ram.size();++n)check(kernel.ram[n]==((n>=0x20080 && n<0x2008C)?std::uint8_t{0xA7}:prior[n]));
        host.set(4,0x200F8);host.set(6,16);prior=kernel.ram;rejected=false;try{kernel.import("sysclib",14,host);}catch(const std::runtime_error&){rejected=true;}check(rejected && kernel.ram==prior);
        host.set(4,0x101C0);host.set(2,42);check(kernel.import("secrman",4,host)==42 && kernel.security_handlers.at(4)==0x101C0);
        check(kernel.import("modload",13,host)==42 && kernel.kelf_path_handler==0x101C0);
        host.set(4,0x101C1);rejected=false;try{kernel.import("secrman",4,host);}catch(const std::runtime_error&){rejected=true;}check(rejected && kernel.security_handlers.at(4)==0x101C0);
        host.set(4,0x20040);kernel.write(0x20040,1,4);kernel.write(0x20048,1,4);kernel.write(0x2004C,2,4);
        const auto semaphore=kernel.import("thsemap",4,host);check(semaphore!=event_id && semaphore!=thread_id && kernel.semaphores.at(semaphore).attr==1);
        host.set(4,semaphore);check(kernel.import("thsemap",8,host)==0 && kernel.semaphores.at(semaphore).count==0);
        rejected=false;try{kernel.import("thsemap",8,host);}catch(const std::runtime_error&){rejected=true;}check(rejected && !kernel.semaphores.at(semaphore).count);
        install_iop_vblank_provider(kernel);kernel.current_thread=thread_id;host.set(2,42);
        const auto registers=host.r;bool yielded=false;try{kernel.import("vblank",5,host);}catch(const IopYield&){yielded=true;}
        kernel.current_thread=0;check(yielded && host.r==registers && worker.waiting && worker.wait_event==kernel.vblank_event && worker.wait_bits==4 && worker.wait_mode==1 && !kernel.events.at(kernel.vblank_event).bits);
        worker.wait_mode=0x11;worker.wait_result=0x20000;
        worker.context={};worker.context.set(4,kernel.vblank_event);worker.context.set(5,4);worker.context.set(6,0x11);worker.context.set(7,0x20000);
        kernel.execute=[&](IopRuntime& m,IopContext& resume) {
            check(m.read(0x20000,4)==4 && !m.events.at(kernel.vblank_event).bits);
            check(m.import("thevent",10,resume)==0 && !worker.wait_satisfied && !worker.wait_event);
            resume.set(5,0x10);m.import("thevent",10,resume);
        };
        host.set(4,kernel.vblank_event);host.set(5,4);check(kernel.import("thevent",6,host)==0 && worker.waiting && worker.wait_bits==0x10 && kernel.current_thread==0);
        host.set(5,~4u);check(kernel.import("thevent",8,host)==0 && !kernel.events.at(kernel.vblank_event).bits);
        IopRuntime nested;install_iop_kernel_providers(nested);
        nested.events.emplace(0x10001,IopRuntime::Event{2,0});
        IopRuntime::Thread parent{},child{};parent.priority=104;
        child.priority=16;child.entry=0x1039C;child.waiting=true;child.wait_event=0x10001;
        child.wait_bits=1;child.wait_mode=0x11;child.wait_order=1;
        nested.threads.emplace(1,parent);nested.threads.emplace(2,child);
        nested.rpc_queues.emplace(0x536E0,IopRuntime::RpcQueue{1,{0x80000400}});
        nested.rpc_servers.emplace(0x80000400,IopRuntime::RpcServer{0x536F8,0x80000400,0x50324,0x532B0,0x536E0,0x59290});
        nested.current_thread=1;IopContext preempt;preempt.pc=0x10EA8;preempt.set(4,0x10001);preempt.set(5,1);
        const auto parent_registers=preempt.r;
        nested.execute=[](IopRuntime& m,IopContext&) {
            check(m.current_thread==2 && !m.events.at(0x10001).bits);
            auto& t=m.threads.at(2);t.waiting=true;t.ready=false;t.wait_satisfied=false;throw IopYield{};
        };
        check(nested.import("thevent",6,preempt)==0 && nested.current_thread==1 && preempt.r==parent_registers);
        nested.execute=[](IopRuntime&,IopContext&){throw std::runtime_error("synthetic child failure");};
        rejected=false;try{nested.import("thevent",6,preempt);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && nested.current_thread==1 && preempt.r==parent_registers);
        auto& nt=nested.threads.at(2);nt.waiting=true;nt.priority=104;nested.events.at(0x10001).bits=0;
        rejected=false;try{nested.import("thevent",6,preempt);}catch(const std::runtime_error&){rejected=true;}
        check(rejected && nested.current_thread==1 && !nested.events.at(0x10001).bits && nt.waiting);
        libraries.execute=[](IopRuntime&,IopContext& export_context) {check(export_context.pc==0x30030 && export_context.get(28)==0x8760);export_context.set(6,export_context.get(28));export_context.set(2,17);export_context.pc=IopRuntime::sentinel;};
        libraries.libraries.at("test").gp=0x8760;caller.pc=0x4560;caller.set(28,0x3450);caller.set(31,0x5670);caller.set(29,0x7890);
        check(libraries.import("test",0,caller)==17 && caller.pc==0x4560 && caller.get(28)==0x3450 && caller.get(31)==0x5670 && caller.get(29)==0x7890 && caller.get(6)==0x8760);
        rejected=false;try{libraries.import("test",1,caller);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        kernel.write(0x20080,0x12345678,4);host.set(4,0x20090);host.set(5,0x20080);host.set(6,4);
        check(kernel.import("sysclib",12,host)==0x20090 && kernel.read(0x20090,4)==0x12345678);
        prior=kernel.ram;host.set(4,0x20082);rejected=false;try{kernel.import("sysclib",12,host);}catch(const std::runtime_error&){rejected=true;}check(rejected && prior==kernel.ram);
        IopRuntime io;io.map(0x10000,std::vector<std::uint8_t>(0x1000));io.map(0x20000,std::vector<std::uint8_t>(128));
        io.execute=bt2_iop_synthetic;install_iop_ioman_provider(io);IopContext request;request.pc=0x10044;request.set(4,0x20060);request.set(28,0x18000);request.set(29,0x20080);
        io.write(0x20060,'m',1);io.write(0x20061,'c',1);io.write(0x20068,'d',1);
        check(io.import("ioman",21,request)==UINT32_MAX && io.steps==0);
        io.write(0x20000,0x20060,4);io.write(0x2000C,0x20068,4);io.write(0x20010,0x20040,4);
        io.write(0x20040,0x10240,4);io.write(0x20044,0x10260,4);request.set(4,0x20000);
        const auto saved=request;check(io.import("ioman",20,request)==0 && io.drivers[0].name=="mc" && io.read(0x20018,4)==0);
        check(request.r==saved.r && request.pc==saved.pc && !io.driver_callback_active && io.entries.contains(0x10240));
        request.set(4,0x20060);check(io.import("ioman",21,request)==0 && !io.drivers[0].descriptor && io.read(0x2001C,4)==7);
        request.set(4,0x20000);io.write(0x20040,0x10280,4);check(io.import("ioman",20,request)==UINT32_MAX && !io.drivers[0].descriptor);
        io.write(0x20040,0x10241,4);rejected=false;try{io.import("ioman",20,request);}catch(const std::runtime_error&){rejected=true;}check(rejected && !io.drivers[0].descriptor && !io.driver_callback_active);
        io.write(0x20040,0x10240,4);
        for(unsigned n=0;n<16;++n)check(io.import("ioman",20,request)==0); // SDK permits duplicate names; preserve first-match deletion.
        const auto full_steps=io.steps;check(io.import("ioman",20,request)==UINT32_MAX && io.steps==full_steps);
        request.set(4,0x20060);check(io.import("ioman",21,request)==0 && !io.drivers[0].descriptor && io.drivers[1].descriptor);
        request.set(4,0x20060);request.set(5,0x20068);request.set(6,0);install_iop_kernel_providers(io);
        check(io.import("sysclib",29,request)==0);request.set(6,2);check(iop_signed(io.import("sysclib",29,request))>0);
        request.set(5,0x20060);check(io.import("sysclib",29,request)==0);
        IopRuntime device;device.map(0x10000,std::vector<std::uint8_t>(0x100));
        IopRuntime::Thread dt{};dt.context.set(29,0x100F0);device.threads.emplace(1,dt);device.current_thread=1;
        device.interrupts.emplace(17,IopRuntime::Interrupt{1,0x10000,0x10080,true,0x18000});
        unsigned handled=0;device.execute=[&](IopRuntime& m,IopContext& irq) {
            check(m.in_interrupt && m.read(0xBF808280,4)==1 && irq.get(4)==0x10080 && irq.get(28)==0x18000);
            m.write(0xBF808280,1,4);irq.pc=IopRuntime::sentinel;++handled;
        };
        for(unsigned port=0;port<2;++port) {
            device.write(0xBF808200,0x140540|port,4);device.write(0xBF808240+port*8,0xFFC00505,4);device.write(0xBF808244+port*8,0x20014,4);
            for(unsigned byte:{1u,0x42u,0u,0u,0u})device.write(0xBF808260,byte,1);
            device.write(0xBF808268,0x3BD,4);check(!device.in_interrupt && !device.sio2_irq_status);
            check(device.read(0xBF80826C,4)==(port?0x2D100u:0x1100u));
            const auto expected=port?std::vector<unsigned>(5,255):std::vector<unsigned>{255,65,90,255,255};
            for(auto byte:expected)check(device.read(0xBF808264,1)==byte);
            rejected=false;try{device.read(0xBF808264,1);}catch(const std::runtime_error&){rejected=true;}check(rejected);
        }
        check(handled==2 && device.threads.at(1).context.get(29)==0x100F0);
        host.set(4,thread_id);host.set(5,0x20040);check(kernel.import("thbase",22,host)==0 && kernel.read(0x20048,4)==4 && kernel.read(0x2004C,4)==worker.entry && kernel.read(0x20064,4)==4);
        IopRuntime sound;
        auto refuses=[&](auto operation){bool failed=false;try{operation();}catch(const std::runtime_error&){failed=true;}check(failed);};
        refuses([&]{sound.read(0xBF8010F0,4);});
        sound.write(0xBF801404,0xBF900000,4);sound.write(0xBF80140C,0xBF900800,4);
        refuses([&]{sound.read(0xBF8010F0,4);});
        for(unsigned channel:{11u,12u}) {auto& dma=sound.dma_channels[channel];dma.priority=3;dma.enabled=true;}
        check(sound.read(0xBF8010F0,4)==0x0777FF77 && sound.read(0xBF801570,4)==0x07BBFF77);
        refuses([&]{sound.write(0xBF8010F0,0,4);});
        sound.write(0xBF8010F0,0x077FFF77,4);sound.write(0xBF801570,0x07BBFF7F,4);
        check(sound.read(0xBF8010F0,4)==0x077FFF77 && sound.read(0xBF801570,4)==0x07BBFF7F);
        refuses([&]{sound.read(0xBF8010F0,2);});
        refuses([&]{sound.read(0xBF900344,2);});
        refuses([&]{sound.write(0xBF9007C6,0x901,2);});
        refuses([&]{sound.write(0xBF9001A4,1,2);});
        for(const auto [address,value]:std::map<unsigned,unsigned>{{0x7C0,0x8000},{0x7C6,0x900},{0x7C8,0x200},{0x7CA,8},
            {0x760,0},{0x762,0},{0x788,0},{0x78A,0},{0x1B0,0},{0x5B0,0},{0x19A,0x8000},{0x59A,0x8000}})sound.write(0xBF900000+address,value,2);
        check(sound.read(0xBF900344,2)==0 && sound.read(0xBF900744,2)==0 && sound.spu2_reset_status_reads==2);
        sound.write(0xBF9001A4,1,2);sound.write(0xBF9001A6,2,2);check(sound.spu2_pending_key_off[0]==0x20001);
        sound.write(0xBF9001A8,0,2);sound.write(0xBF9001AA,0x2800,2);
        refuses([&]{sound.read(0xBF900344,2);});refuses([&]{sound.write(0xBF90019A,0x8010,2);});
        sound.enabled_interrupts.insert(36);refuses([&]{sound.write(0xBF9001AC,1,2);});sound.enabled_interrupts.erase(36);
        for(unsigned i=0;i<8;++i)sound.write(0xBF9001AC,0x1234+i,2);
        check(sound.spu2_fifo_halfwords==8 && sound.spu2_configuration.at(0x1F9001AA)==0x2808);
        for(unsigned i=0;i<8;++i)check(sound.spu2_sound_ram.at(0x2800+i)==0x1234+i);
        refuses([&]{sound.write(0xBF9001AC,0,2);});refuses([&]{sound.write(0xBF9001AC,0,4);});
        sound.write(0xBF90019A,0x8010,2);check(sound.read(0xBF900344,2)==0x80);
        refuses([&]{sound.read(0xBF900344,2);});sound.write(0xBF90019A,0x8000,2);check(sound.spu2_io_transfer_completed);
        refuses([&]{sound.write(0xBF9001A0,1,2);});
        for(unsigned voice=0;voice<24;++voice)for(unsigned side=0;side<2;++side)sound.write(0xBF900000+voice*16+side*2,0,2);
        sound.write(0xBF9001A0,1,2);check(sound.read(0xBF9001A0,2)==1);refuses([&]{sound.read(0xBF9001A0,2);});
        sound.write(0xBF9001A0,2,2);sound.steps+=2;refuses([&]{sound.read(0xBF9001A0,2);});
        sound.write(0xBF900342,0,2);check(sound.read(0xBF900342,2)==0);refuses([&]{sound.read(0xBF900342,2);});
        refuses([&]{sound.read(0xBF90000A,2);});
        install_iop_kernel_providers(sound);sound.map(0x20000,std::vector<std::uint8_t>(16,0xAA));IopContext irq;
        irq.set(4,36);irq.set(5,0x20000);check(sound.import("intrman",7,irq)==static_cast<unsigned>(-103) && sound.read(0x20000,4)==static_cast<unsigned>(-103));
        check(sound.import("intrman",5,irq)==static_cast<unsigned>(-105));
        sound.interrupts.emplace(36,IopRuntime::Interrupt{1,0x20000,0,true,0});
        check(sound.import("intrman",7,irq)==0 && sound.read(0x20000,4)==36 && !sound.interrupts.at(36).enabled);
        check(sound.import("intrman",5,irq)==0 && !sound.interrupts.contains(36));
        irq.set(4,8);refuses([&]{sound.import("intrman",7,irq);});
        irq.set(4,0x20004);irq.set(5,8);irq.set(2,0xBAD);check(sound.import("sysclib",17,irq)==0xBAD);
        check(sound.read(0x20004,4)==0 && sound.read(0x20008,4)==0 && sound.read(0x2000C,4)==0xAAAAAAAA);
        irq.set(4,0x2000C);refuses([&]{sound.import("sysclib",17,irq);});
        irq.set(4,40);check(sound.import("intrman",6,irq)==0 && sound.enabled_interrupts.contains(40) && !sound.interrupts.contains(40));
        irq.set(5,1);irq.set(6,0x20000);irq.set(7,0);check(sound.import("intrman",4,irq)==0 && sound.interrupts.at(40).enabled);
        irq.set(5,0x20000);check(sound.import("intrman",7,irq)==0 && !sound.enabled_interrupts.contains(40));
        sound.write(0x20004,0x01000100,4);sound.write(0x20008,0x02000100,4);
        irq.set(4,0x20004);irq.set(5,0x20008);irq.set(6,4);check(iop_signed(sound.import("sysclib",11,irq))<0);
        irq.set(6,3);check(sound.import("sysclib",11,irq)==0);irq.set(4,0);irq.set(5,0);irq.set(6,0);check(sound.import("sysclib",11,irq)==0);
        irq.set(4,0x2000C);irq.set(6,8);refuses([&]{sound.import("sysclib",11,irq);});
        sound.abi_read_only_words={{0,0},{4,0},{8,0}};check(sound.read(0,4)==0 && sound.read(8,4)==0);
        refuses([&]{sound.write(0,0,4);});refuses([&]{sound.read(12,4);});refuses([&]{sound.read(0,1);});
        IopRuntime scheduling;install_iop_kernel_providers(scheduling);
        scheduling.threads.emplace(1,IopRuntime::Thread{0,0,0x100,0x1000,0x100,39,0x800,{}});
        scheduling.threads.emplace(2,IopRuntime::Thread{0,0,0x200,0x2000,0x100,48,0x900,{}});
        std::vector<unsigned> order;scheduling.execute=[&](IopRuntime& m,IopContext& context) {
            if(m.current_thread==1) {
                order.push_back(1);IopContext start;start.set(4,2);start.set(5,0xAABB);
                check(m.import("thbase",6,start)==0 && order.size()==1 && m.current_thread==1 && m.threads.at(2).ready);
                order.push_back(3);check(context.get(28)==0x800 && context.get(29)==0x10E0);
            } else {order.push_back(2);check(context.get(4)==0xAABB && context.get(28)==0x900 && context.get(29)==0x20E0);}
            auto& thread=m.threads.at(m.current_thread);thread.waiting=true;thread.ready=false;throw IopYield{};
        };
        IopContext start;start.set(4,1);check(scheduling.import("thbase",6,start)==0 && order==std::vector<unsigned>{1,3,2} && !scheduling.current_thread);
        IopRuntime card;install_iop_kernel_providers(card);
        card.map(0x1000,std::vector<std::uint8_t>(32));card.map(0x32020,std::vector<std::uint8_t>(144));
        card.map(0x32650,std::vector<std::uint8_t>(144,0xA5));
        card.threads[1].entry=0x1039C;card.threads[1].priority=16;
        card.rpc_servers[0x80000400]={0,0x80000400,0,0,0x800,0};card.rpc_queues[0x800]={1,{}};card.current_thread=1;
        for(auto ch:{11u,12u})card.dma_channels[ch]={3,true,true,true};
        card.sio2_parameters={{0x1F808200,0x100472},{0x1F808204,0},{0x1F808250,0xFF020405},{0x1F808254,0x5FFFF}};
        card.ram[0x32020]=0x81;card.ram[0x32021]=0x11;
        IopContext slice;slice.pc=0x10E28;slice.set(4,11);slice.set(5,0x32020);slice.set(6,36);slice.set(7,1);slice.set(29,0x1000);card.write(0x1010,1,4);
        card.current_thread=0;refuses([&]{card.import("dmacman",28,slice);});check(!card.dma_channels.at(11).slice_known);card.current_thread=1;
        slice.set(5,0x32024);refuses([&]{card.import("dmacman",28,slice);});check(!card.dma_channels.at(11).slice_known);slice.set(5,0x32020);
        check(card.import("dmacman",28,slice)==1);IopContext dma_start;dma_start.pc=0x10E30;dma_start.set(4,11);dma_start.set(2,0xBAD);
        card.sio2_card_absent=false;refuses([&]{card.import("dmacman",32,dma_start);});check(card.sio2_transmit.empty() && card.dma_channels.at(11).madr==0x32020);card.sio2_card_absent=true;
        card.ram[0x32021]=0x12;refuses([&]{card.import("dmacman",32,dma_start);});check(card.sio2_transmit.empty());card.ram[0x32021]=0x11;
        check(card.import("dmacman",32,dma_start)==0xBAD && card.sio2_card_sends==1 && card.dma_channels.at(11).madr==0x320B0);
        refuses([&]{card.import("dmacman",32,dma_start);});check(card.sio2_card_sends==1);
        slice.set(4,12);slice.set(5,0x32650);card.write(0x1010,0,4);check(card.import("dmacman",28,slice)==1);
        dma_start.set(4,12);check(card.import("dmacman",32,dma_start)==0xBAD && card.sio2_card_receives==1);
        check(std::all_of(card.ram.begin()+0x32650,card.ram.begin()+0x32656,[](auto v){return v==0xFF;}));
        check(std::all_of(card.ram.begin()+0x32656,card.ram.begin()+0x326E0,[](auto v){return v==0;}));
        refuses([&]{card.import("dmacman",32,dma_start);});check(card.sio2_card_receives==1);
        { IopRuntime timer;IopContext caller;caller.pc=0x1110;caller.set(29,0x1800);caller.load_register=7;caller.load_value=99;
        timer.threads.emplace(1,IopRuntime::Thread{0,0,0x1110,0,0,104,0,caller});
        timer.threads.emplace(2,IopRuntime::Thread{0,0,0x2220,0,0,24,0,{}});
        timer.threads.emplace(3,IopRuntime::Thread{0,0,0x3330,0,0,16,0,{}});
        timer.threads.at(2).ready=timer.threads.at(3).ready=true;timer.current_thread=1;
        const auto retained=caller;std::vector<unsigned> dispatch_order;
        timer.execute=[&](IopRuntime& rt,IopContext& cpu) {
            dispatch_order.push_back(rt.current_thread);cpu.set(2,rt.current_thread);
            auto& thread=rt.threads.at(rt.current_thread);thread.ready=false;thread.waiting=true;throw IopYield{};
        };
        check(timer.delayCurrent(caller,300)==0 && caller==retained && dispatch_order==std::vector<unsigned>{3,2}
            && timer.logical_time_us==300 && timer.current_thread==1 && !timer.timed_wait_owner);
        timer.cpu_interrupts_enabled=false;refuses([&]{timer.delayCurrent(caller,300);});check(timer.logical_time_us==300);
        timer.cpu_interrupts_enabled=true;timer.threads.at(2).ready=true;timer.threads.at(2).waiting=false;
        timer.execute=[](IopRuntime&,IopContext&){throw std::runtime_error("worker failure");};
        refuses([&]{timer.delayCurrent(caller,300);});check(timer.current_thread==1 && timer.timed_wait_owner==1 && caller==retained);
        refuses([&]{timer.delayCurrent(caller,300);});check(timer.logical_time_us==300);
        }
        IopRuntime logging;logging.map(0x20000,std::vector<std::uint8_t>{'h','i',0,'%','d',0});install_iop_kernel_providers(logging);
        IopContext log;log.set(4,0x20000);check(logging.import("sysmem",14,log)==0 && logging.kernel_messages==std::vector<std::string>{"hi"});
        log.set(4,0x20003);refuses([&]{logging.import("sysmem",14,log);});check(logging.kernel_messages.size()==1);
        IopRuntime disc;install_iop_cdvd_provider(disc);IopContext query;query.set(4,0xFFFFFFF5);query.set(5,0xFFFFFFFF);
        refuses([&]{disc.import("cdvdman",50,query);});check(disc.events.empty());
        disc.sif_transport_attached=true;disc.rpc_initialized=true;
        const auto cd_event=disc.import("cdvdman",50,query);
        check(cd_event==disc.cdvd_interrupt_event && disc.events.size()==1 && disc.events.at(cd_event).attr==2
            && disc.events.at(cd_event).option==2 && disc.events.at(cd_event).bits==0x29);
        check(disc.import("cdvdman",50,query)==cd_event && disc.events.size()==1);
        query.set(4,0xFFFFFFFF);refuses([&]{disc.import("cdvdman",50,query);});check(disc.events.size()==1);
        IopRuntime print;print.map(0x20000,std::vector<std::uint8_t>{'%','s',' ','%','d',' ','%','%',0,'o','k',0,'%','x',0});install_iop_kernel_providers(print);
        IopContext message;message.set(4,0x20000);message.set(5,0x20009);message.set(6,static_cast<std::uint32_t>(-39));
        check(print.import("stdio",4,message)==8 && print.stdio_messages==std::vector<std::string>{"ok -39 %"});
        message.set(4,0x2000C);refuses([&]{print.import("stdio",4,message);});check(print.stdio_messages.size()==1);
        install_iop_library_provider(print);message.set(2,0xBAD);check(print.import("loadcore",5,message)==0xBAD && print.data_cache_flushes==1);
        std::cout<<"Native MIPS-I, timed priority/context retention and fail-closed imports verified.\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
