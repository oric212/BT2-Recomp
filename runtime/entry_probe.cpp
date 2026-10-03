#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
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
        if (argc != 3) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT");
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(PS2Runtime::ram_size))
            throw std::runtime_error("Invalid prepared guest image size");
        std::vector<std::uint8_t> image(PS2Runtime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size())))
            throw std::runtime_error("Could not read guest image");
        PS2Runtime runtime;
        runtime.loadMemory(image);
        constexpr std::uint32_t zero_start = 0x00387E80, zero_end = 0x0043058F;
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
        bool stopped = false;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestBranch& stop) {
            require(stop.target == 0x003383C8 && stop.source_pc == 0x00338464 && stop.resume_pc == 0x0033846C, "Unexpected entry dispatch boundary");
            stop_target = stop.target;
            stop_source = stop.source_pc;
            stop_resume = stop.resume_pc;
            stopped = true;
        }
        require(stopped, "Entry did not stop at the unsupported guest branch");
        require(std::all_of(runtime.memory().begin() + zero_start, runtime.memory().begin() + zero_end,
                            [](std::uint8_t value) { return value == 0; }), "Entry zeroing was incomplete");
        require(runtime.memory()[zero_start - 1] == image[zero_start - 1]
                && runtime.memory()[zero_end] == image[zero_end], "Entry zeroing crossed its bounds");
        require(runtime.quadword_writes == (zero_end - zero_start) / 16
                && runtime.byte_writes == (zero_end - zero_start) % 16, "Unexpected startup store counts");
        require(context.hi == 0 && context.lo == 0 && context.hi1 == 0 && context.lo1 == 0
                && context.sa == 0 && context.fcr31 == 0 && context.f_acc == 0, "CPU startup state incorrect");
        require(std::all_of(context.f.begin(), context.f.end(), [](float value) { return value == 0; }), "FPR clearing failed");
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        require(runtime.thread_arguments == std::array<std::uint32_t, 5>{0x0038BDF0, 0xFFFFFFFF, 0x00100000, 0x0038B980, 0x00100250},
                "SetupThread arguments differ from the reviewed entry");
        require(runtime.thread_initialized && runtime.initial_sp == 0x01FFED60
                && runtime.stack_base == 0x01EFF000 && GPR_U32(&context, 29) == runtime.initial_sp - 144,
                "SetupThread stack model was not applied");
        require(GPR_U32(&context, 28) == runtime.thread_arguments[0], "Unexpected gp after SetupThread");
        require(runtime.heap_initialized && runtime.heap_start == 0x0043058F
                && runtime.heap_end == runtime.stack_base,
                "Unexpected SetupHeap state or return");
        require(runtime.guest_calls == 7 && runtime.returns == 5 && runtime.doubleword_writes == 10 && runtime.word_writes == 8
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
                "Generated callee did not save the caller return address");
        const auto frame = runtime.initial_sp - 144;
        require(runtime.read64(runtime.memory().data(), frame + 112) == 0x00338538,
                "Current frame did not save return address");
        for (const auto offset : {0u, 16u, 32u, 48u, 64u, 80u, 96u})
            require(runtime.read64(runtime.memory().data(), frame + offset) == 0,
                    "Unexpected saved callee register");
        require(runtime.syscall_registrations == 2 && runtime.syscall_handlers[0x83] == 0x00338388
                && runtime.syscall_handlers[0x5A] == 0x00338350,
                "Expected syscall registrations missing");
        require(args[0] == 0x80000000 && args[1] == 0x80080000 && args[2] == 0x00338388,
                "Unexpected reached guest call arguments");
        require(runtime.semaphores.size() == 2
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 1
                && runtime.semaphores[0].option == 0x00383BF0
                && runtime.semaphores[1].count == 1 && runtime.semaphores[1].maximum == 1
                && runtime.semaphores[1].option == 0x00383C00,
                "Unexpected created semaphore state");
        require(runtime.read32(runtime.memory().data(), 0x0036AB58) == 0
                && runtime.read32(runtime.memory().data(), 0x0036AB5C) == 1,
                "Generated code did not store semaphore IDs");
        std::ofstream report(argv[2]);
        if (!report) throw std::runtime_error("Could not write entry stop report");
        report << "{\"status\":\"blocked at unsupported guest branch\",\"entry_executed\":true,"
               << "\"target\":" << stop_target << ",\"source_pc\":" << stop_source
               << ",\"resume_pc\":" << stop_resume << ",\"pc\":" << context.pc << ",\"arguments\":[";
        for (unsigned index = 0; index < args.size(); ++index) report << (index ? "," : "") << args[index];
        report << "],\"quadword_writes\":" << runtime.quadword_writes
               << ",\"guest_calls\":" << runtime.guest_calls << ",\"doubleword_writes\":" << runtime.doubleword_writes
               << ",\"semaphores_created\":" << runtime.semaphores.size() << ",\"returns\":" << runtime.returns
               << ",\"syscall_registrations\":" << runtime.syscall_registrations
               << ",\"word_writes\":" << runtime.word_writes
               << ",\"byte_writes\":" << runtime.byte_writes << ",\"game_booted\":false}\n";
        report.close();
        require(!report.fail(), "Entry stop report write failed");
        std::cout << "BT2 generated entry executed: CPU initialization and guest zeroing verified.\n"
                  << "SetupThread/SetupHeap models applied; executed nested startup code; created two semaphore objects and returned; executed FUN_00338410 prologue; registered two syscall handlers; stopped at FUN_003383C8.\n"
                  << "Empty launch arguments; retail BIOS comparison and game boot remain pending.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Entry probe failed: " << error.what() << '\n';
        return 1;
    }
}
