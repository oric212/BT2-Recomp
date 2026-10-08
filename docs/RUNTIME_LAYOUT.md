# Runtime source layout

`runtime/` holds the handwritten PS2 compatibility layer: the code that runs the generated BT2 functions and the original IOP modules. Generated code is never stored here. It stays in the ignored `game/generated/` and `work/` folders.

## Folders

| Folder | Responsibility |
| --- | --- |
| `ee/` | EE runtime used by generated code. `ps2_runtime.h` holds `PS2Runtime`'s state and small hot helpers. Its implementation is split into `ps2_runtime_syscalls.cpp` (syscall and SIF RPC dispatch), `ps2_runtime_memory.cpp` (guest memory and memory-mapped I/O), `ps2_runtime_services.cpp` (owned SIF/RPC service handlers) and `ps2_runtime_dispatch.cpp` (guest calls and kernel binding). The folder also has the FPU, the retained EE stack, logical timers and startup checkpoints. |
| `gs/` | GS and GIF/VIF1 path: the GS command sink (`gs_command_sink.*`: GS memory, transfers, sprite and triangle drawing), GIF DMA staging and the startup texture/VIF checkpoints. |
| `host/` | Host presentation: decoding the displayed framebuffer and the live viewer window. |
| `iop/core/` | `IopRuntime`: IOP CPU context, owned RAM, memory-mapped hardware (`native_runtime_io.cpp`: SPU2, SIO2, sound DMA, CDVD registers) and logical time (`native_runtime_timing.cpp`: timed waits, alarms, sound timer, VBlank edges). Generated IOP modules include `native_runtime.h`. |
| `iop/devices/` | Device models used by the IOP runtime: SPU2 voice playback, the DualShock 2 with its deterministic input script (`pad_script.h`), and disc metadata/sector reads from the local ISO. |
| `iop/providers/` | High-level IOP kernel and library services, registered as import providers and declared in `providers.h`. Kernel libraries are grouped by function: system, interrupts, DMA, sync, time and threads. Each module-facing library has its own file (loadcore, ioman, sifcmd, vblank, cdvdman). |
| `iop/services/` | `NativeIopService` (`module_service.h`): it loads the compiled original modules (`compiled_modules.cpp`) and runs their RPC services. There is one file per service: boot heap, FILEIO, sound, pad (DBCMAN), memory card, disc and CRI. |
| `iop/ee_bridge/` | EE side of the native IOP: the `PS2Runtime::receive*` handlers that validate EE requests and forward them to `NativeIopService`, one file per service. Built only with `BT2_LINK_NATIVE_IOP`. |
| `probe/` | Executables. `entry_probe.cpp` runs the startup checkpoint in stages, each in its own file: evidence inputs, generated-code bindings, checkpoint checks and reports, sharing `probe_state.h`. The selection loop's entries are bound in `selected_bindings.cpp` and `overlay_bindings.cpp`, so a new selection recompiles only those small files. The folder also has the generated-code, RPC-bind and IOP module probes. |
| `tests/`, `tests/iop/` | Unit and subsystem tests registered with CTest. |

`main.cpp` is the plain host executable (`bt2-port`).

## Conventions

- **Includes:** handwritten code includes headers from another folder by their path from `runtime/`, for example `#include "gs/gs_command_sink.h"`. Headers in the same folder are included by name. Generated code includes `ps2_runtime.h` and `native_runtime.h` by name, through the `runtime/ee` and `runtime/iop/core` include directories.
- **Headers reached by generated code:** keep these to declarations, state and small hot helpers (`ps2_runtime.h`, `gs_command_sink.h`, `native_runtime.h`). An edit to one of them recompiles every generated unit that includes it. Put implementation in the folder's `.cpp` files.
- **New code:** a new subsystem or service gets its own file in the matching folder. Add the file to the corresponding source list in `CMakeLists.txt`:
  - `BT2_EE_RUNTIME_SOURCES`;
  - `BT2_IOP_RUNTIME_SOURCES`;
  - `BT2_IOP_SERVICE_SOURCES`;
  - `BT2_EE_IOP_BRIDGE_SOURCES`.
