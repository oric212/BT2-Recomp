#include "module_service.h"

// Sound services: the sound-driver RPC (SDRDRV) and SOUNDS commands, the scoped SPU2 voice/sleep
// providers they run under, the IOP sound timer, and the EE sound server bind.

std::vector<std::uint8_t> NativeIopService::callSoundRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t service) {
    const bool sounds=service==0x02000004;auto& requests=sounds?sounds_rpc_requests:sound_rpc_requests;
    const auto client=sounds?0x6351C0u:0x3A5C80u;const auto result=sounds?0x635180u:0x3A5C40u;
    if((service!=0x80000701 && !sounds) || epoch!=epoch_ || failed_ || requests || command!=(sounds?16u:0xE620u)
        || input.size()!=(sounds?256u:64u) || result_size!=(sounds?64u:16u) || (sounds && sound_rpc_requests!=1) || residents.size()!=13 || boot_heap_allocations!=2)
        throw std::runtime_error("Unsupported registered RPC native request");
    auto response=executeRegisteredRpc(service,command,input,result_size,client,result);++requests;return response;
}

namespace {
// Later sound commands: owned SPU2 voice transfers and scoped sleep/wakeup providers, restored after.
struct SoundScope {
    IopRuntime& rt;std::map<std::pair<std::string,unsigned>,IopRuntime::Provider> saved;
    static constexpr std::array<std::pair<const char*,unsigned>,7> keys{{{"thbase",24},{"thbase",25},{"thbase",26},
        {"timrman",4},{"timrman",20},{"timrman",22},{"timrman",23}}};
    explicit SoundScope(IopRuntime& runtime):rt(runtime) {
        for(const auto& [library,ordinal]:keys) {const auto found=rt.providers.find({library,ordinal});if(found!=rt.providers.end())saved.insert(*found);}
        install_iop_sound_scope_sleep(rt);rt.spu2_voice.enabled=true;
    }
    ~SoundScope() {
        rt.spu2_voice.enabled=false;
        for(const auto& [library,ordinal]:keys)rt.providers.erase({library,ordinal});
        for(const auto& [key,provider]:saved)rt.providers[key]=provider;
    }
};
}

std::vector<std::uint8_t> NativeIopService::callSoundCommand(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input) {
    if(epoch!=epoch_ || failed_ || sound_rpc_requests!=1 || input.size()!=64 || residents.size()!=13)
        throw std::runtime_error("Unsupported later sound command");
    SoundScope scope(runtime);
    try {auto response=executeRegisteredRpc(0x80000701,command,input,16,0x3A5C80,0x3A5C40);++sound_command_requests;return response;}
    catch(const std::exception& e){failed_=true;throw std::runtime_error(std::string(e.what())+" / command="+std::to_string(command)+" / pc="+std::to_string(runtime.executing_pc)+" / address="+std::to_string(runtime.blocked_address));}
}

void NativeIopService::advanceSoundTimer(unsigned epoch) {
    // One NTSC field of IOP system clock (36.864 MHz x 1001/60 ms = 615,014.4 clocks, kept in tenths).
    auto& t=runtime.sound_timer;
    if(epoch!=epoch_ || failed_ || !t.started)return;
    SoundScope scope(runtime);
    try {runtime.advanceSoundTimerField();} catch(...) {failed_=true;throw;}
}

std::vector<std::uint8_t> NativeIopService::callSoundsCommand(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input) {
    // Later SOUNDS service commands from main (256-byte request, 64-byte reply): original handler.
    if(epoch!=epoch_ || failed_ || sounds_rpc_requests!=1 || input.size()!=256 || residents.size()!=13)
        throw std::runtime_error("Unsupported later SOUNDS command");
    SoundScope scope(runtime);
    try {auto response=executeRegisteredRpc(0x02000004,command,input,64,0x6351C0,0x635180);++sound_command_requests;return response;}
    catch(const std::exception& e){failed_=true;throw std::runtime_error(std::string(e.what())+" / command="+std::to_string(command)+" / pc="+std::to_string(runtime.executing_pc)+" / address="+std::to_string(runtime.blocked_address));}
}

