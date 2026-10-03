#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#ifdef BT2_KERNEL_PAYLOAD
#include <bt2_kernel_functions.h>
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
        if (argc != 3 && argc != 4 && argc != 5 && argc != 6 && argc != 7 && argc != 8) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT [KERNEL_SNAPSHOT [TIMER3_CAPTURE_WORD [TIMER2_CAPTURE_WORD [INTC_MASK_WORD [TIMER2_READBACK_WORD]]]]]");
        const bool snapshot_mode = argc >= 4;
        const bool timer_mode = argc >= 5;
        const bool timer2_mode = argc >= 6;
        const bool intc_mode = argc >= 7;
        const bool readback_mode = argc == 8;
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
        runtime.registerFunction(0x0032FA38, FUN_0032FA38_0x32fa38);
        runtime.registerFunction(0x0032EA60, FUN_0032EA60_0x32ea60);
        runtime.registerFunction(0x00338DB8, FUN_00338DB8_0x338db8);
        runtime.registerFunction(0x0032F7A8, FUN_0032F7A8_0x32f7a8);
        runtime.registerFunction(0x0032E9A0, FUN_0032E9A0_0x32e9a0);
        runtime.registerFunction(0x00338B58, FUN_00338B58_0x338b58);
        runtime.registerFunction(0x00338AD8, FUN_00338AD8_0x338ad8);
        runtime.registerFunction(0x00338268, FUN_00338268_0x338268);
        runtime.registerFunction(0x0032E950, FUN_0032E950_0x32e950);
        if (timer_mode) {
#ifdef BT2_KERNEL_PAYLOAD
            runtime.registerKernelFunction(0x80076000, FUN_80076000_0x80076000,
                std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036B328, 0x740));
#else
            throw std::runtime_error("Timer replay requires generated kernel payload integration");
