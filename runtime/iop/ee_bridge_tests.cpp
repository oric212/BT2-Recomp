#include "ps2_runtime.h"
#include "module_service.h"
#include <iostream>
void check(bool value){if(!value)throw std::runtime_error("EE/IOP bridge test differs");}
int main() {
    try {
        PS2Runtime rt;rt.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        rt.prior_iop_services_valid=false;rt.initial_sp=0x1FFF000;rt.iop_epoch=1;rt.post_loadfile.supplied=true;rt.post_loadfile.binds=1;rt.post_loadfile.versions=1;
        rt.post_cd.modes=1;rt.post_sif.rpc_initialized=true;rt.semaphores.resize(8);rt.semaphores[7].maximum=1;rt.semaphores[7].option=0x3838E8;
        R5900Context c;c.pc=0x32F028;c.r[3].low=0x77;c.r[4].low=rt.initial_sp-528;c.r[29].low=c.r[4].low;c.r[5].low=2;c.r[2].low=0xABCD;
        auto stop=[&]{bool rejected=false;try{rt.handleSyscall(rt.memory().data(),&c,0);}catch(const UnsupportedSyscall&){rejected=true;}check(rejected);};
        stop();check(!rt.native_iop && !rt.native_module_loads); // malformed descriptor
        const std::array<std::uint32_t,8> d={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
        for(unsigned n=0;n<8;++n)rt.write32(rt.memory().data(),static_cast<std::uint32_t>(c.r[4].low)+n*4,d[n]);
        const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,10,0x3ACC00,0,512,0x3ACA00,8,1,0x22568};
        for(unsigned n=0;n<14;++n)rt.write32(rt.memory().data(),0x3A97C0+n*4,request[n]);
        rt.write32(rt.memory().data(),0x3ACC00,0x203A97C0);rt.write32(rt.memory().data(),0x3ACC04,10);rt.write32(rt.memory().data(),0x3ACC08,7);
        constexpr char path[]="cdrom0:\\IRX\\DS2U_D.IRX;1";std::memcpy(rt.memory().data()+0x3ACA08,path,sizeof(path));
        rt.write32(rt.memory().data(),0x3ACA00,1);stop();check(!rt.native_iop); // unsupported arguments
        rt.write32(rt.memory().data(),0x3ACA00,0);rt.prior_iop_services_valid=true;stop();check(!rt.native_iop);rt.prior_iop_services_valid=false;rt.iop_epoch=2;stop();check(!rt.native_iop);rt.iop_epoch=1;
        rt.native_iop=std::make_shared<NativeIopService>(1,bt2_compiled_iop_modules());
        rt.native_iop->load(1,"cdrom0:\\IRX\\SIO2MAN.IRX;1");rt.native_iop->load(1,"cdrom0:\\IRX\\MCMAN.IRX;1");rt.native_iop->attachEeTransport(1);rt.native_iop->load(1,"cdrom0:\\IRX\\MCSERV.IRX;1");rt.native_iop->load(1,"cdrom0:\\IRX\\DBCMAN.IRX;1");rt.native_iop->load(1,"cdrom0:\\IRX\\SIO2D.IRX;1");rt.native_module_loads=5;
        rt.native_iop->runtime.providers.erase({"thbase",22});
        const auto prior=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());stop();
        check(rt.native_module_loads==5 && rt.native_iop->residents.size()==5 && rt.native_iop_error=="Unresolved IOP import: thbase:22");
        check(rt.semaphores[7].count==0 && c.r[2].low==0xABCD && std::equal(prior.begin(),prior.end(),rt.memory().begin()));
        const std::array<std::uint32_t,16> heap_reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0};
        rt.supplyHeapBind(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(heap_reply.data()),sizeof(heap_reply)));
        rt.native_module_loads=13;rt.semaphores[7].option=0x3838D8;c.r[5].low=1;c.r[2].low=0xABCD;
        rt.write32(rt.memory().data(),0x3A97E0,0x80000003);
        const auto heap_before=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());
        stop(); // Module-load descriptor must not be accepted as a heap bind.
        check(!rt.heap_binds && !rt.semaphores[7].count && c.r[2].low==0xABCD && std::equal(heap_before.begin(),heap_before.end(),rt.memory().begin()));
        rt.heap_copy_supplied=true;rt.heap_binds=1;rt.heap_calls=1;c.r[4].low=rt.initial_sp-144;c.r[29].low=c.r[4].low;
        rt.write32(rt.memory().data(),static_cast<std::uint32_t>(c.r[4].low),0x369F90);
        rt.write32(rt.memory().data(),static_cast<std::uint32_t>(c.r[4].low)+4,0xCE500);
        rt.write32(rt.memory().data(),static_cast<std::uint32_t>(c.r[4].low)+8,0x361);
        rt.write32(rt.memory().data(),static_cast<std::uint32_t>(c.r[4].low)+12,0);
        const auto copy_before=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());stop();
        check(!rt.heap_transfers && c.r[2].low==0xABCD && std::equal(copy_before.begin(),copy_before.end(),rt.memory().begin()));
        rt.heap_transfers=1;c.r[3].low=0x76;c.pc=0x32F008;c.r[4].low=0x50002;stop();check(!rt.heap_dma_polls);
        c.r[4].low=0x50001;rt.iop_epoch=2;stop();check(!rt.heap_dma_polls);rt.iop_epoch=1;rt.handleSyscall(rt.memory().data(),&c,0);
        check(rt.heap_dma_polls==1 && c.r[2].low==UINT64_MAX);stop();check(rt.heap_dma_polls==1);
        c.r[3].low=0x77;c.pc=0x32F028;c.r[5].low=2;c.r[4].low=rt.initial_sp-640;c.r[29].low=c.r[4].low;
        rt.write32(rt.memory().data(),0x3A97E0,6);rt.semaphores[7].option=0x3838E8;
        const auto buffer_before=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());stop();
        check(rt.native_module_loads==13 && std::equal(buffer_before.begin(),buffer_before.end(),rt.memory().begin()));
        rt.native_module_loads=14;c.r[4].low=rt.initial_sp-544;c.r[29].low=c.r[4].low;
        rt.write32(rt.memory().data(),0x3A97DC,0x3AC840);rt.write32(rt.memory().data(),0x3A97E0,2);
        const auto free_before=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());stop();
        check(!rt.heap_frees && !rt.native_iop->boot_heap_frees && std::equal(free_before.begin(),free_before.end(),rt.memory().begin()));
        std::cout<<"Malformed/stale requests and failed native loads leave EE packet, result and semaphore pending.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
