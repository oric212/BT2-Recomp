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
  <a href="docs/progress/ACTIVE_PASS.md">Active work</a>
</p>

BT2 Recomp aims to run the original PS2 game logic natively on Windows 10/11 using PS2Recomp and a custom compatibility runtime. The initial target is the North American release, **SLUS-21441**; the first major gameplay goal is a **playable versus battle**.

> **Game files are not included.** Supply your own local copy under ignored `rom/` or `game_data/`. Proprietary assets, captures and generated game code stay outside Git.

## Progress
<!-- progress-summary:start -->
![BT2 component status map](docs/progress/dashboard.svg)

| Milestone | Status |
| --- | --- |
| Native host, retail validation and recompilation pipeline | ✅ Verified |
| Generated BT2 startup execution | 🔧 Partial — reaches a checked runtime boundary |
| Initial disc read and metadata lookups | ✅ Verified |
| Complete startup and thread scheduling | 🔧 In progress — latest worker changes unverified |
| Game asset loading and first presented frame | ⏳ Pending |
| Title screen, menu and playable versus battle | ⏳ Pending |
| Native audio playback and full match | ⏳ Pending |

**Current blocker:** worker `ReferThreadStatus` at EE PC `0x32EB68`.

**Last measured, 2026-10-06:** **291 EE entries entered / 286 completed**, **37 CRI calls**, **six IOP heap allocations**. Complete startup is not yet verified.
<!-- progress-summary:end -->

[Detailed measurements](docs/PROG.md) · [Unfinished-pass handoff](docs/progress/ACTIVE_PASS.md) · [Progress history](docs/progress/2026-10.md)

## Building

**Windows 10/11 x64 · Visual Studio 2022 with C++ tools · CMake**

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\bt2-port.exe
```

The foundation host prints its startup message. Testing generated BT2 startup uses a separate probe with locally prepared inputs and your ISO. See [building and startup tests](docs/BUILDING.md) for setup and commands. The game is not yet playable.

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