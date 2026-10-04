#include "native_runtime.h"
#include "library_provider.h"
#include "kernel_provider.h"
#include "ioman_provider.h"
#include <iostream>
void bt2_iop_synthetic(IopRuntime&,IopContext&);
void check(bool v) {if(!v)throw std::runtime_error("IOP synthetic result differs");}
int main() {
    try {
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
        kernel.write(0xBF808268,0x3BC,4);check(kernel.sio2_control_known && kernel.sio2_control==0x3BC);
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
        std::cout<<"Native MIPS-I branches, load delays, HI/LO, unaligned pairs and import failure verified.\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
