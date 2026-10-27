#include "ee_runtime.h"

// The startup evidence the probe supplies (captures of the original, checked against what the native
// services answer: services/sif_system.cpp, services/disc_service.cpp, services/module_service.cpp) and
// the completion of SIF commands and RPCs through the game's own handlers.
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::supplySifDma(std::span<const std::uint8_t> before, std::uint32_t result) {
    if (!sif_command_supplied_ || !sif_iop_.empty() || before.size() != sif_iop_size || result != 0x701)
        throw std::runtime_error("SIF DMA checkpoint size/return/order differs");
    std::uint32_t buffer, system_buffer, old_ee;
    std::memcpy(&buffer, before.data() + 0x196C0, 4);
    std::memcpy(&system_buffer, before.data() + 0x196C4, 4);
    std::memcpy(&old_ee, before.data() + 0x196C8, 4);
    if (buffer != 0x19600 || system_buffer != 0x19680 || old_ee != 0x93640)
        throw std::runtime_error("SIF IOP checkpoint table differs");
    sif_iop_.assign(before.begin(), before.end());
    sif_ee_receive_address = old_ee;
}

void EeRuntime::supplySifBind(std::span<const std::uint8_t> bundle) {
    if (!sif_rpc_supplied_ || sif_bind_supplied_ || (bundle.size() != sif_iop_size + 68 && bundle.size() != sif_iop_size + 96))
        throw std::runtime_error("Invalid/repeated RPC bind capture bundle");
    auto word = [&](std::size_t offset) { std::uint32_t value; std::memcpy(&value, bundle.data() + offset, 4); return value; };
    const std::array<std::uint32_t, 16> reply{0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
        0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0};
    if (word(sif_iop_size + 64) != 0x801 || word(0x3F570) != 0x80000592
            || word(0x3F578) != 0x3FB20 || word(0x3F584) != 0
            || word(0x3F5B0) != 0x3F558 || word(0x3F560) != 0x3F570
            || std::memcmp(bundle.data() + sif_iop_size, reply.data(), 64))
        throw std::runtime_error("RPC bind service/queue/response capture differs");
    if (bundle.size() == sif_iop_size + 96) {
        const std::array<std::uint32_t, 7> semaphore{0, 1, 0, 0, 0, 0x3838E8, 3};
        if (std::memcmp(bundle.data() + sif_iop_size + 68, semaphore.data(), 28))
            throw std::runtime_error("RPC call semaphore checkpoint differs");
        sif_call_sema_supplied_ = true;
    }
    sif_bind_supplied_ = true;
}

void EeRuntime::supplyCdInit(std::span<const std::uint8_t> bundle) {
    if (!sif_bind_supplied_ || !sif_call_sema_supplied_ || cd_init_supplied_ || (bundle.size() != 88 && bundle.size() != 128))
        throw std::runtime_error("Invalid/repeated CD-init completion bundle");
    const std::array<std::uint32_t, 22> expected{0, 0x902,
        0x1000, 0x3695C0, 0x80000008, 0, 0, 0, 0, 0x3A87A8, 0x8000000A, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0};
    if (std::memcmp(bundle.data(), expected.data(), 88))
        throw std::runtime_error("CD-init captured mode/result/reply differs");
    if (bundle.size() == 128) {
        const std::array<std::uint32_t, 10> expected_semaphores{0x370000, 1, 0, 0, 0x370000, 0x3825E8, 3, 4, 5, 6};
        if (std::memcmp(bundle.data() + 88, expected_semaphores.data(), 40))
            throw std::runtime_error("CD semaphore setup checkpoint differs");
        cd_semaphores_supplied_ = true;
    }
    cd_init_supplied_ = true;
}

