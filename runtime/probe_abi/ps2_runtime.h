#pragma once

// Authored bootstrap adapter for generated probes. Broader PS2 APIs fail explicitly.
#include <array>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>

struct alignas(16) ProbeGpr {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
};

struct R5900Context {
    std::array<ProbeGpr, 32> r{};
    std::uint32_t pc = 0;
    std::uint64_t hi = 0, lo = 0, hi1 = 0, lo1 = 0;
    std::uint32_t sa = 0, fcr31 = 0, cop0_status = 0, branch_pc = 0;
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

    void registerFunction(std::uint32_t address, RecompiledFunction function) {
        if (!function || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate guest registration");
    }

    void callFunction(std::uint32_t address, R5900Context& context) {
        const auto found = functions_.find(address);
        if (found == functions_.end()) throw std::runtime_error("Unregistered guest target");
        context.pc = address;
        checkpoints_ = 0;
        found->second(memory_.empty() ? nullptr : memory_.data(), &context, this);
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
        if (code == 0 && selector == 0x74) {
            const auto number = static_cast<std::uint32_t>(context->r[4].low);
            const auto address = static_cast<std::uint32_t>(context->r[5].low);
            if (number >= syscall_handlers.size() || address % 4)
                throw std::runtime_error("Unsupported SetSyscall registration");
            if (address) checkMemory(rdram, address, 4);
            syscall_handlers[number] = address;
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
            stack_base = top - size;
            initial_sp = top - 0x2A0;
            // Explicit empty launch: argc=0, argv[0]=nullptr.
            std::memset(memory_.data() + args, 0, 8);
            thread_arguments = {static_cast<std::uint32_t>(context->r[4].low), stack, size,
                                args, static_cast<std::uint32_t>(context->r[8].low)};
            context->r[2].low = initial_sp;
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
        if (static_cast<std::uint32_t>(context->r[4].low) != 0)
            throw std::runtime_error("Unsupported probe cache operation");
        // Record writeback request only. This memory-free probe has no guest cache/DMA.
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

    std::uint32_t read32(std::uint8_t* rdram, std::uint32_t address) {
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
    struct Semaphore { std::uint32_t count, maximum, option; };
    // Bounded bootstrap objects; no wait queues or scheduling implementation.
    std::vector<Semaphore> semaphores;
    std::array<std::uint32_t, 256> syscall_handlers{};
    unsigned syscall_registrations = 0, custom_syscall_calls = 0;
    unsigned cache_requests = 0;
    unsigned returns = 0, guest_calls = 0;
    std::uint64_t doubleword_writes = 0, word_writes = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

private:
    void checkMemory(std::uint8_t* rdram, std::uint32_t address, std::size_t count) const {
        if (memory_.empty() || rdram != memory_.data() || address > memory_.size() || count > memory_.size() - address)
            throw UnsupportedGuestMemory(address, count);
    }
    std::unordered_map<std::uint32_t, RecompiledFunction> functions_;
    std::vector<std::uint8_t> memory_;
    std::vector<std::uint32_t> call_returns_;
    unsigned checkpoints_ = 0, dispatch_depth_ = 0;
};
