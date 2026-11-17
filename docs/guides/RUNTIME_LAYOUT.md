# Runtime source layout

The `runtime/` folder holds the hand-written part of the port: the code that runs the game's generated functions in
place of the PS2, and the native services that answer the game's requests to the PS2's second processor (the IOP).
This page says what each folder and file is for, and where new code goes.

Generated code is never stored in `runtime/`. It stays in the ignored `game/generated/` and `work/` folders.

## The folders at a glance

| Folder | What it holds |
| --- | --- |
| [`ee/`](#ee-the-main-processor) | The stand-in for the PS2's main processor (the EE) and the hardware around it. Generated code calls into this. |
| [`vu/`](#vu-the-3d-pipeline) | The 3D pipeline: VIF1 and the VU1 vector unit, which turn the game's models into drawing commands. |
| [`gs/`](#gs-graphics) | The graphics chip (the GS): checking its commands and drawing them on the GPU with paraLLEl-GS. |
| [`services/`](#services-the-native-iop-services) | The native services that answer the game's IOP requests: disc, files, memory card, controller, sound. |
| [`host/`](#host-the-pc-side) | Everything on the PC side: window, sound output, controllers, timing, the launcher and menus, online battles. |
| [`game/`](#game-the-players-programs) | The player's programs `bt2-game` and `bt2-launcher`. |
| [`probe/`](#probe-the-runtime-program-and-test-tools) | The runtime program itself (`bt2-entry-probe`) and its test-only tools. |
| [`tests/`](#tests) | Unit and subsystem tests. |

`main.cpp`, at the top of `runtime/`, is a plain host program (`bt2-port`).

## ee: the main processor

This is the runtime the generated code runs on.

**What generated code sees.** Generated code includes only three headers, and an edit to any of them recompiles every
generated file (about 6 minutes):

- `ps2_runtime.h`: the CPU context and `PS2Runtime`, the entry points generated code calls.
- `ps2_runtime_macros.h`: the instruction macros. Main memory is read and written directly here; every other address
  goes through `PS2Runtime`.
- `ps2_fpu.h`: the floating-point unit.

`ps2_stubs.*` and `ps2_syscalls.h` exist only so the generated includes compile; they supply nothing.

**The runtime itself.** `ee_runtime.h` holds `EeRuntime`, the full runtime with its state, derived from `PS2Runtime`.
`guest_interface.cpp` forwards each entry point to it. Generated code never includes `ee_runtime.h`. The work is split
by subject:

- `ps2_runtime_syscalls.cpp`: system calls and SIF RPC dispatch.
- `ps2_runtime_memory.cpp`: guest memory and memory-mapped hardware registers.
- `ps2_runtime_services.cpp`: the handlers of the SIF and RPC services the runtime owns.
- `ps2_runtime_dispatch.cpp`: calls between game functions, entries in the middle of a function, and kernel
  bindings.
- `ee_hardware_registers.*`: hardware registers with one behavior wherever the game reaches them: Timers 1 to 3, the
  DMA controller's control registers, VIF1 and GIF control, the VU reset.
- `ee_gs_readback.cpp`: reading data back from the GS (VIF1 normal-mode DMA, the FIFO and the bus direction).
- `ee_gs_privileged.cpp`: the GS library's reset path (CSR reset and read, the interrupt mask, SetGsCrt).
- `post_reset_sif.*`: the SIF handshake after the IOP reset, as the original performs it.
- `ee_sound_dispatch.*`: the start of the game's sound thread.

**Threads and time.**

- `ee_thread_scheduler.cpp`: thread system calls, worker threads on their own retained stacks, and the main thread's
  sleep and wake. `ee_execution_owner.h` says which thread is running.
- `retained_ee_stack.*`: one cooperative guest stack that can pause and resume a worker thread.
- `logical_timer1.*`, `logical_timer2.*`: Timers 1 and 2 in game time, advanced once per field, not by the PC's
  clock.

**Starting the game.** `guest_image.*` builds the game's starting memory from the disc and checks the executable's
identity.

**Hardware the game drives directly.**

- The video decoder (IPU): `ipu_device.*` (registers, commands and output), `ipu_input.*` (input DMA, the input FIFO
  and the bit pointer), `ipu_vlc.*` (variable-length codes), `ipu_block.*` (macroblock decoding), `ipu_csc.*` (colour
  conversion).
- `dma_chain.*`: walks the game's DMA source chains.
- `vu0_macro.cpp`: VU0 used from the main processor (VU0 data memory instructions, status flags, VPU_STAT).
- `stopped_dma_channels.h`: DMA channels the port does not model. Their registers are kept, but starting one is
  refused unless something models it, as scratchpad DMA is.

**The interpreter bridge.** `ee_interpreter.cpp` runs code that no generated function holds yet, until the next build
compiles it. Its handler table is described in `ee_interpreter_handlers.h`; the handlers themselves are generated, and
`probe/interpreter_handlers_none.cpp` stands in when they are missing.

**Replacing and changing game behavior.**

- `native_functions.*`: game functions replaced by host code where that pays off (such as the C library's memcpy).
  Each is installed only when the game's code matches the expected hash, and falls back to the generated function
  where its fast path does not apply.
- `game_patches.*`: the player's optional patches of the executable (widescreen), applied after it is loaded.
- `struggle_buttons.*`: "Beam struggles by buttons", which turns button presses into the game's own push measure.
- Online battles change only what each player sees and their menus, the same way on both PCs: `online_view.*` (each
  player's view full screen, with their own radar and button prompts), `online_menus.*` (no Return to Main Menu in a
  fight's menus) and `online_lock_on.*` (rounds start locked on).

**Diagnostics.** `state_hash.*` hashes the game's memory every field, to compare runs (`BT2_STATE_HASH`).
`frame_profile.h` measures where each frame's time goes (`BT2_VIEWER_STATS`).

## vu: the 3D pipeline

The game sends its models through VIF1 to the VU1 vector unit, whose small programs ("microprograms") turn them into
drawing commands for the GS ([VU1 pipeline plan](../plans/graphics/VU1_PIPELINE_PLAN.md)). This folder does not depend
on the EE runtime: `EeRuntime` owns one `Vif1` and hands it the words of each VIF1 transfer.

- `vif1.*`: VIF1's registers and commands (UNPACK, MPG, program starts, DIRECT).
- `vu1.*`: VU1's data and program memory, the microprogram interpreter, and the packets it sends to the GS (XGKICK).
- `vu1_program.h`: the key that identifies which program a start runs.
- Native programs: the game's programs are translated to C++ at build time (`vu1_native_none.cpp` stands in when the
  translation is missing). The heaviest ones are written by hand and chosen by program key in
  `vu1_handwritten_registry.cpp`: `vu1_skinned_vertex_programs.cpp` (vertices blended from two bones),
  `vu1_triangle_strip_program.cpp` (a triangle strip with a view-volume test per triangle) and
  `vu1_triangle_clipper.cpp` (clipping one triangle), sharing `vu1_handwritten_helpers.h`.
- `vu1_simd.h`: SSE helpers shared with the translated programs.
- For 60 fps battles: `vif1_blend.*` and `gs_packet_blend.*` blend two frames' VIF1 data and GS packets into the
  picture in between.

## gs: graphics

The GS is the PS2's graphics chip. The port checks every command the game sends it and draws them on the graphics card
with paraLLEl-GS. There is only this one renderer; the CPU renderer was removed on 2026-10-21.

**`gs/cpu/`**: `gs_command_sink.*` checks the GIF data, follows the GS registers, data read back to the game and
FINISH, hands accepted data to the GPU renderer, and feeds the texture names in step with it. It keeps no GS memory
and draws nothing. Its GS memory layout functions are the reference for the texture names.

**`gs/gpu/`**: the GPU renderer, built with `BT2_GPU_GS`.

- `gpu_gs_backend.*`: the paraLLEl-GS backend.
- `gpu_gs_mirror.*`: hands each renderer its GS data on a GS thread of its own.
- `gs_render_thread.*`: the GS threads' ordered queue of GS data and tasks.
- `hd_texture_pack.*` and `hd_texture_image.*`: HD texture packs, and the GPU images of loaded replacements.
- `gpu_window_presenter.*`: shows pictures in the window through a swapchain, behind `host/window_presenter.h`.
- `in_between_frames.*`: the in-between pictures of 60 fps battles, drawn by a second renderer from the game's
  recorded commands.

**`gs/gpu/native/`**: native passes inside paraLLEl-GS, which draw some effects better or faster than the game's own
commands ([hybrid rendering plan](../plans/graphics/HYBRID_RENDERING_PLAN.md)). `native_passes.*` and `native_pass.h`
hold the list of passes and their switches. Each pass has its own file: `ink_edges`, `glow`, `fighters`,
`dof_weight`, `underwater` and `alpha_fill`. Their GLSL shaders are in `shaders/`, with the compiled `*_spv.h`
headers made by `tools/graphics/gpu/build_native_shaders.py`.

**`gs/textures/`**: `gs_texture_names.*` names textures the way PCSX2 does, so HD packs made for PCSX2 work. It keeps
its own copy of GS memory and the colour table, fed by the command sink. `texture_colours.*` sums up a texture's
colours, to choose a pack's lit variant.

## services: the native IOP services

On the PS2, the IOP handles the disc, files, memory cards, controllers and sound. The port runs no IOP code: these
services answer the game's requests directly ([native services plan](../plans/iop/NATIVE_SERVICES_PLAN.md)).

- **The SIF layer:** `sif_rpc.*` (remote procedure calls between the game and the IOP) and `sif_system.*` (the SIF
  registers, DMA and system commands, with one behavior wherever they are reached).
- **One file per service:** `disc_service`, `fileio_service`, `memory_card_service`, `pad_service`,
  `iop_memory_service` (the IOP heap and uploads from the game), `module_service` (LOADFILE), `sound_service` (the
  sound driver and SOUNDS) and `cri_service` (CRI ADX streams).
- **Their pieces:** `iop_heap.h` (where things are placed in IOP memory), `module_loads.*` (LOADFILE's replies),
  `memory_card_info.*` (the memory card's GetInfo answer), `card_folder.*` (a memory card as a folder on the PC),
  `pad_block.h` (the controller data layout), `cri_stream.h` and `cri_audio.h` (the stream buffers and the flow of
  sound data).
- **Sound output:** `audio_mixer.h` mixes the streams, and `sound_sampler.h` plays the sound effects, with
  `sound_bank.h` (the game's sound banks) and `spu_adpcm.h` (their sample format).
- **`devices/`:** `disc_metadata.*` and `disc_sectors.*` (reading the ISO), `dualshock2.*` (a DualShock 2 on the
  controller bus), `pad_input.h` (what a controller holds at one field) and `pad_script.h` (scripted input for test
  runs).

## host: the PC side

**Pictures and sound.**

- `display_frame.h`: a picture in host colour order, and saving it as a PPM file.
- `viewer_window.*` and `window_presenter.h`: the game's window.
- `audio_device.*`: sound output through waveOut. `wav_writer.*`: saving the sound as a WAV file.

**Controllers.**

- `host_input.h` reads the keyboard, Xbox controllers (XInput) and HID gamepads (`hid_gamepads.h`).
- `playstation_report.h` decodes DualSense and DualShock 4 reports; `generic_gamepad.h` handles other HID gamepads.
- `gamepad.h` is the common gamepad state, and `input_mapping.h` maps it to the game's controller.
- `press_latch.*` keeps short taps until the game has seen them (in a fight it reads the controller every other
  field).

**Timing and speed.**

- `frame_pacer.h`: holds the game at the PS2's speed.
- `early_picture.h`: "Low input lag", which shows each picture as soon as it is drawn.
- `loading_skip.*`: "Fast loading", which runs silent loading waits at full speed.
- `power_throttling.h`, `thread_priority.h`: keep Windows from slowing the game's threads.
- `thread_time.h`: a thread's processor time, to tell its work from its waits.
- `sample_profiler.h`: the sampling profiler.

**The player's side.**

- `settings.h`: the settings file. `game_folders.*`: the `data` and `saves` folders next to the programs.
  `utf8.h`: text and paths as UTF-8, so folder names in any language work.
- Menus: `menu_model.h`, `menu_input.h` and `menu_renderer.h` (the menu's contents, input and drawing), and
  `menu_host.h` (the menu loop and the settings screens shared by the launcher and the game).
- `launcher.h` (the launcher, with `logo_image.*` for its logo), `overlay.h` (the in-game menu) and
  `player_session.h` (a player's run inside the runtime).
- `clipboard.*`: copying and pasting connection codes.

**Online battles.** `net_lockstep.*` (lockstep over UDP), `stun.*` (finding the PC's internet address without an open
port), `upnp_port.*` (opening the router's port by UPnP), `connection_code.h` (the codes players swap) and
`online_screens.h` (the screens while connecting).

## game: the player's programs

`game_main.cpp` builds the two programs the player starts, `bt2-game` and `bt2-launcher`: they read the settings, show
the launcher when needed, and then start the runtime program as a child process. They contain no game code.
`bt2_game.rc` and `bt2_launcher.rc` add the icons.

## probe: the runtime program and test tools

`entry_probe.cpp` is the runtime program, `bt2-entry-probe`, which the player's programs start. Its stages each have
their own file and share `probe_state.h`:

- `probe_inputs.cpp`: the disc image. (The console's starting state is `EeRuntime::bootState`.)
- `startup_bindings.cpp`: binds the fixed startup functions to their addresses. The functions the discovery loop
  selects are bound in `selected_bindings.cpp` and `overlay_bindings.cpp`, so a new selection recompiles only those
  small files.
- `checkpoint_checks.cpp`: the state each startup checkpoint must reach.
- `probe_reports.cpp`: the stop report, its evidence files and the console summary.

Features used by the player's game:

- `quick_start.*`: "Quick start", which reaches the main menu by the game's own skips (`BT2_QUICK_START`).
- `online_session.*`: sets up an online battle (`BT2_NET`) and plays both controller ports from it.

Tools for test runs only:

- `game_starter.cpp`: starting straight into a battle, a screen or a story episode (`BT2_START_BATTLE`,
  `BT2_START_SCREEN`, `BT2_START_EPISODE`).
- PCSX2 comparison: `call_replay.*` (runs one game function from a PCSX2 state, `BT2_REPLAY_CALL`), `call_ram.*`,
  `call_set.*` and `call_sums.*` (memory, arguments and checksums at chosen calls) and `rpc_log.*` (the service log).
- Older probes: `generated_probe.cpp` (the first test of generated code) and `rpc_bind_probe.cpp` (an isolated test
  of the first RPC bind).

The [tools guide](TOOLS.md) explains each `BT2_` setting.

## tests

Unit and subsystem tests, one file per subject (`*_tests.cpp`), registered with CTest. The gate runs them all.

## Rules for the code

**Includes.** Hand-written code includes a header from another folder by its path from `runtime/`, for example
`#include "gs/cpu/gs_command_sink.h"`, and a header from the same folder by its name. Generated code includes
`ps2_runtime.h` by name, through the `runtime/ee` include directory; hand-written code includes `ee/ee_runtime.h`.

**Keep the generated code's headers small.** `ps2_runtime.h`, `ps2_runtime_macros.h` and `ps2_fpu.h` hold only the CPU
context and the entry points generated code calls, because any edit to them recompiles every generated file. Runtime
state belongs in `ee_runtime.h`, and code in the folder's `.cpp` files. To give generated code a new entry point,
declare it in `PS2Runtime`, implement it in `EeRuntime` and forward it in `guest_interface.cpp`.

**Build targets.** The hand-written runtime is built once as the `bt2-ee-runtime` library, which every program and
test links. Its busiest sources (`BT2_EE_HOT_SOURCES` in `CMakeLists.txt`) and the generated code
(`bt2-entry-generated`) are compiled with optimization in Debug builds too.

**Where new code goes.** A new subsystem or service gets its own file in the matching folder, using the table at the
top of this page. Then add the file to the right list in `CMakeLists.txt`:

- `BT2_EE_RUNTIME_SOURCES` for the EE runtime and the native services;
- `BT2_EE_HOT_SOURCES` for interpreters and other busy loops (FPU, GS, IPU, VIF1 and VU1).
