# Runtime source layout

`runtime/` holds the handwritten compatibility layer: the code that runs the generated BT2 functions and the native services that answer their IOP requests. Generated code is never stored here. It stays in the ignored `game/generated/` and `work/` folders.

## Folders

| Folder | Responsibility |
| --- | --- |
| `ee/` | EE runtime used by generated code. `ps2_runtime.h` holds `PS2Runtime`'s state and small hot helpers. Its implementation is split into `ps2_runtime_syscalls.cpp` (syscall and SIF RPC dispatch), `ps2_runtime_memory.cpp` (guest memory and memory-mapped I/O), `ps2_runtime_services.cpp` (owned SIF/RPC service handlers) and `ps2_runtime_dispatch.cpp` (guest calls and kernel binding). Thread scheduling (thread syscalls, workers on retained stacks, main sleep/wake) is in `ee_thread_scheduler.cpp`, with the worker execution owner in `ee_execution_owner.h`. The folder also has the FPU, the retained EE stack, logical timers, startup checkpoints, the IPU setup model (`ipu_device.*`: tables, thresholds and reset; no decoding) and stopped DMA channels (`stopped_dma_channels.h`: channel registers retained, starts refused unless an owner models them, as scratchpad DMA does). |
| `gs/` | GIF/VIF1 feeders shared by both renderers: GIF DMA staging and the startup texture/VIF checkpoints. |
| `gs/cpu/` | CPU GS model: the command sink (`gs_command_sink.*`: packet validation, GS memory, transfers, sprite and triangle drawing; `rasterize=false` keeps validation only) and the displayed-framebuffer decoder (`cpu_display.h`). Fallback renderer for systems without a suitable Vulkan GPU, and the test reference. |
| `gs/gpu/` | paraLLEl-GS backend (`gpu_gs_backend.*`) and mirror/GPU-only driver (`gpu_gs_mirror.*`), built with `BT2_GPU_GS`. |
| `host/` | Host presentation: decoding the displayed framebuffer and the live viewer window. |
| `services/` | Native SIF RPC services that replace the IOP ([plan](../plans/iop/NATIVE_SERVICES_PLAN.md)); no IOP code runs. The generic SIF RPC layer (`sif_rpc.*`), one file per service (disc, FILEIO, memory card, controller, IOP heap and uploads, LOADFILE, sound driver and SOUNDS, CRI ADX), the IOP heap (`iop_heap.h`), the pad block layout (`pad_block.h`), and `devices/`: ISO metadata and sector reads, and the DualShock 2 with its deterministic input script (`pad_script.h`). |
| `probe/` | Executables. `entry_probe.cpp` runs the startup checkpoint in stages, each in its own file: evidence inputs, generated-code bindings, checkpoint checks and reports, sharing `probe_state.h`. The selection loop's entries are bound in `selected_bindings.cpp` and `overlay_bindings.cpp`, so a new selection recompiles only those small files. The folder also has the generated-code and RPC-bind probes. |
| `tests/` | Unit and subsystem tests registered with CTest. |

`main.cpp` is the plain host executable (`bt2-port`).

## Conventions

- **Includes:** handwritten code includes headers from another folder by their path from `runtime/`, for example `#include "gs/cpu/gs_command_sink.h"`. Headers in the same folder are included by name. Generated code includes `ps2_runtime.h` by name, through the `runtime/ee` include directory.
- **Headers reached by generated code:** keep these to declarations, state and small hot helpers (`ps2_runtime.h`, `gs_command_sink.h`). An edit to one of them recompiles every generated unit that includes it. Put implementation in the folder's `.cpp` files.
- **New code:** a new subsystem or service gets its own file in the matching folder. Add the file to the corresponding source list in `CMakeLists.txt`:
  - `BT2_EE_RUNTIME_SOURCES` (EE runtime and native services).
