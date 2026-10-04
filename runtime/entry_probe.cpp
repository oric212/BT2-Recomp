#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#ifdef BT2_KERNEL_PAYLOAD
#include <bt2_kernel_functions.h>
#endif
#ifdef BT2_STARTUP_KERNEL_PAYLOAD
#include <bt2_startup_kernel_functions.h>
#endif
#ifdef BT2_EXTRA_STARTUP_BINDINGS
#include <bt2_extra_startup.h>
#endif
#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4 && argc != 5 && argc != 6 && argc != 7 && argc != 8 && argc != 9 && argc != 10 && argc != 11 && argc != 12 && argc != 13 && argc != 14 && argc != 15 && argc != 16 && argc != 17 && argc != 18) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT [KERNEL_SNAPSHOT [TIMER3_CAPTURE_WORD [TIMER2_CAPTURE_WORD [INTC_MASK_WORD [TIMER2_READBACK_WORD [OSD_CONFIG_WORD [OSD_READBACK_WORD [DMA_STATUS_WORD [DMA5_CONTROL_WORD [SIF_COMMAND_WORD [SIF_DMA_BUNDLE [SIF_RPC_STATE_WORD [SIF_BIND_BUNDLE]]]]]]]]]]]]]");
        const bool cd_mode = argc >= 17;
        const bool bind_mode = argc >= 16;
        const bool snapshot_mode = argc >= 4;
        const bool timer_mode = argc >= 5;
        const bool timer2_mode = argc >= 6;
        const bool intc_mode = argc >= 7;
        const bool readback_mode = argc >= 8;
        const bool osd_mode = argc >= 9;
        const bool osd_readback_mode = argc >= 10;
        const bool dma_status_mode = argc >= 11;
        const bool sif_rpc_mode = argc >= 15;
        const bool sif_dma_mode = argc >= 14;
        const bool sif_mode = argc >= 13;
        const bool dma5_mode = argc >= 12;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(PS2Runtime::ram_size))
            throw std::runtime_error("Invalid prepared guest image size");
        std::vector<std::uint8_t> image(PS2Runtime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size())))
            throw std::runtime_error("Could not read guest image");
        PS2Runtime runtime;
        runtime.loadMemory(image);
        if (snapshot_mode) {
            std::ifstream kernel(argv[3], std::ios::binary | std::ios::ate);
            if (!kernel || kernel.tellg() != static_cast<std::streamoff>(PS2Runtime::kernel_snapshot_size))
                throw std::runtime_error("Invalid kernel snapshot size");
            std::vector<std::uint8_t> bytes(PS2Runtime::kernel_snapshot_size);
            kernel.seekg(0);
            if (!kernel.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
                throw std::runtime_error("Could not read kernel snapshot");
            runtime.loadKernelSnapshot(bytes);
        }
        if (timer_mode) {
            std::ifstream timer(argv[4], std::ios::binary | std::ios::ate);
            if (!timer || timer.tellg() != 4) throw std::runtime_error("Invalid Timer 3 capture word");
            std::uint32_t word;
            timer.seekg(0);
            if (!timer.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer capture read failed");
            require(word == 0x83, "Timer capture differs from the validated startup observation");
            runtime.supplyTimer3Mode(word);
            runtime.enableKernelPatch(0x80076000, 0x740);
        }
        if (timer2_mode) {
            std::ifstream timer(argv[5], std::ios::binary | std::ios::ate);
            if (!timer || timer.tellg() != 4) throw std::runtime_error("Invalid Timer 2 capture word");
            std::uint32_t word;
            timer.seekg(0);
            if (!timer.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer 2 capture read failed");
            require(word == 0x80, "Timer 2 capture differs from the validated startup observation");
            runtime.supplyTimer2Mode(word);
        }
        if (intc_mode) {
            std::ifstream mask(argv[6], std::ios::binary | std::ios::ate);
            if (!mask || mask.tellg() != 4) throw std::runtime_error("Invalid INTC checkpoint word");
            std::uint32_t word;
            mask.seekg(0);
            if (!mask.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("INTC checkpoint read failed");
            require(word == 0x1002, "INTC checkpoint differs from validated startup capture");
            runtime.supplyIntcMask(word);
        }
        if (readback_mode) {
            std::ifstream capture(argv[7], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid Timer 2 readback word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer 2 readback word read failed");
            require(word == 0x382, "Readback capture differs from validated startup observation");
            runtime.supplyTimer2Readback(word);
        }
        std::uint32_t osd_word = 0;
        if (osd_mode) {
            std::ifstream capture(argv[8], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid OSD checkpoint word");
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&osd_word), 4)) throw std::runtime_error("OSD checkpoint read failed");
            runtime.supplyOsdConfig(osd_word);
        }
        if (osd_readback_mode) {
            std::ifstream capture(argv[9], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid OSD readback word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("OSD readback read failed");
            runtime.supplyOsdReadback(word);
            runtime.enableStartupKernelPayload(std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
        }
        for (int index = 10; index < std::min(argc, 12); ++index) {
            std::ifstream capture(argv[index], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid DMA checkpoint word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("DMA checkpoint read failed");
            if (index == 10) runtime.supplyDmaStatus(word);
            else runtime.supplyDma5Control(word);
        }
        if (sif_mode) {
            std::ifstream capture(argv[12], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid SIF checkpoint word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("SIF checkpoint read failed");
            runtime.supplySifCommand(word);
        }
        if (sif_dma_mode) {
            std::ifstream capture(argv[13], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != static_cast<std::streamoff>(PS2Runtime::sif_iop_size + 4))
                throw std::runtime_error("Invalid SIF DMA checkpoint bundle");
            std::vector<std::uint8_t> bytes(PS2Runtime::sif_iop_size);
            std::uint32_t result;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bytes.data()), bytes.size())
                    || !capture.read(reinterpret_cast<char*>(&result), 4))
                throw std::runtime_error("SIF DMA checkpoint read failed");
            runtime.supplySifDma(bytes, result);
        }
        if (sif_rpc_mode) {
            std::ifstream capture(argv[14], std::ios::binary | std::ios::ate);
            std::uint32_t word;
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid SIF RPC checkpoint word");
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("SIF RPC checkpoint read failed");
            runtime.supplySifRpcState(word);
        }
        const std::vector<std::uint8_t> kernel_before(runtime.kernelBaseline().begin(), runtime.kernelBaseline().end());
        constexpr std::uint32_t zero_start = 0x00387E80, zero_end = 0x0043058F;
        runtime.expectStartupZeroRegion(zero_start, zero_end);
        // Poison only the observed startup zero region so zero stores are observable.
        std::fill(runtime.memory().begin() + zero_start, runtime.memory().begin() + zero_end, std::uint8_t{0xA5});
        R5900Context context{};
        for (auto& reg : context.r) reg = {UINT64_MAX, UINT64_MAX};
        context.f.fill(1.0f);
        context.hi = context.lo = context.hi1 = context.lo1 = UINT64_MAX;
        context.fcr31 = context.sa = UINT32_MAX;
        context.f_acc = 1.0f;
        if (bind_mode) {
            std::ifstream capture(argv[15], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg() != PS2Runtime::sif_iop_size + 68
                    && capture.tellg() != PS2Runtime::sif_iop_size + 96))
                throw std::runtime_error("Invalid RPC bind capture bundle");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()), bundle.size()))
                throw std::runtime_error("RPC bind bundle read failed");
            runtime.supplySifBind(bundle);
        }
        if (cd_mode) {
            std::ifstream capture(argv[16], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg() != 88 && capture.tellg() != 128)) throw std::runtime_error("Invalid CD-init bundle size");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()), bundle.size()))
                throw std::runtime_error("Could not read CD-init bundle");
            runtime.supplyCdInit(bundle);
        }
        runtime.registerFunction(0x31EDB0, FUN_0031EDB0_0x31edb0);
        runtime.registerFunction(0x31F010, FUN_0031F010_0x31f010);
        runtime.registerFunction(0x332970, FUN_00332970_0x332970);
        runtime.registerFunction(0x32EC70, FUN_0032EC70_0x32ec70);
        runtime.registerFunction(0x3322E8, FUN_003322E8_0x3322e8);
        runtime.registerFunction(0x332258, FUN_00332258_0x332258);
        runtime.registerFunction(0x32EC90, FUN_0032EC90_0x32ec90);
        runtime.registerFunction(0x00100008, FUN_00100008_0x100008);
        runtime.registerFunction(0x00338520, FUN_00338520_0x338520);
        runtime.registerFunction(0x003382D0, FUN_003382D0_0x3382d0);
        runtime.registerFunction(0x0032EC60, FUN_0032EC60_0x32ec60);
        runtime.registerFunction(0x00338410, FUN_00338410_0x338410);
        runtime.registerFunction(0x00338510, FUN_00338510_0x338510);
        runtime.registerFunction(0x003383C8, FUN_003383C8_0x3383c8);
        runtime.registerFunction(0x00338388, FUN_00338388_0x338388);
        runtime.registerFunction(0x00338A00, FUN_00338A00_0x338a00);
        runtime.registerFunction(0x00338998, FUN_00338998_0x338998);
        runtime.registerFunction(0x003389A8, FUN_003389A8_0x3389a8);
        runtime.registerFunction(0x003389B8, FUN_003389B8_0x3389b8);
        runtime.registerFunction(0x0032EEE0, FUN_0032EEE0_0x32eee0);
        runtime.registerFunction(0x003389F0, FUN_003389F0_0x3389f0);
        runtime.registerFunction(0x00338B78, FUN_00338B78_0x338b78);
        runtime.registerFunction(0x003251DC, FUN_003251DC_0x3251dc);
        runtime.registerFunction(0x00339E48, FUN_00339E48_0x339e48);
        runtime.registerFunction(0x00338648, FUN_00338648_0x338648);
        runtime.registerFunction(0x003385E0, FUN_003385E0_0x3385e0);
        runtime.registerFunction(0x00321E50, FUN_00321E50_0x321e50);
        runtime.registerFunction(0x00321D90, FUN_00321D90_0x321d90);
        runtime.registerFunction(0x00322F90, FUN_00322F90_0x322f90);
        runtime.registerFunction(0x0032EC80, FUN_0032EC80_0x32ec80);
        runtime.registerFunction(0x0032ECA0, FUN_0032ECA0_0x32eca0);
        runtime.registerFunction(0x003242D0, FUN_003242D0_0x3242d0);
        runtime.registerFunction(0x00331FE8, FUN_00331FE8_0x331fe8);
        runtime.registerFunction(0x00331888, FUN_00331888_0x331888);
        runtime.registerFunction(0x003382B8, FUN_003382B8_0x3382b8);
        runtime.registerFunction(0x0032E970, FUN_0032E970_0x32e970);
        runtime.registerFunction(0x0032F878, FUN_0032F878_0x32f878);
        runtime.registerFunction(0x0032E9C0, FUN_0032E9C0_0x32e9c0);
        runtime.registerFunction(0x0032F070, FUN_0032F070_0x32f070);
        runtime.registerFunction(0x00331D70, FUN_00331D70_0x331d70);
        runtime.registerFunction(0x00331C38, FUN_00331C38_0x331c38);
        runtime.registerFunction(0x00331B70, FUN_00331B70_0x331b70);
        runtime.registerFunction(0x0031F8B0, FUN_0031F8B0_0x31f8b0);
        runtime.registerFunction(0x0031F638, FUN_0031F638_0x31f638);
        runtime.registerFunction(0x00332B70, FUN_00332B70_0x332b70);
        runtime.registerFunction(0x00332790, FUN_00332790_0x332790);
        runtime.registerFunction(0x003321B0, FUN_003321B0_0x3321b0);
        runtime.registerFunction(0x0032F020, FUN_0032F020_0x32f020);
        runtime.registerFunction(0x00331F38, FUN_00331F38_0x331f38);
        runtime.registerFunction(0x00100388, FUN_00100388_0x100388);
        runtime.registerFunction(0x0033A238, FUN_0033A238_0x33a238);
        runtime.registerFunction(0x0033A820, FUN_0033A820_0x33a820);
        runtime.registerFunction(0x0033A810, FUN_0033A810_0x33a810);
        runtime.registerFunction(0x0033A800, FUN_0033A800_0x33a800);
        runtime.registerFunction(0x0033A7C8, FUN_0033A7C8_0x33a7c8);
        runtime.registerFunction(0x0033A7B8, FUN_0033A7B8_0x33a7b8);
        runtime.registerFunction(0x0032ED00, FUN_0032ED00_0x32ed00);
        runtime.registerFunction(0x0032ED10, FUN_0032ED10_0x32ed10);
        runtime.registerFunction(0x0032EAF0, FUN_0032EAF0_0x32eaf0);
        runtime.registerFunction(0x0032FDA0, FUN_0032FDA0_0x32fda0);
        runtime.registerFunction(0x0032F730, FUN_0032F730_0x32f730);
        runtime.registerFunction(0x0032EB50, FUN_0032EB50_0x32eb50);
        runtime.registerFunction(0x0032FA38, FUN_0032FA38_0x32fa38);
        runtime.registerFunction(0x0032EA60, FUN_0032EA60_0x32ea60);
        runtime.registerFunction(0x00338DB8, FUN_00338DB8_0x338db8);
        runtime.registerFunction(0x0032F7A8, FUN_0032F7A8_0x32f7a8);
        runtime.registerFunction(0x0032E9A0, FUN_0032E9A0_0x32e9a0);
        runtime.registerFunction(0x00338B58, FUN_00338B58_0x338b58);
        runtime.registerFunction(0x00338AD8, FUN_00338AD8_0x338ad8);
        runtime.registerFunction(0x00338268, FUN_00338268_0x338268);
        runtime.registerFunction(0x0032E950, FUN_0032E950_0x32e950);
