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
        constexpr char path[]="cdrom0:\\IRX\\SIO2D.IRX;1";std::memcpy(rt.memory().data()+0x3ACA08,path,sizeof(path));
        rt.write32(rt.memory().data(),0x3ACA00,1);stop();check(!rt.native_iop); // unsupported arguments
        rt.write32(rt.memory().data(),0x3ACA00,0);rt.prior_iop_services_valid=true;stop();check(!rt.native_iop);rt.prior_iop_services_valid=false;rt.iop_epoch=2;stop();check(!rt.native_iop);rt.iop_epoch=1;
        rt.native_iop=std::make_shared<NativeIopService>(1,bt2_compiled_iop_modules());
        rt.native_iop->load(1,"cdrom0:\\IRX\\SIO2MAN.IRX;1");rt.native_iop->load(1,"cdrom0:\\IRX\\MCMAN.IRX;1");rt.native_iop->attachEeTransport(1);rt.native_iop->load(1,"cdrom0:\\IRX\\MCSERV.IRX;1");rt.native_iop->load(1,"cdrom0:\\IRX\\DBCMAN.IRX;1");rt.native_module_loads=4;
        const auto prior=std::vector<std::uint8_t>(rt.memory().begin(),rt.memory().end());stop();
        check(rt.native_module_loads==4 && rt.native_iop->residents.size()==4 && rt.native_iop_error=="Unresolved IOP import: vblank:5");
        check(rt.semaphores[7].count==0 && c.r[2].low==0xABCD && std::equal(prior.begin(),prior.end(),rt.memory().begin()));
        std::cout<<"Malformed/stale requests and failed native loads leave EE packet, result and semaphore pending.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
