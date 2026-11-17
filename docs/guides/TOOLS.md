# Developer tools

This page explains the tools used to build, run, test and measure the port. It is for people working on the project.
To build the programs for the first time, start with [building from source](BUILDING.md); to play, you need neither
page.

**How to read it.** Run every command from the root of your checkout. Commands are written for PowerShell. Settings
that start with `BT2_` are environment variables for a test run of the game (see
[Running the game for tests](#running-the-game-for-tests)); set them before the run, for example
`$env:BT2_SUPER_SAMPLING = '4'`. Folders such as `rom/`, `work/` and `game/generated/` are ignored by Git: they
hold your game files and everything made from them, and they are empty in a fresh clone.

**Game content stays local.** Many tools write files made from the game: dumps, captures, recordings, generated code.
Keep them in the ignored folders and never commit them ([content rules](../project/LEGAL.md)).

## Contents

1. [Checking your game files](#checking-your-game-files)
2. [Building and the gate](#building-and-the-gate)
3. [Turning the game's code into C++](#turning-the-games-code-into-c)
4. [Running the game for tests](#running-the-game-for-tests)
5. [Automated sweeps](#automated-sweeps)
6. [Pictures and graphics](#pictures-and-graphics)
7. [60 fps battles](#60-fps-battles)
8. [VU1 programs](#vu1-programs)
9. [Sound](#sound)
10. [Speed and input lag](#speed-and-input-lag)
11. [Finding a bug in the game's logic](#finding-a-bug-in-the-games-logic)
12. [Comparing with PCSX2](#comparing-with-pcsx2)
13. [Online battles](#online-battles)
14. [The player's programs, saves and the release](#the-players-programs-saves-and-the-release)
15. [The startup capture record](#the-startup-capture-record)

## Checking your game files

These tools only read your files; they never change them. They need Python 3.10 or newer and nothing else.

**The executable.** This checks the file name and that `SLUS_214.41` is a little-endian MIPS ELF executable. If a
`SYSTEM.CNF` lies next to it (or you pass `--system-cnf PATH`), it must name `SLUS_214.41` as the file to boot. The
result is printed as JSON, with the file's size and SHA-256.

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41
```

Passing these checks does not prove the file is the retail version. To check that, give the hash you trust with
`--expected-sha256 HASH`, or check the disc image as below. The tool exits with an error code when a check fails.

**The disc image.** This compares your ISO's size, MD5 and SHA-1 with the official Redump values stored in
`config/retail_revision.json`, and checks that the executable inside the ISO is the same as yours. The report goes to
`work/disc-validation/`.

```powershell
python tools/verify_game/verify_disc.py rom/BT2-USA.iso --executable rom/SLUS_214.41
```

**The executable's layout.** `inspect_elf.py` prints the ELF header, the program headers (what is loaded where) and
the section headers as JSON, with addresses as decimal numbers. It checks that every table and its contents fit in
the file; unusual alignments are reported as warnings. It is an inspector only: it does not load or disassemble
anything. Its tests use made-up files, so they need no game data.

```powershell
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41 > work/elf_report.json
python -m unittest discover -s tools/inspect_elf -p "test_*.py"
```

The field layouts follow the ELF specification: [header](https://gabi.xinuos.com/elf/02-eheader.html),
[program headers](https://gabi.xinuos.com/elf/07-pheader.html) and
[section headers](https://gabi.xinuos.com/elf/03-sheader.html).

## Building and the gate

**Building.** `tools/build/msvc.py` runs any command inside the Visual Studio x64 compiler environment, with Visual
Studio's own Ninja on the path, so you don't need a Developer PowerShell. It picks the Visual Studio installation that
has the C++ tools; set `BT2_VS_ROOT` to choose another one.

```powershell
python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe
```

**The gate** must pass before every commit. It builds Debug and Release, then runs the unit tests (CTest), the Python
test suites, the Debug test runs of the game with their comparisons with the original, and the Release battle run (a
scripted fight whose result must equal its recorded one, `tools/gate/battle_run.py`). The steps run side by side. It
exits with an error code when a step fails; `--skip-build` uses the current build. A few comparisons need their own
captured input or a stop at an earlier point; they are listed in `NOT_IN_GATE` in the script and are run by hand.

```powershell
python tools/gate/run_gate.py --disc rom/BT2-USA.iso
```

**Open-source parts.** These scripts download pinned versions into the ignored `work/dependencies/` folder and need
internet access the first time:

- `tools/ps2recomp/build_tools.py` builds the recompiler, PS2Recomp, at the revision locked in
  `config/ps2recomp.lock.json`. The tools are built in `build/ps2recomp-tools/`, one step at a time; PS2Recomp's own
  runtime, studio and test targets are left out. `smoke_test.py` then checks the recompiler on a tiny ELF of our own
  (one function that returns 42) and looks at the C++ it writes.
- `tools/graphics/gpu/fetch_parallel_gs.py` fetches the GPU renderer, paraLLEl-GS, at the revision in
  `config/parallel_gs.lock.json`, with the Granite submodules it needs, and applies the project's patches to it.
- `tools/graphics/textures/fetch_xxhash.py` fetches xxHash (`config/xxhash.lock.json`), which names textures the way
  PCSX2 does for HD packs.

```powershell
python tools/ps2recomp/build_tools.py
python tools/ps2recomp/smoke_test.py
python -m unittest discover -s tools/ps2recomp -p "test_*.py"
```

**The GPU renderer** is switched on by configuring with `-DBT2_GPU_GS=ON`. That builds `bt2-gpu-gs-tests` too and
links the renderer into the game, which then draws every picture. A build without it runs without pictures. If
`BT2_GPU_GS` is set for a run, it must be `on` ([GPU rendering plan](../plans/graphics/GPU_RENDERING_PLAN.md)).

**The native passes' shaders.** `python tools/graphics/gpu/build_native_shaders.py` compiles the GLSL shaders of the
native passes into the headers the runtime includes. It needs the Vulkan SDK's `glslc`. `--check` only compares, as
the graphics Python suite does. A change to one of paraLLEl-GS's own shaders also needs its SPIR-V updated in
`slangmosh.hpp`: compile it with `glslc --target-env=vulkan1.1` and the include path from `slangmosh.json`.

## Turning the game's code into C++

The game's code goes through four stages: Ghidra finds the functions, `prepare_map.py` checks them and makes the
function map, PS2Recomp turns each function into C++, and `prepare_runtime.py` adjusts that C++ for the runtime. The
[build guide](BUILDING.md) gives the order for a first build. Everything these tools make stays in ignored folders.

### Analysing the game with Ghidra

You need Ghidra and a JDK installed wherever you like; nothing of Ghidra is in this repository. The tested setup is
Ghidra 12.1.4 with JDK 25, and the R5900 processor extension v2.1.37 built from pinned source for that Ghidra, with
Gradle 9.8.0 next to Ghidra. The versions and archive hashes are in `config/ghidra.lock.json`.

```powershell
python tools/ghidra/analyze_game.py --ghidra-home "PATH_TO_GHIDRA" --java-home "PATH_TO_JDK"
```

This imports the executable as `r5900:LE:32:default`, runs our exporter, and writes the project, logs, listings and the
list of candidate functions to `work/ghidra/`. An existing project is opened again without a new analysis; delete it
when you want a fresh one. Functions get neutral names (their address), and the list is only a starting point.

Other options:

- `--discover-prologues` makes, in one pass, the functions the automatic analysis missed: code right after a finished
  function that lies outside every function.
- `--discover-blocker` looks at the address where the last run stopped for lack of a function, makes a function there
  and exports again. It refuses addresses inside an existing function and kernel addresses. It also separates a
  function that is reached by a tail jump (`J`) from the function Ghidra merged it into, when the target is called
  elsewhere, opens a stack frame in its first two instructions, or directly follows a finished `JR RA` (also after up
  to three alignment NOPs).
- `--program dbzp` works on the `DBZP.BIN` overlay instead of the main executable (see
  [the overlay](#the-overlay-dbzpbin)).

### The function map

`prepare_map.py` turns Ghidra's candidates into the function map in `work/bt2-generation/` (a CSV with the columns
`Name,Start,End,Size`, the end exclusive). It keeps only candidates it can prove: whole, aligned, inside the code
section and not overlapping another. Everything else is set aside with the reason. VU code is never treated as EE code.

The proofs it can make, beyond plain functions:

- **Switch tables.** A jump through a table of addresses is accepted when the index is checked against a bound, the
  table lies in the file, and every entry lands inside the function. Several compiler layouts are recognized: the
  bound right before the jump or further away (the table is then found by scanning, at least four entries), a `BEQ`
  or `BEQL` guard, the index moved to another register, reloaded from the stack in the guard's delay slot, or kept in
  a register nothing changes on the way, FPU instructions between the bound and the guard, and tables of any length.
  The table's holes are still checked.
- **Case blocks Ghidra did not follow.** A candidate with holes is accepted when each hole holds only a switch target,
  NOP padding and straight case code that ends in `JR RA` or a jump back into the function, and nothing else branches
  into it. Case blocks may contain FPU condition branches.
- **Padding.** NOPs after a finished `JR RA` or a proven switch jump count as padding. Coprocessor arithmetic counts
  as ordinary data flow there; `BC0`, `BC1` and `BC2` branches are checked like other branches; COP0 operations are
  refused. In the overlay, jumps into the main executable's code are allowed.
- **Entries.** Addresses listed in a program's `entries.json` become functions: in unmapped code from the entry up to
  its first finished return, or by splitting a mapped function where the code before is complete and no branch
  crosses (`add_entries`). A candidate that Ghidra merged from two separate pieces of code is split
  (`split_merged`).

`map_gaps.py` finds what the map is still missing. For the main executable and the overlay, it lists addresses that are
not functions although mapped code calls or jumps to them, data points at them, or code builds them as a pointer
(`lui` with `addiu`/`ori`), and code between functions that follows finished code although nothing names it (small
routines reached through tables built at run time). Kernel-mode code (exception handlers ending in `ERET`) is reported
but never mapped. After generation it also lists addresses inside a function, other than its start, that code builds
or data holds but that have no label in the generated code.

- `--write` adds what it found to `entries.json`, or for addresses inside functions to `entry-hints.json`, which
  `generate_game.py` passes to the recompiler so they become labels in their function.
- `--add <hex>...` adds given addresses.
- `--check` exits with an error code when anything is left; the gate runs it. Since 2026-10-13 nothing is left in
  either program; whatever remains can only be reached through pointers computed at run time.

### Generating and preparing the C++

```powershell
python tools/ps2recomp/prepare_map.py
python tools/ps2recomp/generate_game.py --candidate-map work/bt2-generation/candidates.csv
python tools/ps2recomp/run_pipeline.py
```

- `generate_game.py` runs PS2Recomp on the map and writes the C++ to `game/generated/bt2/`. It first checks the
  executable, the map and the locked tool versions, and saves the previous output aside so no stale file is mixed in.
  The recompiler's own instruction patches are off, and nothing is stubbed or skipped by hand. `--function-map PATH`
  uses a map you reviewed yourself.
- `run_pipeline.py` does the whole chain: map, generation, hashes of everything written, preparation, the build and
  the tests. `--reuse-generated` skips generation when the executable, map, tool lock and output still match their
  hashes. Logs go to `work/bt2-generation/`.
- `prepare_runtime.py` writes adjusted copies of the generated files for the build; the generated files themselves are
  left as they are, and unchanged copies are not touched, so builds stay incremental. The check of the generated
  files caches each file's digest by size and modification time
  (`work/bt2-generation/generated-digests.cache.json`). It also drops the generator's
  declaration headers, so a map change recompiles only the files that changed. Run it after any change to the
  selection below.
- `expand_startup.py --all` registers every mapped function with the runtime, overlay functions included (457 for
  `dbzp`, each checked against the bytes it was generated from). Every label inside a function (where code resumes
  after a call, targets found from elsewhere, entry hints) is registered with the function that holds it: a jump to
  such an address runs that function, which continues at the label. `BT2_NO_INTERIOR_ENTRIES=1` turns this off, for
  comparison. Older options select single functions: `--report <stop report>` the one a run stopped at,
  `--callees N` up to N functions it calls directly, `--neighbours BYTES` those nearby.
- `generate_interpreter.py` makes the instruction handlers of the [interpreter bridge](#the-interpreter-bridge).
- `inspect_blocker.py` says, after a run stopped at code without a function, where that address lies in the map and in
  Ghidra's candidates (`work/bt2-generation/blocker-map.json`). The pipeline runs it by itself.
- `runtime_census.py` lists what the game's code can ask of the runtime: every system call number (with how often the
  code refers to it, so unused library stubs stand apart), every hardware register it reads, writes or points at (by
  block), and whether the VU1 programs use only instructions our VU1 model has. It then checks the runtime's sources
  for each one: handled plainly, handled only under a condition on the game's program counter, a call count, a stack
  offset or a startup phase, or not handled. System calls the game installs itself are taken from a run's report
  (`--report`, by default the gate's battle run). Details go to `work/bt2-generation/runtime-census.json`
  ([whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), step 4).

**What `prepare_runtime.py` corrects in the generated code.** Each correction is counted per file in
`runtime-preparation.json`.

- **Sign tests of branches.** `BLTZ`, `BGEZ`, `BLEZ` and `BGTZ` test the full 64-bit register, as the R5900 does; the
  generator tested 32 bits.
- **`SQRT.S`** takes the root of `ft`, as on the R5900; the generator used `fs` (150 sites, `sqrt_operand_fixes`).
- **`VF0` stays (0, 0, 0, 1).** An instruction that writes `VF0` keeps only its flag effects; the game's clip tests
  subtract into it (34 sites, `vf0_writes_dropped`).
- **`VCLIP`.** For x, y and z, the low bit means above `+|w|` and the high bit below `-|w|`; the generator swapped them
  and compared with `w` (one site, `0x145A80`, used in the demo fight; `clip_fixes`).
- **VU0 data memory** instructions get the runtime's version, and flag-setting VU0 instructions between a status clear
  and its read are reported to the runtime. The `BC2T`/`BC2F` condition goes to `PS2Runtime::cop2Condition`.
- **Calls and jumps.** A function calling itself goes through the dispatcher. A `JR` the generator did not resolve (a
  switch it did not recognize) continues at the function's own label when it is one; otherwise the dispatcher refuses
  it (`local_indirect_jumps`). A jump into another program's function is made a tail jump.
- **Patched instructions.** Instructions that an optional [game patch](#image-settings-for-test-runs) changes are read from memory
  instead of being compiled in.

### The overlay (DBZP.BIN)

The game loads a second program, `DBZP.BIN`, at run time.

- `python tools/ps2recomp/overlay_programs.py --extract <ISO>` takes it from the disc, checks it against its pinned
  identity and wraps it as an ELF in an ignored folder.
- `analyze_game.py`, `prepare_map.py` and `generate_game.py` take `--program dbzp` and keep a separate Ghidra
  project, map and output for it.
- `inspect_blocker.py` and `expand_startup.py` send addresses in the overlay's range to its own map
  (`work/bt2-generation/overlays.json`, `overlays.cmake`, `bt2_overlays.h`).

### The discovery loop

`discovery_loop.py` maps, generates and builds until nothing is missing. Its log is
`work/bt2-generation/discovery-loop.log`, with times; follow it with `Get-Content "<absolute path>" -Wait -Tail 30`.

- `--map-all` maps everything `map_gaps.py` finds, regenerates every program and builds, without a run.
- `--player-stop` starts from where the last session played with `bt2-game.exe` stopped (from its report, or from its
  log when a closed window left no report). That address becomes an entry, so everything it leads to comes in too.
  Ghidra is asked only when the map builder cannot take it.
- `--disc <iso> --frames <n> [--pad-script-file <file>] [--rounds <n>]` runs the Release game and repeats at every stop
  at an unmapped function.

A generated file that was already in use may change only where a tail jump became a direct call, where the map split
its function, or where its labels changed. Any other change stops the loop. Only the newest `previous-output-*`
folder of each program is kept.

## Running the game for tests

The runtime program is `bt2-entry-probe.exe`, "the probe". It is the game itself, with extra options for tests. Its
command line is `bt2-entry-probe STOP_REPORT DISC_IMAGE`: it builds the game's starting memory from the disc
(`ee/guest_image.*`) and sets the console's starting state itself (`EeRuntime::bootState`). At the end it writes a
report (JSON) with how far it got and why it stopped.

`run_startup_probe.py` builds that command for you and is the usual way to run it.

```powershell
python tools/ps2recomp/run_startup_probe.py --disc rom/BT2-USA.iso --config Release --frames 3000 --pad-script "<script>"
```

| Option | What it does |
| --- | --- |
| `--config Release` | Runs the optimized build, about 7 times faster than Debug, with the same reports. Use it for long runs; the gate uses Debug. |
| `--frames N` | How many display flips to run (see the frame limit below). |
| `--pad-script <script>` | Buttons to press (see below). The default answers the no-memory-card question with Yes; an empty script presses nothing. |
| `--report PATH` | Where the report and its dumps go, so several runs can run at once. |
| `--viewer [--scale integer\|stretch]` | Shows every picture in a window (`BT2_VIEWER`, `BT2_VIEWER_SCALE`); the last picture stays after the run ends. |
| `--sound` | Plays the sound on the default audio device (`BT2_AUDIO_DEVICE`). |
| `--play` | Opens the window with sound and takes your input (see below). |

**Pad scripts** press buttons of a DualShock 2 at given fields (a field is a 60th of a second): steps of
`FIELD:buttons`, for example `900:square`. `-` releases everything. Sticks are written `lx=HH`, `ly=HH`, `rx=HH`,
`ry=HH` in hex, with `00` for left or up and `80` for rest; a stick a step does not name is at rest
(`900:square+lx=00`). Pad scripts count fields, but pictures count flips; the two drift apart while the game loads
(about 695 fields by the title screen with the films skipped). Each picture saved prints
`Snapshot: flip N at field M`, which is how a button press is timed against a picture.

**Playing in a test run.** `--play` takes input from the keyboard, the first Xbox controller or a HID gamepad
(DualSense, DualShock 4, generic pads, by USB or Bluetooth) while the window has the focus (`BT2_HOST_INPUT`; default
keys in `runtime/host/input_mapping.h`). Pad scripts are ignored. `--record <file>` (`BT2_PAD_RECORD`) writes every
change of buttons and sticks as a pad script, so the session can be played again with `--pad-script` or
`discovery_loop.py --pad-script-file <file>`. A gamepad connected through XInput gives the keyboard to port 2.

### Limits that stop a run

A run stops with a report, instead of hanging, when one of these is reached:

- **Frame limit.** 120 flips by default; `BT2_FRAME_LIMIT=<n>` changes it, `0` removes it.
- **No flip for too long.** `BT2_STALL_FIELDS=<n>` (default 3,600) fields without a flip. This counts only after the
  first picture or once the main thread was busy past a field, so the boot does not count. The report names the
  interrupted `pc`, `ra`, thread and last call, which usually point at the loop the game waits in, and
  `stop_returns` gives the call chain. A busy main thread takes about a quarter of a second per field, so a small
  value such as 120 turns a hang into a report in seconds.
- **A runaway loop.** `BT2_EE_LOOP_LIMIT=<n>` loop iterations by one thread in one field (default 400 million once
  pictures are shown; loading a battle needs tens of millions at once). Each Timer 1 wait is limited separately to
  100,000 polls.
- **A chosen address.** `BT2_PHASE_STOP=<hex address>` stops at the first entry to, or return to, that address, with
  a normal report. Copy the report before the next run. It changes nothing in the game.

### Starting straight into a battle, a screen or the story

These start points are for test runs only; a player's session refuses them. They need no pad script
(`--pad-script ""`). The code is in `runtime/probe/game_starter.cpp`
([whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), step 6).

**A battle: `BT2_START_BATTLE=<fighter>[:<variant>],<fighter>[:<variant>],<stage>[,<music>]`** (decimal numbers)
starts a CPU against CPU battle at once. It skips the card check, the logos and the films, loads what the game loads on
its first start, and runs the title's New Game so the save has a new game's Z-items. With `BT2_CARD_FOLDER` holding a
BT2 save, it copies that save in instead (the report says `starter_save_loaded`). It then sets the battle up through
the game's own functions, the ones its demo fight uses. The battle runs to its K.O. and the game waits in the menu after
it: about 4,000 flips for one pairing.

- Fighters are 0 to 129 (from 130 the game reads past its table, so they are refused), variants 0 to 8.
- Stages are the game's numbers: 0 is the Wasteland, 1 the Rocky Area, 5 an ocean stage.
- The music number (default 9) picks file 15903 plus that number.
- `BT2_START_BATTLE_PLAYER=1` gives side 0 to controller port 1; `2` also gives side 1 to port 2.
- `BT2_START_BATTLE_CPU_LEVEL` sets side 1's CPU level (default 2, the demo's).
- `BT2_START_BATTLE_STRUGGLE_CPU=1` hands side 1 to the CPU when a beam struggle starts, for a struggle against the
  CPU's push.

**A main menu screen: `BT2_START_SCREEN=<mode>`** (1 to 64) sets the mode the main menu would set and starts the game,
which shows that screen and waits for input. The main menu's entries lead to these modes: 6 or 7 Dragon Adventure,
10 Ultimate Battle Z, 24 Dragon Tournament, 30 Dueling, 61 Ultimate Training, 53 Evolution Z, 5 Item Shop, 38 Data
Center, 48 Options and 43 Dragon Library. In battles the screen leads to, the CPU plays both sides, side 0 with at
least the opponent's level and strength. The report's `starter_mode_battles` lists each battle's kind, rule and
result. `BT2_START_SCREEN_PLAYERS=1` keeps the sides the menus choose instead.

**A story episode: `BT2_START_EPISODE=<episode>[,<part>[,marker:<m>]]`** (episodes 0 to 24) sets the story progress
and starts story mode: the episode's map loads and waits for input. With a marker, the player is placed at that map
marker after 120 map frames, and the game goes on to the scene the marker leads to. Then the run plays the part by
itself:

- The CPU plays both sides of the story's battles.
- In chase parts, the chased enemy counts as caught after 120 map frames.
- In towns, it goes to each character in turn until that character's block has run.
- Wherever the game waits for a button (messages, the screen before a battle, results, yes/no questions, the "GET!!"
  banner, the item shop), one call in every 30 sees Cross pressed. Before a battle it presses Down while team slots
  are empty; at "Exit saving?" it presses Left first, so nothing is saved. In the shop it presses Cross on the
  messages and once in the menu, then Triangle until the shop closes.
- The run ends 300 flips after the part's after-battle step. A screen that needs too many presses ends it as stuck.

The report says how far it got: `starter_scenes`, `starter_town_blocks`, `starter_town_done`, `starter_shop_state`,
`starter_chase_caught`, `starter_parts_completed`, `starter_battles_cleared`, `starter_battles_lost`, `starter_stuck`
and `starter_confirmations`. `starter_battles` lists each battle: its rule, flags, the Dragon Ball placed and picked
up, the result, the reward items, whether it was cleared and the saga items (390 to 414) it unlocked
([story mode plan](../plans/ee/STORY_MODE_PLAN.md)).

### The interpreter bridge

Code that no generated function or label holds is run by an interpreter instead of stopping the run
([whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), step 3; `runtime/ee/ee_interpreter.cpp`). This is
a bridge only: the next build compiles that code natively, and the gate's battle run fails when anything was
interpreted.

- Branches and jumps follow the same rules as the generated code: the condition and target are read before the delay
  slot, a jump inside the function stays there, calls to other functions go through the dispatcher, and `jr ra` or
  `ERET` ends the frame.
- Every other instruction runs a handler made by `generate_interpreter.py` from that exact instruction word, with the
  same recompiler and preparation, so an instruction means the same interpreted as compiled. There is one handler per
  distinct non-branch word in the mapped code (82,667, in 41 files).
- A word without a handler, a branch in a delay slot or an unknown branch form stops the run and names it.
- Each interpreted entry is printed (`EE interpreter: unmapped code at 0x... runs interpreted`) and listed in the
  report (`ee_interpreted_entries`, `ee_interpreted_instructions`). `discovery_loop.py` maps those entries.
- `BT2_EE_INTERPRET=off` stops at such code instead. `BT2_EE_INTERPRET=all` interprets every function, to compare a
  whole run with the compiled one through the state hash.

### Checking that a change alters nothing

**State hash.** `BT2_STATE_HASH=<file>` writes one line per vertical blank (`<field> <flips> <hash>`): a hash of EE
memory, the scratchpad, VU0's data memory and VU1's data and program memory, taken per 4 KB page.
`BT2_STATE_HASH_PAGES=<field>` also writes every page's hash at that field to `<file>.pages`.
`python tools/ps2recomp/compare_state_hash.py <a> <b>` names the first field where two runs with the same input
differ; given two `.pages` files, it lists the pages that differ
([whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md), step 2).

`BT2_NO_NATIVE_FUNCTIONS=1` uses the generated code for the game functions that `ee/native_functions.cpp` replaces,
to compare the two with the state hash.

**Kernel reads.** `BT2_KERNEL_READS=<file>` writes each kernel word the run read, with its offset and value. A boot, a
fight, a story episode and a two-player fight read only the two system call table slots the game looks for.

## Automated sweeps

These tools run the Release game many times without anyone playing. Several runs can go at once with `--jobs` (at most
6, so the PC stays usable; each run takes about one processor core and 600 MB). Interrupted sweeps resume: cases
already in the summary are skipped, unless you pass `--again`.

**Battle sweep.** Runs one battle per case and writes `work/coverage/battles/summary.tsv`: whether the battle reached
its frame budget, the flips, the functions entered, where the interpreter ran, and the stop. A stopped case keeps its
report and log; the memory dumps are deleted. Interpreted entries are collected in `interpreted.txt`, and each
battle's picture at flip 2,500 is kept as `<case>.png` (`--picture`). About 65 seconds per battle.

```powershell
python tools/ps2recomp/battle_sweep.py --disc <iso> [--characters 0-120] [--variants 0-8] [--opponent n] [--stages 0-30] [--frames 4500]
```

`--case <BT2_START_BATTLE text>` (repeatable) runs given battles instead.

**Story scenes.** `python tools/ps2recomp/story_scenes.py --disc <iso>` reads the story's scene scripts from the disc
(files 33 to 653 of the first archive) and lists, for each part of each episode: the map markers and the scenes they
lead to, the marker that leads to the part's story scene (`story_marker`), whether the first scene goes there by itself
(`direct_story`) and whether after a chase (`chase`), and the commands the scenes use. The list goes to
`work/coverage/story/scenes.json` ([story mode plan](../plans/ee/STORY_MODE_PLAN.md)).

**Story sweep.** Runs every story part that `scenes.json` gives a way into, with `BT2_START_EPISODE`, and writes
`work/coverage/story/sweep/summary.tsv`. A part's result is `cleared` or `lost` (its battle's outcome), or `stop`. With
4 jobs, 8 parts took 2 minutes; with 6 jobs, all 200 parts took 56 minutes.

```powershell
python tools/ps2recomp/story_sweep.py --disc <iso> [--episodes 0-24] [--jobs 4] [--frames 15000] [--pictures 2500,4000,5500]
```

- `--case <BT2_START_EPISODE text>` (repeatable) runs given parts.
- `--optional` instead runs one case per scene an optional marker leads to: 73 towns, the shop among them. The result
  is `town` when every character was visited, or the fight's `cleared` or `lost`. Output goes to
  `work/coverage/story/optional/`; about 25 minutes with 6 jobs.
- Pictures at the chosen flips go to `<out>/e<episode>-p<part>[-m<marker>]-<flip>.png`.
- Each part's battles go to `<out>/battles.json`, and in short to the summary's stop column. The last line counts the
  battles of rule 3, the sagas unlocked and the Dragon Balls picked up.
- `--watch <hex,...>` also lists which of those functions each part entered.

**Screen runs.** Runs main menu screens through their battles without input: a generated pad script presses Cross
every 120 fields, and with `--down-every N` also Down for 60 fields every N fields, which the training lessons need.
Each run prints its result and the battles it went through (kind, rule, start field, the lesson in training, the
result). Pictures every `--pictures` flips go to `work/coverage/modes/`. Training (mode 61) with
`--flips 90000 --down-every 400` runs all 21 lessons; Ultimate Battle Z (mode 10) with `--flips 600000` clears all 15
courses.

```powershell
python tools/ps2recomp/screen_run.py --disc <iso> [--modes 10,24,61] [--flips 30000] [--down-every 400] [--jobs 3]
```

**Menu explorer.** Explores the main menu's screens beyond their first page. Each run starts a screen with
`BT2_START_SCREEN` and presses a path of buttons, one every 60 fields from field 800. Paths grow step by step: a path
is extended by each button (`--buttons`, default Cross, Down, Right, Triangle, Circle) when its run entered functions
that no earlier run of that screen entered, or when it ends in fewer than three cursor moves or Crosses in a row.
After that, a path may still add Crosses alone, up to `--crosses` in a row (default 12), to get through chains of
messages. It reports runs that stopped and code that ran interpreted, in `work/coverage/menus/<mode>/summary.tsv` and
`entered.txt`. `--buttons cross --crosses 60` leads Ultimate Battle Z, Dragon Tournament and Ultimate Training into
their battles.

```powershell
python tools/ps2recomp/menu_explorer.py --disc <iso> [--modes 10,24,61,53,5,38,48,43] [--runs 200] [--jobs 6]
```

Mode 7 (the episode select) cannot be started directly: it stops at once at a divide-by-zero trap (`break 7`, the only
`BREAK` in the game's code), most likely because a value the game sets on its way through mode 6 is missing. In the
game, mode 7 is reached from mode 6, which does start.

**Quick start check.** `python tools/ps2recomp/quick_start_check.py --disc <iso> [--card <card folder>]` checks
[Quick start](#the-players-programs-saves-and-the-release) from end to end: a save created as by hand, Continue
reaching the same progress as by hand, the card left unchanged, no card at all, and a button pressed midway. The gate
runs it.

## Pictures and graphics

### Saving pictures

- **`BT2_GPU_GS_SNAPSHOT=<flip>:<path>`** saves the picture of that flip as a PPM file. Separate several with `;`. The
  path must end in `.ppm`, since the file is a PPM whatever its name.
- **`BT2_SCANOUT_QUAD=1`** makes a run without a window scan out at four times the game's resolution with 16x
  super-sampling (a window chooses by its size), for pictures of the sharpest output.
- **`BT2_GS_MEMORY_HASHES=<file>`** writes, after each flip, a hash of GS memory. Two runs with and without a native
  pass must give the same file at 1x.

### Image settings for test runs

Each of these is also a player setting in the launcher.

- **`BT2_SUPER_SAMPLING=<1|2|4|8|16>`** draws with that many samples per pixel. From 4 up, the picture is scanned out
  at twice the resolution, 1,280 × 896 ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)).
  `BT2_SUPER_SAMPLED_TEXTURES=0` turns off paraLLEl-GS's super-sampled textures, for comparisons.
- **`BT2_HD_TEXTURES=<pack folder>[:<budget MB>]`** draws with a PCSX2 texture pack's replacements. The budget is by
  default half of the video memory free at the start, between 256 MB and 8 GB. `BT2_HD_BUTTONS=xbox` uses the pack's
  Xbox button pictures. `BT2_HD_SHOW_MISSING=1` draws every texture the pack lacks in magenta. At the end the run
  prints how many draws were replaced, how many replacements were loaded and the most video memory they used
  ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)). This needs the project's paraLLEl-GS patch, which
  `fetch_parallel_gs.py` applies.
- **`BT2_NATIVE_PASSES=<name>[,<name>...]`** or `none` chooses the native passes inside paraLLEl-GS; without it, the
  default ones run. A name written `-name` removes that pass from the defaults. At the end the run prints how many
  render passes each one drew ([hybrid rendering plan](../plans/graphics/HYBRID_RENDERING_PLAN.md)). The passes:
  - `ink-edges`: the ink outline's edges per sample; the game's own at 1x, sharp with super-sampling. On by default;
    the player setting "HD outline" off sets `-ink-edges`.
  - `glow`: glows such as a super attack's (blended, textured, depth-tested triangles that don't write depth), shaded
    once per game pixel. Only with super-sampling; on by default.
  - `fighters`: the fighters' two layers drawn by the GPU's rasterizer at one pixel per sample. On with 60 fps battles,
    which then keep the in-between renderer on the graphics queue
    ([native fight rendering plan](../plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md)).
  - `dof-weight`: the depth of field's strength, set by `BT2_DOF_STRENGTH=<percent>`; on when that is below 100. The
    setting "Light depth of field" sets 25.
  - `alpha-fill`: a check pass, off by default.
- **`BT2_GAME_PATCHES=<name>[,<name>]`** applies optional patches to the game after it is loaded
  (`runtime/ee/game_patches.cpp`). There is one so far, `widescreen`. Each patched word's
  original is checked first. An instruction a patch changes must be listed in `config/game_patches.json`, so that
  `prepare_runtime.py` makes the generated code read it from memory; then run `prepare_runtime.py` and build.
- **Window.** The window shows each picture through a swapchain (`gs/gpu/gpu_window_presenter.cpp`);
  `BT2_VIEWER_GDI=1` reads each picture back and draws it with GDI instead. The window enlarges a picture by less
  than twice with Lanczos-2 (`runtime/gs/gpu/native/shaders/present_scale.comp`); `BT2_SHARP_SCALING=0` uses a
  plain bilinear stretch instead, to compare. `BT2_VIEWER_FULLSCREEN=1` gives a test run the player's fullscreen
  layout. `BT2_VIEWER_CLOSE=1` closes the window at the end of the run instead of waiting.

### Textures

- **`BT2_TEXTURE_NAMES=<file>`** names every texture the run draws the way PCSX2's texture replacement does
  (`runtime/gs/textures/gs_texture_names.*`), and writes the names, how often each was used, and what its copy of GS
  memory did not follow.
- **`python tools/graphics/textures/texture_name_check.py <file> <pack folder>...`** counts how many of those names
  each pack has, by subfolder and by the texture hash alone.
- **`BT2_TEXTURE_DUMP=<folder>`** (with `BT2_HD_TEXTURES`) writes each texture the pack has no replacement for, once,
  as a TGA named by its PCSX2 name, to see which parts of a screen are still the original's. Don't combine it with
  `BT2_HD_SHOW_MISSING`: the magenta reaches buffers the game reads back.

### Looking inside a frame

- **`BT2_GIF_DUMP=<first flip>:<flips>:<file>`** records everything the main renderer receives over those flips:
  each GIF transfer, the privileged registers and the end of each flip (`runtime/gs/gpu/gpu_gs_backend.cpp`). Runs are
  repeatable, so the transfer numbers in a dump hold for the next run.
- **`python tools/graphics/gpu/gif_dump_draws.py <file> [--flip N] [--touching-y Y] [--registers] [--area]`** lists
  a dump's draws: runs of primitives with the same state, with their rectangle, texture coordinates and their frame,
  texture, scissor, alpha and test settings, plus copies, display registers, and where each draw ends
  (`until <transfer>:<quadword>`). `--area` adds the pixels each draw covers (the shading work); `--registers` adds
  each draw's PRIM, colour, TEXA, COLCLAMP, DTHE, PABE, FBA and TEST, and the words of small uploads such as palettes.
  Draws after the last flip are listed under "after the last flip".
- **`BT2_GS_PEEK=<transfer>[,...]:<path prefix>`** with **`BT2_GS_PEEK_BUFFERS=<block>,<width>,<psm>[;...]`** (hex
  block address, width in units of 64 pixels, the pixel format number) saves those buffers just before those
  transfers, as `<prefix>-<transfer>-<block>.ppm` and `-alpha.ppm` (the alpha, shown over black and white). It needs a
  GIF dump in the same run, and a run without a window needs a picture saved before the dump, which sets the display
  registers. `BT2_GS_PEEK_MEMORY=1` also saves the 4 MB of GS memory itself (`<prefix>-<transfer>.gsmem`); this is how
  the ink outline's chain of draws was worked out.
- **`BT2_READBACK_DUMP=<file>`** writes every byte the GS hands back to the game, in order. With a dump's transfer
  list it showed which upload first went wrong on one graphics card
  ([GPU compatibility](../plans/graphics/GPU_COMPATIBILITY.md)).
- **`BT2_VIF1_DUMP=<first flip>,<flips>:<folder>`** writes every VIF1 transfer of those flips to
  `<folder>/vif1-<flip>.bin` (each transfer as its word count and words). **`BT2_VIF1_REPLACE=<flip>:<file>`**
  replaces that flip's VIF1 transfers, one for one and of equal size, with a file in the same form: what VU1 and the
  GS receive changes, the game's memory does not ([60 fps plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md)).
- **`python tools/graphics/gpu/driver_check/check_unaligned_load.py`** checks whether this PC's Vulkan driver reads a
  3-byte vector through a buffer address correctly (the read behind the striped fade on the RX 9070 XT) and writes the
  driver's compiled code to `work/driver_check/`.

## 60 fps battles

**`BT2_SIXTY_FPS=1`** (the setting "60 fps battles"; GPU renderer only) draws an extra picture between each two fight
frames. A second renderer on a worker thread draws it from the main renderer's state kept at each flip
(`runtime/gs/gpu/in_between_frames.*`), and the window shows it a field after the flip. The second renderer works on
the graphics card's async compute queue, beside the main one; its scanouts stay on the graphics queue.

At the end of the run, a line reports how many in-between pictures were drawn and shown, the fight frames whose draw
lists did not line up, how long the game's thread waited for the worker, and the worker's time per picture by step
(taking the state, pairing and blending the lists, VIF1 and VU1, waiting for the second renderer, processor time),
with the second renderer's own time per frame.

Variants and checks:

- **`BT2_SIXTY_FPS=check`** draws each in-between picture all the way to the newer frame, so it must equal the main
  picture one flip later.
- **`BT2_SIXTY_FPS=replay`** only replays the frames on the second renderer.
- **`BT2_SIXTY_FPS_SNAPSHOT="<flip>:<path>;..."`** saves the second renderer's picture of the frame started at that
  flip.
- **`BT2_SIXTY_FPS_DISPLAY=<n>`** scans the in-between pictures out with the display registers of n flips back
  (default 1, checked).
- **`BT2_IN_BETWEEN_GRAPHICS_QUEUE=1`** puts the second renderer back on the graphics queue, for comparisons.
- **`BT2_IN_BETWEEN_GIF_DUMP=<first flip>:<flips>:<file>`** records the in-between pictures' GS data in the same form
  as `BT2_GIF_DUMP`, so `gif_dump_draws.py` can list them. With a main dump of the same flips, the draws can be compared
  one by one (the newer frame at the same flip, the older one a flip before).
- **`BT2_SIXTY_FPS_SLOW=<from flip>:<to flip>:<ms>`** makes each in-between picture of those flips that much slower, to
  check that the pictures pause and resume under load; the end of the run counts these stretches.
- **`BT2_FIGHT_UNCAPPED=<flip>`** (test runs) lifts the fight's 30 fps wait (`0x28C790`) from that flip on, so the
  fight steps once per field and runs at double speed. Used in the 60 fps study.

With HD textures, in-between pictures can differ between runs, because replacements stream in with timing. Without HD
they are repeatable.

## VU1 programs

The game's VU1 microprograms are translated to native C++ ([performance plan](../plans/ee/PERFORMANCE_PLAN.md)). All
files these tools write describe the game's programs and stay local.

- **`BT2_VU1_CENSUS=<existing folder>`** writes every distinct VU1 program a run starts, and `cost.txt`: per program
  its key, size, starts, instruction pairs run and milliseconds.
- **`python tools/vu1/translate_programs.py <folder> game/generated/vu1/vu1_native.cpp`** translates them to C++. The
  build uses that file when it exists; configure again after creating it. The report's `vu1_native_runs` counts the
  starts that ran native code.
- **`python tools/vu1/disassemble.py <census file>`** lists a program as text, one line per instruction pair (`E`
  marks the end bit, `>` the start address).
- **Hand-written programs** (`runtime/vu/vu1_handwritten_registry.cpp`) are used before the translations;
  `BT2_VU1_NO_HAND_WRITTEN=1` leaves them out. The report's `vu1_hand_written_runs` counts their starts.
- **`BT2_VU1_INTERPRET=1`** runs every program in the interpreter, for comparison.
- **`BT2_VU1_VERIFY=1`** runs each start with a native program twice from the same state, first in the interpreter,
  then natively. The output packets and everything left behind (memory, registers, ACC, Q, I, clipping flags, stop
  address, pair count) must be equal, or the run stops and says which program and what differs. The report's
  `vu1_verified_runs` counts the starts checked. Several times slower than a normal run.
- **`BT2_VU1_KICK_HASH=1`** hashes every GS packet VU1 sends into the report's `vu1_kick_hash`, to prove a VU1 change
  leaves the output unchanged on a scripted run. The gate's battle run sets it.

## Sound

- **`BT2_AUDIO_WAV=<path>`** writes the run's mixed sound (streams and sound effects) as a 48 kHz stereo WAV file.
  There are 800.8 samples per field, so the file's length is the run's game time. The report counts the sound used:
  `audio_stream_bytes`, `audio_speech_stream_bytes` (one-channel streams), `audio_voice_samples` and
  `audio_underruns` (samples a voice could not supply); `cri_stream_mixes` lists the bytes heard per channel count,
  volume, pan and rate.
- **`--sound`** (`BT2_AUDIO_DEVICE`) plays the sound on the default audio device, alone or with the window and the
  WAV file. With the window or the audio device, a clock-based pacer holds the run at the game's field rate
  (`runtime/host/frame_pacer.h`), following the device's queue within 1%. At the end it prints the fields played and
  those that found the device empty (the run was slower than the game). Runs without either are not paced.
- **`BT2_MASTER_VOLUME=<percent>`** sets a test run's volume; 0 lets two paced runs play side by side in silence.
- **`BT2_SOUND_LOG=<existing folder>`** writes `requests.txt` (one line per request: the field, `sounds` or `driver`,
  the function and the command words in hex) and, for each sound bank loaded, its samples, head and body. The report
  counts what the effects player did: `sound_effects_played`, `_looping`, `_kept`, `_kept_refused`, `_stopped`,
  `_stops_unmatched`, `_silenced`, and `sound_voices_peak` and `sound_voices_stolen`. The files come from the game and
  stay local.
- **`python tools/audio/compare_audio.py <port.wav> <reference.wav> [--segment s] [--search s]`** finds the port's
  sound in a recording of the original (both 16-bit stereo at the same rate) and prints, per segment, the offset, the
  level ratio, how closely the waveforms match (with the same and with swapped channels) and the peaks. It needs numpy
  and scipy. Recordings stay local.

## Speed and input lag

See the [performance plan](../plans/ee/PERFORMANCE_PLAN.md), the [responsiveness plan](../plans/ee/RESPONSIVENESS_PLAN.md)
and the [performance findings](../reference/PERFORMANCE_FINDINGS.md) for what was measured with these.

### Frame times

- **`BT2_VIEWER_STATS=1`** (with the window) prints every 120 flips the flips and fields per second and the time per
  flip by part: picture readback, showing, VIF1, VU1, GS packets on the game's thread, the GS threads' work and the
  game's wait for them, and GPU flush. Each thread keeps its own profile (`ee/frame_profile.h`). Its "scanout" is
  the renderer's scanout; with `BT2_VIEWER_GDI=1` it includes the readback.
- **`BT2_FLIP_TIMES=<file>`** (with `BT2_VIEWER_STATS=1`) writes each flip's number and time, the game's wait for the
  GS threads and their work, in milliseconds. It finds single slow flips, such as a super attack's, which the
  120-flip averages hide.
- **`BT2_PROFILE_TOTALS=1`** prints at the end of a run without a window the seconds spent in VIF1, VU1, GS packets on
  the game's thread, the GS threads' work (texture names and the renderers' intake), the game's wait for them and the
  flush.

### GPU time

- **`BT2_GPU_TIMESTAMPS=1`** times the renderer's work on the graphics card by stage and prints the milliseconds per
  frame at the end (a card at low clocks shows longer times for the same work). The native passes are a stage of their
  own. Granite's report at the end names each render pass's shading by kind
  (`shading fb <FBP> psm <PSM> x<samples> [textured] [blended] prims <n>+`). To measure one stretch of a fight, make two
  runs to its start and its end, scale each run's numbers so they add up to its shading total, and subtract.
- **`BT2_GPU_PASS_TIMES=<file>`** (turns timestamps on) writes one line per render pass and per native pass: the flip,
  its GPU milliseconds and its kind, so costly passes can be found and recorded with `BT2_GIF_DUMP`.

### Where the processor's time goes

- **`BT2_SAMPLE_PROFILE=<path>`** samples where the game's thread is running. `BT2_SAMPLE_FROM=<flip>` and
  `BT2_SAMPLE_TO=<flip>` limit the stretch; `BT2_SAMPLE_INTERVAL_US=<n>` sets the interval (below 1000 it spins: about
  20,000 samples a second at 50).
- **`BT2_SAMPLE_GS_THREADS=<prefix>`** samples each GS thread (the main renderer's, the in-between thread, the second
  renderer's, in that order) into `<prefix>-<n>.txt`.
- **`python tools/perf/profile_report.py <profile> build/probe/Release/bt2-entry-probe.map [--top N]`** lists the
  functions by share of samples, with the calling function for samples in library code (the game is linked with a map
  file for this). **`python tools/perf/profile_lines.py <profile> build/probe/Release/bt2-entry-probe.exe`** lists
  source lines; the busiest runtime files keep line information in Release.
- **`BT2_FIELD_LOG=<file>`** writes one line per field: its time on the game's thread in microseconds, the game calls
  so far, flips, disc sector reads, file and stream requests, loading waits, the peak of the field's sound, and the
  video macroblocks decoded so far. It finds hitches, loading stretches and when the films play.
- **`BT2_FIELD_CALLS=<file>:<field>`** writes how often each function was called in that field, with its busiest
  caller: what a slow field was doing.

### Input lag and showing pictures

- **`BT2_LATENCY_LOG=<file>`** (with the window) writes each shown flip's time and when it was handed over. The
  presenter adds `<file>.presents` with each present and the refresh Windows reported for it.
  **`python tools/perf/latency_report.py <file>`** prints the time from the game's flip to the display by stage, and
  the rhythm a variable-refresh display shows.
- **`BT2_LOW_INPUT_LAG=1`** (the setting "Low input lag", on by default for players) shows the picture after a flip
  at the end of that flip's field, instead of at the next flip (`host/early_picture.h`, responsiveness plan step 6).
  The end of the run prints how many pictures went out early. It is not used in fast forward, online, with
  `BT2_PICTURE_AT_DEADLINE`, or in 60 fps fights.
- **`BT2_PICTURE_AT_DEADLINE=1`** shows every picture at its field's deadline instead of ahead of it
  (`FramePacer::waitToShow`).
- **`BT2_VARIABLE_REFRESH=1`** (the setting "Variable refresh rate") shows each picture at once, with tearing allowed,
  for variable-refresh displays (`gs/gpu/gpu_window_presenter.cpp`). Windows' frame statistics in the `.presents`
  file then no longer say when a picture was shown.
- **The pacer's wait.** With 8 or more logical processors, the frame pacer spins for the last 6 ms of each wait, to
  keep the game's processor core awake (`host/frame_pacer.h`). `BT2_PACER_SPIN_US=<n>` sets another window;
  `BT2_PACER_SLEEP=1` sleeps all the way, for comparisons.

### Fast forward and loading

- **`BT2_FAST_LOADING=1`** (the setting "Fast loading", with the window or the audio device) runs silent loading
  stretches at full speed (`host/loading_skip.h`). The end of the run says how many fields ran so.
- **`BT2_FAST_FORWARD=1`** (with the window or the audio device) runs the whole run in fast forward, as when Tab is
  held. With `BT2_VIEWER_STATS=1` the statistics show its speed.

## Finding a bug in the game's logic

These follow values through the game's code without changing it. Together they let you go backwards from a wrong
value: who stored it, with which arguments, from which calls. The camera bug of 2026-10-13 was found this way in about
ten runs.

- **Call trace: `BT2_EE_CALL_TRACE=<file>:<from flip>:<hex address>[,<hex address>...]`** writes one line per call of
  those functions from that flip on: the flip, the calling address, the target, `a0` to `a3`, `f12`, `f13`, `f0` and
  `v0` (the last two still hold the previous call's results), and the four floats each of `a0` to `a2` points at, when
  it points into memory. Trace a library function and read the next line's `f0` to get its inputs and result. Writing
  `a0=<hex>` instead of addresses traces every call whose `a0` holds that value: all users of one structure. That is how
  the beam struggle's input was found ([beam struggle plan](../plans/input/BEAM_STRUGGLE_PLAN.md)).
- **Store watch: `BT2_EE_WATCH_QUADWORD=<file>:<from flip>:<hex address>`** writes one line per 128-bit store to that
  address: the flip, the main thread's program counter and return address, the last call and the four floats stored.
- **Word watch: `BT2_EE_WATCH_WORD=<file>:<hex address>[,<hex address>...]`** writes a line each time a word there
  has changed at a game call: the flip, the call it changed before, and the old and new value. Combined with
  `BT2_REPLAY_CALL` it follows a value through one call.
- **Float watch: `BT2_EE_WATCH_FLOAT=<hex address>:<limit>`** stops the run at the first call that finds the float at
  that address not a number, infinite, or larger than the limit, and names that call, the one before and the main
  registers. An address below 32 watches the x field of that VU0 register (`1F` is `vf31`).
- **Call sums: `BT2_EE_CALL_SUMS=<file>:<hex call address>[,...]:<hex word>[:<start>-<end>...]`** writes one line per
  call from those addresses: the address, the field, the flips, the word, and checksums of the given memory ranges
  (`runtime/probe/call_sums.cpp`). It is the port's side of `input_delay.py`.
- **Memory at a call: `BT2_EE_CALL_RAM=<file>:<hex call address>:<n>`** writes all 32 MB of EE memory before the n-th
  call from that address (`runtime/probe/call_ram.cpp`).
- **Changing a call: `BT2_EE_CALL_SET=%<hex function>:<n>:<reg>=<value>,...[;...]`** sets `a0` to `a3` or `t0` to `t3`
  at the function's n-th entry and prints a line when it does (`runtime/probe/call_set.cpp`). It is the port's side of
  `session.py --set`.
- **Replaying one call: `BT2_REPLAY_CALL`** runs one function from a state taken from PCSX2; see
  [Comparing with PCSX2](#comparing-with-pcsx2).

## Comparing with PCSX2

The port is checked by running the same game code from PCSX2's own state and comparing the results byte for byte
([plan and record](../plans/validation/PCSX2_COMPARISON_PLAN.md)). These tools are in `tools/pcsx2_compare/`.

**Before you start**, set your own PCSX2 program and BIOS. Neither is in this repository.

```powershell
$Pcsx2Path = 'PATH_TO_PCSX2_EXE'
$BiosPath = 'PATH_TO_YOUR_PS2_BIOS'
```

### Sessions

A session is one PCSX2 run with checkpoints. The tool makes an instrumented copy of the executable in which chosen
calls jump to a small stub (`context_block.py`). At each checkpoint, the stub publishes all registers (the general
registers at 128 bits, HI/LO, SA, the FPU registers, FCR31 and VU0's VF, VI and control registers; not the two ACCs;
`t8` and `t9` are the stub's own), waits while the tool reads EE memory, VU memory and the registers out of PCSX2, and
then lets the game continue.

```powershell
python tools/pcsx2_compare/session.py --pcsx2 $Pcsx2Path --bios $BiosPath --disc <iso> --card <Mcd001.ps2 with a BT2 save> --session <name> --site NAME:FIRST:COUNT:STRIDE[:after] ...
```

- **Sites.** A site is a name from `config/pcsx2_compare_sites.json`, `@<hex call address>` (any `jal` in the
  executable), or `%<hex function>` (the function's entry: every call, also from the overlay or through a pointer).
  `--callees-of FUNC:...` takes every call inside a function. Sites may lie in the overlay (`0x3B3100` to
  `0x43058F`): a stub on the main program's call into the overlay (`0x100688`) installs them once the overlay is
  loaded.
- **Which calls.** Calls FIRST, FIRST + STRIDE, and so on, COUNT of them, are taken before the call; with `:after`,
  after it too.
- **Output** goes to `work/pcsx2-compare/<name>/`: `session.json` and a folder per checkpoint. EE memory is stored
  whole every 64th checkpoint, and otherwise only the 4 KB pages that changed (`ram_store.py`), so thousands of
  checkpoints take a few GB.
- **`--idle S`** ends the run S seconds after the last checkpoint; **`--until <site>`** ends it at a site.
- **`--edit SITE:REG=VALUE[/VALUE...]`** changes `a0` to `a3` or `t0` to `t3` (before the call) or `v0`, `v1` (after it)
  at that site's checkpoints in turn.
- **`--set %FUNC:CALL:REG=VALUE[,...]`** sets `a0` to `a3` or `t0` to `t3` at a function's CALL-th entry, without
  stopping. For example, it turns the title's demo into a versus battle. `BT2_EE_CALL_SET` does the same in the port;
  with the PCSX2 card's save, the port then takes the same route as the session.
- **`--pad-script <script or @file>`** presses buttons in PCSX2 with the port's pad script syntax, counted in calls
  of the pad update (`0x28BDA0`, about one per field). `config/pcsx2_pad_main_menu.txt` reaches the main menu with
  the card's save; `config/pcsx2_pad_character_select.txt` reaches character selection in Dueling, 1P vs COM, single
  battle.
- **`--log NAME:COUNT[,word=ADDR][,sum=START-END][,press=FIRST/HOLD/ADDR/VALUE]`** adds a logging site that does not
  stop, so PCSX2's timing stays the game's. After each of its first COUNT calls, it records the call's number, COP0
  Count, up to two words and three checksums of memory ranges (VU1 data memory is at `1100C000`) in a ring of 4,096
  records, written to `<site>.log.json`. `press=` sets the bits of VALUE in the word at ADDR (a bitwise OR) after
  calls FIRST to FIRST + HOLD − 1: a button press the game reads.
- **`--realtime`** runs PCSX2 as a player sees it (full speed, the usual renderer and sound, window shown, the keyboard
  as controller 1 with the port's keys). It writes `ready` in the session folder once the `--until` site is done, then
  runs until a file named `stop` appears.
- **`--vu1-interpreter`** uses PCSX2's VU1 interpreter instead of microVU. Sessions always run with MTVU off and the
  software renderer on one thread (`extrathreads = 0`), because with worker threads a draw that reads back what the
  frame just drew was sometimes caught half done.
- **`--save-state SITE`** asks PCSX2 for a save state through PINE (`pine.py`, port 28011) at that site's checkpoints,
  kept as `state.p2s`. It is needed for comparing GS memory.
- **`--rpc-log COUNT`** logs every SIF RPC request and synchronous reply the game makes (two patches of
  `sceSifCallRpc`, `rpc_log.py`): function, client, mode, the buffer's size, address, checksum and first 24 words;
  replies are marked by bit 31 of the mode. The log goes to `rpc.log.json`, without stopping.

Each checkpoint also holds `hardware.bin` (VIF1's registers `0x10003C00` to `0x10003D70`, GIF_STAT, D1_CHCR, D1_MADR,
D1_QWC and D1_TADR) and `vu1-regs.bin` (VU1's VF, VI and ACC, found once per session by `vu_registers.py`).
`test_context_block.py` tests the stub on a small interpreter.

### Comparing the results

Boot the port with the PCSX2 card's save (`BT2_CARD_FOLDER`); otherwise the game takes a different route at the card.

- **One call at a time: `python tools/pcsx2_compare/compare_calls.py --disc <iso> --session <name>`** replays every
  before/after pair in the Release game and lists the words the port wrote differently, and differences in the return
  registers, the kept registers and VU0's random number register R. The results go to `compare.json`. A call that
  reaches a system call is listed as stopped there, not as a difference. `--quiet` prints only the calls that differ
  and a count per site. Words only PCSX2 changed while it waited (interrupts, other threads) are listed apart. The tool
  restores the overlay's original words for the replay.

  The replay itself is `BT2_REPLAY_CALL="<RAM in>|<registers in>|<hex function>|<RAM out>[|<VU memory in>|...]"`. Its
  sixth and seventh parts are the checkpoint's `hardware.bin` and `vu1-regs.bin`: VU1's memory is then loaded too, and
  VU1's data memory after the call is written to `<RAM out>.vu1`. Its eighth part is GIF data from `gs_memory.py`,
  sent before the call; the port's GS memory after the call goes to `<RAM out>.gs` (function `0` runs nothing, to check
  the loading).
- **Memory: `python tools/pcsx2_compare/loaded_data.py --pcsx2 <checkpoint folder> --port <file> [--out <json>]`**
  compares a checkpoint with a `BT2_EE_CALL_RAM` file. It lists the ranges where they differ (merging gaps under 64
  bytes) and sets them against the explained regions in `config/pcsx2_compare_regions.json`, each with its reason,
  and the session's patched words. Ranges outside them are findings.
- **Graphics: `python tools/pcsx2_compare/gs_compare.py --disc <iso> --session <name> --call <checkpoint> --settled <later checkpoint>`**
  replays the call with GS memory from the save state (`gs_memory.py` reads it from `GS.bin`), with native passes off
  and one sample per pixel. It compares GS memory by region (the frame and depth buffers, then the rest) and the
  picture's pixels by size of difference, and saves both pictures and their difference to
  `work/frames/pcsx2-compare/`. Take the settled state at the end of the frame, since PCSX2's GS may still be drawing
  just after the call.
- **Services: `python tools/pcsx2_compare/rpc_compare.py --session <name> --port <file> [--out <json>]`** compares
  the session's service log with the port's, per client and in order, showing each client's first difference and the
  requests around it. `BT2_RPC_LOG=<absolute file>` writes the port's log at the same points
  (`runtime/probe/rpc_log.cpp`), with `B` lines for the binds that name each client's service.
- **Input delay: `python tools/pcsx2_compare/input_delay.py --without <session> --with <session>`** compares two
  sessions of the fight loop, with and without a button press (`@102EE0` with the buttons word, `@102F08` with the
  draw lists and VU1 checksums), and reports the frame and field at which the draw lists and VU1 memory first differ
  after the press. `--port-without` and `--port-with` read the port's side, written by `BT2_EE_CALL_SUMS`.
- **Press to screen: `python tools/pcsx2_compare/press_to_screen.py --program pcsx2|port --disc <iso> --label NAME [--presses 30] [--port-env BT2_SIXTY_FPS=1 ...]`**
  measures the time from a button press to the screen in the demo's fight. It brings the program's window to the front
  (it takes the keyboard focus during the run), pauses with Start, and times each unpause from the key press to the
  first changed picture, read from the screen about every 7 ms. Results go to `work/latency/<label>.json`, and the
  screen region it watched to `work/frames/latency/<label>-region.png`.

The comparison of the game's AI with PCSX2 ([AI verification plan](../plans/ee/AI_VERIFICATION_PLAN.md)) is done with
these sessions too, at the sites `input` and `think`.

## Online battles

See the [online battle plan](../plans/online/ONLINE_BATTLE_PLAN.md).

- **`BT2_NET=host:<port>`** or **`join:<address>:<port>`** runs an online battle in lockstep over UDP
  (`host/net_lockstep.*`, `probe/online_session.*`). It sets up the rest of the session itself: a fresh start into
  Dueling with the menus' sides kept, every fighter unlocked, the local player's view, no memory card, and no fast
  forward or fast loading. It refuses settings it cannot honour.
- **`BT2_NET_DELAY=<fields>`** is the host's input delay, 1 to 8. With 0 or unset, the joiner picks it from the round
  trip it measures when connecting.
- **`BT2_NET_UPNP=1`** makes the host open its port on the router by UPnP (`host/upnp_port.*`).
- **`BT2_NET_STUN=1`** makes a test run ask the public STUN servers for its internet address, as a player's game
  always does. `BT2_TEST_STUN=1 bt2-net-lockstep-tests` checks that against the real servers; it is not in the gate.
- **`BT2_NET_SIM=latency=<ms>,jitter=<ms>,loss=<percent>`** simulates a worse network at that side's receiving end. Set
  it on both sides; the round trip is then twice the latency.
- In a test run, `BT2_PAD_SCRIPT` is the local player's input (port 1 for the host, port 2 for the joiner). The host
  waits 5 minutes for a joiner; a joiner waits 1 minute for the host. The last line of the run counts the fields, the
  drift checks that matched, the waits for the other player and the round trip.
- **Two players on one PC:** start the host, then the joiner with `join:127.0.0.1:<port>`, each with its own
  `--report`.
- **`BT2_UNLOCK_ROSTER=1`** with a fresh start makes every fighter selectable.
- **`BT2_ONLINE_VIEW=1|2`** shows only that player's view of 1P vs 2P, full screen (`ee/online_view.h`), with the
  online changes to the game: no Return to Main Menu (`ee/online_menus.h`), and rounds start locked on
  (`ee/online_lock_on.h`). With `BT2_START_SCREEN=30` and `BT2_PAD2_SCRIPT`, a two-player session runs from scripts.

**Controllers.**

- **`BT2_PAD2_SCRIPT=<pad script>`** connects a scripted controller in port 2.
- **`BT2_PAD2_LIVE=gamepad`** gives port 2 the same live input as port 1 (both sides do what the player does);
  `keyboard` gives the gamepad to port 1 and the keyboard to port 2. With `--record`, port 2's input goes to
  `<file>.pad2.txt`.
- **`BT2_PAD_TIMING=1`** prints, at the end of a run with live input, each HID gamepad's report count and longest
  gap, how old the first gamepad's newest report was when the game read it, and how many reports had waited in
  Windows' queue.
- **A scripted beam struggle:** both sides played, both scripts
  `1700:ly=00,1900:-,1910:l2,2270:-,2280:l2,2284:l2+triangle,2292:l2,2298:-`, with `BT2_START_BATTLE=0,1,0`. The beams
  meet at flip 987. Super attacks need the opponent within range
  ([beam struggle plan](../plans/input/BEAM_STRUGGLE_PLAN.md)). `BT2_STRUGGLE_BUTTONS=1` and
  `BT2_TEXTURE_OVERRIDES=<folder>` are the setting "Beam struggles by buttons" and its button pictures.

## The player's programs, saves and the release

**Building and starting the player's programs.**

```powershell
python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-game bt2-launcher bt2-entry-probe
```

Then start `build\probe\Release\bt2-game.exe` (the launcher opens the first time) or `bt2-launcher.exe` (always the
launcher). The settings are in `user/settings`, a text file with one `key = value` per line; delete it to start over.
The last run's messages are in `user/last-run.log`; when a run stops by itself, the game shows its last lines. In the
game, Escape, the PS button or both stick buttons open the in-game menu; Tab fast-forwards. For runs that should record
a pad script, use `run_startup_probe.py --play`.

**Quick start.** `BT2_QUICK_START=1` (the setting "Quick start", `probe/quick_start.h`) starts at the main menu. After
the memory card check, it presses the game's own skips, unseen and in fast forward: Start through the logos, the film
and the title, then Cross on Continue, or on New Game and Yes to create a save on a card without one. Any other
question, a button you press, or a step that takes too long hands the game to the player, and the log says how it
ended. `BT2_QUICK_TITLE=1` (set by the game after the in-game menu's "Return to the title screen") runs the startup in
fast forward until the title begins or a button is pressed; it needs the window's pacing.

**Pause test.** In a player's run (`BT2_PLAYER_SETTINGS`), `BT2_PAUSE_TEST=<field>:<milliseconds>[:<picture file>]`
opens the in-game menu by itself at that field, closes it after that time and saves the last picture it drew. With
`BT2_VIEWER_CLOSE=1` this reproduces a pause at a given moment without pressing keys.

**Launcher pictures and logo.** `BT2_MENU_PICTURES=<folder> build/probe/Release/bt2-menu-tests.exe` saves each
launcher screen from the tests as a `.ppm` picture. `python tools/launcher/cut_logo.py` makes
`assets/launcher/logo.png` from the README banner.

**Program icons.** `python tools/build/make_icons.py [--preview <png>]` draws `assets/icons/bt2-game.ico` and
`bt2-launcher.ico` (no game artwork); the builds include them (`runtime/game/*.rc`).

**Saves.** Each memory card is a folder; the launcher's Memory card screen picks the one in slot 1, and a test run
takes one from `BT2_CARD_FOLDER=<folder>` (the log names it: `Memory card in slot 1:`). Loading, saving and
overwriting are verified, and saves are identical to the original's in PCSX2
([memory card plan](../plans/memory-card/MEMORY_CARD_PLAN.md)). Two helpers bring saves in
([saves guide](release/SAVES.md)):

- `python tools/saves/import_save.py SAVE.cbs "<game folder>/saves/Card 1"` unpacks a CodeBreaker save into a card
  folder (one folder per save, with its files and `entries.txt`).
- `python tools/saves/card_image.py IMAGE [SAVE CARD_FOLDER]` lists the saves on a PS2 card image (PCSX2's `.ps2`),
  and copies one onto a folder card the same way.

**The release package.** `python tools/build/package.py [--config Release] [--out dist]` makes
`dist/BT2-Recomp-<version>/` and its zip: the three programs, the C runtime DLLs, the port's own pictures (not the
game's title logo), empty `data` folders with notes for the player's disc and HD pack, the player guides from
`docs/guides/release/` as text files, the license and the third-party notices. It refuses game or console files by
name ([release plan](../plans/launcher/RELEASE_PLAN.md)). Nothing is published until the project owner decides to.

## The startup capture record

Before the port could boot on its own, its startup was brought up one step at a time against captures from PCSX2. The
tools for that (`tools/capture_ee/`, its `verify_*.py` checkers, `probe_kernel.py`, `generate_kernel.py`, the
Ghidra review scripts and `run_pipeline.py`'s capture options) still exist, and their record is kept in the
[startup capture record](../reference/STARTUP_CAPTURE_RECORD.md). Everyday work does not need them.
