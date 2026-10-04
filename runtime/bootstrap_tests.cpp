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

void clobber_rpc_callback(std::uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    const auto return_address = ctx->r[31].low;
    const auto sp = ctx->r[29].low;
    ctx->r[3].low = 0xFFFFFFBD;
    ctx->r[4].low = 3;
    runtime->handleSyscall(ram, ctx, 0);
    runtime->write32(ram, 0x3A87A8, 0);
    runtime->write32(ram, 0x3A87CC, 0x3F570);
    runtime->write32(ram, 0x3A87BC, 0x3FB20);
    runtime->write32(ram, 0x3A97D0, 4);
    runtime->write32(ram, 0x3A97D8, 0);
    for (auto& reg : ctx->r) reg = {UINT64_MAX, UINT64_MAX};
    for (auto& value : ctx->f) value = 123.0f;
    ctx->hi = ctx->lo = ctx->hi1 = ctx->lo1 = UINT64_MAX;
    ctx->cop0_status = ctx->fcr31 = UINT32_MAX;
    std::fill_n(ram + sp - 64, 64, 0xCC);
    ctx->r[29].low = sp;
    ctx->r[31].low = return_address;
    return_leaf(ram, ctx, runtime);
}

}
int main() {
    try {
        {
            IntcCheckpoint wait;reject([&]{wait.acknowledge(4);});
            wait.arm(12,0x1802);reject([&]{wait.arm(12,0x1802);});
            reject([&]{wait.acknowledge(8);});wait.acknowledge(4);
            require(wait.status()==8 && wait.events==0 && !wait.callbackPending());
            reject([&]{wait.acknowledge(4);});
            require(wait.poll()==8 && wait.events==0);
            require(wait.poll()==12 && wait.events==1 && !wait.callbackPending());
            require(wait.poll()==12 && wait.events==1);wait.acknowledge(4);
            require(wait.status()==8);reject([&]{wait.acknowledge(4);});
            IntcCheckpoint absent;absent.arm(12,0x1802);absent.acknowledge(4);
            for(unsigned i=0;i<64;++i)require(absent.poll(false)==8);
            reject([&]{absent.poll(false);});require(absent.events==0);
        }
        {
            PS2Runtime widths;std::vector<std::uint8_t> image(PS2Runtime::ram_size,0xA5);
            widths.loadMemory(image);auto* ram=widths.memory().data();R5900Context ctx{};
            widths.write16(ram,2,0x1234);
            require(widths.read16(ram,2)==0x1234);
            reject([&]{(void)widths.read16(ram,3);});
            reject([&]{(void)widths.read16(ram,PS2Runtime::ram_size-1);});
            const auto bits=PS2_POR({0x0123456789ABCDEF,0xF00000000000000F},{0xFEDCBA9876543210,0x000000000000FFF0});
            require(bits.low==UINT64_MAX && bits.high==0xF00000000000FFFF);
            require(ram[1]==0xA5 && ram[2]==0x34 && ram[3]==0x12 && ram[4]==0xA5 && widths.halfword_writes==1);
            reject([&]{widths.write16(ram,3,0);});
            reject([&]{widths.write16(ram,PS2Runtime::ram_size-1,0);});
            reject([&]{widths.write16(ram,0x12001000,0);});
            reject([&]{widths.handleBreak(ram,&ctx);});
            reject([&]{widths.Store64(ram,&ctx,0x12001000,0x200);});
            reject([&]{(void)widths.Load64(ram,&ctx,0x12001000);});
            require(widths.halfword_writes==1 && widths.gs_csr_reads==0 && widths.gs_csr_reset_writes==0);
        }

        {
            PS2Runtime sif;
            std::vector<std::uint8_t> sif_image(PS2Runtime::ram_size);
            sif.loadMemory(sif_image);
            auto* sif_ram = sif.memory().data();
            R5900Context sif_context{};
            sif_context.r[3].low = 0x7A;
            sif_context.r[4].low = 0xFFFFFFFF80000000ull;
            sif_context.r[2] = {0xA5, 0xB6};
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            reject([&] { sif.supplySifCommand(0x19600); });
            sif.supplyDmaStatus(0x800040);
            sif.supplyDma5Control(0x184);
            reject([&] { sif.supplySifCommand(0x19601); });
            sif.supplySifCommand(0x19600);
            reject([&] { sif.supplySifCommand(0x19600); });
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.dmac_enable_calls = 1; // Synthetic prerequisite fixture.
            sif_context.r[4].low = 0x80000001;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            require(sif.sif_command_reads == 0 && sif_context.r[2].low == 0xA5);
            sif_context.r[4].low = 0xFFFFFFFF80000000ull;
            sif.handleSyscall(sif_ram, &sif_context, 0);
            require(sif_context.r[2].low == 0x19600 && sif_context.r[2].high == 0xB6 && sif.sif_command_reads == 1);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.memory().back() = 0xF1;
            require(sif.read8(sif_ram, PS2Runtime::ram_size - 1) == 0xF1);
            reject([&] { sif.read8(sif_ram, PS2Runtime::ram_size); });
            reject([&] { sif.read8(nullptr, 0); });
            sif.dataCacheLine(sif_ram, PS2Runtime::ram_size - 64);
            reject([&] { sif.dataCacheLine(sif_ram, 1); });
            reject([&] { sif.dataCacheLine(sif_ram, PS2Runtime::ram_size); });
            require(sif.data_cache_lines == 1);
            SET_GPR_ZE32(&sif_context, 2, 0x80000000);
            require(sif_context.r[2].low == 0x80000000 && sif_context.r[2].high == 0xB6);
            SET_GPR_ZE32(&sif_context, 0, 1);
            require(GPR_U32(&sif_context, 0) == 0 && SUB32(0, 1) == UINT32_MAX);
            std::vector<std::uint8_t> iop_fixture(PS2Runtime::sif_iop_size, 0xA5);
            const std::array<std::uint32_t, 3> iop_table{0x19600, 0x19680, 0x93640};
            std::memcpy(iop_fixture.data() + 0x196C0, iop_table.data(), 12);
            reject([&] { sif.supplySifDma(iop_fixture, 0x702); });
            sif.supplySifDma(iop_fixture, 0x701);
            reject([&] { sif.supplySifRpcState(0); });
            sif.supplySifRpcState(1);
            reject([&] { sif.supplySifRpcState(1); });
            sif_context.r[4].low = 0x80000002;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            reject([&] { sif.read32(sif_ram, 0x20000200); });
            require(sif.sif_rpc_reads == 0);
            reject([&] { sif.supplySifDma(iop_fixture, 0x701); });
            const std::array<std::uint32_t, 4> descriptor{0x3A9580, 0x19600, 20, 0x44};
            const std::array<std::uint32_t, 5> packet{20, 0, 0x80000000, 0, 0x3A94C0};
            for (unsigned index = 0; index < descriptor.size(); ++index) sif.write32(sif_ram, 0x200 + index * 4, descriptor[index]);
            for (unsigned index = 0; index < packet.size(); ++index) sif.write32(sif_ram, 0x3A9580 + index * 4, packet[index]);
            sif_context.r[3].low = 0x77;
            sif_context.r[4].low = 0x200;
            sif_context.r[5].low = 2;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif_context.r[5].low = 1;
            sif.write32(sif_ram, 0x3A9588, 0x80000009);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            require(std::equal(iop_fixture.begin(), iop_fixture.end(), sif.sifIopMemory().begin()) && sif.sif_dma_submissions == 0);
            sif.write32(sif_ram, 0x3A9588, 0x80000000);
            const std::vector<std::uint8_t> ee_before_dma(sif.memory().begin(), sif.memory().end());
            sif.handleSyscall(sif_ram, &sif_context, 0);
            auto expected_iop = iop_fixture;
            std::memcpy(expected_iop.data() + 0x19600, packet.data(), 20);
            expected_iop[0x19600] = 0;
            const std::uint32_t new_receive = 0x3A94C0;
            std::memcpy(expected_iop.data() + 0x196C8, &new_receive, 4);
            require(std::equal(expected_iop.begin(), expected_iop.end(), sif.sifIopMemory().begin())
                    && sif.sif_dma_submissions == 1 && sif.sif_ee_receive_address == new_receive
                    && sif_context.r[2].low == 0x701 && sif_context.r[2].high == 0xB6
                    && std::equal(ee_before_dma.begin(), ee_before_dma.end(), sif.memory().begin()));
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif_context.r[3].low = 0x7A;
            sif_context.r[4].low = 0xFFFFFFFF80000002ull;
            sif.handleSyscall(sif_ram, &sif_context, 0);
            require(sif.sif_rpc_reads == 1 && sif_context.r[2].low == 1 && sif_context.r[2].high == 0xB6);
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); });
            sif.write32(sif_ram, 0x20000100, 0x89ABCDEF);
            require(sif.read32(sif_ram, 0x100) == 0x89ABCDEF && sif.read32(sif_ram, 0x20000100) == 0x89ABCDEF);
            sif.write32(sif_ram, 0x104, 0x76543210);
            require(sif.read32(sif_ram, 0x20000104) == 0x76543210);
            reject([&] { sif.read32(sif_ram, 0x20000001); });
            reject([&] { sif.write32(sif_ram, 0x22000000, 0); });
            reject([&] { sif.read32(nullptr, 0x20000000); });
            require(sif.uncached_word_accesses == 3);
            sif.write8(sif_ram, 0x20000101, 0x55);
            require(sif.read8(sif_ram, 0x101) == 0x55 && sif.read8(sif_ram, 0x20000101) == 0x55);
            reject([&] { sif.read8(sif_ram, 0x22000000); });
            sif.dataCacheLine(sif_ram, 0x20000100);
            require(sif.uncached_byte_accesses == 2 && sif.data_cache_lines == 2);
            // Authored hostile callback fixture checks interruption isolation,
            // not the generated callback's implementation.
            std::vector<std::uint8_t> bind_bundle(PS2Runtime::sif_iop_size + 96);
            auto put = [&](std::size_t offset, std::uint32_t value) {
                std::memcpy(bind_bundle.data() + offset, &value, 4);
            };
            const std::array<std::uint32_t, 16> reply{0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
                0x3A87A8, 0x80000009, 0x3F570, 0x3FB20, 0, 0, 0, 0, 0};
            std::memcpy(bind_bundle.data() + PS2Runtime::sif_iop_size, reply.data(), 64);
            put(PS2Runtime::sif_iop_size + 64, 0x801);
            put(0x3F570, 0x80000592); put(0x3F578, 0x3FB20);
            put(0x3F5B0, 0x3F558); put(0x3F560, 0x3F570);
            const std::array<std::uint32_t, 7> call_sema{0, 1, 0, 0, 0, 0x3838E8, 3};
            std::memcpy(bind_bundle.data() + PS2Runtime::sif_iop_size + 68, call_sema.data(), 28);
            put(0x3F560, 0);
            reject([&] { sif.supplySifBind(bind_bundle); });
            put(0x3F560, 0x3F570);
            sif.supplySifBind(bind_bundle);
            reject([&] { sif.supplySifBind(bind_bundle); });
            sif.initial_sp = 0x1FFED60;
            const auto frame = sif.initial_sp - 464;
            const std::array<std::uint32_t, 4> bind_descriptor{0x203A97C0, 0x19600, 64, 0x44};
            const std::array<std::uint32_t, 16> bind_packet{64, 0, 0x80000009, 0, 5, 0x203A97C0,
                2, 0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0};
            std::memcpy(sif_ram + frame, bind_descriptor.data(), 16);
            std::memcpy(sif_ram + 0x3A97C0, bind_packet.data(), 64);
            sif.write32(sif_ram, 0x3A87A8, 0x203A97C0);
            sif.write32(sif_ram, 0x3A87AC, 2); sif.write32(sif_ram, 0x3A87B0, 3);
            sif.write32(sif_ram, 0x3A9620, 0x3322E8);
            sif.write32(sif_ram, 0x3A9624, 0x3AAFC0); sif.write32(sif_ram, 0x3A9628, 0x38BDF0);
            sif.semaphores.resize(4); sif.semaphores[3] = {0, 1, 0x3838D8};
            for (unsigned index = 0; index < 32; ++index) {
                sif_context.r[index] = {0x1234567800000000ull + index, 0xABCDEF0010000000ull + index};
                sif_context.f[index] = static_cast<float>(index);
            }
            sif_context.r[3].low = 0x77; sif_context.r[4].low = frame;
            sif_context.r[5].low = 1; sif_context.r[29].low = frame;
            sif_context.hi = 19; sif_context.lo = 23; sif_context.cop0_status = 0x1234;
            sif_context.pc = 0x32F028;
            const auto cpu_before = sif_context;
            std::fill_n(sif_ram + frame - 64, 64, 0xA7);
            const auto iop_before = std::vector<std::uint8_t>(sif.sifIopMemory().begin(), sif.sifIopMemory().end());
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Missing compiled handler.
            require(sif.sif_bind_callbacks == 0 && sif.semaphores[3].count == 0
                    && std::equal(iop_before.begin(), iop_before.end(), sif.sifIopMemory().begin()));
            sif.registerFunction(0x3322E8, clobber_rpc_callback);
            sif.registerFunction(0x332258, return_leaf); sif.registerFunction(0x32EC90, return_leaf);
            sif.handleSyscall(sif_ram, &sif_context, 0);
            auto expected_cpu = cpu_before; expected_cpu.r[2].low = 0x801;
            require(std::memcmp(&expected_cpu, &sif_context, sizeof(R5900Context)) == 0
                    && sif.sif_bind_callbacks == 1 && sif.sif_bind_context_preservations == 1
                    && sif.sif_dma_submissions == 2 && sif.semaphores[3].count == 1
                    && std::all_of(sif_ram + frame - 64, sif_ram + frame, [](auto value) { return value == 0xA7; }));
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Repeated/unknown transfer.
            sif_context.r[3].low = 0xFFFFFFBD;
            reject([&] { sif.handleSyscall(sif_ram, &sif_context, 0); }); // Scope closed.
        }
        const auto left = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{UINT32_MAX, 0, 0x80000000, 1});
        const auto right = std::bit_cast<ProbeGpr>(std::array<std::uint32_t, 4>{1, 2, 0x80000000, 2});
        require(std::bit_cast<std::array<std::uint32_t, 4>>(ps2_paddu32(left, right))
                == std::array<std::uint32_t, 4>{UINT32_MAX, 2, UINT32_MAX, 3});
        R5900Context context{};
        context.r[0] = left;
        SET_GPR_VEC(&context, 0, right);
        require(GPR_VEC(&context, 0).low == 0 && context.r[0].low == left.low);
        context.r[4] = {0x8000000000000000ull, 0x12345678};
        require(GPR_S64(&context, 4) == INT64_MIN);
        SET_GPR_S64(&context, 4, GPR_S64(&context, 4) >> 32);
        require(context.r[4].low == 0xFFFFFFFF80000000ull && context.r[4].high == 0x12345678);
        SET_GPR_S64(&context, 4, INT64_MAX);
        require(GPR_S64(&context, 4) >> 32 == 0x7FFFFFFF);
        SET_GPR_S64(&context, 0, -1);
        require(GPR_S64(&context, 0) == 0 && context.r[0].low == left.low);
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
        runtime.Store32(memory, &context, 0x500, 0x12345678);
        require(runtime.read32(memory, 0x500) == 0x12345678);
        reject([&] { runtime.Store32(memory, &context, 0x1000E010, 1); });
        require(runtime.read32(memory, 0x500) == 0x12345678);
        PS2Runtime dma_checkpoint;
        dma_checkpoint.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size, 0xA5));
        auto* dma_ram = dma_checkpoint.memory().data();
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000E010); });
        reject([&] { dma_checkpoint.supplyDma5Control(0x184); });
        reject([&] { dma_checkpoint.supplyDmaStatus(0); });
        dma_checkpoint.supplyDmaStatus(0x00800040);
        reject([&] { dma_checkpoint.supplyDmaStatus(0x00800040); });
        reject([&] { dma_checkpoint.supplyDma5Control(0); });
        dma_checkpoint.supplyDma5Control(0x184);
        reject([&] { dma_checkpoint.supplyDma5Control(0x184); });
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000C000); });
        reject([&] { dma_checkpoint.Load32(nullptr, &context, 0x1000E010); });
        require(dma_checkpoint.dma_status_reads == 0 && dma_checkpoint.dma5_control_reads == 0);
        require(dma_checkpoint.Load32(dma_ram, &context, 0x1000E010) == 0x00800040);
        require(dma_checkpoint.read32(dma_ram, 0x1000C000) == 0x184);
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000E010); });
        reject([&] { dma_checkpoint.read32(dma_ram, 0x1000C000); });
        reject([&] { dma_checkpoint.Store32(dma_ram, &context, 0x1000E010, 0x20); });
        require(dma_checkpoint.dma_status_reads == 1 && dma_checkpoint.dma5_control_reads == 1
                && std::all_of(dma_checkpoint.memory().begin(), dma_checkpoint.memory().end(),
                               [](auto byte) { return byte == 0xA5; }));
        R5900Context dma_context{};
        dma_context.r[3].low = 0x16;
        dma_context.r[4].low = 5;
        reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
        dma_context.r[3].low = 0x12;
        dma_context.r[5].low = 0x1000;
        dma_context.r[6].low = 0;
        dma_context.r[7].low = 0x20;
        dma_context.r[28].low = 0x2000;
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 1 && dma_checkpoint.dmac_handlers.size() == 1
                && dma_checkpoint.dmac_handlers[0].channel == 5
                && dma_checkpoint.dmac_handlers[0].address == 0x1000
                && dma_checkpoint.dmac_handlers[0].arg == 0x20
                && dma_checkpoint.dmac_handlers[0].gp == 0x2000
                && dma_checkpoint.dmac_handler_order == std::vector<std::uint32_t>{1});
        for (const auto index : {4u, 5u, 6u}) {
            const auto saved = dma_context.r[index];
            dma_context.r[index].low = index == 4 ? 6 : index == 5 ? 0x1001 : 1;
            reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
            dma_context.r[index] = saved;
        }
        require(dma_checkpoint.dmac_handlers.size() == 1);
        dma_context.r[3].low = 0x16;
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 1 && dma_checkpoint.dmac_mask == 0x00A00000
                && dma_checkpoint.dmac_enable_calls == 1);
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == 0 && dma_checkpoint.dmac_enable_calls == 2);
        dma_context.r[4].low = 6;
        reject([&] { dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0); });
        require(dma_checkpoint.dmac_mask == 0x00A00000 && dma_checkpoint.dmac_enable_calls == 2);
        dma_context.r[4].low = 5;
        dma_context.r[3].low = 0x12;
        dma_checkpoint.dmac_handlers.resize(128);
        dma_checkpoint.handleSyscall(dma_ram, &dma_context, 0);
        require(dma_context.r[2].low == UINT64_MAX && dma_checkpoint.dmac_handlers.size() == 128);
        PS2Runtime semaphore_runtime;
        semaphore_runtime.semaphores.push_back({1, 1, 0});
        R5900Context semaphore_context{};
        semaphore_context.r[3].low = 0x44;
        semaphore_context.r[4].low = 0;
        semaphore_context.r[2] = {0x1234, 0x5678};
        semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
        require(semaphore_context.r[2].low == 0 && semaphore_context.r[2].high == 0x5678
                && semaphore_runtime.semaphores[0].count == 0 && semaphore_runtime.semaphore_waits == 1);
        semaphore_context.r[2].low = 0x1234;
        reject([&] { semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0); });
        require(semaphore_context.r[2].low == 0x1234 && semaphore_runtime.semaphore_waits == 1);
        semaphore_context.r[3].low = 0x42;
        semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
        require(semaphore_context.r[2].low == 0 && semaphore_runtime.semaphores[0].count == 1
                && semaphore_runtime.semaphore_signals == 1);
        reject([&] { semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0); });
        require(semaphore_runtime.semaphores[0].count == 1 && semaphore_runtime.semaphore_signals == 1);
        for (const auto selector : {0x42u, 0x44u}) {
            semaphore_context.r[3].low = selector;
            semaphore_context.r[4].low = UINT32_MAX;
            semaphore_runtime.handleSyscall(nullptr, &semaphore_context, 0);
            require(semaphore_context.r[2].low == UINT64_MAX && semaphore_context.r[2].high == 0x5678);
        }
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
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        reject([&] { runtime.supplyTimer2Readback(0x10000); });
        runtime.supplyTimer2Readback(0x382);
        reject([&] { runtime.supplyTimer2Readback(0x382); });
        reject([&] { (void)runtime.Load32(nullptr, &context, 0x10001010); });
        require(runtime.timer2_mode_reads == 1);
        require(runtime.Load32(memory, &context, 0x10001010) == 0x382 && runtime.timer2_mode_reads == 2);
        reject([&] { (void)runtime.Load32(memory, &context, 0x10001010); });
        PS2Runtime mismatch;
        mismatch.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        mismatch.supplyTimer2Mode(0x80);
        mismatch.supplyTimer2Readback(0x782);
        auto* mismatch_ram = mismatch.memory().data();
        require(mismatch.Load32(mismatch_ram, &context, 0x10001010) == 0x80);
        reject([&] { (void)mismatch.Load32(mismatch_ram, &context, 0x10001010); });
        mismatch.write32(mismatch_ram, 0xB0001010, 0x382);
        reject([&] { (void)mismatch.Load32(mismatch_ram, &context, 0x10001010); });
        require(mismatch.timer2_mode_reads == 1 && mismatch.timer2_mode_after_write == 0x382);
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
        PS2Runtime intc;
        R5900Context intc_context{};
        intc_context.r[3].low = 0x14;
        intc_context.r[4].low = 11;
        intc_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { intc.handleSyscall(nullptr, &intc_context, 0); });
        reject([&] { intc.supplyIntcMask(0x10000); });
        intc.supplyIntcMask(0x1002);
        reject([&] { intc.supplyIntcMask(0x1002); });
        for (const auto cause : {0u, 12u, 32u, UINT32_MAX}) {
            intc_context.r[4].low = cause;
            reject([&] { intc.handleSyscall(nullptr, &intc_context, 0); });
        }
        require(intc.intc_mask == 0x1002 && intc.intc_enable_calls == 0 && intc_context.r[2].low == 0xABCD);
        intc_context.r[4].low = 11;
        reject([&] { intc.handleSyscall(nullptr, &intc_context, 1); });
        intc.handleSyscall(nullptr, &intc_context, 0);
        require(intc.intc_mask == 0x1802 && intc_context.r[2].low == 1 && intc_context.r[2].high == UINT64_MAX);
        intc.handleSyscall(nullptr, &intc_context, 0);
        require(intc.intc_mask == 0x1802 && intc_context.r[2].low == 0 && intc.intc_enable_calls == 2);
        PS2Runtime already_enabled;
        already_enabled.supplyIntcMask(0x1802);
        already_enabled.handleSyscall(nullptr, &intc_context, 0);
        require(intc_context.r[2].low == 0 && already_enabled.intc_mask == 0x1802);
        PS2Runtime creation;
        creation.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size, 0xA5));
        auto* creation_ram = creation.memory().data();
        const auto descriptor = [&](unsigned offset, std::uint32_t value) {
            creation.write32(creation_ram, 0x300 + offset, value);
        };
        descriptor(4, 0x1000); descriptor(8, 0x10000); descriptor(12, 0x400);
        descriptor(16, 0x2000); descriptor(20, 0);
        R5900Context create_context{};
        create_context.r[3].low = 0x20;
        create_context.r[4].low = 0x300;
        create_context.r[2] = {0xCAFE, UINT64_MAX};
        reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        creation.thread_initialized = true;
        reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        creation.heap_initialized = true;
        creation.heap_start = 0x4000;
        for (const auto address : {0x301u, static_cast<std::uint32_t>(PS2Runtime::ram_size - 32)}) {
            create_context.r[4].low = address;
            reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
        }
        create_context.r[4].low = 0x300;
        for (const auto [offset, invalid, original] : std::array<std::array<std::uint32_t, 3>, 8>{
                {{4, 0, 0x1000}, {4, 0x1001, 0x1000}, {8, 0x10001, 0x10000},
                 {8, 0x1FFFF00, 0x10000}, {8, 0, 0x10000}, {12, 0x2A0, 0x400}, {20, 128, 0}, {16, 0x80002000, 0x2000}}}) {
            descriptor(offset, invalid);
            reject([&] { creation.handleSyscall(creation_ram, &create_context, 0); });
            descriptor(offset, original);
        }
        require(creation.threads.empty() && create_context.r[2].low == 0xCAFE);
        const std::vector<std::uint8_t> before_creation(creation.memory().begin(), creation.memory().end());
        const auto before_words = creation.word_writes;
        creation.handleSyscall(creation_ram, &create_context, 0);
        require(creation.threads.size() == 1 && create_context.r[2].low == 2
                && create_context.r[2].high == UINT64_MAX && creation.word_writes == before_words + 4);
        const auto& created = creation.threads[0];
        require(created.id == 2 && created.entry == 0x1000 && created.stack_base == 0x10000
                && created.stack_size == 0x400 && created.gp == 0x2000 && created.initial_priority == 0
                && created.heap_base == 0x4000 && created.context_address == 0x10160 && created.initial_sp == 0x103E0);
        auto expected_creation = before_creation;
        for (const auto [index, value] : std::array<std::array<std::uint32_t, 2>, 4>{
                {{28, 0x2000}, {29, 0x103E0}, {30, 0x103E0}, {31, 0x1FC03000}}}) {
            std::memcpy(expected_creation.data() + 0x10160 + index * 16, &value, 4);
        }
        require(std::equal(creation.memory().begin(), creation.memory().end(), expected_creation.begin()));
        for (unsigned id = 3; id <= 255; ++id) {
            creation.handleSyscall(creation_ram, &create_context, 0);
            require(create_context.r[2].low == id);
        }
        const auto full_words = creation.word_writes;
        creation.handleSyscall(creation_ram, &create_context, 0);
        require(create_context.r[2].low == UINT64_MAX && create_context.r[2].high == UINT64_MAX
                && creation.threads.size() == 254 && creation.word_writes == full_words);
        PS2Runtime identity;
        R5900Context identity_context{};
        identity_context.r[3].low = 0x2F;
        identity_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 0); });
        identity.thread_initialized = true;
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 0); });
        identity.current_thread_id = 1;
        identity.handleSyscall(nullptr, &identity_context, 0);
        require(identity_context.r[2].low == 1 && identity_context.r[2].high == UINT64_MAX);
        reject([&] { identity.handleSyscall(nullptr, &identity_context, 1); });
        R5900Context priority_context{};
        priority_context.r[3].low = 0x29;
        priority_context.r[4].low = priority_context.r[5].low = 1;
        priority_context.r[2] = {0xABCD, UINT64_MAX};
        PS2Runtime priority;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        priority.thread_initialized = true;
        priority.current_thread_id = 1;
        priority_context.r[4].low = 2;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        priority_context.r[4].low = 1;
        for (const auto value : {0u, 2u, 128u, UINT32_MAX}) {
            priority_context.r[5].low = value;
            reject([&] { priority.handleSyscall(nullptr, &priority_context, 0); });
        }
        require(priority.main_thread_priority == 0 && priority.priority_changes == 0 && priority_context.r[2].low == 0xABCD);
        priority_context.r[5].low = 1;
        reject([&] { priority.handleSyscall(nullptr, &priority_context, 1); });
        priority.handleSyscall(nullptr, &priority_context, 0);
        require(priority_context.r[2].low == 0 && priority_context.r[2].high == UINT64_MAX && priority.main_thread_priority == 1);
        priority.handleSyscall(nullptr, &priority_context, 0);
        require(priority_context.r[2].low == 1 && priority.priority_changes == 2);
        PS2Runtime osd;
        osd.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        R5900Context osd_context{};
        osd_context.r[3].low = 0x4B;
        osd_context.r[4].low = 0x100;
        osd_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd.supplyOsdConfig(0x21C12010);
        reject([&] { osd.supplyOsdConfig(0); });
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 1); });
        reject([&] { osd.handleSyscall(nullptr, &osd_context, 0); });
        osd_context.r[4].low = 0x101;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd_context.r[4].low = PS2Runtime::ram_size;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        osd_context.r[4].low = UINT64_MAX;
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        require(osd.osd_config_reads == 0 && osd.word_writes == 0);
        osd_context.r[4].low = 0x100;
        osd.memory()[0xFF] = osd.memory()[0x104] = 0xA5;
        osd.handleSyscall(osd.memory().data(), &osd_context, 0);
        require(osd.read32(osd.memory().data(), 0x100) == 0x21C12010 && osd.osd_config_reads == 1
                && osd.word_writes == 1 && osd_context.r[2].low == 0xABCD && osd_context.r[2].high == UINT64_MAX
                && osd.memory()[0xFF] == 0xA5 && osd.memory()[0x104] == 0xA5);
        reject([&] { osd.handleSyscall(osd.memory().data(), &osd_context, 0); });
        PS2Runtime osd_sequence;
        osd_sequence.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        R5900Context sequence_context{};
        sequence_context.r[3].low = 0x4B;
        sequence_context.r[4].low = 0x100;
        sequence_context.r[2] = {0xABCD, UINT64_MAX};
        reject([&] { osd_sequence.supplyOsdReadback(0x21C12010); });
        osd_sequence.supplyOsdConfig(0x21C12010);
        reject([&] { osd_sequence.supplyOsdReadback(0); });
        osd_sequence.supplyOsdReadback(0x21C12010);
        reject([&] { osd_sequence.supplyOsdReadback(0x21C12010); });
        sequence_context.r[3].low = 0x4A;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4A;
        sequence_context.r[4].low = 0x104;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = PS2Runtime::ram_size;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = UINT64_MAX;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = 0x101;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[4].low = 0x100;
        reject([&] { osd_sequence.handleSyscall(nullptr, &sequence_context, 0); });
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 1); });
        require(osd_sequence.osd_config_sets == 0 && osd_sequence.osd_config_reads == 1);
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.osd_config_sets == 1 && osd_sequence.word_writes == 1);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        sequence_context.r[4].low = 0x104;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.read32(osd_sequence.memory().data(), 0x104) == 0x21C12010
                && osd_sequence.osd_config_reads == 2 && osd_sequence.word_writes == 2
                && sequence_context.r[2].low == 0xABCD && sequence_context.r[2].high == UINT64_MAX);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4A;
        osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0);
        require(osd_sequence.osd_config_sets == 2 && osd_sequence.word_writes == 2);
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        sequence_context.r[3].low = 0x4B;
        reject([&] { osd_sequence.handleSyscall(osd_sequence.memory().data(), &sequence_context, 0); });
        PS2Runtime second_overlay;
        second_overlay.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        const std::vector<std::uint8_t> payload(0x330, 0xC3);
        reject([&] { second_overlay.enableStartupKernelPayload(payload); });
        second_overlay.loadKernelSnapshot(baseline);
        second_overlay.enableKernelPatch(0x80076000, 8);
        reject([&] { second_overlay.enableStartupKernelPayload(std::span<const std::uint8_t>(payload).first(4)); });
        second_overlay.enableStartupKernelPayload(payload);
        reject([&] { second_overlay.enableStartupKernelPayload(payload); });
        auto* overlay_ram = second_overlay.memory().data();
        reject([&] { second_overlay.write32(nullptr, 0x80075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075002, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80074FFC, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075330, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0xA0075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.write32(overlay_ram, 0x80075000, 0); });
        R5900Context overlay_registration{};
        overlay_registration.r[3].low = 0x74;
        overlay_registration.r[4].low = 0x5B;
        overlay_registration.r[5].low = 0x80075000;
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        second_overlay.write32(overlay_ram, 0x80075000, 0xC3C3C3C3);
        reject([&] { second_overlay.write32(overlay_ram, 0x80075000, 0xC3C3C3C3); });
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        for (std::uint32_t offset = 4; offset < 0x330; offset += 4)
            second_overlay.write32(overlay_ram, 0x80075000 + offset, 0xC3C3C3C3);
        second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0);
        require(second_overlay.syscall_handlers[0x5B] == 0x80075000
                && second_overlay.startup_kernel_word_writes == 204 && second_overlay.kernel_word_writes == 204
                && second_overlay.word_writes == 204 && second_overlay.read32(overlay_ram, 0x8007532C) == 0xC3C3C3C3
                && second_overlay.read32(overlay_ram, 0x80075330) == 0x5A5A5A5A
                && std::equal(baseline.begin(), baseline.end(), second_overlay.kernelBaseline().begin()));
        overlay_registration.r[5].low = 0x80075004;
        reject([&] { second_overlay.handleSyscall(overlay_ram, &overlay_registration, 0); });
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, std::span<const std::uint8_t>(payload).first(4)); });
        auto changed_payload = payload;
        changed_payload.back() ^= 1;
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, changed_payload); });
        reject([&] { second_overlay.registerKernelFunction(0x80075004, return_leaf, payload); });
        second_overlay.registerKernelFunction(0x80075000, return_leaf, payload);
        reject([&] { second_overlay.registerKernelFunction(0x80075000, return_leaf, payload); });
        second_overlay.expected_return = 0xFFFFFFFC;
        R5900Context bound_context{};
        bound_context.r[31].low = second_overlay.expected_return;
        second_overlay.callFunction(0x80075000, bound_context);
        require(second_overlay.kernel_dispatch_validations == 1 && second_overlay.returns == 1);
        PS2Runtime incomplete_binding;
        incomplete_binding.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        incomplete_binding.loadKernelSnapshot(baseline);
        incomplete_binding.enableKernelPatch(0x80076000, 8);
        incomplete_binding.enableStartupKernelPayload(payload);
        incomplete_binding.registerKernelFunction(0x80075000, return_leaf, payload);
        incomplete_binding.write32(incomplete_binding.memory().data(), 0x80075000, 0xC3C3C3C3);
        reject([&] { incomplete_binding.callFunction(0x80075000, bound_context); });
        require(incomplete_binding.kernel_dispatch_validations == 0 && incomplete_binding.guest_calls == 0);
        PS2Runtime table_registration;
        table_registration.loadMemory(std::vector<std::uint8_t>(PS2Runtime::ram_size));
        table_registration.loadKernelSnapshot(baseline);
        table_registration.enableKernelPatch(0x80076000, 8);
        auto table_payload = payload;
        const std::array<std::uint32_t, 4> test_pairs{0x55, 0x80075038, 0x56, 0x800750C8};
        std::memcpy(table_payload.data() + 0x300, test_pairs.data(), sizeof(test_pairs));
        table_registration.enableStartupKernelPayload(table_payload);
        R5900Context table_context{};
        table_context.r[3].low = 0x74;
        table_context.r[4].low = 0x55;
        table_context.r[5].low = 0x80075038;
        table_context.r[2] = {0x1234, UINT64_MAX};
        auto* table_ram = table_registration.memory().data();
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        for (std::uint32_t offset = 0; offset < 0x330; offset += 4) {
            std::uint32_t value;
            std::memcpy(&value, table_payload.data() + offset, 4);
            table_registration.write32(table_ram, 0x80075000 + offset, value);
        }
        table_registration.handleSyscall(table_ram, &table_context, 0);
        require(table_registration.syscall_handlers[0x55] == 0x80075038
                && table_context.r[2].low == 0x1234 && table_context.r[2].high == UINT64_MAX);
        table_context.r[4].low = 0x57;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[4].low = 0x55;
        table_context.r[5].low = 0x8007503C;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[5].low = 0x80075330;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        table_context.r[3].low = 0x55;
        reject([&] { table_registration.handleSyscall(table_ram, &table_context, 0); });
        require(table_registration.custom_syscall_calls == 0 && table_registration.syscall_registrations == 1);
        PS2Runtime poll;
        poll.semaphores.push_back({1, 1, 0});
        R5900Context poll_context{};
        poll_context.r[3].low = 0x45; poll_context.r[4].low = 0;
        poll_context.r[2].high = 0x12345678;
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == 0 && poll.semaphores[0].count == 0);
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == UINT64_MAX && poll.semaphores[0].count == 0);
        poll_context.r[4].low = 99;
        poll.handleSyscall(nullptr, &poll_context, 0);
        require(poll_context.r[2].low == UINT64_MAX && poll_context.r[2].high == 0x12345678
                && poll.semaphore_polls == 3 && poll.semaphore_waits == 0 && poll.semaphore_signals == 0);
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
        PS2Runtime shutdown;
        R5900Context shutdown_context{};
        shutdown_context.r[3].low = 0x17; shutdown_context.r[4].low = 5;
        shutdown_context.r[2].high = 0x1122334455667788ULL;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        shutdown.scmd_mmode_calls = 1;
        shutdown.dmac_handlers.push_back({5, 0x331DF0, 0, 0x38BDF0});
        shutdown.dmac_handler_order.push_back(1); shutdown.dmac_mask = 0xA00000;
        shutdown_context.r[4].low = 4;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.dmac_mask == 0xA00000 && shutdown.dmac_disable_calls == 0);
        shutdown_context.r[4].low = 5;
        shutdown.handleSyscall(nullptr, &shutdown_context, 0);
        require(shutdown.dmac_mask == 0x800000 && shutdown_context.r[2].low == 1
                && shutdown_context.r[2].high == 0x1122334455667788ULL);
        shutdown_context.r[3].low = 0x13; shutdown_context.r[5].low = 2;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.dmac_handlers.size() == 1 && shutdown.dmac_removals == 0);
        shutdown_context.r[5].low = 1;
        shutdown.handleSyscall(nullptr, &shutdown_context, 0);
        require(shutdown.dmac_handlers.empty() && shutdown.dmac_handler_order.empty()
                && shutdown.dmac_removals == 1 && shutdown_context.r[2].low == 0
                && shutdown_context.r[2].high == 0x1122334455667788ULL);
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        shutdown_context.r[3].low = 0x6B; shutdown_context.r[4].low = 1;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.sif_stop_calls == 0);
        shutdown_context.r[3].low = 0x79; shutdown_context.r[4].low = 4;
        shutdown_context.r[5].low = 0x40000; shutdown.sif_stop_calls = 1;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.reset_smflag_writes == 0);
        shutdown_context.r[3].low = 0x7A; shutdown_context.r[4].low = 0x80000000;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.sif_reset_subaddr_reads == 0);
        shutdown_context.r[3].low = 0x77;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.iop_epoch == 0 && !shutdown.iop_reset_pending && shutdown.prior_iop_services_valid);
        shutdown.iop_reset_pending = true;
        shutdown_context.r[3].low = 0x79; shutdown_context.r[4].low = 4;
        shutdown_context.r[5].low = 0x10000;
        reject([&] { shutdown.handleSyscall(nullptr, &shutdown_context, 0); });
        require(shutdown.reset_clear_writes == 0);
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
        require(SLL32(1, 31) == 0x80000000 && SLL32(0x80000000, 1) == 0
                && SLL32(1, 32) == 1 && SLL32(0xFFFFFFFF, 4) == 0xFFFFFFF0);
        require(SRL32(0x740, 2) == 0x1D0);
        require(SRL32(0x80000000, 1) == 0x40000000);
        require(SRL32(0xFFFFFFFF, 31) == 1 && SRL32(0xFFFFFFFF, 32) == 0xFFFFFFFF);
        require(SRA32(0x80000000, 31) == UINT32_MAX && SRA32(0x80000000, 0) == 0x80000000
                && SRA32(UINT32_MAX, 32) == UINT32_MAX && SRA32(0x7FFFFFFF, 1) == 0x3FFFFFFF);
        require(FPU_ADD_S(0, 0) == 0);
        reject([] { (void)FPU_ADD_S(1, 0); });
        std::cout << "Bootstrap register, saturation, memory bounds and unsupported FPU checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
