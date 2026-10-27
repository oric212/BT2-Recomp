#include "ee/ee_runtime.h"
#include <algorithm>

// The SIF with one behavior wherever it is reached (RUNTIME_GENERALIZATION_PLAN.md, group 5): the EE
// kernel's SIF register, SIF0 chain, DMAC handler and channel calls, and the SIF system commands the
// native IOP answers. No IOP code runs. The IOP side keeps what the original's modules show: SIFCMD's
// buffer per boot (iopCommandBuffer) and SMFLG with SIF and SIFCMD initialized and the boot complete
// (bits 16 to 18) once a reboot has finished, which it has when the EE first polls SMFLG after asking.

namespace {
constexpr std::uint32_t change_saddr=0x80000000,set_sreg=0x80000001,init_cmd=0x80000002,reset_cmd=0x80000003;
constexpr std::uint32_t smflag_booted=0x70000;
std::uint64_t kernelInt(std::uint32_t value) {return std::uint64_t(std::int64_t(std::int32_t(value)));}
bool softwareRegister(std::uint32_t reg) {return reg>=0x80000000u && reg<0x80000020u;}
}

bool EeRuntime::handleSifSystemSyscall(std::uint8_t* rdram,std::uint32_t selector,R5900Context* context) {
    const auto a0=static_cast<std::uint32_t>(context->r[4].low),a1=static_cast<std::uint32_t>(context->r[5].low);
    switch(selector) {
    case 0x7A: { // SifGetReg: SMCOM (2), SMFLG (4) or a software register.
        std::uint32_t value;
        if(a0==2)value=iopCommandBuffer();
        else if(a0==4) {
            if(iop_ready_epoch!=iop_epoch) {sif_smflag|=smflag_booted;iop_ready_epoch=iop_epoch;}
            value=sif_smflag;
        } else if(softwareRegister(a0))value=sif_sregs[a0-0x80000000u];
        else throw UnsupportedSyscall(selector,context->pc);
        ++sif_register_reads;context->r[2].low=kernelInt(value);return true;
    }
    case 0x79: { // SifSetReg: an EE write to SMFLG clears its bits and returns what remains.
        std::uint32_t value;
        if(a0==4)value=sif_smflag&=~a1;
        else if(softwareRegister(a0)) {
            value=sif_sregs[a0-0x80000000u]=a1;
            if(a0==0x80000002u && a1 && iop_epoch)post_sif.rpc_initialized=true; // Diagnostic: RPC set up after the reset.
        } else throw UnsupportedSyscall(selector,context->pc);
        ++sif_register_writes;context->r[2].low=kernelInt(value);return true;
    }
    case 0x78: // SifSetDChain: SIF0 receives in chain mode with its interrupt (STR, TIE, MOD 1). Void: v0 kept.
        sif0_chcr=(sif0_chcr&0xFFFF0000u)|0x184u;return true;
    case 0x6B: // The kernel's SIF DMA stop: SIF2's control and count go to zero. Nothing is in flight: 0 left.
        if(a0!=1)throw UnsupportedSyscall(selector,context->pc);
        sif2_chcr=0;sif2_qwc=0;++sif_stop_calls;context->r[2].low=0;return true;
    case 0x12: { // AddDmacHandler(channel, handler, next 0: first, argument): the lowest free id.
        if(a0>9 || !a1 || a1%4 || context->r[6].low)throw UnsupportedSyscall(selector,context->pc);
        checkMemory(rdram,a1,4);
        const DmacHandler handler{a0,a1,static_cast<std::uint32_t>(context->r[7].low),static_cast<std::uint32_t>(context->r[28].low)};
        auto slot=std::find_if(dmac_handlers.begin(),dmac_handlers.end(),[](const DmacHandler& h){return h.channel==~0u;});
        if(slot==dmac_handlers.end()) {
            if(dmac_handlers.size()==128){context->r[2].low=UINT64_MAX;return true;}
            dmac_handlers.push_back(handler);slot=dmac_handlers.end()-1;
        } else *slot=handler;
        const auto id=static_cast<std::uint32_t>(slot-dmac_handlers.begin())+1;
        dmac_handler_order.insert(dmac_handler_order.begin(),id);context->r[2].low=id;return true;
    }
    case 0x13: { // RemoveDmacHandler(channel, id): returns the handlers left on the channel.
        const auto order=std::find(dmac_handler_order.begin(),dmac_handler_order.end(),a1);
        if(order==dmac_handler_order.end() || dmac_handlers.at(a1-1).channel!=a0)throw UnsupportedSyscall(selector,context->pc);
        dmac_handler_order.erase(order);dmac_handlers.at(a1-1).channel=~0u;++dmac_removals;
        context->r[2].low=static_cast<std::uint64_t>(std::count_if(dmac_handlers.begin(),dmac_handlers.end(),[&](const DmacHandler& h){return h.channel==a0;}));
        return true;
    }
    case 0x16: case 0x17: { // _EnableDmac / _DisableDmac(channel): its D_STAT mask bit; 1 when that changed it.
        if(a0>9)throw UnsupportedSyscall(selector,context->pc);
        const auto bit=0x10000u<<a0;const bool enable=selector==0x16,changed=enable!=((dmac_mask&bit)!=0);
        if(enable) {dmac_mask|=bit;++dmac_enable_calls;} else {dmac_mask&=~bit;++dmac_disable_calls;}
        context->r[2].low=changed?1:0;return true;
    }
    case 0x77: return sifSystemCommand(rdram,context);
    }
    return false;
}

