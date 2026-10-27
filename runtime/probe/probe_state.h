#pragma once
#include "ee/ee_runtime.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Shared state of the entry probe (entry_probe.cpp runs the stages below in order): the boundary
// where the run stopped.

// The program, the guest image, the report, 20 evidence files, the disc image and the command bind.
inline constexpr int probe_argument_count=25;

// The checked boundary where the generated entry stopped.
struct ProbeStop {
    bool stopped=false,execution_budget_exhausted=false;
    std::uint32_t address=0;
    std::size_t size=0;
    std::uint32_t selector=0,handler=0,target=0,source=0,resume=0;
};

// The observed startup zero region, poisoned before the run so zero stores are observable.
inline constexpr std::uint32_t startup_zero_start=0x00387E80,startup_zero_end=0x0043058F;

inline void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

// Evidence inputs (probe_inputs.cpp): the boot's state and the captures the native services' answers
// are checked against, in the order the startup reaches them.
void loadStartupEvidence(EeRuntime& runtime,char** argv);
void loadContinuationEvidence(EeRuntime& runtime,char** argv);
void loadScmdEvidence(EeRuntime& runtime,char** argv);

// Generated-code bindings (startup_bindings.cpp).
void registerStartupFunctions(EeRuntime& runtime);
void registerKernelPayloads(EeRuntime& runtime);
void registerSelectedStartup(EeRuntime& runtime);  // selected_bindings.cpp
void registerSelectedOverlays(EeRuntime& runtime); // overlay_bindings.cpp

// Checkpoint validation (checkpoint_checks.cpp): each throws if the reached state differs.
void checkRuntimeBoundary(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop);

// Reports (probe_reports.cpp): the private JSON stop report at argv[2] and its evidence sidecars.
void writeRuntimeBoundaryReport(EeRuntime& runtime,R5900Context& context,const ProbeStop& stop,char** argv);
void writeEvidenceDumps(EeRuntime& runtime,char** argv);
