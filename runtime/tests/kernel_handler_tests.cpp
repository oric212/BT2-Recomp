#include "ee/ee_runtime.h"
#include "ee/ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <iostream>

// The game's FindAddress (0x338388, installed as syscall 0x83) over the kernel's memory as the runtime shows it: the
// syscall table at 0x80014D00 holds the handlers registered with SetSyscall, the rest reads as zero.
void require(bool ok) { if (!ok) throw std::runtime_error("Kernel handler check failed"); }
int main() {
    try {
        EeRuntime runtime;
        runtime.loadMemory(std::vector<std::uint8_t>(EeRuntime::ram_size));
        runtime.expected_return = 0xFFFFFFFC;
        runtime.registerFunction(0x338388, FUN_00338388_0x338388);
        const std::uint32_t target = 0x123456;
        runtime.syscall_handlers[2] = target; // As SetSyscall(2, target) leaves it.
        const auto find = [&](std::uint32_t start, std::uint32_t end, std::uint32_t value) {
            R5900Context ctx{};
            SET_GPR_S32(&ctx, 4, static_cast<std::int32_t>(start));
            SET_GPR_S32(&ctx, 5, static_cast<std::int32_t>(end));
            SET_GPR_U32(&ctx, 6, value);
            SET_GPR_U32(&ctx, 31, runtime.expected_return);
            runtime.callFunction(0x338388, ctx);
            require(ctx.pc == runtime.expected_return);
            return GPR_U32(&ctx, 2);
        };
        require(find(0x80000000, 0x80080000, target) == 0x80014D08); // Found in the table's slot 2, as the game finds its own.
        require(find(0x80000000, 0x80080000, 0x654321) == 0);        // Not registered: not found.
        auto* ram = runtime.memory().data();
        require(runtime.read32(ram, 0x80014D08) == target && runtime.read32(ram, 0x80014D04) == 0 && runtime.read32(ram, 0x80080000) == 0);
        bool rejected = false;
        try { (void)runtime.read32(ram, 0x80080004); } catch (const UnsupportedGuestMemory&) { rejected = true; }
        require(rejected);
        rejected = false;
        try { runtime.write32(ram, 0x80000000, 1); } catch (const UnsupportedGuestMemory&) { rejected = true; }
        require(rejected);
        std::cout << "Generated FindAddress over the native kernel view: match, no match and read-only bounds passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
