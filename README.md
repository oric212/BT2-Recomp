<p align="center">
  <img src="assets/BT2%20Recomp%20Banner.png" alt="BT2 Recomp banner" width="960">
</p>

<h1 align="center">BT2 Recomp</h1>
<p align="center">Dragon Ball Z: Budokai Tenkaichi 2 · Native Windows x64 recompilation</p>
<p align="center"><strong>SLUS-21441</strong> &nbsp;·&nbsp; <strong>C++20</strong> &nbsp;·&nbsp; <strong>MSVC / CMake</strong> &nbsp;·&nbsp; <strong>Early development</strong></p>

<p align="center">
  <a href="#progress">Progress</a> &nbsp;·&nbsp;
  <a href="#building">Build</a> &nbsp;·&nbsp;
  <a href="#documentation">Documentation</a> &nbsp;·&nbsp;
  <a href="docs/progress/ACTIVE_PASS.md">Active work</a> &nbsp;·&nbsp;
  <a href="#credits--license">Credits &amp; license</a>
</p>

BT2 Recomp aims to run the original PS2 game logic natively on Windows 10/11 using PS2Recomp and a custom compatibility runtime. The initial target is the North American release, **SLUS-21441**; the first major gameplay goal is a **playable versus battle**.

> **Game files are not included.** You must provide the game files from your own legally obtained copy to build and test the project. This repository contains only project code, tools and documentation; it does not distribute game disc images, executables, assets or generated game code.