void NativeIopService::completeEeSoundBind(unsigned epoch,std::span<const std::uint8_t> ee) {
    auto& rt=runtime;const auto pending=rt.pending_ee_bind;
    if(epoch!=epoch_ || failed_ || ee_sound_bind_returns || ee.size()!=32u*1024*1024
        || rt.current_thread || rt.timed_wait_owner || pending.client!=0xA1CA0 || pending.server_id!=0x80000704
        || pending.packet!=0x1FF400 || pending.rpc_id!=2 || !rt.threads.contains(pending.thread))
        throw std::runtime_error("Reverse bind lacks owned pending request");
    auto word=[&](unsigned address){std::uint32_t value;std::memcpy(&value,ee.data()+address,4);return value;};
    if(word(0x3A6D00)!=3 || word(0x3A6D08)!=0x3A6D18 || word(0x3A6D18)!=pending.server_id
        || word(0x3A6D1C)!=0x3153F0 || word(0x3A6D20)!=0x3A6CC0)
        throw std::runtime_error("Reverse bind lacks generated EE registration");
    auto& thread=rt.threads.at(pending.thread);
    if(!thread.waiting || thread.ready || !thread.wait_ee_bind || thread.context.pc!=0xA1244
        || thread.entry!=0xA0FF0 || thread.priority!=24 || thread.context.get(28)!=thread.gp || thread.gp!=0xA94E0
        || rt.providers.contains({"sifcmd",15}) || rt.providers.contains({"thbase",24}))
        throw std::runtime_error("Reverse bind helper continuation differs");
    const std::array<unsigned,10> client={pending.packet,2,0,0,0,0,0,0,0,0};
    const std::array<unsigned,9> packet={64,0,0x80000009,0,6,pending.packet,2,pending.client,pending.server_id};
    for(unsigned n=0;n<client.size();++n)if(rt.read(pending.client+4*n,4)!=client[n])throw std::runtime_error("Reverse bind client differs");
    for(unsigned n=0;n<packet.size();++n)if(rt.read(pending.packet+4*n,4)!=packet[n])throw std::runtime_error("Reverse bind packet differs");
    rt.providers[{"sifcmd",15}]=[this,pending](IopRuntime& running,IopContext& c) {
        if(ee_sound_bind_returns || running.current_thread!=pending.thread || c.pc!=0xA1244
            || c.get(4)!=pending.client || c.get(5)!=pending.server_id || c.get(6))
            throw std::runtime_error("Reverse bind completion outside retained API");
        // Original blocking API and client capture plus SDK RPC_END ownership:
        // publish registered EE pointers, release the request, then return zero.
        running.write(pending.client,0,4);running.write(pending.client+20,0x3A6CC0,4);
        running.write(pending.client+36,0x3A6D18,4);
        running.write(pending.packet+16,4,4);running.write(pending.packet+24,0,4);
        running.pending_ee_bind={};++ee_sound_bind_returns;return 0u;
    };
    rt.providers[{"thbase",24}]=[this,pending](IopRuntime& running,IopContext& c)->unsigned {
        auto& owner=running.threads.at(pending.thread);
        if(ee_sound_bind_returns!=1 || running.current_thread!=pending.thread || c.pc!=0xA1304
            || c.get(31)!=0xA0F24 || c.get(28)!=owner.gp || owner.waiting || !owner.ready
            || running.pending_ee_bind.thread || running.read(pending.client,4)
            || running.read(pending.client+20,4)!=0x3A6CC0 || running.read(pending.client+36,4)!=0x3A6D18)
            throw std::runtime_error("SDRDRV first sleep outside completed reverse bind");
        owner.waiting=true;owner.ready=false;owner.wait_ee_bind=false;
        throw IopYield{}; // Actual blocking boundary; no wake or successful return.
    };
    rt.current_thread=pending.thread;thread.waiting=false;thread.wait_ee_bind=false;thread.ready=true;
    try {
        rt.execute(rt,thread.context);
        throw std::runtime_error("Unexpected reverse-bind helper exit");
    } catch(const IopYield&) {
        if(!thread.waiting || thread.ready) {
            rt.current_thread=0;rt.providers.erase({"sifcmd",15});rt.providers.erase({"thbase",24});
            throw std::runtime_error("Reverse-bind helper yielded without owned wait");
        }
    } catch(...) {
        failed_=true;rt.current_thread=0;rt.providers.erase({"sifcmd",15});rt.providers.erase({"thbase",24});throw;
    }
    rt.current_thread=0;rt.providers.erase({"sifcmd",15});rt.providers.erase({"thbase",24});
}

void NativeIopService::ownEeSoundServer(unsigned epoch,std::span<const std::uint8_t> ee) {
    if(epoch!=epoch_ || failed_ || ee_sound_server_owned || ee_sound_bind_returns
        || ee.size()!=32u*1024*1024 || runtime.pending_ee_bind.thread || mc_query_requests)
        throw std::runtime_error("EE sound registration outside queued bind phase");
    auto word=[&](unsigned address){std::uint32_t value;std::memcpy(&value,ee.data()+address,4);return value;};
    if(word(0x3A6D00)!=3 || word(0x3A6D08)!=0x3A6D18 || word(0x3A6D18)!=0x80000704
        || word(0x3A6D1C)!=0x3153F0 || word(0x3A6D20)!=0x3A6CC0)
        throw std::runtime_error("EE sound registration not produced by generated worker");
    ee_sound_server_owned=true;
}
