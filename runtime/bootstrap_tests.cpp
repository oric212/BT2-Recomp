#include "ps2_runtime_macros.h"
#include <iostream>

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
        require(FPU_ADD_S(0, 0) == 0);
        reject([] { (void)FPU_ADD_S(1, 0); });
        std::cout << "Bootstrap register, saturation, memory bounds and unsupported FPU checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
