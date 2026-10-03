#pragma once

// Authored adapter for the first generated-code probe. Not a full PS2 runtime ABI.
#include <array>
#include <cstdint>
#include <stdexcept>

struct alignas(16) ProbeGpr {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
};

struct R5900Context {
    std::array<ProbeGpr, 32> r{};
    std::uint32_t pc = 0;
};

class PS2Runtime {
public:
    using RecompiledFunction = void (*)(std::uint8_t*, R5900Context*, PS2Runtime*);
    enum class GuestBranchKind { Return };

    void registerFunction(std::uint32_t address, RecompiledFunction function) {
        if (function_ || !function) throw std::runtime_error("Probe supports one registered function");
        address_ = address;
        function_ = function;
    }

    void callFunction(std::uint32_t address, R5900Context& context) {
        if (!function_ || address != address_) throw std::runtime_error("Unregistered guest target");
        context.pc = address;
        function_(nullptr, &context, this); // The selected routine accesses no guest memory.
    }

    void handleSyscall(std::uint8_t*, R5900Context* context, std::uint32_t code) {
        if (code != 0 || static_cast<std::uint32_t>(context->r[3].low) != 0x64)
            throw std::runtime_error("Unsupported probe syscall");
        if (static_cast<std::uint32_t>(context->r[4].low) != 0)
            throw std::runtime_error("Unsupported probe cache operation");
        // Record writeback request only. This memory-free probe has no guest cache/DMA.
        ++cache_requests;
        syscall_resume_pc = context->pc;
    }

    bool dispatchGuestBranch(std::uint8_t*, R5900Context*, std::uint32_t target,
                             std::uint32_t, std::uint32_t, GuestBranchKind, const char*) {
        if (target != expected_return) throw std::runtime_error("Unexpected probe return target");
        ++returns;
        return true;
    }

    std::uint32_t expected_return = 0;
    std::uint32_t syscall_resume_pc = 0;
    unsigned cache_requests = 0;
    unsigned returns = 0;

private:
    std::uint32_t address_ = 0;
    RecompiledFunction function_ = nullptr;
};
