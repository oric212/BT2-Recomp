#include "ee/ee_runtime.h"
#include <string>

// Native LOADFILE calls (0x80000006): its version (fn 0xFF), IOP module loads by path (fn 0) and
// from an IOP buffer (fn 6). No IOP module runs; their services are native. The load replies come from
// ModuleLoads (services/module_loads.h).

namespace {
constexpr std::uint32_t load_by_path=0,load_from_buffer=6,version=0xFF;
constexpr std::uint32_t loadfile_version=0x30303033; // IOPRP300's LOADFILE (capture).
}

bool EeRuntime::nativeModuleCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.rpc_number==version) {
        if(r.send_size || r.recv_size!=4)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,loadfile_version);completeNativeCall(rdram,context,r,server);
        ++post_loadfile.versions;context->r[2].low=submitSifDma(context);return true;
    }
    if(r.rpc_number!=load_by_path && r.rpc_number!=load_from_buffer)throw UnsupportedSyscall(0x77,context->pc);
    if(r.send_size!=512 || r.recv_size!=8)throw UnsupportedSyscall(0x77,context->pc);
    ModuleLoadReply reply;
    if(r.rpc_number==load_by_path) {
        std::string path;for(unsigned n=8;n<260;++n){const auto ch=read8(rdram,r.send_source+n);if(!ch)break;path+=static_cast<char>(ch);}
        const auto found=module_loads.byPath(path);
        if(!found)throw UnsupportedSyscall(0x77,context->pc);
        reply=*found;
    } else {
        // Load from the IOP buffer the EE uploaded the embedded module into.
        const auto buffer=read32(rdram,r.send_source);
        if(!native_heap_.isBlock(buffer) || read32(rdram,r.send_source+4))throw UnsupportedSyscall(0x77,context->pc);
        reply=module_loads.fromBuffer();
    }
    write32(rdram,r.receive,reply.id);write32(rdram,r.receive+4,reply.result);
    completeNativeCall(rdram,context,r,server);
    context->r[2].low=submitSifDma(context);return true;
}
