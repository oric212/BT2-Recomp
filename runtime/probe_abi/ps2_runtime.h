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
    UnsupportedSyscall(std::uint32_t selector, std::uint32_t pc)
        : std::runtime_error("Unsupported bootstrap syscall"), selector(selector), resume_pc(pc) {}
    std::uint32_t selector, resume_pc;
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
        if (code != 0 || static_cast<std::uint32_t>(context->r[3].low) != 0x64)
            throw UnsupportedSyscall(static_cast<std::uint32_t>(context->r[3].low), context->pc);
        if (static_cast<std::uint32_t>(context->r[4].low) != 0)
            throw std::runtime_error("Unsupported probe cache operation");
        // Record writeback request only. This memory-free probe has no guest cache/DMA.
        ++cache_requests;
        syscall_resume_pc = context->pc;
    }

    bool dispatchGuestBranch(std::uint8_t*, R5900Context*, std::uint32_t target,
                             std::uint32_t, std::uint32_t, GuestBranchKind kind, const char*) {
        if (kind != GuestBranchKind::Return) throw std::runtime_error("Unsupported bootstrap guest dispatch");
        if (target != expected_return) throw std::runtime_error("Unexpected probe return target");
        ++returns;
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
    bool thread_initialized = false;
    std::uint32_t stack_base = 0, initial_sp = 0;
    std::array<std::uint32_t, 5> thread_arguments{};
    unsigned cache_requests = 0;
    unsigned returns = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

private:
    void checkMemory(std::uint8_t* rdram, std::uint32_t address, std::size_t count) const {
        if (memory_.empty() || rdram != memory_.data() || address > memory_.size() || count > memory_.size() - address)
            throw std::runtime_error("Unsupported or out-of-bounds guest memory access");
    }
    std::unordered_map<std::uint32_t, RecompiledFunction> functions_;
    std::vector<std::uint8_t> memory_;
    unsigned checkpoints_ = 0;
};