#ifdef BT2_EXTRA_STARTUP_BINDINGS
        registerExtraStartup(runtime);
#endif
        if (timer_mode) {
#ifdef BT2_KERNEL_PAYLOAD
            runtime.registerKernelFunction(0x80076000, FUN_80076000_0x80076000,
                std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036B328, 0x740));
#else
            throw std::runtime_error("Timer replay requires generated kernel payload integration");
#endif
        }
        if (osd_readback_mode) {
#ifdef BT2_STARTUP_KERNEL_PAYLOAD
            runtime.registerKernelFunction(0x80075000, FUN_80075000_0x80075000,
                std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
#else
            throw std::runtime_error("OSD readback requires generated startup kernel payload");
#endif
        }
        if (argc >= 18) {
            std::ifstream capture(argv[17], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg()!=88 && capture.tellg()!=184)) throw std::runtime_error("Invalid SCMD bundle size");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()),bundle.size()) || capture.peek()!=EOF)
                throw std::runtime_error("Invalid SCMD bind bundle size");
            runtime.supplyScmdBind(bundle);
        }
        bool stopped = false;
        std::uint32_t stop_address = 0;
        std::size_t stop_size = 0;
        std::uint32_t stop_selector = 0, stop_handler = 0;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestBranch& stop) {
            std::cerr << "Branch target/source/resume: " << std::hex << stop.target << "/" << stop.source_pc << "/" << stop.resume_pc << "\n";
            if (!bind_mode) throw;
            stop_target = stop.target; stop_source = stop.source_pc; stop_resume = stop.resume_pc; stopped = true;
        }
        catch (const UnsupportedGuestMemory& stop) {
            if (dma5_mode) throw;
            if (dma_status_mode) require(stop.address == 0x1000C000 && stop.size == 4
                    && context.pc == 0x003319EC && runtime.dma_status_reads == 1,
                    "Unexpected DMA channel control boundary");
            else if (osd_readback_mode) require(stop.address == 0x1000E010 && stop.size == 4
                    && context.pc == 0x003319CC && GPR_U32(&context, 4) == 0
                    && runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1,
                    "Unexpected DMA status boundary");
            else if (readback_mode) throw;
            else if (intc_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338DD8
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1
                    && runtime.timer2_mode_reads == 1 && runtime.timer2_mode_writes == 1,
                    "Unexpected post-EnableIntc timer boundary");
            else if (timer2_mode) throw;
            else if (timer_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338C58,
                                    "Unexpected Timer 2 MMIO boundary");
            else if (snapshot_mode) require(stop.address == 0x10001810 && stop.size == 4 && context.pc == 0x00338A1C,
                                       "Unexpected snapshot MMIO boundary");
            else require(stop.address == 0x80000000 && stop.size == 4 && context.pc == 0x00338388,
                         "Unexpected entry memory boundary");
            stop_address = stop.address;
            stop_size = stop.size;
            stopped = true;
        }
        catch (const UnsupportedSyscall& stop) {
            if (bind_mode) std::cerr << "Syscall selector/handler/pc: " << std::hex << stop.selector << "/" << stop.handler << "/" << context.pc << "\n";
            else if (sif_rpc_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 464
                    && GPR_U32(&context, 5) == 1 && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 1,
                    "Unexpected first RPC bind submission boundary");
            else if (sif_dma_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000002
                    && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 0,
                    "Unexpected RPCINIT query boundary");
            else if (sif_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 240
                    && GPR_U32(&context, 5) == 1 && runtime.sif_command_reads == 1
                    && runtime.data_cache_lines == 1, "Unexpected first SIF DMA submission boundary");
            else if (dma5_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000000,
                    "Unexpected first SIF register boundary");
            else if (osd_readback_mode) throw;
            else if (osd_mode) require(stop.selector == 0x4A && !stop.handler && stop.resume_pc == 0x0032ED08
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 124
                    && runtime.osd_config_reads == 1 && runtime.priority_changes == 1
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 128) == osd_word
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 124) == ((osd_word & 0xFFFF1FFF) | 0x2000),
                    "Unexpected SetOsdConfigParam boundary");
            else if (readback_mode) require(stop.selector == 0x4B && !stop.handler && stop.resume_pc == 0x0032ED18
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 128
                    && runtime.main_thread_priority == 1 && runtime.priority_changes == 1
                    && runtime.timer2_mode_reads == 2 && runtime.timer2_mode_writes == 1
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1,
                    "Unexpected GetOsdConfigParam boundary after priority change");
            else require(!intc_mode && timer2_mode && stop.selector == 0x14 && !stop.handler && stop.resume_pc == 0x0032E9A8
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 11
                    && runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382
                    && context.cop0_errorepc == 0x00338C90 && !context.ll_bit,
                    "Unexpected syscall after generated Timer 2 write/ERET");
            stop_selector = stop.selector;
            stop_handler = stop.handler;
            stopped = true;
        }
        if (cd_mode && argc < 18) {
            require(stopped && !stop_handler && (stop_target || (stop_selector == 0x40 && context.pc == 0x32EC68)),
                    "CD continuation did not reach a checked boundary");
            require(runtime.cd_init_calls == 1 && runtime.cd_init_context_preservations == 1
                    && runtime.sif_dma_submissions == 3 && runtime.semaphore_deletions == 2
                    && runtime.semaphore_waits == 3 && runtime.semaphore_signals == 3,
                    "CD-init callback or cleanup effects differ");
            if (stop_target) {
                const bool mapped_stop = (stop_target == 0x320488 && stop_source == 0x1003AC && stop_resume == 0x1003B4
                        && runtime.guest_calls == 160 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 32)
                    || (stop_target == 0x31F6A8 && stop_source == 0x3204A8 && stop_resume == 0x3204B0
                        && runtime.guest_calls == 161 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 112)
                    || (stop_target == 0x32ECB0 && stop_source == 0x31F6CC && stop_resume == 0x31F6D4
                        && runtime.guest_calls == 163 && runtime.returns == 159 && context.r[29].low == runtime.initial_sp - 176);
                require(mapped_stop && runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7,
                        "Reached mapped-entry boundary differs");
            } else if (runtime.cd_semaphores_created) {
                require(runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7
                        && runtime.semaphore_polls == 1 && runtime.guest_calls == 173 && runtime.returns == 167
                        && context.r[29].low == runtime.initial_sp - 288,
                        "CD setup and next bind progress differs");
            } else require(runtime.semaphores.size() == 3 && context.r[29].low == runtime.initial_sp - 304,
                           "CD semaphore checkpoint boundary differs");
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write CD continuation report");
            report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
                << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
                << ",\"resume_pc\":" << stop_resume << ",\"calls\":" << runtime.guest_calls
                << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
                << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
                << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
                << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
                << ",\"lo\":" << context.lo << ",\"cd_init_calls\":" << runtime.cd_init_calls
                << ",\"deletions\":" << runtime.semaphore_deletions
                << ",\"polls\":" << runtime.semaphore_polls << ",\"cd_semaphores\":" << runtime.cd_semaphores_created
                << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
                << ",\"descriptor_words\":[";
            for (unsigned index = 0; index < (stop_target ? 0u : 6u); ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
            report << "]}\n";
            std::cout << "CD-init continuation report written.\n";
            return 0;
        }
        if (argc >= 18) {
            require(stopped && !stop_handler && runtime.scmd_bind_calls == 1 && runtime.cd_init_calls == 1
                    && runtime.semaphores.size() == 7, "SCMD continuation state differs");
            if (!runtime.scmd_mmode_calls) {
                require(!stop_target && stop_selector == 0x40 && context.pc == 0x32EC68
                        && runtime.guest_calls == 190 && runtime.returns == 185
                        && context.r[29].low == runtime.initial_sp - 304 && runtime.semaphore_deletions == 3,
                        "SCMD call-semaphore boundary differs");
            } else {
                require(runtime.semaphore_deletions == 4 && runtime.semaphore_waits == 5 && runtime.semaphore_signals == 6
                        && runtime.sif_dma_submissions == 5, "SCMD media-mode cleanup differs");
                if (stop_target) {
                    const std::array<std::array<std::uint32_t,2>,9> reached{{
                        {0x338158,0x1003B8},{0x332188,0x3381D4},{0x331B08,0x332190},
                        {0x32F810,0x331B10},{0x32E9D0,0x32F840},{0x32E990,0x331B20},
                        {0x337F88,0x33824C},{0x33A3F0,0x337FA4},{0x32EF30,0x337FB8}}};
                    require(std::any_of(reached.begin(),reached.end(),[&](const auto& entry) {
                        return entry[0] == stop_target && entry[1] == stop_source && stop_resume == stop_source + 8;
                    }), "Unexpected SCMD mapped-entry boundary");
                } else require(stop_selector == 0x6B && context.pc == 0x32EF38
                    && runtime.guest_calls == 214 && runtime.returns == 209
                    && context.r[29].low == runtime.initial_sp - 224 && GPR_U32(&context,4) == 1
                    && runtime.dmac_disable_calls == 1 && runtime.dmac_removals == 1
                    && runtime.dmac_mask == 0x800000 && runtime.dmac_handlers.empty()
                    && runtime.read32(runtime.memory().data(),0x36AA90) == 0
                    && runtime.read32(runtime.memory().data(),0x36AA8C) == 0,
                    "SIF shutdown and DMA-stop boundary differs");
            }
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write SCMD report");
            report << "{\"status\":\"blocked at checked runtime boundary\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc
                << ",\"selector\":" << stop_selector << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
                << ",\"resume_pc\":" << stop_resume << ",\"calls\":" << runtime.guest_calls << ",\"returns\":" << runtime.returns
                << ",\"words\":" << runtime.word_writes << ",\"doublewords\":" << runtime.doubleword_writes
                << ",\"bytes\":" << runtime.byte_writes << ",\"quads\":" << runtime.quadword_writes
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"deletions\":" << runtime.semaphore_deletions << ",\"dma_disables\":" << runtime.dmac_disable_calls
                << ",\"dma_removals\":" << runtime.dmac_removals << ",\"media_mode_calls\":" << runtime.scmd_mmode_calls
                << ",\"sp\":" << context.r[29].low << ",\"scmd_binds\":" << runtime.scmd_bind_calls << ",\"arguments\":[" << GPR_U32(&context,4) << "," << GPR_U32(&context,5) << "]"
                << ",\"descriptor_words\":[";
            for(unsigned i=0;i<((stop_selector==0x40 || stop_selector==0x77)?8u:0u);++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4);
            report << "],\"packet_words\":[";
            for(unsigned i=0;i<16;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3A97C0+i*4);
            report << "]}\n"; return 0;
        }
        if (bind_mode) {
            require(stopped && !stop_target && !stop_handler && runtime.sif_bind_callbacks == 1
                    && runtime.sif_bind_context_preservations == 1 && runtime.sif_dma_submissions == 2
                    && runtime.semaphore_waits == 2 && runtime.semaphore_signals == 2
                    && runtime.semaphore_deletions == 1 && runtime.startup_zero_verified
                    && runtime.syscall_registrations == 18 && runtime.custom_syscall_calls == 17
                    && runtime.kernel_word_writes == 668 && runtime.kernel_dispatch_validations == 12
                    && runtime.timer3_mode_reads == 1 && runtime.timer2_mode_reads == 2
                    && runtime.timer2_mode_writes == 1 && runtime.intc_mask == 0x1802
                    && runtime.dmac_mask == 0xA00000 && runtime.dmac_enable_calls == 1
                    && runtime.dma_status_reads == 1 && runtime.dma5_control_reads == 1
                    && runtime.sif_rpc_reads == 1 && runtime.sif_command_reads == 1,
                    "RPC bind continuation prerequisites/effects differ");
            require(runtime.read32(runtime.memory().data(), 0x3A87CC) == 0x3F570
                    && runtime.read32(runtime.memory().data(), 0x3A87BC) == 0x3FB20
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
                    && std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
                    "Bind response/persistent frame/kernel baseline differs");
            if (stop_selector == 0x77) require(context.pc == 0x32F028 && GPR_U32(&context, 5) == 2
                    && GPR_U32(&context, 4) == runtime.initial_sp - 544
                    && GPR_U32(&context, 29) == runtime.initial_sp - 544
                    && runtime.rpc_call_semaphores == 1 && runtime.semaphores.size() == 4
                    && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
                    && runtime.semaphores[3].option == 0x3838E8
                    && runtime.guest_calls == 146 && runtime.returns == 139
                    && runtime.word_writes == 1133 && runtime.doubleword_writes == 131
                    && runtime.byte_writes == 530 && runtime.quadword_writes == 43632
                    && runtime.uncached_word_accesses == 30 && runtime.uncached_byte_accesses == 4
                    && runtime.data_cache_lines == 6, "Unexpected first RPC call submission");
            else require(stop_selector == 0x40 && context.pc == 0x32EC68
                    && GPR_U32(&context, 4) == runtime.initial_sp - 400
                    && runtime.rpc_call_semaphores == 0 && runtime.semaphores.size() == 3,
                    "Unexpected RPC call semaphore checkpoint boundary");
            if (stop_selector == 0x77) {
                const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
                    0x203A97C0, 0x19600, 64, 0x44};
                const std::array<std::uint32_t, 16> packet{0x440, 0x3FB20, 0x8000000A, 0,
                    5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
                for (unsigned index = 0; index < descriptors.size(); ++index)
                    require(runtime.read32(runtime.memory().data(), runtime.initial_sp - 544 + index * 4) == descriptors[index],
                            "Pending first CD-init RPC descriptors differ");
                for (unsigned index = 0; index < packet.size(); ++index)
                    require(runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4) == packet[index],
                            "Pending first CD-init RPC packet differs");
            }
            require(context.hi == 0 && context.lo == 144 && context.hi1 == 0 && context.lo1 == 0
                    && context.cop0_status == 0x10000 && GPR_U32(&context, 28) == 0x38BDF0,
                    "CPU state changed across RPC continuation");
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write bind continuation report");
            report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
                << ",\"target\":" << stop_address << ",\"calls\":" << runtime.guest_calls
                << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
                << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
                << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
                << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
                << ",\"lo\":" << context.lo << ",\"bind_callbacks\":" << runtime.sif_bind_callbacks
                << ",\"bind_context_preservations\":" << runtime.sif_bind_context_preservations
                << ",\"semaphore_deletions\":" << runtime.semaphore_deletions
                << ",\"call_semaphore_unused_attr\":" << runtime.rpc_call_sema_attributes
                << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
                << ",\"descriptor_words\":[";
            for (unsigned index = 0; index < (stop_selector == 0x77 ? 8u : 6u); ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
            report << "],\"packet_words\":[";
            for (unsigned index = 0; index < 16; ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4);
            report << "]}\n";
            std::cout << "Generated RPC bind and cleanup returned; first CD-init RPC call reached a checked runtime boundary.\n";
            return 0;
        }
        require(stopped, "Entry did not stop at the expected checked boundary");
        require(runtime.startup_zero_verified && runtime.startup_quadword_writes == (zero_end - zero_start) / 16
                && runtime.startup_byte_writes == (zero_end - zero_start) % 16,
                "Entry zeroing did not complete before SetupThread");
        require(runtime.memory()[zero_start - 1] == image[zero_start - 1]
                && runtime.memory()[zero_end] == image[zero_end], "Entry zeroing crossed its bounds");
        require(runtime.quadword_writes == (zero_end - zero_start) / 16 + (timer_mode ? 512u : 0u)
                && runtime.byte_writes == (zero_end - zero_start) % 16 + (osd_readback_mode ? 512u : 0u) + (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected startup/fill store counts");
        require(context.hi == 0 && context.lo == (sif_dma_mode ? 144u : 0u) && context.hi1 == 0 && context.lo1 == 0
                && context.sa == 0 && context.fcr31 == 0 && context.f_acc == 0, "CPU startup state incorrect");
        require(std::all_of(context.f.begin(), context.f.end(), [](float value) { return value == 0; }), "FPR clearing failed");
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        require(runtime.thread_arguments == std::array<std::uint32_t, 5>{0x0038BDF0, 0xFFFFFFFF, 0x00100000, 0x0038B980, 0x00100250},
                "SetupThread arguments differ from the reviewed entry");
        require(runtime.thread_initialized && runtime.initial_sp == 0x01FFED60
                && runtime.stack_base == 0x01EFF000 && GPR_U32(&context, 29) == runtime.initial_sp - (sif_rpc_mode ? 464 : sif_dma_mode ? 96 : sif_mode ? 240 : osd_readback_mode ? 192 : readback_mode ? 128 : intc_mode ? 48 : timer2_mode ? 160 : timer_mode ? 112 : snapshot_mode ? 80 : 144),
                "SetupThread stack model was not applied");
        require(GPR_U32(&context, 28) == runtime.thread_arguments[0], "Unexpected gp after SetupThread");
        require(runtime.heap_initialized && runtime.heap_start == 0x0043058F
                && runtime.heap_end == runtime.stack_base,
                "Unexpected SetupHeap state or return");
        if (!snapshot_mode) require(runtime.guest_calls == 9 && runtime.custom_syscall_calls == 1 && runtime.returns == 5 && runtime.doubleword_writes == 10 && runtime.word_writes == 8
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
                "Generated callee did not save the caller return address");
        if (snapshot_mode) {
            require(runtime.guest_calls == (sif_rpc_mode ? 129u : sif_dma_mode ? 115u : sif_mode ? 108u : dma5_mode ? 105u : osd_readback_mode ? 99u : osd_mode ? 59u : readback_mode ? 58u : intc_mode ? 48u : timer2_mode ? 46u : timer_mode ? 43u : 12u) && runtime.custom_syscall_calls == (osd_readback_mode ? 17u : timer_mode ? 10u : 2u) && runtime.returns == (sif_rpc_mode ? 122u : sif_dma_mode ? 111u : sif_mode ? 102u : dma5_mode ? 100u : osd_readback_mode ? 95u : osd_mode ? 54u : readback_mode ? 53u : intc_mode ? 45u : timer_mode ? 41u : 10u)
                    && runtime.word_writes == (sif_rpc_mode ? 1093u : sif_dma_mode ? 1062u : sif_mode ? 1040u : dma5_mode ? 1031u : osd_readback_mode ? 1030u : osd_mode ? 701u : readback_mode ? 699u : timer2_mode ? 682u : timer_mode ? 681u : 9u) && runtime.doubleword_writes == (sif_rpc_mode ? 107u : sif_mode ? 76u : dma5_mode ? 69u : osd_readback_mode ? 66u : readback_mode ? 40u : intc_mode ? 26u : timer2_mode ? 24u : timer_mode ? 21u : 14u),
                    "Unexpected snapshot startup execution counts");
            require(runtime.read32(runtime.memory().data(), 0x0036AB40) == 0x80014D00,
                    "Two generated scans did not converge on the captured table base");
        }
        if (timer_mode) {
            require(runtime.intc_handlers.size() == 2 && runtime.intc_handler_order == std::vector<std::uint32_t>{2, 1}
                    && runtime.intc_handlers[0].cause == 12 && runtime.intc_handlers[0].address == 0x80076488
                    && runtime.intc_handlers[1].cause == 11 && runtime.intc_handlers[1].address == 0x00339118
                    && runtime.intc_handlers[1].arg == 0 && runtime.intc_handlers[1].gp == 0x0038BDF0
                    && runtime.read32(runtime.memory().data(), 0x0036BAD8) == 2 && context.cop0_status == (osd_readback_mode ? 0x10000u : 0u),
                    "Interrupt registration ID/state or generated status helper differs");
            require(runtime.timer3_mode_reads == 1, "Unexpected Timer 3 replay read count");
            require(runtime.kernel_word_writes == (osd_readback_mode ? 668u : 0x740 / 4) && runtime.cache_requests == (osd_readback_mode ? 6u : 2u) && runtime.instruction_cache_invalidations == (osd_readback_mode ? 2u : 1u),
                    "Unexpected copied kernel size/cache request count");
            require(runtime.kernel_dispatch_validations == (osd_readback_mode ? 12u : 6u), "Unexpected copied kernel dispatch count");
            std::array<bool, 4> registered_slots{};
            for (std::uint32_t index = 0; index < 4; ++index) {
                const auto slot = runtime.read32(runtime.memory().data(), 0x80076710 + index * 8);
                require(slot >= 0xFC && slot <= 0xFF && !registered_slots[slot - 0xFC] && runtime.syscall_handlers[slot]
                        == runtime.read32(runtime.memory().data(), 0x80076714 + index * 8),
                        "Generated kernel table lookup/registration mismatch");
                registered_slots[slot - 0xFC] = true;
            }
            require(runtime.read32(runtime.memory().data(), 0x80076730) == 0x12C
                    && runtime.read32(runtime.memory().data(), 0x80076734) == runtime.timer3_interrupt_handler
                    && runtime.timer3_interrupt_handler == 0x80076488 && runtime.timer3_interrupt_enable_requested
                    && runtime.extended_registrations == 1
                    && runtime.syscall_handlers[8] == runtime.read32(runtime.memory().data(), 0x8007673C),
                    "Completed kernel registrations differ from copied table");
            for (std::uint32_t offset = 0; offset < 0x740; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x80076000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036B328 + offset), "Kernel copy mismatch");
            for (std::uint32_t offset = 0; offset < 0x28; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x00082000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036BA68 + offset), "Main RAM copy mismatch");
        }
        if (osd_readback_mode) {
            for (std::uint32_t offset = 0; offset < 0x330; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x80075000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036BAF8 + offset), "Startup kernel payload copy differs");
            require(runtime.syscall_handlers[0x5B] == 0x80075000, "Startup kernel entry not registered");
        }
        if (osd_readback_mode) {
            require(runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1
                    && runtime.read32(runtime.memory().data(), 0x003A8830) == 1
                    && runtime.read32(runtime.memory().data(), 0x003A8838) == 0x003A8818
                    && runtime.read32(runtime.memory().data(), 0x003A8820) == 0x0036C148,
                    "Generated helper/semaphore list state differs");
            for (const auto offset : {0u, 4u, 12u, 16u, 20u})
                require(runtime.read32(runtime.memory().data(), 0x003A8818 + offset) == 0,
                        "Generated helper descriptor differs");
            for (std::uint32_t offset = 0x300; offset < 0x328; offset += 8) {
                const auto selector = runtime.read32(runtime.memory().data(), 0x80075000 + offset);
                const auto handler = runtime.read32(runtime.memory().data(), 0x80075004 + offset);
                require(runtime.syscall_handlers[selector] == handler, "Copied table registration differs");
            }
            require(runtime.read32(runtime.memory().data(), 0x0036BE70) == 0x80075330
                    && runtime.read32(runtime.memory().data(), 0x0036BAF0) == 0
                    && runtime.read32(runtime.memory().data(), 0x0036BAF4) == 0
                    && std::all_of(runtime.memory().begin() + 0x003AF0C8, runtime.memory().begin() + 0x003AF2C8,
                                   [](auto byte) { return byte == 0; }), "Generated startup cleanup differs");
        }
        require(runtime.dma_status_reads == (dma_status_mode ? 1u : 0u)
                && runtime.dma5_control_reads == (dma5_mode ? 1u : 0u), "Unexpected DMA checkpoint read counts");
        if (dma5_mode) {
            require(runtime.dmac_handlers.size() == 1 && runtime.dmac_handler_order == std::vector<std::uint32_t>{1}
                    && runtime.dmac_handlers[0].channel == 5 && runtime.dmac_handlers[0].address == 0x00331DF0
                    && runtime.dmac_handlers[0].arg == 0x20 && runtime.dmac_handlers[0].gp == 0x0038BDF0
                    && runtime.dmac_mask == 0x00A00000 && runtime.dmac_enable_calls == 1
                    && runtime.read32(runtime.memory().data(), 0x003A9594) == 1,
                    "Generated DMA registration/enable state differs");
        } else require(runtime.dmac_handlers.empty() && runtime.dmac_enable_calls == 0,
                       "DMA handler state advanced before checkpoint");
        require(runtime.sif_command_reads == (sif_mode ? 1u : 0u)
                && runtime.data_cache_lines == (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected SIF/cache checkpoint counts");
        if (sif_mode && !sif_dma_mode) {
            const auto frame = runtime.initial_sp - 240;
            const std::array<std::uint32_t, 4> descriptor{0x003A9580, 0x19600, 20, 0x44};
            for (unsigned index = 0; index < descriptor.size(); ++index)
                require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                        "Generated SIF DMA descriptor differs");
            require(runtime.read32(runtime.memory().data(), 0x003A95A0) == 0x19600
                    && runtime.read32(runtime.memory().data(), 0x003A9580) == 20
                    && runtime.read32(runtime.memory().data(), 0x003A9584) == 0
                    && runtime.read32(runtime.memory().data(), 0x003A9588) == 0x80000000
                    && runtime.read32(runtime.memory().data(), 0x003A9590) == 0x003A94C0
                    && runtime.read64(runtime.memory().data(), frame + 112) == 0x00331DA0
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 112) == 0x00332038
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                    "Generated SIF packet/current stack differs");
        }
        require(runtime.sif_dma_submissions == (sif_dma_mode ? 1u : 0u)
                && runtime.sif_rpc_reads == (sif_rpc_mode ? 1u : 0u)
                && runtime.uncached_word_accesses == (sif_rpc_mode ? 11u : 0u)
                && runtime.uncached_byte_accesses == (sif_rpc_mode ? 2u : 0u), "Unexpected receive/RPC/alias counts");
        if (sif_dma_mode) {
            const auto iop = runtime.sifIopMemory();
            const std::array<std::uint32_t, 5> consumed{0, 0, 0x80000000, 0, 0x3A94C0};
            require(iop.size() == PS2Runtime::sif_iop_size && runtime.sif_ee_receive_address == 0x3A94C0
                    && std::memcmp(iop.data() + 0x19600, consumed.data(), 20) == 0
                    && std::memcmp(iop.data() + 0x196C8, &consumed[4], 4) == 0,
                    "Bounded IOP receive effects differ from capture");
            const std::array<std::uint32_t, 4> handlers{0x3322E8, 0x3326C0, 0x3328E0, 0x3323F8};
            const std::array<unsigned, 4> ids{8, 9, 10, 12};
            for (unsigned index = 0; index < ids.size(); ++index) {
                const auto address = 0x3A95C0 + ids[index] * 12;
                require(runtime.read32(runtime.memory().data(), address) == handlers[index]
                        && runtime.read32(runtime.memory().data(), address + 4) == 0x3AAFC0
                        && runtime.read32(runtime.memory().data(), address + 8) == 0x38BDF0,
                        "Generated command handler registrations differ");
            }
            require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                    "Persistent main frame changed");
        }
        if (sif_rpc_mode) {
            const auto frame = runtime.initial_sp - 464;
            const std::array<std::uint32_t, 4> descriptor{0x203A97C0, 0x19600, 64, 0x44};
            for (unsigned index = 0; index < descriptor.size(); ++index)
                require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                        "Pending RPC DMA descriptor differs");
            require(runtime.read32(runtime.memory().data(), 0x3A97C0) == 64
                    && runtime.read32(runtime.memory().data(), 0x3A97C8) == 0x80000009
                    && runtime.read32(runtime.memory().data(), 0x3A97D8) == 2
                    && runtime.read32(runtime.memory().data(), 0x3A97DC) == 0x3A87A8
                    && runtime.read32(runtime.memory().data(), 0x3A97E0) == 0x80000592
                    && runtime.read32(runtime.memory().data(), 0x3A87B0) == 3
                    && runtime.semaphores.size() == 4 && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
                    && runtime.semaphores[3].option == 0x3838D8,
                    "Pending RPC bind packet/semaphore differs");
            require(runtime.read64(runtime.memory().data(), frame + 112) == 0x331DA0
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 336) == 0x33284C
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 224) == 0x31F984
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003AC,
                    "Active RPC bind frames differ");
        } else if (sif_dma_mode) require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003A4,
                                      "Active RPC initialization frame differs");
        require(std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
                "Captured kernel baseline changed");
        if (timer_mode) {
            const auto frame = runtime.initial_sp - (osd_readback_mode ? 80 : readback_mode ? 128 : intc_mode ? 48 : 112);
            if (!sif_mode) require(runtime.read64(runtime.memory().data(), frame + (osd_readback_mode ? 48 : readback_mode ? 32 : intc_mode ? 16 : 80)) == (osd_readback_mode ? 0x00338568 : readback_mode ? 0x00338660 : intc_mode ? 0x00338550 : 0x00338548)
                    && (osd_readback_mode
                        ? runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                          && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
                        : runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0),
                    "Current startup frames did not save return addresses");
            if (!intc_mode) for (const auto offset : {0u, 16u, 32u, 48u, 64u})
                require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved startup register");
            for (std::uint32_t index = 0; index < 128; ++index) {
                const auto block = 0x003ACCC0 + index * 64;
                require(runtime.read32(runtime.memory().data(), block) == (index == 127 ? 0 : block + 64),
                        "Generated buffer links differ from observed loop");
                require(std::all_of(runtime.memory().begin() + block + 4, runtime.memory().begin() + block + 64,
                                   [](auto byte) { return byte == 0; }), "Generated buffer payload is not zero");
            }
        } else {
            const auto frame = runtime.initial_sp - 144;
            require(runtime.read64(runtime.memory().data(), frame + 112) == (snapshot_mode ? 0x00338540 : 0x00338538),
                    "Current frame did not save return address");
            for (const auto offset : {0u, 16u, 32u, 48u, 64u, 80u, 96u})
                require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved callee register");
        }
        require(runtime.syscall_registrations == (osd_readback_mode ? 18u : timer_mode ? 10u : 2u) && runtime.syscall_handlers[0x83] == 0x00338388
                && runtime.syscall_handlers[0x5A] == (osd_readback_mode ? 0x0033A7C8u : timer_mode ? 0x003389B8u : 0x00338350u),
                "Expected syscall registrations missing");
        if (!snapshot_mode) require(args[0] == 0x80000000 && args[1] == 0x80080000 && args[2] == 0x00338388,
                "Unexpected reached guest call arguments");
        require(runtime.semaphores.size() == (sif_rpc_mode ? 4u : readback_mode ? 3u : 2u)
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 1
                && runtime.semaphores[0].option == 0x00383BF0
                && runtime.semaphores[1].count == 1 && runtime.semaphores[1].maximum == 1
                && runtime.semaphores[1].option == 0x00383C00,
                "Unexpected created semaphore state");
        if (readback_mode) {
            require(runtime.threads.size() == 1 && runtime.current_thread_id == 1, "Dormant thread not recorded");
            const auto& thread = runtime.threads[0];
            require(thread.id == 2 && thread.entry == 0x0032F960 && thread.stack_base == 0x003A8840
                    && thread.stack_size == 0x400 && thread.gp == 0x0038BDF0 && thread.initial_priority == 0
                    && thread.heap_base == runtime.heap_start && thread.context_address == 0x003A89A0
                    && thread.initial_sp == 0x003A8C20
                    && runtime.read32(runtime.memory().data(), 0x0036AA84) == 2,
                    "Generated CreateThread descriptor/return differs");
            for (const auto index : {28u, 29u, 30u, 31u}) {
                const auto expected = index == 28 ? thread.gp : index == 31 ? 0x1FC03000 : thread.initial_sp;
                require(runtime.read32(runtime.memory().data(), thread.context_address + index * 16) == expected,
                        "Dormant thread context initialization differs");
            }
        }
        if (readback_mode) require(runtime.semaphores[2].count == 0 && runtime.semaphores[2].maximum == 255
                && runtime.semaphores[2].option == 0x00383660
                && runtime.read32(runtime.memory().data(), 0x003A8C40) == 2,
                "Generated semaphore creation/result differs");
        require(runtime.read32(runtime.memory().data(), 0x0036AB58) == 0
                && runtime.read32(runtime.memory().data(), 0x0036AB5C) == 1,
                "Generated code did not store semaphore IDs");
        std::ofstream report(argv[2]);
        if (!report) throw std::runtime_error("Could not write entry stop report");
        report << "{\"status\":\"" << (dma5_mode ? "blocked at unsupported syscall" : osd_readback_mode ? "blocked at unsupported guest memory" : readback_mode ? "blocked at unsupported syscall" : intc_mode ? "blocked at unsupported guest memory" : timer2_mode ? "blocked at unsupported syscall" : "blocked at unsupported guest memory") << "\",\"entry_executed\":true,"
               << "\"target\":" << stop_target << ",\"source_pc\":" << stop_source << ",\"resume_pc\":" << stop_resume << ","
               << "\"selector\":" << stop_selector << ","
               << "\"kernel_word_writes\":" << runtime.kernel_word_writes << ",\"access\":\"" << (dma5_mode ? "syscall" : osd_readback_mode ? "read" : readback_mode ? "syscall" : intc_mode ? "read" : timer2_mode ? "syscall" : "read") << "\",\"address\":" << stop_address << ",\"size\":" << stop_size
               << ",\"timer3_interrupt_handler\":" << runtime.timer3_interrupt_handler
               << ",\"timer3_interrupt_enable_requested\":" << (runtime.timer3_interrupt_enable_requested ? "true" : "false")
               << ",\"extended_registrations\":" << runtime.extended_registrations
               << ",\"startup_zero_verified\":" << (runtime.startup_zero_verified ? "true" : "false")
               << ",\"kernel_dispatch_validations\":" << runtime.kernel_dispatch_validations
               << ",\"sif_dma_submissions\":" << runtime.sif_dma_submissions << ",\"sif_rpc_reads\":" << runtime.sif_rpc_reads
               << ",\"uncached_word_accesses\":" << runtime.uncached_word_accesses << ",\"uncached_byte_accesses\":" << runtime.uncached_byte_accesses
               << ",\"sif_command_reads\":" << runtime.sif_command_reads << ",\"data_cache_lines\":" << runtime.data_cache_lines
               << ",\"dma_status_reads\":" << runtime.dma_status_reads << ",\"dma5_control_reads\":" << runtime.dma5_control_reads
               << ",\"dmac_handlers\":" << runtime.dmac_handlers.size() << ",\"dmac_mask\":" << runtime.dmac_mask
               << ",\"dmac_enable_calls\":" << runtime.dmac_enable_calls
               << ",\"cache_requests\":" << runtime.cache_requests << ",\"instruction_cache_invalidations\":" << runtime.instruction_cache_invalidations << ",\"syscall_selector\":" << (osd_readback_mode ? stop_selector : GPR_U32(&context, 3))
               << ",\"write_value\":" << (osd_readback_mode ? 0 : runtime.timer2_mode_writes ? 0x382 : 0)
               << ",\"osd_config_reads\":" << runtime.osd_config_reads << ",\"osd_config_sets\":" << runtime.osd_config_sets
               << ",\"main_thread_priority\":" << runtime.main_thread_priority << ",\"priority_changes\":" << runtime.priority_changes
               << ",\"intc_mask\":" << runtime.intc_mask << ",\"intc_enable_calls\":" << runtime.intc_enable_calls
               << ",\"timer2_mode_writes\":" << runtime.timer2_mode_writes
               << ",\"timer2_mode_after_write\":" << runtime.timer2_mode_after_write
               << ",\"timer2_mode_reads\":" << runtime.timer2_mode_reads
               << ",\"timer3_mode_reads\":" << runtime.timer3_mode_reads << ",\"kernel_snapshot_loaded\":" << (snapshot_mode ? "true" : "false") << ",\"handler\":" << (timer_mode ? stop_handler : runtime.syscall_handlers[0x83]) << ",\"pc\":" << context.pc << ",\"arguments\":[";
        for (unsigned index = 0; index < args.size(); ++index) report << (index ? "," : "") << args[index];
        report << "],\"quadword_writes\":" << runtime.quadword_writes
               << ",\"guest_calls\":" << runtime.guest_calls << ",\"doubleword_writes\":" << runtime.doubleword_writes
               << ",\"semaphore_waits\":" << runtime.semaphore_waits << ",\"semaphore_signals\":" << runtime.semaphore_signals
               << ",\"semaphores_created\":" << runtime.semaphores.size() << ",\"returns\":" << runtime.returns
               << ",\"syscall_registrations\":" << runtime.syscall_registrations
               << ",\"word_writes\":" << runtime.word_writes
               << ",\"byte_writes\":" << runtime.byte_writes << ",\"game_booted\":false}\n";
        report.close();
        require(!report.fail(), "Entry stop report write failed");
        std::cout << "BT2 entry CPU initialization, zeroing and bootstrap state verified.\n";
        if (sif_rpc_mode) std::cout << "Generated initialization returned; native RPC bind packet reached unsupported IOP service 0x80000592.\n";
        else if (sif_dma_mode) std::cout << "Validated CHANGE_SADDR receive effects completed; reached RPCINIT query selector 0x80000002.\n";
        else if (sif_mode) std::cout << "Captured SIF query returned; generated descriptor/cache path reached SifSetDma syscall 0x77.\n";
        else if (dma5_mode) std::cout << "DMA checkpoints replayed; channel 5 handler/enable request recorded; reached SifGetReg selector 0x80000000.\n";
        else if (dma_status_mode) std::cout << "Captured D_STAT replayed; reached channel 5 control read 0x1000C000.\n";
        else if (osd_readback_mode) std::cout << "Generated helper tail chain returned; DMA initialization reached unsupported D_STAT read 0x1000E010.\n";
        else if (osd_mode) std::cout << "Captured OSD configuration returned; reached SetOsdConfigParam syscall 0x4A.\n";
        else if (readback_mode) std::cout << "Dormant thread ID 2 created; generated setup helper returned; main priority changed to 1; reached GetOsdConfigParam syscall 0x4B.\n";
        else if (intc_mode) std::cout << "EnableIntc changed cause 11; timer startup returned; stopped at a second Timer 2 read, PC 0x00338DD8.\n";
        else if (timer2_mode) std::cout << "Timer 2 startup mode write completed; generated ErrorEPC/ERET returned; stopped at EnableIntc syscall 0x14, cause 11.\n";
        else if (timer_mode) std::cout << "Interrupt handler ID 2 stored; generated status helper returned; stopped at Timer 2 MMIO read 0x10001010 at PC 0x00338C58.\n";
        else if (snapshot_mode) std::cout << "Snapshot scans returned; FUN_00338A00 reached unsupported MMIO read 0x"
                                     << std::hex << stop_address << " at PC 0x" << context.pc << std::dec << "\n";
        else std::cout << "No kernel snapshot supplied; stopped at first kernel read 0x80000000.\n";
        std::cout << "Game boot remains incomplete.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Entry probe failed: " << error.what() << '\n';
        return 1;
    }
}
