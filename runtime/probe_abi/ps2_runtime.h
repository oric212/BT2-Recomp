#pragma once

// Authored bootstrap adapter for generated probes. Broader PS2 APIs fail explicitly.
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>
#include <immintrin.h>

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
                const bool startup_payload = address == 0x80075000 && startup_kernel_word_writes == 0x330 / 4;
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
            if (attributes != 0) throw std::runtime_error("Unsupported semaphore attributes");
            if (!maximum || maximum > INT32_MAX || initial > maximum || semaphores.size() == 256) {
                context->r[2].low = UINT64_MAX;
                return;
            }
            const auto id = static_cast<std::uint32_t>(semaphores.size());
            semaphores.push_back({initial, maximum, option});
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

    void write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value) {
        checkMemory(rdram, address, 1);
        memory_[address] = value;
        ++byte_writes;
    }

    void write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value) {
        checkMemory(rdram, address, sizeof(value));
        if (address % 16) throw std::runtime_error("Unsupported unaligned quadword store");
        std::memcpy(memory_.data() + address, &value, sizeof(value));
        ++quadword_writes;
    }

    void write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
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

    // Emitted slow-path load ABI; unsupported MMIO still fails explicitly.
    std::uint32_t Load32(std::uint8_t* rdram, R5900Context*, std::uint32_t address) {
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
    std::uint64_t doubleword_writes = 0, word_writes = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

private:
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
