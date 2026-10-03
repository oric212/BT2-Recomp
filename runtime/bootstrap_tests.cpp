#include "ps2_runtime_macros.h"
#include <iostream>
#include <algorithm>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("Bootstrap helper test failed"); }
template<class Action> void reject(Action action) {
    bool caught = false;
    try { action(); } catch (const std::runtime_error&) { caught = true; }
    require(caught);
}
void return_leaf(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    runtime->dispatchGuestBranch(ram, ctx, GPR_U32(ctx, 31), ctx->pc, 0,
                                PS2Runtime::GuestBranchKind::Return, "synthetic return");
}
void nested_call(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    const auto saved_ra = GPR_U32(ctx, 31);
    SET_GPR_U32(ctx, 31, 0x204);
    runtime->dispatchGuestBranch(ram, ctx, 0x300, 0x200, 0x204,
                                PS2Runtime::GuestBranchKind::IndirectCall, "synthetic indirect call");
    SET_GPR_U32(ctx, 31, saved_ra);
    return_leaf(ram, ctx, runtime);
}
void bad_return(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    runtime->dispatchGuestBranch(ram, ctx, 0xBAD, ctx->pc, 0,
                                PS2Runtime::GuestBranchKind::Return, "synthetic invalid return");
}
void tail_call(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    runtime->dispatchGuestBranch(ram, ctx, 0x300, ctx->pc, 0,
                                PS2Runtime::GuestBranchKind::TailJump, "synthetic tail");
}

}
int main() {
    try {
        const auto left = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{UINT32_MAX, 0, 0x80000000, 1});
        const auto right = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{1, 2, 0x80000000, 2});
        require(std::bit_cast<std::array<std::uint32_t, 4>>(ps2_paddu32(left, right))
                == std::array<std::uint32_t, 4>{UINT32_MAX, 2, UINT32_MAX, 3});
        R5900Context context{};
        context.r[0] = left;
        SET_GPR_VEC(&context, 0, right);
        require(GPR_VEC(&context, 0).low == 0 && context.r[0].low == left.low);
        PS2Runtime runtime;
        std::vector<std::uint8_t> image(PS2Runtime::ram_size, 0xA5);
        runtime.loadMemory(image);
        auto* memory = runtime.memory().data();
        runtime.write128(memory, 16, right);
        require(runtime.read32(memory, 16) == 1 && memory[15] == 0xA5 && memory[32] == 0xA5);
        runtime.write8(memory, static_cast<std::uint32_t>(PS2Runtime::ram_size - 1), 7);
        require(runtime.memory().back() == 7);
        reject([&] { runtime.write8(memory, static_cast<std::uint32_t>(PS2Runtime::ram_size), 0); });
        reject([&] { runtime.write128(memory, static_cast<std::uint32_t>(PS2Runtime::ram_size - 8), right); });
        reject([&] { runtime.write128(memory, 1, right); });
        reject([&] { (void)runtime.read32(memory, 1); });
        reject([&] { (void)runtime.read32(memory, 0x10000000); });
        reject([&] { runtime.write8(nullptr, 0, 0); });
        context.r[3].low = 0x3C;
        context.r[5].low = UINT32_MAX;
        context.r[6].low = 0x100000;
        context.r[7].low = 0x100;
        context.r[2].high = 0x1234;
        context.r[6].low = UINT32_MAX;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.thread_initialized && memory[0x100] == 0xA5);
        context.r[6].low = 0x100000;
        context.r[7].low = PS2Runtime::ram_size - 4;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.thread_initialized);
        context.r[7].low = 0x101;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[7].low = 0x100;
        context.r[5].low = 0x100000;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[5].low = UINT32_MAX;
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == 0x01FFED60 && context.r[2].high == 0x1234
                && runtime.stack_base == 0x01EFF000 && runtime.read32(memory, 0x100) == 0
                && runtime.read32(memory, 0x104) == 0 && memory[0x108] == 0xA5);
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[3].low = 0x3D;
        context.r[4].low = runtime.stack_base;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(!runtime.heap_initialized);
        context.r[4].low = 0x43058F;
        context.r[5].low = 0x1000;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[5].low = UINT32_MAX;
        runtime.handleSyscall(memory, &context, 0);
        require(runtime.heap_start == 0x43058F && runtime.heap_end == 0x1EFF000
                && context.r[2].low == runtime.heap_end && context.r[2].high == 0x1234);
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        PS2Runtime uninitialized;
        reject([&] { uninitialized.handleSyscall(nullptr, &context, 0); });
        runtime.write32(memory, 0x170, 0x89ABCDEF);
        require(runtime.read32(memory, 0x170) == 0x89ABCDEF && memory[0x174] == 0xA5);
        reject([&] { runtime.write32(memory, 1, 0); });
        reject([&] { runtime.write32(memory, PS2Runtime::ram_size - 2, 0); });
        runtime.write64(memory, 0x180, UINT64_C(0xFEDCBA9876543210));
        require(runtime.read64(memory, 0x180) == UINT64_C(0xFEDCBA9876543210) && memory[0x188] == 0xA5);
        reject([&] { runtime.write64(memory, 3, 0); });
        reject([&] { (void)runtime.read64(memory, PS2Runtime::ram_size - 4); });
        runtime.registerFunction(0x200, nested_call);
        runtime.registerFunction(0x300, return_leaf);
        runtime.registerFunction(0x400, bad_return);
        runtime.registerFunction(0x500, tail_call);
        SET_GPR_U32(&context, 31, 0x104);
        require(runtime.dispatchGuestBranch(memory, &context, 0x200, 0x100, 0x104,
                PS2Runtime::GuestBranchKind::DirectCall, "synthetic call") && context.pc == 0x104);
        require(runtime.returns == 2 && runtime.guest_calls == 2);
        reject([&] { runtime.dispatchGuestBranch(memory, &context, 0x400, 0x100, 0x104,
                     PS2Runtime::GuestBranchKind::DirectCall, "bad callee"); });
        SET_GPR_U32(&context, 31, 0x104);
        require(runtime.dispatchGuestBranch(memory, &context, 0x500, 0x100, 0x104,
                PS2Runtime::GuestBranchKind::DirectCall, "tail callee") && context.pc == 0x104);
        context.r[3].low = 0x40;
        context.r[4].low = 0x600;
        context.r[2].high = 0x1234;
        runtime.write32(memory, 0x600, UINT32_MAX); // Output-only count ignored.
        runtime.write32(memory, 0x604, 2);
        runtime.write32(memory, 0x608, 1);
        runtime.write32(memory, 0x60C, UINT32_MAX); // Output-only wait count ignored.
        runtime.write32(memory, 0x610, 0);
        runtime.write32(memory, 0x614, 0xDEADBEEF);
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == 0 && context.r[2].high == 0x1234
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 2
                && runtime.semaphores[0].option == 0xDEADBEEF && runtime.read32(memory, 0x600) == UINT32_MAX);
        runtime.write32(memory, 0x608, 3);
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == UINT64_MAX && runtime.semaphores.size() == 1);
        runtime.write32(memory, 0x608, 1);
        runtime.write32(memory, 0x610, 1);
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        runtime.write32(memory, 0x610, 0);
        context.r[4].low = PS2Runtime::ram_size - 20;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 0x601;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 0x600;
        for (unsigned id = 1; id < 256; ++id) {
            runtime.handleSyscall(memory, &context, 0);
            require(context.r[2].low == id);
        }
        runtime.handleSyscall(memory, &context, 0);
        require(context.r[2].low == UINT64_MAX && runtime.semaphores.size() == 256);
        context.r[3].low = 0x74;
        context.r[4].low = 0x83;
        context.r[5].low = 0x700;
        context.r[2].low = 0x987654;
        runtime.handleSyscall(memory, &context, 0);
        require(runtime.syscall_handlers[0x83] == 0x700 && context.r[2].low == 0x987654);
        context.r[5].low = 0x701;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        require(runtime.syscall_handlers[0x83] == 0x700);
        context.r[5].low = PS2Runtime::ram_size;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[4].low = 256;
        reject([&] { runtime.handleSyscall(memory, &context, 0); });
        context.r[3].low = 0x83;
        bool handler_stop = false;
        try { runtime.handleSyscall(memory, &context, 0); }
        catch (const UnsupportedSyscall& stop) {
            handler_stop = stop.selector == 0x83 && stop.handler == 0x700 && stop.resume_pc == context.pc;
        }
        require(handler_stop);
        runtime.registerFunction(0x700, return_leaf);
        context.pc = 0x104;
        context.r[31] = {0xABCDEF, 0x123456};
        runtime.handleSyscall(memory, &context, 0);
        require(context.pc == 0x104 && context.r[31].low == 0xABCDEF
                && context.r[31].high == 0x123456 && runtime.custom_syscall_calls == 1);
        bool mmio_stop = false;
        try { (void)runtime.Load32(memory, &context, 0x10001810); }
        catch (const UnsupportedGuestMemory& stop) { mmio_stop = stop.address == 0x10001810 && stop.size == 4; }
        require(mmio_stop);
        runtime.supplyTimer3Mode(0x83);
        require(runtime.Load32(memory, &context, 0x10001810) == 0x83 && runtime.timer3_mode_reads == 1);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001810); });
        reject([&] { runtime.supplyTimer3Mode(0x83); });
        reject([&] { runtime.write32(memory, 0x10001810, 0); });
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001800); });
        PS2Runtime invalid_timer;
        reject([&] { invalid_timer.supplyTimer3Mode(0x10000); });
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { invalid_timer.supplyTimer2Mode(0x10000); });
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        runtime.supplyTimer2Mode(0x80);
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        reject([&] { runtime.supplyTimer2Mode(0x80); });
        reject([&] { (void)runtime.Load32(nullptr, &context, 0x10001010); });
        require(runtime.timer2_mode_reads == 0);
        require(runtime.Load32(memory, &context, 0x10001010) == 0x80 && runtime.timer2_mode_reads == 1);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { runtime.write32(memory, 0x10001010, 0x382); });
        reject([&] { runtime.write32(memory, 0xB0001020, 0x382); });
        reject([&] { runtime.write32(memory, 0xB0001010, 0x381); });
        reject([&] { runtime.write32(nullptr, 0xB0001010, 0x382); });
        runtime.write32(memory, 0xB0001010, 0x382);
        require(runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382);
        reject([&] { runtime.write32(memory, 0xB0001010, 0x382); });
        context.ll_bit = true;
        runtime.clearLLBit(&context);
        require(!context.ll_bit);
        PS2Runtime patched;
        patched.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        auto* patch_ram = patched.memory().data();
        reject([&] { patched.enableKernelPatch(0x80076000, 8); });
        const std::vector<std::uint8_t> baseline(PS2Runtime::kernel_snapshot_size, 0x5A);
        patched.loadKernelSnapshot(baseline);
        reject([&] { patched.enableKernelPatch(0x80076001, 8); });
        reject([&] { patched.enableKernelPatch(0x80076000, 0); });
        reject([&] { patched.enableKernelPatch(0x8007FFFC, 8); });
        reject([&] { patched.enableKernelPatch(0x80076000, UINT32_MAX); });
        patched.enableKernelPatch(0x80076000, 8);
        reject([&] { patched.enableKernelPatch(0x80076000, 8); });
        reject([&] { patched.loadKernelSnapshot(baseline); });
        patched.write32(patch_ram, 0x80076000, 0x12345678);
        patched.write32(patch_ram, 0x80076004, 0xABCDEF01);
        require(patched.read32(patch_ram, 0x80076000) == 0x12345678
                && patched.read32(patch_ram, 0x80076004) == 0xABCDEF01);
        reject([&] { patched.write32(patch_ram, 0x80075FFC, 0); });
        reject([&] { patched.write32(patch_ram, 0x80076008, 0); });
        reject([&] { patched.write32(patch_ram, 0x80076002, 0); });
        reject([&] { patched.write32(nullptr, 0x80076000, 0); });
        reject([&] { patched.write32(patch_ram, 0xA0076000, 0); });
        require(std::equal(baseline.begin(), baseline.end(), patched.kernelBaseline().begin()));
        require(patched.kernel_word_writes == 2 && patched.word_writes == 2
                && patched.read32(patch_ram, 0x80076008) == 0x5A5A5A5A);
        R5900Context registration{};
        registration.r[3].low = 0x74;
        registration.r[4].low = 0x5B;
        registration.r[2] = {0x1234, 0x5678};
        registration.pc = 0x204;
        for (const auto address : {0x80075FFCu, 0x80076008u, 0xA0076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        PS2Runtime unwritten;
        unwritten.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        unwritten.loadKernelSnapshot(baseline);
        unwritten.enableKernelPatch(0x80076000, 8);
        registration.r[5].low = 0x80076000;
        reject([&] { unwritten.handleSyscall(unwritten.memory().data(), &registration, 0); });
        reject([&] { patched.handleSyscall(nullptr, &registration, 0); });
        require(patched.syscall_registrations == 0 && patched.syscall_handlers[0x5B] == 0);
        for (const auto address : {0x80076000u, 0x80076004u}) {
            registration.r[5].low = address;
            patched.handleSyscall(patch_ram, &registration, 0);
            require(patched.syscall_handlers[0x5B] == address && registration.r[2].low == 0x1234
                    && registration.r[2].high == 0x5678 && registration.pc == 0x204);
        }
        registration.r[3].low = 0x5B;
        bool kernel_callback_stop = false;
        try { patched.handleSyscall(patch_ram, &registration, 0); }
        catch (const UnsupportedSyscall& stop) {
            kernel_callback_stop = stop.selector == 0x5B && stop.handler == 0x80076004 && stop.resume_pc == 0x204;
        }
        require(kernel_callback_stop && patched.custom_syscall_calls == 0);
        registration.r[3].low = 0x74;
        registration.r[5].low = 0;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.syscall_handlers[0x5B] == 0 && patched.syscall_registrations == 3);
        registration.r[5].low = 0x80076000;
        patched.handleSyscall(patch_ram, &registration, 0);
        const std::array<std::uint32_t, 2> compiled_words{0x12345678, 0xABCDEF01};
        const auto compiled_bytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(compiled_words.data()), 8);
        reject([&] { patched.registerFunction(0x80076000, return_leaf); });
        reject([&] { patched.registerKernelFunction(0x80076000, nullptr, compiled_bytes); });
        reject([&] { patched.registerKernelFunction(0x80076004, return_leaf, compiled_bytes); });
        reject([&] { patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes.first(4)); });
        patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes);
        reject([&] { patched.registerKernelFunction(0x80076000, return_leaf, compiled_bytes); });
        registration.r[3].low = 0x5B;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.kernel_dispatch_validations == 1 && patched.returns == 1 && registration.pc == 0x204);
        patched.write32(patch_ram, 0x80076004, 0);
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.kernel_dispatch_validations == 1 && patched.returns == 1);
        unwritten.registerKernelFunction(0x80076000, return_leaf, compiled_bytes);
        reject([&] { unwritten.callFunction(0x80076000, registration); });
        require(unwritten.kernel_dispatch_validations == 0);
        registration.r[3].low = 0x74;
        registration.r[4].low = 0x12C;
        registration.r[5].low = 0x80076000;
        reject([&] { unwritten.handleSyscall(unwritten.memory().data(), &registration, 0); });
        require(unwritten.timer3_interrupt_handler == 0 && !unwritten.timer3_interrupt_enable_requested);
        for (const auto address : {0u, 0x100u, 0x80076008u, 0x80076001u, 0xA0076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        require(!patched.timer3_interrupt_handler && !patched.extended_registrations);
        registration.r[5].low = 0x80076004;
        const auto ordinary_handlers = patched.syscall_handlers;
        const auto registrations_before = patched.syscall_registrations;
        registration.r[2] = {0x1234, 0x5678};
        patched.handleSyscall(patch_ram, &registration, 0);
        require(patched.timer3_interrupt_handler == 0x80076004 && patched.timer3_interrupt_enable_requested
                && patched.extended_registrations == 1 && patched.syscall_registrations == registrations_before + 1
                && patched.syscall_handlers == ordinary_handlers && registration.r[2].low == 0x1234
                && registration.r[2].high == 0x5678);
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[4].low = 0x12D;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[3].low = 0x12C;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.timer3_interrupt_handler == 0x80076004 && patched.extended_registrations == 1
                && patched.syscall_registrations == registrations_before + 1 && patched.syscall_handlers == ordinary_handlers);
        require(patched.intc_handlers.size() == 1 && patched.intc_handler_order == std::vector<std::uint32_t>{1});
        registration.r[3].low = 0x10;
        registration.r[4].low = 11;
        registration.r[5].low = 0x100;
        registration.r[6].low = registration.r[7].low = 0;
        registration.r[28].low = 0x12345678;
        patched.handleSyscall(patch_ram, &registration, 0);
        require(registration.r[2].low == 2 && registration.r[2].high == 0x5678
                && patched.intc_handlers[1].cause == 11 && patched.intc_handlers[1].address == 0x100
                && patched.intc_handlers[1].arg == 0 && patched.intc_handlers[1].gp == 0x12345678
                && patched.intc_handler_order == std::vector<std::uint32_t>{2, 1});
        for (const auto address : {0u, 0x101u, static_cast<std::uint32_t>(PS2Runtime::ram_size), 0x80076000u}) {
            registration.r[5].low = address;
            reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        }
        registration.r[5].low = 0x100;
        registration.r[6].low = UINT32_MAX;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        registration.r[6].low = 0;
        registration.r[4].low = 12;
        reject([&] { patched.handleSyscall(patch_ram, &registration, 0); });
        require(patched.intc_handlers.size() == 2 && registration.r[2].low == 2);
        registration.r[4].low = 11;
        for (unsigned id = 3; id <= 128; ++id) {
            patched.handleSyscall(patch_ram, &registration, 0);
            require(registration.r[2].low == id);
        }
        patched.handleSyscall(patch_ram, &registration, 0);
        require(registration.r[2].low == UINT64_MAX && registration.r[2].high == 0x5678
                && patched.intc_handlers.size() == 128 && patched.intc_handler_order.front() == 128);
        PS2Runtime cache;
        R5900Context cache_context{};
        cache_context.r[3].low = 0x64;
        cache_context.r[2] = {0x12345678, 0xABCDEF};
        cache_context.pc = 0x204;
        for (const auto operation : {0u, 2u}) {
            cache_context.r[4].low = operation;
            cache.handleSyscall(nullptr, &cache_context, 0);
            require(cache_context.r[2].low == 0x12345678 && cache_context.r[2].high == 0xABCDEF
                    && cache_context.pc == 0x204);
        }
        require(cache.cache_requests == 2 && cache.instruction_cache_invalidations == 1);
        for (const auto operation : {1u, 3u, UINT32_MAX}) {
            cache_context.r[4].low = operation;
            reject([&] { cache.handleSyscall(nullptr, &cache_context, 0); });
        }
        cache_context.r[4].low = 2;
        reject([&] { cache.handleSyscall(nullptr, &cache_context, 1); });
        require(cache.cache_requests == 2 && cache.instruction_cache_invalidations == 1);
        PS2Runtime zero_checkpoint;
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 32); });
        zero_checkpoint.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size, 0xA5));
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 16); });
        zero_checkpoint.expectStartupZeroRegion(16, 32);
        reject([&] { zero_checkpoint.expectStartupZeroRegion(16, 32); });
        R5900Context zero_context{};
        zero_context.r[3].low = 0x3C;
        zero_context.r[5].low = UINT32_MAX;
        zero_context.r[6].low = 0x100000;
        zero_context.r[7].low = 0x100;
        auto* zero_ram = zero_checkpoint.memory().data();
        reject([&] { zero_checkpoint.handleSyscall(zero_ram, &zero_context, 0); });
        require(!zero_checkpoint.startup_zero_verified && !zero_checkpoint.thread_initialized);
        for (std::uint32_t offset = 16; offset < 32; offset += 4) zero_checkpoint.write32(zero_ram, offset, 0);
        zero_checkpoint.handleSyscall(zero_ram, &zero_context, 0);
        require(zero_checkpoint.startup_zero_verified && zero_checkpoint.thread_initialized);
        const auto lanes = std::bit_cast<std::array<std::uint32_t, 4>>(
            PS2_PCPYLD({0x1111111122222222ULL, UINT64_MAX}, {0x3333333344444444ULL, UINT64_MAX}));
        require(lanes == std::array<std::uint32_t, 4>{0x44444444, 0x33333333, 0x22222222, 0x11111111});
        R5900Context signed_context{};
        signed_context.r[0] = {UINT64_MAX, UINT64_MAX};
        signed_context.r[1] = {0xABCDEF0080000000ULL, UINT64_MAX};
        require(GPR_S32(&signed_context, 0) == 0 && GPR_S32(&signed_context, 1) == INT32_MIN);
        signed_context.r[1].low = UINT64_MAX;
        require(GPR_S32(&signed_context, 1) == -1);
        signed_context.r[1].low = 0xABCDEF007FFFFFFFULL;
        require(GPR_S32(&signed_context, 1) == INT32_MAX);
        require(SRL32(0x740, 2) == 0x1D0);
        require(SRL32(0x80000000, 1) == 0x40000000);
        require(SRL32(0xFFFFFFFF, 31) == 1 && SRL32(0xFFFFFFFF, 32) == 0xFFFFFFFF);
        require(FPU_ADD_S(0, 0) == 0);
        reject([] { (void)FPU_ADD_S(1, 0); });
        std::cout << "Bootstrap register, saturation, memory bounds and unsupported FPU checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
