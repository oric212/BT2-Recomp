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
| Game asset loading and first presented frame | 🔧 In progress — first screen rendered and shown in a live window; an emulated DualShock 2 takes the game past the controller check; memory card check next |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** with the emulated controller connected, the game leaves the controller-missing screen and starts a memory card check. Its frame-phase card query is only admitted in the captured startup profile, so it stops as a checked boundary (frame 27).

**Last measured, 2026-10-08:** **682 EE entries entered / 672 completed**, **27 rendered frames**, **42 CRI calls**, **59 original IOP disc reads**, **150 logical VBlanks**, **22 Timer 2 overflows**. Complete startup is not yet verified.

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

## Repository layout

| Directory | Purpose |
| --- | --- |
| `runtime/` | Native host, compatibility services and execution probes |
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