## Progress
<!-- progress-summary:start -->
![BT2 component status map](docs/progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Native host, retail validation and recompilation pipeline | ✅ Verified |
| Generated BT2 startup execution | 🔧 Partial — reaches a checked runtime boundary |
| Initial disc read and metadata lookups | ✅ Verified |
| First archive (AFS) table load | ✅ Verified — byte-identical to original |
| Complete startup and thread scheduling | 🔧 In progress — startup reaches main's frame loop; VBlank, display setup, Timer 2, streamed reads, sound bank verified |
| Game asset loading and first presented frame | 🔧 In progress — first screens rendered and shown in a live window; an emulated DualShock 2 takes the game past the controller check; the memory card check runs through the original modules (106 frames) |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Verified blocker (commit `6adf795`, verified 2026-10-08):** the frame-phase memory card query runs through the original IOP modules with absent cards, matching an original capture. The game shows its memory card check screen and draws textured triangle strips, then stops at the next unselected function (`FUN_0026F888`).

**Last measured, 2026-10-08:** **772 EE entries entered / 763 completed**, **106 rendered frames**, **42 CRI calls**, **59 original IOP disc reads**, **229 logical VBlanks**, **2 memory card queries**. Complete startup is not yet verified.

Helper 5/6 execution is now part of normal startup; the former separate helper diagnostic was retired.

<!-- progress-summary:end -->

[Detailed measurements](docs/PROG.md) · [Unfinished-pass handoff](docs/progress/ACTIVE_PASS.md) · [Progress history](docs/progress/2026-10.md)

## Building

**Windows 10/11 x64 · Visual Studio 2022 with C++ tools · CMake**

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\bt2-port.exe
```

The foundation host prints its startup message. Before recompiling or testing BT2, prepare your own North American SLUS-21441 disc image, extract its original executable and boot configuration, and validate them using the [game-file preparation steps](docs/BUILDING.md#3-prepare-your-own-game-files-before-recompilation). Testing generated startup uses a separate developer probe with locally prepared inputs and your ISO. See [building and startup tests](docs/BUILDING.md) for setup and commands. The game is not yet playable.

## Architecture

BT2 Recomp translates the game's original code to C++ ahead of time and runs it natively on Windows. It does not interpret or emulate PS2 code at run time. What it adds is a small handwritten compatibility runtime.

```text
 your disc image ──► tools/ (validate, analyse, map functions)
                         │
                         ▼
             PS2Recomp + local IOP translator ──► generated C++ (ignored, never committed)
                         │
                         ▼
native executable (MSVC, x64)
  ├─ generated EE game code ──► EE runtime ............ runtime/ee
  │                              ├─ GS / GIF / VIF1 ... runtime/gs
  │                              └─ SIF RPC bridge .... runtime/iop/ee_bridge
  │                                       │
  ├─ generated original IOP modules ◄── IOP runtime ... runtime/iop
  │   (SIO2MAN, MCMAN, LIBSD, CDVD, CRI, ...)   kernel providers, devices, services
  └─ host presentation (window, framebuffer) ......... runtime/host
```

- **EE (main CPU):** each recompiled game function calls into `PS2Runtime`, which provides guest memory, syscalls, the kernel, threads, timers, DMA and memory-mapped I/O. Code from overlays loaded at run time is generated separately. It runs only while RAM holds the exact bytes it was generated from.
- **IOP (I/O processor):** the game's own IOP modules are translated and run, not replaced. They cover controllers, memory cards, sound, disc and CRI streaming. `IopRuntime` supplies:
  - the IOP kernel and library services they import;
  - device models: SPU2, SIO2 with an emulated DualShock 2, and the CDVD drive reading your local ISO.

  EE requests reach these modules over a SIF RPC bridge.
- **Graphics:** GIF and VIF1 transfers feed a GS model that keeps GS memory and draws sprites and triangles. The displayed framebuffer is decoded and presented in a window.
- **Time and checking:**
  - **Logical time:** time is logical, one NTSC field per VBlank, so runs are deterministic.
  - **Unknown hardware access:** hardware use that is not yet supported stops the run at a checked boundary instead of guessing.
  - **Verification:** behavior is verified against captures from the original game running in an emulator.

Source layout and conventions: [runtime layout](docs/RUNTIME_LAYOUT.md). Design references: [reference architecture](docs/REFERENCE_ARCHITECTURE.md) and [implementation plan](docs/IMPLEMENTATION_PLAN.md).

## Repository layout

| Directory | Purpose |
| --- | --- |
| `runtime/` | Native host, compatibility services and execution probes ([layout](docs/RUNTIME_LAYOUT.md)) |
| `game/` | Handwritten BT2 integration; generated game code is ignored |
| `config/` | Recompiler configuration, layout and pinned tool versions |
| `tools/` | Validation, generation, capture and progress tooling |
| `docs/` | Plans, evidence, build instructions and progress history |
| `external/` | Dependency integration notes |

Dependency checkouts, generated translations and analysis output live in ignored local directories. The workflow follows actual execution blockers; unknown functions retain neutral names.

## Documentation

| Start here | Details |
| --- | --- |
| [Build and test](docs/BUILDING.md) | Host build, local prerequisites and startup probe |
| [Current progress](docs/PROG.md) | Verified measurements, blocker and milestones |
| [Active pass](docs/progress/ACTIVE_PASS.md) | Pending changes, missing checks and resume steps |
| [Implementation plan](docs/IMPLEMENTATION_PLAN.md) | Goals and subsystem-scale working rules |
| [Reference architecture](docs/REFERENCE_ARCHITECTURE.md) | Reference projects and adaptation guidance |
| [Tools and evidence](docs/TOOLS.md) | Local tooling; [reverse-engineering notes](docs/REVERSE_ENGINEERING.md) |
| [History](docs/progress/2026-10.md) | Detailed pass records; [tracking rules](docs/PROGRESS_TRACKING.md) |
| [Content and licenses](docs/LEGAL.md) | Local game-file policy and dependency licenses |
## AI tools and manual testing

Development of this project involves AI tools for coding, investigation and documentation. AI-assisted changes will be reviewed and tested manually, alongside automated tests and comparisons against the original game's behavior. Unverified work is labeled in the progress records; use of AI does not establish correctness or mean that all current changes have passed manual testing.

## Credits & license

### Project

- [oric212](https://github.com/oric212) — project owner and maintainer.

### Tools and references

| Project | Contribution |
| --- | --- |
| [PS2Recomp](https://github.com/ran-j/PS2Recomp) by ran-j and contributors | Static recompilation toolchain; GPL-3.0 |
| [BT3-Recomp](https://github.com/z3xox/BT3-Recomp) by z3xox and contributors | Primary architectural and tooling reference |
| [Chronicle](https://github.com/TheMoonPeople/Chronicle) by TheMoonPeople and contributors | Reverse-engineering and progress-reporting reference |
| [PCSX2](https://github.com/PCSX2/pcsx2) and [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | Behavioral comparison and reverse-engineering tools |

Reference projects inform architecture and investigation; BT2-specific behavior is validated against local evidence. Dependency versions and licenses are recorded in [the content and license documentation](docs/LEGAL.md).

### License and game ownership

This repository is **GPL-3.0** (see [LICENSE](LICENSE)). *Dragon Ball Z: Budokai Tenkaichi 2* © Spike / Bandai Namco. This project is not affiliated with or endorsed by them; it exists for preservation and interoperability and distributes no game content. Game code is recompiled locally during the generation/build workflow from files supplied from the user's own disc image; proprietary-derived generated code is not distributed in this repository.