#endif
        }
        bool stopped = false;
        std::uint32_t stop_address = 0;
        std::size_t stop_size = 0;
        std::uint32_t stop_selector = 0, stop_handler = 0;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestMemory& stop) {
            if (readback_mode) throw;
            if (intc_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338DD8
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
            if (readback_mode) require(stop.selector == 0x20 && !stop.handler && stop.resume_pc == 0x0032EA68
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 160
                    && runtime.timer2_mode_reads == 2 && runtime.timer2_mode_writes == 1
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1,
                    "Unexpected CreateThread boundary after Timer 2 readback");
            else require(!intc_mode && timer2_mode && stop.selector == 0x14 && !stop.handler && stop.resume_pc == 0x0032E9A8
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 11
                    && runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382
                    && context.cop0_errorepc == 0x00338C90 && !context.ll_bit,
                    "Unexpected syscall after generated Timer 2 write/ERET");
            stop_selector = stop.selector;
            stopped = true;
        }
        require(stopped, "Entry did not stop at the expected checked boundary");
        require(runtime.startup_zero_verified && runtime.startup_quadword_writes == (zero_end - zero_start) / 16
                && runtime.startup_byte_writes == (zero_end - zero_start) % 16,
                "Entry zeroing did not complete before SetupThread");
        require(runtime.memory()[zero_start - 1] == image[zero_start - 1]
                && runtime.memory()[zero_end] == image[zero_end], "Entry zeroing crossed its bounds");
        require(runtime.quadword_writes == (zero_end - zero_start) / 16 + (timer_mode ? 512u : 0u)
                && runtime.byte_writes == (zero_end - zero_start) % 16, "Unexpected startup/fill store counts");
        require(context.hi == 0 && context.lo == 0 && context.hi1 == 0 && context.lo1 == 0
                && context.sa == 0 && context.fcr31 == 0 && context.f_acc == 0, "CPU startup state incorrect");
        require(std::all_of(context.f.begin(), context.f.end(), [](float value) { return value == 0; }), "FPR clearing failed");
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        require(runtime.thread_arguments == std::array<std::uint32_t, 5>{0x0038BDF0, 0xFFFFFFFF, 0x00100000, 0x0038B980, 0x00100250},
                "SetupThread arguments differ from the reviewed entry");
        require(runtime.thread_initialized && runtime.initial_sp == 0x01FFED60
                && runtime.stack_base == 0x01EFF000 && GPR_U32(&context, 29) == runtime.initial_sp - (readback_mode ? 160 : intc_mode ? 48 : timer2_mode ? 160 : timer_mode ? 112 : snapshot_mode ? 80 : 144),
                "SetupThread stack model was not applied");
        require(GPR_U32(&context, 28) == runtime.thread_arguments[0], "Unexpected gp after SetupThread");
        require(runtime.heap_initialized && runtime.heap_start == 0x0043058F
                && runtime.heap_end == runtime.stack_base,
                "Unexpected SetupHeap state or return");
        if (!snapshot_mode) require(runtime.guest_calls == 9 && runtime.custom_syscall_calls == 1 && runtime.returns == 5 && runtime.doubleword_writes == 10 && runtime.word_writes == 8
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
                "Generated callee did not save the caller return address");
        if (snapshot_mode) {
            require(runtime.guest_calls == (readback_mode ? 51u : intc_mode ? 48u : timer2_mode ? 46u : timer_mode ? 43u : 12u) && runtime.custom_syscall_calls == (timer_mode ? 10u : 2u) && runtime.returns == (readback_mode ? 47u : intc_mode ? 45u : timer_mode ? 41u : 10u)
                    && runtime.word_writes == (readback_mode ? 692u : timer2_mode ? 682u : timer_mode ? 681u : 9u) && runtime.doubleword_writes == (readback_mode ? 30u : intc_mode ? 26u : timer2_mode ? 24u : timer_mode ? 21u : 14u),
                    "Unexpected snapshot startup execution counts");
            require(runtime.read32(runtime.memory().data(), 0x0036AB40) == 0x80014D00,
                    "Two generated scans did not converge on the captured table base");
        }
        if (timer_mode) {
            require(runtime.intc_handlers.size() == 2 && runtime.intc_handler_order == std::vector<std::uint32_t>{2, 1}
                    && runtime.intc_handlers[0].cause == 12 && runtime.intc_handlers[0].address == 0x80076488
                    && runtime.intc_handlers[1].cause == 11 && runtime.intc_handlers[1].address == 0x00339118
                    && runtime.intc_handlers[1].arg == 0 && runtime.intc_handlers[1].gp == 0x0038BDF0
                    && runtime.read32(runtime.memory().data(), 0x0036BAD8) == 2 && context.cop0_status == 0,
                    "Interrupt registration ID/state or generated status helper differs");
            require(runtime.timer3_mode_reads == 1, "Unexpected Timer 3 replay read count");
            require(runtime.kernel_word_writes == 0x740 / 4 && runtime.cache_requests == 2 && runtime.instruction_cache_invalidations == 1,
                    "Unexpected copied kernel size/cache request count");
            require(runtime.kernel_dispatch_validations == 6, "Unexpected copied kernel dispatch count");
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
        require(std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
                "Captured kernel baseline changed");
        if (timer_mode) {
            const auto frame = runtime.initial_sp - (readback_mode ? 160 : intc_mode ? 48 : 112);
            require(runtime.read64(runtime.memory().data(), frame + (readback_mode ? 128 : intc_mode ? 16 : 80)) == (readback_mode ? 0x00338558 : intc_mode ? 0x00338550 : 0x00338548)
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
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
        require(runtime.syscall_registrations == (timer_mode ? 10u : 2u) && runtime.syscall_handlers[0x83] == 0x00338388
                && runtime.syscall_handlers[0x5A] == (timer_mode ? 0x003389B8u : 0x00338350u),
                "Expected syscall registrations missing");
        if (!snapshot_mode) require(args[0] == 0x80000000 && args[1] == 0x80080000 && args[2] == 0x00338388,
                "Unexpected reached guest call arguments");
        require(runtime.semaphores.size() == (readback_mode ? 3u : 2u)
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 1
                && runtime.semaphores[0].option == 0x00383BF0
                && runtime.semaphores[1].count == 1 && runtime.semaphores[1].maximum == 1
                && runtime.semaphores[1].option == 0x00383C00,
                "Unexpected created semaphore state");
        if (readback_mode) require(runtime.semaphores[2].count == 0 && runtime.semaphores[2].maximum == 255
                && runtime.semaphores[2].option == 0x00383660
                && runtime.read32(runtime.memory().data(), 0x003A8C40) == 2,
                "Generated semaphore creation/result differs");
        require(runtime.read32(runtime.memory().data(), 0x0036AB58) == 0
                && runtime.read32(runtime.memory().data(), 0x0036AB5C) == 1,
                "Generated code did not store semaphore IDs");
        std::ofstream report(argv[2]);
        if (!report) throw std::runtime_error("Could not write entry stop report");
        report << "{\"status\":\"" << (readback_mode ? "blocked at unsupported syscall" : intc_mode ? "blocked at unsupported guest memory" : timer2_mode ? "blocked at unsupported syscall" : "blocked at unsupported guest memory") << "\",\"entry_executed\":true,"
               << "\"target\":" << stop_target << ",\"source_pc\":" << stop_source << ",\"resume_pc\":" << stop_resume << ","
               << "\"selector\":" << stop_selector << ","
               << "\"kernel_word_writes\":" << runtime.kernel_word_writes << ",\"access\":\"" << (readback_mode ? "syscall" : intc_mode ? "read" : timer2_mode ? "syscall" : "read") << "\",\"address\":" << stop_address << ",\"size\":" << stop_size
               << ",\"timer3_interrupt_handler\":" << runtime.timer3_interrupt_handler
               << ",\"timer3_interrupt_enable_requested\":" << (runtime.timer3_interrupt_enable_requested ? "true" : "false")
               << ",\"extended_registrations\":" << runtime.extended_registrations
               << ",\"startup_zero_verified\":" << (runtime.startup_zero_verified ? "true" : "false")
               << ",\"kernel_dispatch_validations\":" << runtime.kernel_dispatch_validations
               << ",\"cache_requests\":" << runtime.cache_requests << ",\"instruction_cache_invalidations\":" << runtime.instruction_cache_invalidations << ",\"syscall_selector\":" << GPR_U32(&context, 3)
               << ",\"write_value\":" << (runtime.timer2_mode_writes ? 0x382 : 0)
               << ",\"intc_mask\":" << runtime.intc_mask << ",\"intc_enable_calls\":" << runtime.intc_enable_calls
               << ",\"timer2_mode_writes\":" << runtime.timer2_mode_writes
               << ",\"timer2_mode_after_write\":" << runtime.timer2_mode_after_write
               << ",\"timer2_mode_reads\":" << runtime.timer2_mode_reads
               << ",\"timer3_mode_reads\":" << runtime.timer3_mode_reads << ",\"kernel_snapshot_loaded\":" << (snapshot_mode ? "true" : "false") << ",\"handler\":" << (timer_mode ? stop_handler : runtime.syscall_handlers[0x83]) << ",\"pc\":" << context.pc << ",\"arguments\":[";
        for (unsigned index = 0; index < args.size(); ++index) report << (index ? "," : "") << args[index];
        report << "],\"quadword_writes\":" << runtime.quadword_writes
               << ",\"guest_calls\":" << runtime.guest_calls << ",\"doubleword_writes\":" << runtime.doubleword_writes
               << ",\"semaphores_created\":" << runtime.semaphores.size() << ",\"returns\":" << runtime.returns
               << ",\"syscall_registrations\":" << runtime.syscall_registrations
               << ",\"word_writes\":" << runtime.word_writes
               << ",\"byte_writes\":" << runtime.byte_writes << ",\"game_booted\":false}\n";
        report.close();
        require(!report.fail(), "Entry stop report write failed");
        std::cout << "BT2 entry CPU initialization, zeroing and bootstrap state verified.\n";
        if (readback_mode) std::cout << "Timer 2 modeled readback matched capture; next initialization created semaphore ID 2 and reached CreateThread syscall 0x20.\n";
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