void EeRuntime::supplyScmdBind(std::span<const std::uint8_t> bundle) {
    const std::array<std::uint32_t, 22> expected{0x370000,1,0,0,2,0x3838D8,
        0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8,0,0,0,0,0};
    if (!cd_semaphores_supplied_ || scmd_bind_supplied_ || (bundle.size() != 88 && bundle.size() != 184 && bundle.size() != 188 && bundle.size() != 192 && bundle.size() != 204 && bundle.size() != 224 && bundle.size() != 228 && bundle.size() != 236 && bundle.size() != 244 && bundle.size() != 252 && bundle.size() != 316 && bundle.size() != 324 && bundle.size() != 616 && bundle.size() != 988 && bundle.size() != 1124 && bundle.size() != 1332 && bundle.size() != 1464 && bundle.size() != 1596)
            || std::memcmp(bundle.data(), expected.data(), 88))
        throw std::runtime_error("Invalid SCMD bind checkpoint");
    if (bundle.size() >= 184) {
        const std::array<std::uint32_t,24> mmode{0,1,0,0,0x370000,0x3838E8,
            0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0,2,0};
        if(std::memcmp(bundle.data()+88,mmode.data(),96)) throw std::runtime_error("Invalid media-mode checkpoint");
        scmd_mmode_supplied_=true;
    }
    if (bundle.size() >= 188) {
        std::uint32_t stop_word; std::memcpy(&stop_word,bundle.data()+184,4);
        if (stop_word) throw std::runtime_error("SIF stop checkpoint differs");
        sif_stop_supplied_=true;
    }
    if (bundle.size() >= 192) {
        std::uint32_t subaddr; std::memcpy(&subaddr,bundle.data()+188,4);
        if (subaddr!=0x19600) throw std::runtime_error("Reset SUBADDR checkpoint differs");
        sif_reset_subaddr_supplied_=true;
    }
    if (bundle.size() >= 204) {
        const std::array<std::uint32_t,3> flags{0x30000,0x30000,0x30000};
        if (std::memcmp(bundle.data()+192,flags.data(),12)) throw std::runtime_error("Reset SMFLAG checkpoint differs");
        sif_smflag=0x30000; reset_smflag_supplied_=true;
    }
    if(bundle.size()>=224) {
        const std::array<std::uint32_t,5> completion{0xE01,0x20000,0,0,0};
        if(std::memcmp(bundle.data()+204,completion.data(),20))throw std::runtime_error("Invalid reboot submission/clear checkpoints");
        reset_submission_supplied_=true;
    }
    if(bundle.size()>=228) {
        std::uint32_t bit;std::memcpy(&bit,bundle.data()+224,4);
        if(bit!=0x40000)throw std::runtime_error("Invalid ready-bit checkpoint");
        reset_ready_supplied_=true;
    }
    if(bundle.size()>=236) {
        std::uint64_t csr;std::memcpy(&csr,bundle.data()+228,8);
        if(csr!=0x551B4000)throw std::runtime_error("Invalid GS CSR checkpoint");
    }
    if(bundle.size()>=244) {
        std::uint64_t result;std::memcpy(&result,bundle.data()+236,8);
        if(result!=0xFF00)throw std::runtime_error("Invalid GS IMR checkpoint");
    }
    if(bundle.size()>=252) {
        std::uint64_t result;std::memcpy(&result,bundle.data()+244,8);
        if(result)throw std::runtime_error("Invalid GS CRT checkpoint");
    }
    if(bundle.size()>=316) {
        const std::array<std::uint32_t,16> evidence{12,0x1802,0x1FFEAD0,1,8,0x1802,0x1FFEAD0,1,
            12,0x1802,0x1FFEAD0,4,8,0x1802,0x1FFEAD0,1};
        if(std::memcmp(bundle.data()+252,evidence.data(),64))throw std::runtime_error("INTC evidence differs");
    }
    if(bundle.size()>=324) {
        std::uint64_t field;std::memcpy(&field,bundle.data()+316,8);
        if(field!=0x551B6004)throw std::runtime_error("GS field checkpoint differs");
    }
    if(bundle.size()>=616) {
        const std::array<std::uint32_t,9> idle{0,0,0x1FFEAF0,0,0,0,1,0x800040,0x1201};
        const std::array<std::uint64_t,2> results{15,0};
        if(std::memcmp(bundle.data()+324,idle.data(),36) || std::memcmp(bundle.data()+360,results.data(),16))
            throw std::runtime_error("GIF idle/staging/reference return evidence differs");
        if(bundle.size()>=1124)post_sif.supply(bundle.subspan(988,136));
        if(bundle.size()>=1332)post_cd.supply(bundle.subspan(1124,bundle.size()>=1464?340:208));
        if(bundle.size()>=1596)post_loadfile.supply(bundle.subspan(1464));
    }
    scmd_bind_supplied_=true;
}

