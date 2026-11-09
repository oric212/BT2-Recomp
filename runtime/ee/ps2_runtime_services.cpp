#include "ee_runtime.h"

// The console's state at the game's start (bootState) and the completion of SIF commands and RPCs through the
// game's own handlers.
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::supplySifDma() {
    if (!sif_command_supplied_ || sif_dma_supplied_) throw std::runtime_error("SIF DMA state out of order or repeated");
    sif_ee_receive_address = 0x93640;
    sif_dma_supplied_ = true;
}

void EeRuntime::supplySifBind() {
    if (!sif_rpc_supplied_ || sif_bind_supplied_) throw std::runtime_error("RPC bind state out of order or repeated");
    sif_bind_supplied_ = sif_call_sema_supplied_ = true;
}

void EeRuntime::bootState() {
    supplyTimer3Mode(0x83);
    enableKernelPatch(0x80076000, 0x740); // The game's own kernel patch (its code at 0x36B328).
    supplyTimer2Mode(0x80);
    supplyIntcMask(0x1002);
    supplyTimer2Readback(0x382);
    // The console's system settings word (language, screen shape, time zone), as the game finds and sets it.
    constexpr std::uint32_t osd_settings = 0x21C12010;
    supplyOsdConfig(osd_settings);
    supplyOsdReadback(osd_settings);
    enableStartupKernelPayload(std::span<const std::uint8_t>(memory_).subspan(0x0036BAF8, 0x330)); // The game's own, at 0x36BAF8.
    supplyDmaStatus(0x00800040);
    supplyDma5Control(0x184);
    supplySifCommand(0x19600);
    supplySifDma();
    supplySifRpcState(1);
    // The IOP services' first binds and replies, the sound thread's start, the CD's initialisation and the IOP's reset:
    // the native services answer them; the flags let them.
    heap_bind_supplied = heap_allocation_supplied = heap_copy_supplied = sound_thread_start_supplied = true;
    ncmd_bind_supplied = disc_bind_supplied = true;
    supplySifBind();
    cd_init_supplied_ = cd_semaphores_supplied_ = true;
    scmd_mmode_supplied_ = sif_stop_supplied_ = sif_reset_subaddr_supplied_ = true;
    sif_smflag = 0x30000;
    reset_smflag_supplied_ = reset_submission_supplied_ = reset_ready_supplied_ = true;
    post_sif.supplied = post_cd.supplied = post_cd.scmd_supplied = post_loadfile.supplied = true;
    scmd_bind_supplied_ = true;
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

