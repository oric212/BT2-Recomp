#pragma once

// Authored bootstrap adapter for generated probes. Broader PS2 APIs fail explicitly.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>
#include <immintrin.h>
#include "intc_checkpoint.h"

struct alignas(16) ProbeGpr {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    operator __m128i() const {
        __m128i value;
        std::memcpy(&value, this, sizeof(value));
        return value;
    }
};

struct R5900Context {
    std::array<ProbeGpr, 32> r{};
    std::uint32_t pc = 0;
    std::uint64_t hi = 0, lo = 0, hi1 = 0, lo1 = 0;
    std::uint32_t sa = 0, fcr31 = 0, cop0_status = 0, branch_pc = 0;
    std::uint32_t cop0_epc = 0, cop0_errorepc = 0;
    bool ll_bit = false;
    bool in_delay_slot = false;
    std::array<float, 32> f{};
    float f_acc = 0;
};

struct UnsupportedSyscall : std::runtime_error {
    UnsupportedSyscall(std::uint32_t selector, std::uint32_t pc, std::uint32_t handler = 0)
        : std::runtime_error("Unsupported bootstrap syscall"), selector(selector), resume_pc(pc), handler(handler) {}
    std::uint32_t selector, resume_pc, handler;
};

struct UnsupportedGuestBranch : std::runtime_error {
    UnsupportedGuestBranch(std::uint32_t target, std::uint32_t source, std::uint32_t resume)
        : std::runtime_error("Unsupported bootstrap guest dispatch"), target(target), source_pc(source), resume_pc(resume) {}
    std::uint32_t target, source_pc, resume_pc;
};

struct UnsupportedGuestMemory : std::runtime_error {
    UnsupportedGuestMemory(std::uint32_t address, std::size_t size)
        : std::runtime_error("Unsupported or out-of-bounds guest memory access"), address(address), size(size) {}
    std::uint32_t address;
    std::size_t size;
};

class PS2Runtime {
public:
    using RecompiledFunction = void (*)(std::uint8_t*, R5900Context*, PS2Runtime*);
    enum class GuestBranchKind { Return, DirectCall, IndirectCall, TailJump };
    static constexpr std::size_t ram_size = 32 * 1024 * 1024;

    void clearLLBit(R5900Context* context) { context->ll_bit = false; }