// A SIF system command (one descriptor, its packet into SIFCMD's buffer): CHANGE_SADDR and INIT_CMD 0
// name the EE's receive buffer; INIT_CMD 1 sets up RPC, which the IOP answers with SET_SREG (register 0
// to 1), received into the EE library's register table 0x1A8 into the command data the EE named in
// software register 1 (as the original shows); RESET_CMD reboots the IOP with IOPRP300, the image the
// native services stand for. Anything else (RPC binds and calls, data) is not a system command.
bool EeRuntime::sifSystemCommand(std::uint8_t* rdram,R5900Context* context) {
    const auto d=static_cast<std::uint32_t>(context->r[4].low);
    if(context->r[5].low!=1 || d%16)return false;
    checkMemory(rdram,d,16);
    const auto source=read32(rdram,d)&0x1FFFFFFFu,destination=read32(rdram,d+4),size=read32(rdram,d+8);
    if(read32(rdram,d+12)!=0x44 || size<16 || size>112 || size%4)return false;
    checkMemory(rdram,source,size);
    const auto cid=read32(rdram,source+8),option=read32(rdram,source+12);
    if(cid!=change_saddr && cid!=init_cmd && cid!=reset_cmd)return false;
    if(destination!=iopCommandBuffer() || read32(rdram,source)!=size)throw UnsupportedSyscall(0x77,context->pc);
    if(cid==change_saddr || (cid==init_cmd && !option)) {
        if(size!=20)throw UnsupportedSyscall(0x77,context->pc);
        sif_ee_receive_address=read32(rdram,source+16);
        if(cid==init_cmd)post_sif.ee_receive_address=sif_ee_receive_address;
    } else if(cid==init_cmd) {
        if(size!=16 || option!=1 || !sif_ee_receive_address || !sif_sregs[1])throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,6> reply={0,0,set_sreg,0,0,1}; // Header, register 0, value 1.
        checkMemory(rdram,sif_ee_receive_address,sizeof(reply));
        for(unsigned n=0;n<reply.size();++n)write32(rdram,sif_ee_receive_address+n*4,reply[n]);
        write32(rdram,sif_sregs[1]+0x1A8,1);++post_sif.sreg_responses;
    } else {
        // The argument (its size, mode 0, then the text) names the update image.
        constexpr char image[]="rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
        if(size!=104 || read32(rdram,source+16)!=sizeof(image)-1 || read32(rdram,source+20)
            || !std::equal(std::begin(image),std::end(image),memory_.begin()+source+24))throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(memory_.data()+source,104,reset_request_.data());
        iop_reset_pending=true;prior_iop_services_valid=false;++iop_epoch;sif0_chcr&=~0x100u; // The reboot stops SIF0's receive.
    }
    if(cid==init_cmd)++post_sif.submissions;
    ++sif_system_commands;++sif_dma_submissions;
    context->r[2].low=0x10000u+sif_system_commands; // A nonzero transfer id.
    return true;
}