void EeRuntime::deliverEeCommand(std::uint8_t* rdram,R5900Context* context,std::uint32_t handler,std::uint32_t argument) {
    std::array<std::uint8_t,sizeof(R5900Context)> saved_cpu;std::memcpy(saved_cpu.data(),context,saved_cpu.size());
    auto callback=*context;callback.r[4].low=0x3A94C0;callback.r[5].low=argument;callback.r[28].low=0x38BDF0;
    constexpr std::uint32_t callback_return=0xFFFFFFF8;callback.r[31].low=callback_return;
    const auto scratch=static_cast<std::uint32_t>(callback.r[29].low)-128;checkMemory(rdram,scratch,128);
    std::array<std::uint8_t,128> saved_stack;std::copy_n(memory_.data()+scratch,128,saved_stack.data());
    rpc_end_callback_active_=true;
    try {dispatchGuestBranch(rdram,&callback,handler,context->pc,callback_return,GuestBranchKind::DirectCall,"FILEIO SIF command");}
    catch(...) {rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);throw;}
    rpc_end_callback_active_=false;std::copy(saved_stack.begin(),saved_stack.end(),memory_.begin()+scratch);
    if(callback.r[29].low!=context->r[29].low || std::memcmp(saved_cpu.data(),context,saved_cpu.size()))
        throw std::runtime_error("FILEIO SIF command handler changed interrupted CPU state");
}

void EeRuntime::deliverRpcEnd(std::uint8_t* rdram, R5900Context* context,
                       std::uint32_t client, unsigned semaphore,
                       std::uint32_t server, std::uint32_t buffer) {
    std::array<std::uint8_t, sizeof(R5900Context)> saved_cpu;
    std::memcpy(saved_cpu.data(), context, saved_cpu.size());
    auto callback = *context;
    callback.r[4].low = 0x3A94C0;
    callback.r[5].low = 0x3AAFC0;
    callback.r[28].low = 0x38BDF0;
    constexpr std::uint32_t callback_return = 0xFFFFFFF8;
    callback.r[31].low = callback_return;
    // Verified handler uses a 64-byte frame below the caller SP. Preserve
    // that scratch window and execute on a CPU copy, retaining every register.
    const auto scratch = static_cast<std::uint32_t>(callback.r[29].low) - 64;
    checkMemory(rdram, scratch, 64);
    std::array<std::uint8_t, 64> saved_stack;
    std::copy_n(memory_.data() + scratch, 64, saved_stack.data());
    rpc_end_callback_active_ = true;
    try {
        dispatchGuestBranch(rdram, &callback, 0x3322E8, context->pc, callback_return,
            GuestBranchKind::DirectCall, "checked logical RPC_END");
    } catch (...) {
        rpc_end_callback_active_ = false;
        std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
        throw;
    }
    rpc_end_callback_active_ = false;
    std::copy(saved_stack.begin(), saved_stack.end(), memory_.begin() + scratch);
    if (callback.r[29].low != context->r[29].low || (semaphore ? semaphores[semaphore].count != 1 :
                read32(rdram,client+8) != UINT32_MAX)
            || read32(rdram, client) != 0 || read32(rdram, client + 36) != server
            || read32(rdram, client + 20) != buffer || read32(rdram, 0x3A97D0) != 4
            || read32(rdram, 0x3A97D8) != 0)
        throw std::runtime_error("Generated RPC_END effects differ from capture");
    if (std::memcmp(saved_cpu.data(), context, saved_cpu.size()))
        throw std::runtime_error("RPC_END changed interrupted CPU state");
}