    void registerFunction(std::uint32_t address, RecompiledFunction function) {
        if (address >= 0x80000000 || !function || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate guest registration");
    }

    void registerKernelFunction(std::uint32_t address, RecompiledFunction function,
                                std::span<const std::uint8_t> expected_payload) {
        const bool primary = kernel_patch_end_ && address == kernel_patch_start_
            && expected_payload.size() == kernel_patch_end_ - kernel_patch_start_;
        const bool startup = address == 0x80075000 && !startup_kernel_payload_.empty()
            && expected_payload.size() == startup_kernel_payload_.size()
            && std::equal(expected_payload.begin(), expected_payload.end(), startup_kernel_payload_.begin());
        if (!function || (!primary && !startup) || functions_.contains(address))
            throw std::runtime_error("Invalid copied kernel binding");
        kernel_code_bindings_.emplace(address, std::vector<std::uint8_t>(expected_payload.begin(), expected_payload.end()));
        functions_.emplace(address, function);
    }

    void callFunction(std::uint32_t address, R5900Context& context) {
        const auto found = functions_.find(address);
        if (found == functions_.end()) throw std::runtime_error("Unregistered guest target");
        verifyKernelBinding(address);
        context.pc = address;
        checkpoints_ = 0;
        found->second(memory_.empty() ? nullptr : memory_.data(), &context, this);
    }

    // Isolated logical RPC_END callback validation. The caller supplies checked
    // capture state; this does not emulate interrupt entry or schedule delivery.
    void validateRpcEndCallback(R5900Context& context) {
        if (rpc_end_callback_active_ || context.r[4].low != 0x3A94C0
                || context.r[5].low != 0x3AAFC0 || context.r[28].low != 0x38BDF0)
            throw std::runtime_error("Invalid isolated RPC_END callback context");
        rpc_end_callback_active_ = true;
        try { callFunction(0x3322E8, context); }
        catch (...) { rpc_end_callback_active_ = false; throw; }
        rpc_end_callback_active_ = false;
    }

    void expectStartupZeroRegion(std::uint32_t start, std::uint32_t end) {
        if (startup_zero_end_ || thread_initialized || end <= start || end > memory_.size())
            throw std::runtime_error("Invalid startup zero checkpoint");
        startup_zero_start_ = start;
        startup_zero_end_ = end;
    }

    void handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code) {
        const auto selector = static_cast<std::uint32_t>(context->r[3].low);
        if (code == 0 && selector < syscall_handlers.size() && syscall_handlers[selector]) {
            const auto handler = syscall_handlers[selector];
            if (!functions_.contains(handler)) throw UnsupportedSyscall(selector, context->pc, handler);
            // Narrow host callback model: preserve wrapper RA, validate native callback return.
            // Does not emulate BIOS exception entry, privilege transitions or ERET.
            const auto saved_ra = context->r[31];
            const auto resume = context->pc;
            constexpr std::uint32_t callback_return = 0xFFFFFFF8;
            context->r[31].low = callback_return;
            ++custom_syscall_calls;
            try {
                dispatchGuestBranch(rdram, context, handler, resume, callback_return,
                                    GuestBranchKind::DirectCall, "registered syscall callback");
            } catch (...) { context->r[31] = saved_ra; throw; }
            context->r[31] = saved_ra;
            context->pc = resume;
            return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 4 && scmd_mmode_supplied_) {
            receiveScmdMmode(rdram,context); return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 3 && scmd_bind_supplied_) {
            receiveScmdBind(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 2 && cd_init_supplied_) {
            receiveFirstCdInit(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && sif_dma_submissions == 1 && !sif_bind_iop_.empty()) {
            receiveFirstRpcBind(rdram, context);
            return;
        }
        if (code == 0 && selector == 0x77 && reset_submission_supplied_ && sif_dma_submissions == 5) {
            if (iop_reset_pending || reset_smflag_writes != 1 || sif_reset_subaddr_reads != 1
                    || context->r[4].low != initial_sp-224 || context->r[29].low != context->r[4].low
                    || context->r[5].low != 1) throw UnsupportedSyscall(selector,context->pc);
            const std::array<std::uint32_t,4> descriptor{0x3ACC40,0x19600,104,0x44};
            const std::array<std::uint32_t,6> header{104,0,0x80000003,0,36,0};
            for(unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])throw UnsupportedSyscall(selector,context->pc);
            for(unsigned i=0;i<6;++i) if(read32(rdram,0x3ACC40+i*4)!=header[i])throw UnsupportedSyscall(selector,context->pc);
            constexpr char path[]="rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
            if(!std::equal(std::begin(path),std::end(path),memory_.begin()+0x3ACC58))throw UnsupportedSyscall(selector,context->pc);
            // Submission acknowledgement only. Retain the request and invalidate
            // old service lifetime; no receive, image execution or ready flag invented.
            std::copy_n(memory_.data()+0x3ACC40,104,reset_request_.data());
            iop_reset_pending=true; prior_iop_services_valid=false; ++iop_epoch;
            ++sif_dma_submissions;context->r[2].low=0xE01;return;
        }
        if (code == 0 && selector == 0x77) {
            if (sif_iop_.empty() || sif_dma_submissions || sif_command_reads != 1 || data_cache_lines != 1
                    || context->r[5].low != 1 || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
            if (descriptor & 15) throw UnsupportedGuestMemory(descriptor, 16);
            checkMemory(rdram, descriptor, 16);
            const auto source = read32(rdram, descriptor), destination = read32(rdram, descriptor + 4);
            if (source != 0x3A9580 || destination != 0x19600 || read32(rdram, descriptor + 8) != 20
                    || read32(rdram, descriptor + 12) != 0x44)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, source, 20);
            // Observed CHANGE_SADDR command only; unknown commands fail before mutation.
            if (read32(rdram, source) != 20 || read32(rdram, source + 4) != 0
                    || read32(rdram, source + 8) != 0x80000000 || read32(rdram, source + 12) != 0
                    || read32(rdram, source + 16) != 0x3A94C0)
                throw UnsupportedSyscall(selector, context->pc);
            std::copy_n(memory_.data() + source, 20, sif_iop_.data() + destination);
            // Model only the validated logical receive effects, synchronously.
            // The service consumes psize and stores the new EE receive address.
            sif_iop_[destination] = 0;
            sif_ee_receive_address = 0x3A94C0;
            std::memcpy(sif_iop_.data() + 0x196C8, &sif_ee_receive_address, 4);
            ++sif_dma_submissions;
            context->r[2].low = sif_dma_result_;
            return;
        }
        if (code == 0 && selector == 0x79 && iop_reset_pending) {
            const auto reg=static_cast<std::uint32_t>(context->r[4].low);
            const auto value=static_cast<std::uint32_t>(context->r[5].low);
            const std::array<std::uint32_t,4> regs{4,4,0x80000002,0x80000000};
            const std::array<std::uint32_t,4> values{0x10000,0x20000,0,0};
            if(!reset_submission_supplied_ || reset_clear_writes>=4 || reg!=regs[reset_clear_writes] || value!=values[reset_clear_writes]
                    || context->r[29].low!=initial_sp-224)throw UnsupportedSyscall(selector,context->pc);
            if(reset_clear_writes<2) { sif_smflag&=~value;context->r[2].low=sif_smflag; }
            else { if(reset_clear_writes==2) reset_rpcinit=0;else reset_subaddr=0;context->r[2].low=0; }
            ++reset_clear_writes;return;
        }
        if (code == 0 && selector == 0x79 && sif_stop_calls) {
            if (!reset_smflag_supplied_ || reset_smflag_writes || sif_reset_subaddr_reads != 1
                    || context->r[4].low != 4 || context->r[5].low != 0x40000
                    || context->r[29].low != initial_sp - 224 || sif_smflag != 0x30000)
                throw UnsupportedSyscall(selector, context->pc);
            // Captured kernel behavior: EE SMFLAG write clears selected bits,
            // returning the readback value (different from the pinned Play! return).
            sif_smflag &= ~0x40000u; ++reset_smflag_writes;
            context->r[2].low = sif_smflag;
            return;
        }
        if (code == 0 && selector == 0x6B) {
            if (!sif_stop_supplied_ || sif_stop_calls || scmd_mmode_calls != 1 || sif_dma_submissions != 5
                    || dmac_removals != 1 || dmac_mask != 0x800000 || !dmac_handlers.empty()
                    || context->r[4].low != 1 || context->r[29].low != initial_sp - 224)
                throw UnsupportedSyscall(selector, context->pc);
            // Local kernel handler writes zero to SIF2 CHCR/QWC and returns QWC.
            // All supported logical submissions have finished; active DMA is unsupported.
            sif2_chcr = 0; sif2_qwc = 0; ++sif_stop_calls; context->r[2].low = 0;
            return;
        }
        if(code==0 && selector==0x71 && gs_imr_supplied_) {
            if(gs_csr_reads!=1 || gs_imr_writes || context->pc!=0x32EFA8 || context->r[4].low!=0xFF00
                    || context->r[29].low!=initial_sp-688 || context->r[31].low!=0x30FE6C)
                throw UnsupportedSyscall(selector,context->pc);
            gs_imr=0xFF00;++gs_imr_writes;context->r[2].low=0xFF00;return;
        }
        if(code==0 && selector==2 && gs_crt_supplied_) {
            if(gs_imr_writes!=1 || gs_crt_calls || context->pc!=0x32E868 || context->r[4].low!=1
                    || context->r[5].low!=2 || context->r[6].low!=1 || context->r[29].low!=initial_sp-608
                    || context->r[31].low!=0x100274)throw UnsupportedSyscall(selector,context->pc);
            gs_crt_request={1,2,1};++gs_crt_calls;context->r[2].low=0;return;
        }
        if (code == 0 && selector == 0x7A && iop_reset_pending && reset_ready_supplied_) {
            if(reset_ready_checkpoint_reads || reset_clear_writes!=4 || context->pc!=0x32F078
                    || context->r[4].low!=4 || context->r[29].low!=initial_sp-48
                    || context->r[31].low!=0x338118)throw UnsupportedSyscall(selector,context->pc);
            // Explicit reference checkpoint replay: this caller tests only bit 0x40000.
            // This does not model full SMFLAG or execute/complete the native IOP boot.
            context->r[2].low=0x40000;++reset_ready_checkpoint_reads;return;
        }
        if (code == 0 && selector == 0x7A && sif_stop_calls) {
            if (!sif_reset_subaddr_supplied_ || sif_reset_subaddr_reads
                    || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000 || context->r[29].low != initial_sp - 224)
                throw UnsupportedSyscall(selector, context->pc);
            ++sif_reset_subaddr_reads; context->r[2].low = 0x19600;
            return;
        }
        if (code == 0 && selector == 0x7A && static_cast<std::uint32_t>(context->r[4].low) == 0x80000002) {
            if (!sif_rpc_supplied_ || sif_rpc_reads || sif_dma_submissions != 1)
                throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = 1;
            ++sif_rpc_reads;
            return;
        }
        if (code == 0 && selector == 0x7A) {
            if (!sif_command_supplied_ || sif_command_reads || static_cast<std::uint32_t>(context->r[4].low) != 0x80000000
                    || dmac_enable_calls != 1) throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = static_cast<std::uint64_t>(static_cast<std::int64_t>(
                std::bit_cast<std::int32_t>(sif_command_word_)));
            ++sif_command_reads;
            return;
        }
        if (code == 0 && selector == 0x4A) {
            if (!osd_readback_supplied || !((osd_config_reads == 1 && osd_config_sets == 0)
                    || (osd_config_reads == 2 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            if (address & 3) throw UnsupportedGuestMemory(address, 4);
            checkMemory(rdram, address, 4);
            // Capture established only the same-value write; mutations fail.
            if (read32(rdram, address) != osd_config_word) throw UnsupportedSyscall(selector, context->pc);
            ++osd_config_sets;
            return;
        }
        if (code == 0 && selector == 0x4B) {
            if (!osd_config_supplied || (osd_config_reads && !(osd_readback_supplied && osd_config_reads == 1 && osd_config_sets == 1)) || context->r[4].low > UINT32_MAX)
                throw UnsupportedSyscall(selector, context->pc);
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            if (address & 3) throw UnsupportedGuestMemory(address, 4);
            checkMemory(rdram, address, 4);
            write32(rdram, address, osd_config_word);
            ++osd_config_reads;
            return; // SDK declares void; preserve the result register.
        }
        if (code == 0 && selector == 0x29) {
            // Only the main thread is runnable; created threads remain dormant.
            if (!thread_initialized || current_thread_id != 1 || context->r[4].low != 1 || context->r[5].low != 1)
                throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = main_thread_priority;
            main_thread_priority = 1;
            ++priority_changes;
            return;
        }
        if (code == 0 && selector == 0x2F) {
            if (!thread_initialized || !current_thread_id) throw UnsupportedSyscall(selector, context->pc);
            context->r[2].low = current_thread_id;
            return;
        }
        if (code == 0 && selector == 0x20) {
            const auto parameter = static_cast<std::uint32_t>(context->r[4].low);
            if (!thread_initialized || !heap_initialized || parameter % 4)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, parameter, 36);
            const auto entry = read32(rdram, parameter + 4);
            const auto stack = read32(rdram, parameter + 8);
            const auto size = read32(rdram, parameter + 12);
            const auto gp = read32(rdram, parameter + 16);
            const auto priority = read32(rdram, parameter + 20);
            if (!entry || entry % 4 || stack % 16 || size % 16 || size < 0x2C0 || priority >= 128)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, entry, 4);
            checkMemory(rdram, stack, size);
            checkMemory(rdram, gp, 4);
            const auto top = stack + size;
            const auto saved_context = top - 0x2A0;
            if (parameter < top && saved_context < parameter + 36)
                throw UnsupportedSyscall(selector, context->pc);
            // Reference model reserves main ID 1 and idle ID 256; no ID reuse/scheduler yet.
            if (threads.size() == 254) { context->r[2].low = UINT64_MAX; return; }
            const auto id = static_cast<std::uint32_t>(threads.size()) + 2;
            threads.push_back({id, entry, stack, size, gp, priority, heap_start, saved_context, top - 0x20});
            // Pinned ThreadReset writes only these low words, preserving other context bytes.
            write32(rdram, saved_context + 28 * 16, gp);
            write32(rdram, saved_context + 29 * 16, top - 0x20);
            write32(rdram, saved_context + 30 * 16, top - 0x20);
            write32(rdram, saved_context + 31 * 16, 0x1FC03000);
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x14) {
            // Observed cause only; explicit mask checkpoint avoids a fabricated BIOS default.
            if (!intc_mask_supplied_ || static_cast<std::uint32_t>(context->r[4].low) != 11)
                throw UnsupportedSyscall(selector, context->pc);
            constexpr std::uint32_t bit = 1u << 11;
            context->r[2].low = (intc_mask & bit) ? 0 : 1;
            intc_mask |= bit;
            ++intc_enable_calls;
            return;
        }
        if (code == 0 && selector == 0x10) {
            const auto cause = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            // Observed startup contract only. No interrupt delivery or removal yet.
            if (cause != 11 || !address || address % 4 || context->r[6].low != 0 || context->r[7].low != 0)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, address, 4);
            const auto id = addIntcHandler(cause, address, 0, static_cast<std::uint32_t>(context->r[28].low));
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x12) {
            const auto channel = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            if (channel != 5 || !address || address % 4 || context->r[6].low != 0)
                throw UnsupportedSyscall(selector, context->pc);
            checkMemory(rdram, address, 4);
            if (dmac_handlers.size() == 128) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            dmac_handlers.push_back({channel, address, static_cast<std::uint32_t>(context->r[7].low),
                                     static_cast<std::uint32_t>(context->r[28].low)});
            const auto id = static_cast<std::uint32_t>(dmac_handlers.size());
            dmac_handler_order.insert(dmac_handler_order.begin(), id);
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x17) {
            // Reached SIF shutdown: toggle only the recorded channel-5 mask bit.
            if (context->r[4].low != 5 || scmd_mmode_calls != 1 || dmac_handlers.size() != 1)
                throw UnsupportedSyscall(selector, context->pc);
            const auto bit = 0x10000u << 5;
            const bool changed = (dmac_mask & bit) != 0;
            dmac_mask &= ~bit;
            ++dmac_disable_calls;
            context->r[2].low = changed ? 1 : 0;
            return;
        }
        if (code == 0 && selector == 0x13) {
            // Sole registered dormant handler; no dispatch or wait queue exists.
            if (context->r[4].low != 5 || context->r[5].low != 1 || dmac_disable_calls != 1
                    || dmac_mask != 0x800000 || dmac_handlers.size() != 1
                    || dmac_handlers[0].channel != 5 || dmac_handler_order != std::vector<std::uint32_t>{1})
                throw UnsupportedSyscall(selector, context->pc);
            dmac_handlers.clear(); dmac_handler_order.clear(); ++dmac_removals;
            context->r[2].low = 0; // Remaining handlers on this channel.
            return;
        }
        if (code == 0 && selector == 0x16) {
            // Captured mask bits only. No status acknowledgements, DMA or interrupt delivery.
            if (context->r[4].low != 5 || dma_status_reads != 1 || dma5_control_reads != 1
                    || dmac_handlers.empty()) throw UnsupportedSyscall(selector, context->pc);
            const auto bit = 0x10000u << 5;
            const bool changed = !(dmac_mask & bit);
            dmac_mask |= bit;
            ++dmac_enable_calls;
            context->r[2].low = changed ? 1 : 0;
            return;
        }
        if (code == 0 && selector == 0x74) {
            const auto number = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            const bool timer_interrupt = number == 0x12C;
            if ((!timer_interrupt && number >= syscall_handlers.size()) || address % 4
                    || (timer_interrupt && (timer3_interrupt_handler || address < 0x80000000)))
                throw UnsupportedSyscall(selector, context->pc);
            if (address >= 0x80000000) {
                // Register only a word actually written into the explicitly enabled patch.
                // Registration does not make these bytes a compiled native function.
                const bool primary_patch = address >= kernel_patch_start_ && address < kernel_patch_end_;
                bool startup_payload = address == 0x80075000 && startup_kernel_word_writes == 0x330 / 4;
                if (!startup_kernel_payload_.empty() && startup_kernel_word_writes == 0x330 / 4
                        && address >= 0x80075000 && address < 0x80075330) {
                    // Exact selector/address pairs from the fully validated copied table.
                    for (std::size_t offset = 0x300; offset < 0x330; offset += 8) {
                        std::uint32_t table_selector, table_address;
                        std::memcpy(&table_selector, startup_kernel_payload_.data() + offset, 4);
                        std::memcpy(&table_address, startup_kernel_payload_.data() + offset + 4, 4);
                        if (number == table_selector && address == table_address) startup_payload = true;
                    }
                }
                if (memory_.empty() || rdram != memory_.data() || (!primary_patch && !startup_payload)
                        || !kernel_patch_words_.contains(address - 0x80000000))
                    throw UnsupportedGuestMemory(address, 4);
            } else if (address) checkMemory(rdram, address, 4);
            if (timer_interrupt) {
                // Pinned Play! maps this extended selector to INTC line 12.
                // Record one startup registration/enable request; no interrupt delivery.
                if (addIntcHandler(12, address, 0, 0) == UINT64_MAX)
                    throw std::runtime_error("Timer 3 interrupt handler pool exhausted");
                timer3_interrupt_handler = address;
                timer3_interrupt_enable_requested = true;
                ++extended_registrations;
            } else syscall_handlers[number] = address;
            ++syscall_registrations;
            // PS2SDK declares void; preserve v0 rather than inventing a result.
            return;
        }
        if (code == 0 && selector == 0x3C) {
            // Bootstrap model follows pinned Play! sc_SetupThread; no scheduler yet.
            const auto stack = static_cast<std::uint32_t>(context->r[5].low);
            const auto size = static_cast<std::uint32_t>(context->r[6].low);
            const auto args = static_cast<std::uint32_t>(context->r[7].low);
            constexpr std::uint32_t top = static_cast<std::uint32_t>(ram_size) - 4096;
            if (thread_initialized || stack != UINT32_MAX || size < 0x2A0 || size > top
                || size % 16 || args % 4)
                throw std::runtime_error("Unsupported bootstrap SetupThread contract");
            checkMemory(rdram, args, 8);
            if (startup_zero_end_) {
                for (auto address = startup_zero_start_; address < startup_zero_end_; ++address)
                    if (memory_[address]) throw std::runtime_error("Startup zero region was not cleared before SetupThread");
                startup_zero_verified = true;
                startup_quadword_writes = quadword_writes;
                startup_byte_writes = byte_writes;
            }
            stack_base = top - size;
            initial_sp = top - 0x2A0;
            // Explicit empty launch: argc=0, argv[0]=nullptr.
            std::memset(memory_.data() + args, 0, 8);
            thread_arguments = {static_cast<std::uint32_t>(context->r[4].low), stack, size,
                                args, static_cast<std::uint32_t>(context->r[8].low)};
            context->r[2].low = initial_sp;
            current_thread_id = 1;
            thread_initialized = true;
            return;
        }
        if (code == 0 && selector == 0x3D) {
            // Pinned Play! default heap end is the initialized main thread's stack base.
            const auto start = static_cast<std::uint32_t>(context->r[4].low);
            const auto size = static_cast<std::uint32_t>(context->r[5].low);
            if (!thread_initialized || heap_initialized || size != UINT32_MAX || start >= stack_base)
                throw std::runtime_error("Unsupported bootstrap SetupHeap contract");
            checkMemory(rdram, start, 1);
            heap_start = start;
            heap_end = stack_base;
            context->r[2].low = heap_end;
            heap_initialized = true;
            return;
        }
        if (code == 0 && selector == 0x40) {
            const auto address = static_cast<std::uint32_t>(context->r[4].low);
            checkMemory(rdram, address, 24);
            if (address % 4) throw std::runtime_error("Unaligned CreateSema parameters");
            // Input count/wait_threads are output fields, not initial state.
            const auto maximum = read32(rdram, address + 4);
            const auto initial = read32(rdram, address + 8);
            const auto attributes = read32(rdram, address + 16);
            const auto option = read32(rdram, address + 20);
            // PS2SDK synchronous RPC leaves attr uninitialized; the pinned Play!
            // model reads count/max/option only. Permit this field solely for the
            // reached first CD-init call, whose local BIOS capture returns ID 3.
            const bool rpc_call_sema = sif_call_sema_supplied_ && sif_bind_callbacks == 1 && semaphore_deletions == 1
                && semaphores.size() == 3 && address == initial_sp - 400
                && maximum == 1 && initial == 0 && option == 0x3838E8;
            const bool cd_sema = cd_semaphores_supplied_ && cd_init_calls == 1 && semaphore_deletions == 2
                && cd_semaphores_created < 4 && semaphores.size() == 3 + cd_semaphores_created
                && address == initial_sp - 304 && maximum == 1
                && initial == (cd_semaphores_created == 3 ? 0u : 1u)
                && option == 0x3825B8 + cd_semaphores_created * 16;
            const bool scmd_sema = scmd_bind_supplied_ && cd_semaphores_created == 4 && !scmd_bind_calls
                && semaphores.size() == 7 && address == initial_sp - 288 && maximum == 1
                && initial == 0 && option == 0x3838D8 && attributes == 2;
            const bool mmode_sema = scmd_mmode_supplied_ && scmd_bind_calls == 1 && !scmd_mmode_calls
                && semaphores.size() == 7 && address == initial_sp - 304 && maximum == 1
                && initial == 0 && option == 0x3838E8 && attributes == 0x370000;
            if (attributes != 0 && !rpc_call_sema && !cd_sema && !scmd_sema && !mmode_sema) {
                if (sif_bind_callbacks) throw UnsupportedSyscall(selector, context->pc);
                throw std::runtime_error("Unsupported semaphore attributes");
            }
            if (cd_sema) ++cd_semaphores_created;
            if (rpc_call_sema) { ++rpc_call_semaphores; rpc_call_sema_attributes = attributes; }
            if (!maximum || maximum > INT32_MAX || initial > maximum || semaphores.size() == 256) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            const auto id = static_cast<std::uint32_t>(semaphores.size());
            semaphores.push_back({initial, maximum, option});
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x45) {
            // Pinned nonblocking PollSema: never sleeps or reschedules.
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            ++semaphore_polls;
            if (id >= semaphores.size() || semaphores[id].count == 0) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            --semaphores[id].count;
            context->r[2].low = id;
            return;
        }
        if (code == 0 && selector == 0x41) {
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            if (id >= semaphores.size()) { context->r[2].low = UINT64_MAX; return; }
            // Observed completed bind cleanup only, with no waiting threads.
            const bool bind_cleanup = sif_bind_callbacks == 1 && semaphore_waits == 2
                && semaphore_deletions == 0 && semaphores[id].option == 0x3838D8;
            const bool cd_cleanup = cd_init_calls == 1 && semaphore_waits == 3
                && semaphore_deletions == 1 && semaphores[id].option == 0x3838E8;
            const bool scmd_cleanup = scmd_bind_calls == 1 && semaphore_waits == 4 && semaphore_deletions == 2
                && id == 7 && semaphores.size() == 8 && semaphores[id].option == 0x3838D8;
            const bool mmode_cleanup = scmd_mmode_calls == 1 && semaphore_waits == 5 && semaphore_deletions == 3
                && id == 7 && semaphores.size() == 8 && semaphores[id].option == 0x3838E8;
            if (semaphores[id].count != 0 || semaphores[id].maximum != 1
                    || (!(id == 3 && semaphores.size() == 4 && (bind_cleanup || cd_cleanup)) && !scmd_cleanup && !mmode_cleanup))
                throw UnsupportedSyscall(selector, context->pc);
            semaphores.pop_back();
            ++semaphore_deletions;
            context->r[2].low = id;
            return;
        }
        if (code == 0 && (selector == 0x44 || selector == 0x42
                || (selector == 0xFFFFFFBD && rpc_end_callback_active_))) {
            const auto id = static_cast<std::uint32_t>(context->r[4].low);
            if (id >= semaphores.size()) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            auto& semaphore = semaphores[id];
            if (selector == 0x44) {
                if (!semaphore.count) throw UnsupportedSyscall(selector, context->pc);
                --semaphore.count;
                ++semaphore_waits;
            } else {
                // No waiting threads exist in this bounded model. Overflow is unverified.
                if (semaphore.count >= semaphore.maximum) throw UnsupportedSyscall(selector, context->pc);
                ++semaphore.count;
                ++semaphore_signals;
            }
            context->r[2].low = id;
            return;
        }
        if (code != 0 || static_cast<std::uint32_t>(context->r[3].low) != 0x64)
            throw UnsupportedSyscall(static_cast<std::uint32_t>(context->r[3].low), context->pc);
        const auto operation = static_cast<std::uint32_t>(context->r[4].low);
        if (operation != 0 && operation != 2) throw UnsupportedSyscall(selector, context->pc);
        // Pinned PS2SDK: 0=writeback D-cache, 2=invalidate I-cache (void).
        // Stores are immediately visible and dispatch uses fixed native functions.
        // No guest instruction cache exists; this does not translate patched kernel code.
        if (operation == 2) ++instruction_cache_invalidations;
        ++cache_requests;
        syscall_resume_pc = context->pc;
    }

    bool dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target,
                             std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char*) {
        if (kind == GuestBranchKind::Return) {
            const auto expected = call_returns_.empty() ? expected_return : call_returns_.back();
            if (target != expected) throw std::runtime_error("Unexpected probe return target");
            context->pc = target;
            ++returns;
            return true;
        }
        const auto found = functions_.find(target);
        if (found == functions_.end()) throw UnsupportedGuestBranch(target, source, resume);
        verifyKernelBinding(target);
        if (dispatch_depth_ >= 64) throw std::runtime_error("Bootstrap call depth exceeded");
        struct DepthGuard {
            unsigned& depth;
            explicit DepthGuard(unsigned& value) : depth(value) { ++depth; }
            ~DepthGuard() { --depth; }
        } depth_guard(dispatch_depth_);
        if (kind == GuestBranchKind::TailJump) {
            context->pc = target;
            found->second(rdram, context, this);
            return true;
        }
        if (!resume || (resume & 3)) throw std::runtime_error("Invalid guest return address");
        call_returns_.push_back(resume);
        ++guest_calls;
        context->pc = target;
        try {
            found->second(rdram, context, this);
            if (context->pc != resume) throw std::runtime_error("Guest callee did not return to caller");
        } catch (...) { call_returns_.pop_back(); throw; }
        call_returns_.pop_back();
        return true;
    }

