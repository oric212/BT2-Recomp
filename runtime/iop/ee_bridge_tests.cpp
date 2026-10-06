#include "ps2_runtime.h"
#include "module_service.h"
#include <iostream>
void check(bool value){if(!value)throw std::runtime_error("EE/IOP bridge test differs");}
namespace {
int parked_guard_destructions=0;
void syntheticSleep(std::uint8_t* memory,R5900Context* c,PS2Runtime* rt) {
    struct Guard {~Guard(){++parked_guard_destructions;}} guard;
    c->pc=0x32EB88;c->r[3].low=0x32;c->r[29].low=0x3A6C80;c->r[31].low=0x333098;
    c->r[28].low=0x38BDF0;c->r[16].low=0x3A6D00;c->cop0_status=0x10001;
    rt->handleSyscall(memory,c,0);throw std::runtime_error("Fixture sleep returned");
}
void syntheticSound(std::uint8_t* memory,R5900Context* c,PS2Runtime* rt) {
    // Authored fixture for stack ownership; actual generated registration is compared separately.
    for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,6>{{{0x3A6D00,3},{0x3A6D08,0x3A6D18},{0x3A6D18,0x80000704},{0x3A6D1C,0x3153F0},{0x3A6D20,0x3A6CC0},{0x3A6C90,0x100250}}})
        rt->write32(memory,address,value);
    rt->dispatchGuestBranch(memory,c,0xABC100,0x315380,0x333098,PS2Runtime::GuestBranchKind::DirectCall,"synthetic sleep");
}
void syntheticReply(std::uint8_t* memory,R5900Context* c,PS2Runtime* rt) {
    rt->write32(memory,0x6351C0,0);rt->write32(memory,0x6351D4,0x2000);rt->write32(memory,0x6351E4,0x1000);
    rt->write32(memory,0x3A97D0,4);rt->write32(memory,0x3A97D8,0);rt->semaphores[7].count=1;
    c->pc=static_cast<unsigned>(c->r[31].low);
}
void syntheticMainWait(std::uint8_t* memory,R5900Context* c,PS2Runtime* rt) {
    c->pc=0x32ECA8;c->r[3].low=0x44;c->r[4].low=7;c->r[31].low=0x332874;
    const auto before=*c;rt->handleSyscall(memory,c,0);
    auto expected=before;expected.r[2].low=7;
    check(!std::memcmp(c,&expected,sizeof(expected)) && !rt->semaphores[7].count);
    check(!parked_guard_destructions && rt->sound_thread_waiting && rt->current_thread_id==1);
    c->pc=0x332874;
}
}
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
        {
            PS2Runtime queued;queued.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
            queued.ee_queued_sound_rpc=true;queued.current_thread_id=1;queued.main_thread_priority=2;queued.iop_epoch=1;
            R5900Context submit;submit.pc=0x32F028;submit.r[2].low=0xABCD;
            std::array<unsigned,16> reply{};reply[7]=0x6351C0;reply[8]=0x80000009;
            queued.queueSoundsBind(&submit,reply);
            check(queued.queued_sounds_bind.pending && queued.queued_sound_submissions==1
                && !queued.queued_sound_completions && !queued.sounds_rpc_binds && submit.r[2].low==0x60003);
            bool duplicate=false;try{queued.queueSoundsBind(&submit,reply);}catch(const UnsupportedSyscall&){duplicate=true;}
            check(duplicate && queued.queued_sound_submissions==1);
            bool wrong_wait=false;try{queued.finishQueuedSoundsBind(queued.memory().data(),&submit);}catch(const UnsupportedSyscall&){wrong_wait=true;}
            check(wrong_wait && queued.queued_sounds_bind.pending && !queued.queued_sound_completions);
            queued.iop_epoch=2;submit.pc=0x32ECA8;submit.r[4].low=7;submit.r[31].low=0x332874;
            bool stale=false;try{queued.finishQueuedSoundsBind(queued.memory().data(),&submit);}catch(const UnsupportedSyscall&){stale=true;}
            check(stale && queued.queued_sounds_bind.pending && !queued.queued_sound_completions);
        }
        {
            PS2Runtime owned;owned.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
            owned.ee_queued_sound_rpc=true;owned.current_thread_id=1;owned.main_thread_priority=2;owned.iop_epoch=1;
            owned.semaphores.resize(8);owned.semaphores[7].maximum=1;owned.semaphores[7].option=0x3838D8;
            owned.threads.resize(2);owned.threads[1].id=3;owned.threads[1].ready=true;owned.threads[1].entry=0x315380;owned.threads[1].context.cop0_status=0x10001;
            owned.native_iop=std::make_shared<NativeIopService>(1,bt2_compiled_iop_modules());
            owned.registerFunction(0x315380,syntheticSound);owned.registerFunction(0xABC100,syntheticSleep);
            owned.registerFunction(0x3322E8,syntheticReply);owned.registerFunction(0xABC000,syntheticMainWait);
            for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,3>{{{0x3A97D8,29},{0x3A97DC,0x6351C0},{0x3A97E0,0x02000004}}})owned.write32(owned.memory().data(),address,value);
            R5900Context parent;parent.r[29].low=0x1FFEFB0;
            std::array<unsigned,16> reply{};reply[7]=0x6351C0;reply[8]=0x80000009;reply[9]=0x1000;reply[10]=0x2000;
            owned.queueSoundsBind(&parent,reply);
            owned.dispatchGuestBranch(owned.memory().data(),&parent,0xABC000,0xABC200,0x332874,PS2Runtime::GuestBranchKind::DirectCall,"synthetic main wait");
            check(owned.queued_sound_completions==1 && !owned.queued_sounds_bind.pending && owned.current_thread_id==1 && !owned.active_ee_stack);
        }
        check(parked_guard_destructions==1); // Destructor cancels with the child's return ownership selected.
        std::cout<<"Malformed/stale requests and failed native loads leave EE packet, result and semaphore pending.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