    void loadMemory(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != ram_size) throw std::runtime_error("Expected bounded 32 MiB guest image");
        memory_.assign(bytes.begin(), bytes.end());
    }

    std::span<std::uint8_t> memory() { return memory_; }

    // PS2SDK UNCACHED_SEG: reached low 32 MiB main-RAM alias only.
    // Enabled after the explicit RPCINIT query; no TLB/MMIO or cache state.
    std::uint32_t uncachedWordAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1 || address % 4) throw UnsupportedGuestMemory(address, 4);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 4);
        ++uncached_word_accesses;
        return physical;
    }
    unsigned uncached_word_accesses = 0;
    std::uint32_t uncachedByteAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 1);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 1);
        ++uncached_byte_accesses;
        return physical;
    }
    unsigned uncached_byte_accesses = 0;
    void write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value) {
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        memory_[address] = value;
        ++byte_writes;
    }

    std::uint16_t read16(std::uint8_t* rdram,std::uint32_t address) const {
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword load");
        std::uint16_t value;std::memcpy(&value,memory_.data()+address,2);return value;
    }
    void write16(std::uint8_t* rdram,std::uint32_t address,std::uint16_t value) {
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword store");
        std::memcpy(memory_.data()+address,&value,2);++halfword_writes;
    }
    void write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value) {
        checkMemory(rdram, address, sizeof(value));
        if (address % 16) throw std::runtime_error("Unsupported unaligned quadword store");
        std::memcpy(memory_.data() + address, &value, sizeof(value));
        ++quadword_writes;
    }

    void write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
        address = uncachedWordAddress(rdram, address);
        // Observed Timer 2 startup store only; no counter advancement or scheduling.
        if (address == 0xB0001010 && timer2_mode_supplied_ && timer2_mode_reads == 1
                && !timer2_mode_writes && value == 0x382) {
            checkMemory(rdram, 0, 0);
            timer2_mode_after_write = (timer2_mode_ & 0xC00 & ~value) | (value & 0x3FF);
            ++timer2_mode_writes;
            ++word_writes;
            return;
        }
        if (!startup_kernel_payload_.empty() && address >= 0x80075000 && address < 0x80075330) {
            if (memory_.empty() || rdram != memory_.data() || address % 4)
                throw UnsupportedGuestMemory(address, 4);
            const auto offset = address - 0x80075000;
            std::uint32_t expected;
            std::memcpy(&expected, startup_kernel_payload_.data() + offset, 4);
            if (value != expected || kernel_patch_words_.contains(address - 0x80000000))
                throw std::runtime_error("Startup kernel payload store differs or repeats");
            kernel_patch_words_[address - 0x80000000] = value;
            ++startup_kernel_word_writes;
            ++kernel_word_writes;
            ++word_writes;
            return;
        }
        if (address >= kernel_patch_start_ && address < kernel_patch_end_) {
            if (!kernel_patch_end_ || memory_.empty() || rdram != memory_.data() || address % 4
                    || kernel_patch_end_ - address < 4) throw UnsupportedGuestMemory(address, 4);
            kernel_patch_words_[address - 0x80000000] = value;
            ++kernel_word_writes;
            ++word_writes;
            return;
        }
        checkMemory(rdram, address, 4);
        if (address % 4) throw std::runtime_error("Unsupported unaligned word store");
        std::memcpy(memory_.data() + address, &value, 4);
        ++word_writes;
    }

    void write64(std::uint8_t* rdram, std::uint32_t address, std::uint64_t value) {
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword store");
        std::memcpy(memory_.data() + address, &value, 8);
        ++doubleword_writes;
    }
    std::uint8_t read8(std::uint8_t* rdram, std::uint32_t address) {
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        return memory_[address];
    }
    std::uint64_t read64(std::uint8_t* rdram, std::uint32_t address) {
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword read");
        std::uint64_t value;
        std::memcpy(&value, memory_.data() + address, 8);
        return value;
    }

    static constexpr std::size_t kernel_snapshot_size = 0x80004;
    void loadKernelSnapshot(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != kernel_snapshot_size) throw std::runtime_error("Kernel snapshot must cover physical [0, 0x80004)");
        if (kernel_patch_end_) throw std::runtime_error("Cannot replace a kernel baseline with enabled patches");
        kernel_snapshot_.assign(bytes.begin(), bytes.end());
    }

    void enableStartupKernelPayload(std::span<const std::uint8_t> expected) {
        if (!kernel_patch_end_ || kernel_snapshot_.empty() || !startup_kernel_payload_.empty() || expected.size() != 0x330)
            throw std::runtime_error("Invalid startup kernel payload capability");
        if (kernel_patch_start_ < 0x80075330 && 0x80075000 < kernel_patch_end_)
            throw std::runtime_error("Startup kernel payload overlaps existing patch");
        startup_kernel_payload_.assign(expected.begin(), expected.end());
    }
    unsigned startup_kernel_word_writes = 0;

    void enableKernelPatch(std::uint32_t start, std::uint32_t size) {
        if (kernel_snapshot_.empty() || kernel_patch_end_ || !size || start % 4 || size % 4
                || start < 0x80000000 || start >= 0x80080000 || size > 0x80080000 - start)
            throw std::runtime_error("Invalid bounded kernel patch range");
        kernel_patch_start_ = start;
        kernel_patch_end_ = start + size;
    }
    std::span<const std::uint8_t> kernelBaseline() const { return kernel_snapshot_; }

    // Single captured startup read, not a timer scheduler or mutable register model.
    void supplyOsdConfig(std::uint32_t word) {
        if (osd_config_supplied) throw std::runtime_error("Duplicate OSD checkpoint");
        osd_config_word = word;
        osd_config_supplied = true;
    }
    void supplyOsdReadback(std::uint32_t expected) {
        if (!osd_config_supplied || osd_readback_supplied || osd_config_reads || expected != osd_config_word
                || ((expected & 0xFFFF1FFF) | 0x2000) != expected)
            throw std::runtime_error("Unsupported OSD same-value readback checkpoint");
        osd_readback_supplied = true;
    }
    unsigned osd_config_reads = 0, osd_config_sets = 0;

    void supplyTimer3Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer3_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 3 capture");
        timer3_mode_ = value;
        timer3_mode_supplied_ = true;
    }

    void supplyTimer2Readback(std::uint32_t expected) {
        if (expected > 0xFFFF || timer2_readback_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 readback validation");
        timer2_readback_expected_ = expected;
        timer2_readback_supplied_ = true;
    }

    void supplyIntcMask(std::uint32_t value) {
        if (value > 0xFFFF || intc_mask_supplied_) throw std::runtime_error("Invalid/repeated INTC mask checkpoint");
        intc_mask = value;
        intc_mask_supplied_ = true;
    }

    void supplyTimer2Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer2_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 capture");
        timer2_mode_ = value;
        timer2_mode_supplied_ = true;
    }

    // Reached CACHE 0x18 path only. This adapter has coherent main RAM and no cache state.
    // Validate a complete aligned line; this does not perform DMA or synchronize an IOP.
    void dataCacheLine(std::uint8_t* rdram, std::uint32_t address) {
        if (address >= 0x20000000 && address < 0x22000000) {
            if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 64);
            address -= 0x20000000;
        }
        if (address & 63) throw UnsupportedGuestMemory(address, 64);
        checkMemory(rdram, address, 64);
        ++data_cache_lines;
    }
    unsigned data_cache_lines = 0;
    static constexpr std::size_t sif_iop_size = 2 * 1024 * 1024;
    void supplySifRpcState(std::uint32_t word) {
        if (sif_iop_.empty() || sif_rpc_supplied_ || word != 1)
            throw std::runtime_error("SIF RPC state checkpoint differs or was already supplied");
        sif_rpc_supplied_ = true;
    }
    unsigned sif_rpc_reads = 0;
    void supplySifDma(std::span<const std::uint8_t> before, std::uint32_t result) {
        if (!sif_command_supplied_ || !sif_iop_.empty() || before.size() != sif_iop_size || result != 0x701)
            throw std::runtime_error("SIF DMA checkpoint size/return/order differs");
        std::uint32_t buffer, system_buffer, old_ee;
        std::memcpy(&buffer, before.data() + 0x196C0, 4);
        std::memcpy(&system_buffer, before.data() + 0x196C4, 4);
        std::memcpy(&old_ee, before.data() + 0x196C8, 4);
        if (buffer != 0x19600 || system_buffer != 0x19680 || old_ee != 0x93640)
            throw std::runtime_error("SIF IOP checkpoint table differs");
        sif_iop_.assign(before.begin(), before.end());
        sif_dma_result_ = result;
        sif_ee_receive_address = old_ee;
    }
    void supplySifBind(std::span<const std::uint8_t> bundle) {
        if (!sif_rpc_supplied_ || !sif_bind_iop_.empty() || (bundle.size() != sif_iop_size + 68 && bundle.size() != sif_iop_size + 96))
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
        sif_bind_iop_.assign(bundle.begin(), bundle.begin() + sif_iop_size);
        std::copy_n(bundle.data() + sif_iop_size, 64, sif_bind_reply_.data());
    }
    void supplyCdInit(std::span<const std::uint8_t> bundle) {
        if (sif_bind_iop_.empty() || !sif_call_sema_supplied_ || cd_init_supplied_ || (bundle.size() != 88 && bundle.size() != 128))
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
        std::copy_n(bundle.begin(), 88, cd_init_bundle_.begin());
        cd_init_supplied_ = true;
    }
    void supplyScmdBind(std::span<const std::uint8_t> bundle) {
        const std::array<std::uint32_t, 22> expected{0x370000,1,0,0,2,0x3838D8,
            0,0,0x80000008,0,0,0x203A97C0,0,0x369F40,0x80000009,0x3F690,0x3F6D8,0,0,0,0,0};
        if (!cd_semaphores_supplied_ || scmd_bind_supplied_ || (bundle.size() != 88 && bundle.size() != 184 && bundle.size() != 188 && bundle.size() != 192 && bundle.size() != 204 && bundle.size() != 224 && bundle.size() != 228 && bundle.size() != 236 && bundle.size() != 244 && bundle.size() != 252 && bundle.size() != 316 && bundle.size() != 324)
                || std::memcmp(bundle.data(), expected.data(), 88))
            throw std::runtime_error("Invalid SCMD bind checkpoint");
        if (bundle.size() >= 184) {
            const std::array<std::uint32_t,24> mmode{0,1,0,0,0x370000,0x3838E8,
                0x400,0x3695C0,0x80000008,0,0,0,0,0x369F40,0x8000000A,0,0,0,0,0,0,0,2,0};
            if(std::memcmp(bundle.data()+88,mmode.data(),96)) throw std::runtime_error("Invalid media-mode checkpoint");
            std::copy_n(bundle.data()+112,64,scmd_mmode_reply_.data());scmd_mmode_supplied_=true;
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
            gs_csr_supplied_=true;
        }
        if(bundle.size()>=244) {
            std::uint64_t result;std::memcpy(&result,bundle.data()+236,8);
            if(result!=0xFF00)throw std::runtime_error("Invalid GS IMR checkpoint");
            gs_imr_supplied_=true;
        }
        if(bundle.size()>=252) {
            std::uint64_t result;std::memcpy(&result,bundle.data()+244,8);
            if(result)throw std::runtime_error("Invalid GS CRT checkpoint");
            gs_crt_supplied_=true;
        }
        if(bundle.size()>=316) {
            const std::array<std::uint32_t,16> evidence{12,0x1802,0x1FFEAD0,1,8,0x1802,0x1FFEAD0,1,
                12,0x1802,0x1FFEAD0,4,8,0x1802,0x1FFEAD0,1};
            if(std::memcmp(bundle.data()+252,evidence.data(),64))throw std::runtime_error("INTC evidence differs");
            gs_interrupt_supplied_=true;
        }
        if(bundle.size()==324) {
            std::uint64_t field;std::memcpy(&field,bundle.data()+316,8);
            if(field!=0x551B6004)throw std::runtime_error("GS field checkpoint differs");
            gs_field_supplied_=true;
        }
        std::copy_n(bundle.data()+24,64,scmd_reply_.data()); scmd_bind_supplied_=true;
    }
    bool iop_reset_pending=false, prior_iop_services_valid=true;
    IntcCheckpoint gs_interrupt;
    unsigned gs_field_reads=0;
    unsigned gs_csr_reset_writes=0,gs_csr_reads=0,gs_imr_writes=0,gs_crt_calls=0;
    std::uint32_t gs_imr=0;
    std::array<std::uint32_t,3> gs_crt_request{};
    unsigned iop_epoch=0, reset_clear_writes=0, reset_ready_checkpoint_reads=0;
    std::uint32_t reset_rpcinit=1, reset_subaddr=0x19600;
    unsigned sif_stop_calls = 0, sif_reset_subaddr_reads = 0, reset_smflag_writes = 0;
    std::uint32_t sif_smflag = 0;
    std::uint32_t sif2_chcr = 0, sif2_qwc = 0;
    unsigned scmd_bind_calls = 0, scmd_mmode_calls = 0;
    unsigned semaphore_polls = 0;
    unsigned cd_init_calls = 0, cd_init_context_preservations = 0, cd_semaphores_created = 0;
    unsigned sif_bind_callbacks = 0;
    unsigned sif_bind_context_preservations = 0;
    unsigned semaphore_deletions = 0;
    unsigned rpc_call_semaphores = 0;
    std::uint32_t rpc_call_sema_attributes = 0;
    std::span<const std::uint8_t> sifIopMemory() const { return sif_iop_; }
    unsigned sif_dma_submissions = 0;
    std::uint32_t sif_ee_receive_address = 0;
    void supplySifCommand(std::uint32_t word) {
        if (!dma5_control_supplied_ || sif_command_supplied_ || word != 0x19600)
            throw std::runtime_error("SIF checkpoint differs or was already supplied");
        sif_command_word_ = word;
        sif_command_supplied_ = true;
    }
    unsigned sif_command_reads = 0;
    void supplyDmaStatus(std::uint32_t word) {
        if (dma_status_supplied_ || word != 0x00800040)
            throw std::runtime_error("Unsupported/repeated DMA status checkpoint");
        dma_status_word_ = word;
        dmac_mask = word & 0xFFFF0000;
        dma_status_supplied_ = true;
    }
    void supplyDma5Control(std::uint32_t word) {
        if (!dma_status_supplied_ || dma5_control_supplied_ || word != 0x184)
            throw std::runtime_error("Unsupported/repeated DMA 5 control checkpoint");
        dma5_control_word_ = word;
        dma5_control_supplied_ = true;
    }
    unsigned dma_status_reads = 0, dma5_control_reads = 0;
    struct DmacHandler { std::uint32_t channel, address, arg, gp; };
    std::vector<DmacHandler> dmac_handlers;
    std::vector<std::uint32_t> dmac_handler_order;
    std::uint32_t dmac_mask = 0, dmac_enable_calls = 0, dmac_disable_calls = 0, dmac_removals = 0;

    // Emitted slow-path memory ABI; unsupported MMIO still fails explicitly.
    void handleBreak(std::uint8_t*,R5900Context*) {
        throw std::runtime_error("Unsupported guest BREAK instruction");
    }
    void Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
        checkMemory(rdram,0,0);
        if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes || gs_csr_reads
                || context->pc!=0x30FE44 || context->r[29].low!=initial_sp-688 || value!=0x200)
            throw UnsupportedGuestMemory(address,8);
        // Reached reset accepted only with matching reference readback evidence.
        // No native reset of unimplemented GS/GIF/renderer state is claimed.
        ++gs_csr_reset_writes;
    }
    std::uint64_t Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        checkMemory(rdram,0,0);
        if(address==0x12001000 && gs_field_supplied_ && gs_interrupt.acknowledgements==2 && !gs_field_reads
            && context->pc==0x3104F4 && context->r[29].low==initial_sp-640) {
            ++gs_field_reads;return 0x551B6004; // Independent captured field, not timed native GS state.
        }
        if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes!=1 || gs_csr_reads
                || context->pc!=0x30FE50 || context->r[29].low!=initial_sp-688)
            throw UnsupportedGuestMemory(address,8);
        ++gs_csr_reads;return 0x551B4000; // Explicit captured reference readback, not native GS state.
    }
    void Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {
        if(address==0x1000F000 && gs_interrupt_supplied_ && gs_crt_calls==1
            && context->r[29].low==initial_sp-656 && value==4
            && ((context->pc==0x32F10C && !gs_interrupt.acknowledgements)
                || (context->pc==0x32F160 && gs_interrupt.acknowledgements==1))) {
            checkMemory(rdram,0,0);
            if(!gs_interrupt.acknowledgements) {
                if(intc_mask!=0x1802)throw std::runtime_error("INTC mask differs from checkpoint");
                gs_interrupt.arm(12,intc_mask);
            }
            gs_interrupt.acknowledge(value);return;
        }
        write32(rdram, address, value);
    }

    std::uint32_t Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        if(address==0x1000F000 && gs_interrupt_supplied_ && context->pc==0x32F130
            && context->r[29].low==initial_sp-656) {
            checkMemory(rdram,0,0);return gs_interrupt.poll();
        }
        if (address == 0x10001810 && timer3_mode_supplied_ && !timer3_mode_reads) {
            checkMemory(rdram, 0, 0);
            ++timer3_mode_reads;
            return timer3_mode_;
        }
        if (address == 0x10001010 && timer2_mode_supplied_ && !timer2_mode_reads) {
            checkMemory(rdram, 0, 0);
            ++timer2_mode_reads;
            return timer2_mode_;
        }
        if (address == 0x10001010 && timer2_readback_supplied_ && timer2_mode_reads == 1 && timer2_mode_writes == 1) {
            checkMemory(rdram, 0, 0);
            if (timer2_mode_after_write != timer2_readback_expected_)
                throw std::runtime_error("Timer 2 modeled mode differs from captured readback");
            ++timer2_mode_reads;
            return timer2_mode_after_write;
        }
        return read32(rdram, address);
    }

    std::uint32_t read32(std::uint8_t* rdram, std::uint32_t address) {
        address = uncachedWordAddress(rdram, address);
        if (address == 0x1000E010 && dma_status_supplied_ && !dma_status_reads) {
            checkMemory(rdram, 0, 0);
            ++dma_status_reads;
            return dma_status_word_;
        }
        if (address == 0x1000C000 && dma5_control_supplied_ && dma_status_reads == 1 && !dma5_control_reads) {
            checkMemory(rdram, 0, 0);
            ++dma5_control_reads;
            return dma5_control_word_;
        }
        // Read-only KSEG0 view of locally supplied kernel bytes; no fabricated kernel.
        if (address >= 0x80000000 && address < 0x80000000 + kernel_snapshot_size) {
            const auto offset = address - 0x80000000;
            if (kernel_snapshot_.empty() || memory_.empty() || rdram != memory_.data()
                || offset % 4 || offset > kernel_snapshot_.size() - 4)
                throw UnsupportedGuestMemory(address, 4);
            if (const auto patched = kernel_patch_words_.find(offset); patched != kernel_patch_words_.end())
                return patched->second;
            std::uint32_t value;
            std::memcpy(&value, kernel_snapshot_.data() + offset, 4);
            return value;
        }
        checkMemory(rdram, address, 4);
        if (address % 4) throw std::runtime_error("Unsupported unaligned word read");
        std::uint32_t value;
        std::memcpy(&value, memory_.data() + address, sizeof(value));
        return value;
    }

    bool eeCheckpointDue() {
        if (++checkpoints_ > 1000000) throw std::runtime_error("Bootstrap execution budget exceeded");
        return false; // Run to a checked boundary, rather than pretending to schedule the EE.
    }

    std::uint32_t expected_return = 0;
    std::uint32_t syscall_resume_pc = 0;
    bool thread_initialized = false, heap_initialized = false;
    std::uint32_t heap_start = 0, heap_end = 0;
    std::uint32_t stack_base = 0, initial_sp = 0;
    std::array<std::uint32_t, 5> thread_arguments{};
    struct DormantThread {
        std::uint32_t id, entry, stack_base, stack_size, gp, initial_priority, heap_base, context_address, initial_sp;
    };
    std::vector<DormantThread> threads;
    std::uint32_t current_thread_id = 0, main_thread_priority = 0;
    unsigned priority_changes = 0;
    struct Semaphore { std::uint32_t count, maximum, option; };
    // Bounded bootstrap objects; no wait queues or scheduling implementation.
    std::vector<Semaphore> semaphores;
    std::uint32_t semaphore_waits = 0, semaphore_signals = 0;
    std::array<std::uint32_t, 256> syscall_handlers{};
    unsigned syscall_registrations = 0, custom_syscall_calls = 0;
    struct IntcHandler { std::uint32_t cause, address, arg, gp; };
    // Shared 128-entry pool, IDs start at one, matching pinned Play!.
    std::vector<IntcHandler> intc_handlers;
    std::vector<std::uint32_t> intc_handler_order;
    std::uint32_t timer3_interrupt_handler = 0;
    bool timer3_interrupt_enable_requested = false;
    unsigned extended_registrations = 0;
    std::uint32_t intc_mask = 0;
    unsigned intc_enable_calls = 0;
    bool startup_zero_verified = false;
    std::uint64_t startup_quadword_writes = 0, startup_byte_writes = 0;
    std::uint64_t kernel_word_writes = 0;
    unsigned timer3_mode_reads = 0, timer2_mode_reads = 0, timer2_mode_writes = 0;
    std::uint32_t timer2_mode_after_write = 0;
    unsigned cache_requests = 0, instruction_cache_invalidations = 0;
    unsigned kernel_dispatch_validations = 0;
    unsigned returns = 0, guest_calls = 0;
    std::uint64_t halfword_writes=0;
    std::uint64_t doubleword_writes = 0, word_writes = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

private:
    void receiveFirstRpcBind(std::uint8_t* rdram, R5900Context* context) {
        if (sif_rpc_reads != 1 || sif_bind_callbacks || data_cache_lines != 2
                || context->r[4].low != initial_sp - 464 || context->r[5].low != 1
                || context->r[29].low != context->r[4].low || rpc_end_callback_active_)
            throw UnsupportedSyscall(0x77, context->pc);
        const auto descriptor = static_cast<std::uint32_t>(context->r[4].low);
        const std::array<std::uint32_t, 4> expected_descriptor{0x203A97C0, 0x19600, 64, 0x44};
        const std::array<std::uint32_t, 16> request{64, 0, 0x80000009, 0, 5, 0x203A97C0, 2,
            0x3A87A8, 0x80000592, 0, 0, 0, 0, 0, 0, 0};
        for (unsigned index = 0; index < expected_descriptor.size(); ++index)
            if (read32(rdram, descriptor + index * 4) != expected_descriptor[index])
                throw UnsupportedSyscall(0x77, context->pc);
        for (unsigned index = 0; index < request.size(); ++index)
            if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
                throw UnsupportedSyscall(0x77, context->pc);
        if (sif_ee_receive_address != 0x3A94C0 || read32(rdram, 0x3A87A8) != 0x203A97C0
                || read32(rdram, 0x3A87AC) != 2 || read32(rdram, 0x3A87B0) != 3
                || semaphores.size() != 4 || semaphores[3].count != 0 || semaphores[3].maximum != 1
                || read32(rdram, 0x3A9620) != 0x3322E8 || read32(rdram, 0x3A9624) != 0x3AAFC0
                || read32(rdram, 0x3A9628) != 0x38BDF0 || !functions_.contains(0x3322E8)
                || !functions_.contains(0x332258) || !functions_.contains(0x32EC90))
            throw UnsupportedSyscall(0x77, context->pc);
        // One checked logical service lookup/response. No IOP instruction execution,
        // hardware DMA padding, interrupt entry, cycles or general RPC scheduler.
        std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
        sif_iop_[0x19600] = 0;
        std::copy(sif_bind_reply_.begin(), sif_bind_reply_.end(), memory_.begin() + 0x3A94C0);
        deliverRpcEnd(rdram, context);
        ++sif_bind_context_preservations;
        ++sif_bind_callbacks;
        ++sif_dma_submissions;
        context->r[2].low = 0x801;
    }
    void deliverRpcEnd(std::uint8_t* rdram, R5900Context* context,
                       std::uint32_t client = 0x3A87A8, unsigned semaphore = 3,
                       std::uint32_t server = 0x3F570, std::uint32_t buffer = 0x3FB20) {
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
        if (callback.r[29].low != context->r[29].low || semaphores[semaphore].count != 1
                || read32(rdram, client) != 0 || read32(rdram, client + 36) != server
                || read32(rdram, client + 20) != buffer || read32(rdram, 0x3A97D0) != 4
                || read32(rdram, 0x3A97D8) != 0)
            throw std::runtime_error("Generated RPC_END effects differ from capture");
        if (std::memcmp(saved_cpu.data(), context, saved_cpu.size()))
            throw std::runtime_error("RPC_END changed interrupted CPU state");
    }
    void receiveFirstCdInit(std::uint8_t* rdram, R5900Context* context) {
        if (cd_init_calls || sif_bind_callbacks != 1 || sif_dma_submissions != 2
                || context->r[4].low != initial_sp - 544 || context->r[29].low != context->r[4].low
                || context->r[5].low != 2 || data_cache_lines != 6 || semaphores.size() != 4
                || semaphores[3].count != 0 || semaphores[3].maximum != 1
                || semaphores[3].option != 0x3838E8 || read32(rdram, 0x3A8800) != 0
                || read32(rdram, 0x3A87A8) != 0x203A97C0 || read32(rdram, 0x3A87AC) != 3
                || read32(rdram, 0x3A87B0) != 3 || read32(rdram, 0x3A87C4) != 0
                || read32(rdram, 0x3A87C8) != 0)
            throw UnsupportedSyscall(0x77, context->pc);
        const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
            0x203A97C0, 0x19600, 64, 0x44};
        const std::array<std::uint32_t, 16> request{0x440, 0x3FB20, 0x8000000A, 0,
            5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
        for (unsigned index = 0; index < descriptors.size(); ++index)
            if (read32(rdram, static_cast<std::uint32_t>(context->r[4].low) + index * 4) != descriptors[index])
                throw UnsupportedSyscall(0x77, context->pc);
        for (unsigned index = 0; index < request.size(); ++index)
            if (read32(rdram, 0x3A97C0 + index * 4) != request[index])
                throw UnsupportedSyscall(0x77, context->pc);
        // One explicit standalone-ELF mode-0 completion checkpoint. Does not
        // execute retail IOP code, access a disc or provide general CD status.
        std::copy_n(memory_.data() + 0x3A8800, 4, sif_iop_.data() + 0x3FB20);
        std::copy_n(memory_.data() + 0x3A97C0, 64, sif_iop_.data() + 0x19600);
        sif_iop_[0x19600] = 0;
        std::copy_n(cd_init_bundle_.data() + 8, 64, memory_.data() + 0x3A94C0);
        std::copy_n(cd_init_bundle_.data() + 72, 16, memory_.data() + 0x3695C0);
        deliverRpcEnd(rdram, context);
        ++cd_init_calls;
        ++cd_init_context_preservations;
        ++sif_dma_submissions;
        context->r[2].low = 0x902;
    }
    void receiveScmdBind(std::uint8_t* rdram, R5900Context* context) {
        if (scmd_bind_calls || cd_init_calls != 1 || cd_semaphores_created != 4
                || context->r[4].low != initial_sp - 432 || context->r[29].low != context->r[4].low
                || context->r[5].low != 1 || semaphores.size() != 8 || semaphores[7].count != 0
                || semaphores[7].maximum != 1 || semaphores[7].option != 0x3838D8
                || read32(rdram,0x369F40) != 0x203A97C0 || read32(rdram,0x369F44) != 4
                || read32(rdram,0x369F48) != 7)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,4> descriptor{0x203A97C0,0x19600,64,0x44};
        const std::array<std::uint32_t,16> request{64,0,0x80000009,0,5,0x203A97C0,4,0x369F40,
            0x80000593,4,0x3695C0,16,1,0x3F570,0,0};
        for (unsigned i=0;i<4;++i) if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptor[i])
            throw UnsupportedSyscall(0x77,context->pc);
        for (unsigned i=0;i<16;++i) if(read32(rdram,0x3A97C0+i*4)!=request[i])
            throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600); sif_iop_[0x19600]=0;
        std::copy(scmd_reply_.begin(),scmd_reply_.end(),memory_.begin()+0x3A94C0);
        deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
        ++scmd_bind_calls; ++sif_dma_submissions;
        // Authored positive logical submission handle; not a captured hardware DMA ID.
        // The synchronous bind consumes only the sign before waiting on its semaphore.
        context->r[2].low=1;
    }
    void receiveScmdMmode(std::uint8_t* rdram,R5900Context* context) {
        if(scmd_mmode_calls || scmd_bind_calls!=1 || semaphore_deletions!=3 || semaphores.size()!=8
            || semaphores[7].count!=0 || semaphores[7].maximum!=1 || semaphores[7].option!=0x3838E8
            || context->r[4].low!=initial_sp-448 || context->r[29].low!=context->r[4].low || context->r[5].low!=2
            || read32(rdram,0x369A00)!=2 || read32(rdram,0x369F40)!=0x203A97C0
            || read32(rdram,0x369F44)!=5 || read32(rdram,0x369F48)!=7
            || read32(rdram,0x369F5C)!=0 || read32(rdram,0x369F60)!=0)
            throw UnsupportedSyscall(0x77,context->pc);
        const std::array<std::uint32_t,8> descriptors{0x369A00,0x3F6D8,4,0,0x203A97C0,0x19600,64,0x44};
        const std::array<std::uint32_t,16> request{0x440,0x3F6D8,0x8000000A,0,5,0x203A97C0,5,0x369F40,34,4,0x3695C0,4,1,0x3F690,0,0};
        for(unsigned i=0;i<8;++i)if(read32(rdram,static_cast<std::uint32_t>(context->r[4].low)+i*4)!=descriptors[i])throw UnsupportedSyscall(0x77,context->pc);
        for(unsigned i=0;i<16;++i)if(read32(rdram,0x3A97C0+i*4)!=request[i])throw UnsupportedSyscall(0x77,context->pc);
        std::copy_n(memory_.data()+0x369A00,4,sif_iop_.data()+0x3F6D8);
        std::copy_n(memory_.data()+0x3A97C0,64,sif_iop_.data()+0x19600);sif_iop_[0x19600]=0;
        std::copy(scmd_mmode_reply_.begin(),scmd_mmode_reply_.end(),memory_.begin()+0x3A94C0);
        std::fill_n(memory_.data()+0x3695C0,4,0);
        deliverRpcEnd(rdram,context,0x369F40,7,0x3F690,0x3F6D8);
        ++scmd_mmode_calls;++sif_dma_submissions;context->r[2].low=1;
    }
    bool gs_interrupt_supplied_=false,gs_field_supplied_=false;
    bool gs_csr_supplied_=false,gs_imr_supplied_=false,gs_crt_supplied_=false;
    bool reset_submission_supplied_=false, reset_ready_supplied_=false;
    std::array<std::uint8_t,104> reset_request_{};
    bool sif_stop_supplied_ = false, sif_reset_subaddr_supplied_ = false, reset_smflag_supplied_ = false;
    bool scmd_mmode_supplied_ = false;
    std::array<std::uint8_t,64> scmd_mmode_reply_{};
    bool scmd_bind_supplied_ = false;
    std::array<std::uint8_t,64> scmd_reply_{};
    bool cd_init_supplied_ = false, cd_semaphores_supplied_ = false;
    std::array<std::uint8_t, 88> cd_init_bundle_{};
    bool sif_call_sema_supplied_ = false;
    std::vector<std::uint8_t> sif_bind_iop_;
    std::array<std::uint8_t, 64> sif_bind_reply_{};

    std::vector<std::uint8_t> sif_iop_;
    std::uint32_t sif_dma_result_ = 0;
    bool sif_rpc_supplied_ = false;
    bool rpc_end_callback_active_ = false;
    bool sif_command_supplied_ = false;
    std::uint32_t sif_command_word_ = 0;
    bool dma_status_supplied_ = false, dma5_control_supplied_ = false;
    std::uint32_t dma_status_word_ = 0, dma5_control_word_ = 0;
    bool osd_config_supplied = false, osd_readback_supplied = false;
    std::uint32_t osd_config_word = 0;
    std::uint64_t addIntcHandler(std::uint32_t cause, std::uint32_t address, std::uint32_t arg, std::uint32_t gp) {
        if (intc_handlers.size() == 128) return UINT64_MAX;
        intc_handlers.push_back({cause, address, arg, gp});
        const auto id = static_cast<std::uint32_t>(intc_handlers.size());
        intc_handler_order.insert(intc_handler_order.begin(), id);
        return id;
    }
    std::uint32_t startup_zero_start_ = 0, startup_zero_end_ = 0;
    void verifyKernelBinding(std::uint32_t address) {
        const auto found = kernel_code_bindings_.find(address);
        if (found == kernel_code_bindings_.end()) return;
        for (std::size_t offset = 0; offset < found->second.size(); offset += 4) {
            std::uint32_t expected;
            std::memcpy(&expected, found->second.data() + offset, 4);
            const auto word = kernel_patch_words_.find(address - 0x80000000 + static_cast<std::uint32_t>(offset));
            if (word == kernel_patch_words_.end() || word->second != expected)
                throw std::runtime_error("Copied kernel payload differs from compiled binding");
        }
        ++kernel_dispatch_validations;
    }
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> kernel_code_bindings_;
    void checkMemory(std::uint8_t* rdram, std::uint32_t address, std::size_t count) const {
        if (memory_.empty() || rdram != memory_.data() || address > memory_.size() || count > memory_.size() - address)
            throw UnsupportedGuestMemory(address, count);
    }
    std::unordered_map<std::uint32_t, RecompiledFunction> functions_;
    std::vector<std::uint8_t> memory_, kernel_snapshot_, startup_kernel_payload_;
    std::unordered_map<std::uint32_t, std::uint32_t> kernel_patch_words_;
    std::uint32_t kernel_patch_start_ = 0, kernel_patch_end_ = 0;
    std::vector<std::uint32_t> call_returns_;
    unsigned checkpoints_ = 0, dispatch_depth_ = 0;
    bool timer3_mode_supplied_ = false;
    std::uint32_t timer3_mode_ = 0, timer2_mode_ = 0;
    bool timer2_mode_supplied_ = false, intc_mask_supplied_ = false, timer2_readback_supplied_ = false;
    std::uint32_t timer2_readback_expected_ = 0;
};
