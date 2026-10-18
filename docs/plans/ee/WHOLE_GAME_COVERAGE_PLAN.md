# Whole-game coverage

## Why

Decided by the user on 2026-10-18: finding what the port is missing by playing, or by scripted button input, does not scale. The game has too many states (menus, story episodes, characters, stages, special moves), the right buttons differ in each, and every stop costs a play session, a discovery run and a rebuild. The goal is the whole game recompiled and running without stops, reached by work that does not depend on someone steering the game to each state.

The user's decisions of the same day:

1. **An EE interpreter is accepted as a logged bridge** for code the map does not hold yet, so a session continues instead of stopping. What it runs is compiled natively at the next build; native code stays the base, and code that matters for speed is native ([performance plan](PERFORMANCE_PLAN.md)).
2. **A tool-only starter of game states** (a replacement of the game's own menu routine, used by the probe and the sweeps, never by the player programs) is accepted.
3. This plan goes ahead of the [performance plan](PERFORMANCE_PLAN.md).

## Evidence (measured 2026-10-18, on the local map of 2026-10-13)

- **The map covers nearly all code.** The main program's 11,122 functions cover 2,317,848 of the 2,340,476 bytes of `.text` (99.03%); of the 5,300 gaps between functions, 16 hold non-zero words (316 words in all, three of them more than 16 words: 0x31EC58, 0x338868, 0x33ABDC). The overlay's 494 functions cover 99.77% of its code range, with no non-zero word outside a function. `map_gaps.py` reports no call, jump or data-pointer target outside a function.
- **Addresses built in code are not scanned yet.** A first scan of `lui` followed by `addiu`/`ori` on the same register (no tracking across branches) finds 448 code addresses that are function starts, 7 in unmapped code and 502 inside functions (115 distinct, most of them constants that merely fall into the code range).
- **The stops left are in the runtime, not the map.** The runtime has about 550 sites that stop a run (342 `runtime_error`, 197 `UnsupportedSyscall`, 49 `UnsupportedGuestMemory`). Many hardware and kernel paths accept a request only from the guest PC, stack offset or call count seen at startup: 46 such conditions in `runtime/ee/ps2_runtime_memory.cpp` (for example Timer 1 writes only from 0x28C770 with the stack at `initial_sp-112`, the texture DMA only from 0x28B96C), 23 in `ps2_runtime_syscalls.cpp` (one requires `cri_rpc_calls==37 && heap_calls==6`), 4 in `ps2_runtime_dispatch.cpp` (load waits recognized by their caller's address). A screen that reaches the same hardware from another place stops there. AGENTS.md already rules these out ("generalize verified special cases instead of adding handlers gated on guest PCs, stack offsets or call counts").
- **Runs are repeatable and fast.** The scripted fight gives the same VU1 output hash on every run (`5a4960667d4a3dff`); headless Release it takes about 14.5 s for 5,100 frames.

## What the references do (consulted 2026-10-18)

BT3-Recomp at `e22a34beb564e2d0f32bd60b02f0a95d9645ddcc` and Tenkaichi3Decomp at `ec3c85a` (its `docs/`, `port/` and `scripts/` only; its decompiled game code was not downloaded). Details in [the reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#whole-game-coverage-2026-10-18).

- **BT3-Recomp** runs code without a recompiled function in a MIPS interpreter (`ps2xRuntime/src/lib/ps2_interp.cpp`) that calls recompiled functions natively when it reaches them, and logs every interpreted address (`PS2X_COVERAGE`). Its orchestrator (`tools/orchestrator/`) keeps a corpus of everything logged, discards what it has seen before and lists what to recompile next. Callback and jump-table targets inside a function get a label and a slot in the function table (`games/bt3/gen_overlay.py`, `REENTRY_LABELS`), found one at a time by play. Its kernel is answered by system call number (`Kernel/Syscalls/`). Not adopted: its many fixed game addresses in the runtime, and its reliance on play to find what is missing.
- **Tenkaichi3Decomp** lists what the game needs when linking: a stub for every missing symbol (`port/tools/link.py`), about 130 at the first link, which became the work list of the native layer. It reaches game states through the game's own code: `port/src/headless.c` stands in for the menu routine, fills the battle setup (`BT3_CHARA`, costume, stage) and starts the game's CPU-vs-CPU demo battle or a recorded replay; a debug routine the developers left in unlocks everything. It hashes the game state at every vertical blank (`BT3_HASH`, `compare_hash.py`) to find the first blank where two runs differ, and compares memory at a battle tick with a PCSX2 save state (`compare_state.py`). Its notes describe the same engine family: the overlay's entry runs the menus, dispatching on a mode field through a jump table, and the battle runs when it returns (`docs/systems/boot_and_frame.md`). That is a hypothesis for BT2, to be checked on BT2's own code.

## Design

### 1. Close the map statically

- `map_gaps.py` also scans addresses built in code (`lui` with `addiu`/`ori`, `lui` with a load/store offset when the result is jumped to), data words that point inside a function, and every switch-table target.
- The 16 non-zero gaps and the 7 built addresses in unmapped code are resolved one by one: mapped as code, or recorded as data with the reason.
- **Entries inside a function:** a target inside a mapped function that is a proven entry (a switch-table target reached from another function, a callback address) gets a re-entry label in its function and a slot in the function table, so dispatch to it resumes in compiled code. A target that is only a constant in the code range is left alone. The map builder's split at an entry address stays the rule where the code before the entry is complete.
- **Done when:** the scan reports nothing uncovered and every built or stored address in a code range is a function, a re-entry label or a recorded non-code constant. The gate checks that the scan stays empty.

### 2. State hash per field

- `BT2_STATE_HASH=<file>`: a hash of EE RAM, the scratchpad and the VU memories at every VBlank, written per field; a tool names the first field at which two runs differ and the 4 KB pages that differ there.
- Used to check that a refactor of a handler (step 5) or the interpreter (step 3) changes nothing: the gate runs and the scripted fight must give identical hashes before and after.

### 3. Interpreter bridge

- **Where it runs:** only where dispatch finds no function and no re-entry label for a target in guest RAM. It runs from the target until control reaches an address with a compiled function (which it calls natively) or the caller's return address, as BT3-Recomp's `interpretUntil` does. Every call it makes into compiled code, and every return from it, passes through the same dispatch, scheduler and syscall paths as compiled code.
- **Same semantics as the generated code:** it uses the runtime's memory and register macros and the corrections `prepare_runtime.py` applies to the generator's output (the operand of `SQRT.S`, writes to VF0, the VU0 random number instructions, the sign extension of `MFC1`, the 64-bit branch tests). An instruction it does not implement stops the run and names the instruction and its address.
- **Checked against the compiled code:** unit tests per instruction class, and a check mode that runs chosen compiled functions in the interpreter as well, from the same register state, with the interpreter's memory writes kept in a journal, and compares registers and writes at the return.
- **Logged and compiled away:** every interpreted entry address, with its instruction count and the hash of its code bytes, goes to the run's coverage file (step 6) and to the report (`ee_interpreted_entries`, `ee_interpreted_instructions`). The discovery tools take these addresses as entries, so the next build runs them natively; the interpreter is a bridge for one build, never the home of code. Code loaded at run time outside the known programs is reported the same way, with its bytes hash, as a candidate for a new generated program.
- **Native where speed matters:** interpreted code is compiled at the next build; functions that cost time in compiled form are replaced by hand-written native code by measurement, in the [performance plan](PERFORMANCE_PLAN.md). The player programs warn in the log when a session interpreted anything; the gate runs must interpret nothing.
- **Files:** `runtime/ee/ee_interpreter.cpp` (with focused companions if it grows), state in `EeRuntime`; no change to the headers generated code includes unless a new entry point is needed.

### 4. What the game asks of the runtime, from its code

A tool reads the main program and the overlay and lists:

- every `syscall` site with its number (the value placed in `v1` before it), against the numbers the runtime handles;
- every hardware register address the code builds (EE registers at `0x1000xxxx`, GS at `0x1200xxxx`, VU and VIF memories), with the access width;
- every SIF RPC call site with its server and function number where they are constants;
- every VU1 microprogram (`.vutext`, the DVP overlay sections, programs uploaded from the overlay), with a check that the VU1 interpreter supports each instruction.

The result is the work list for step 5, in the role Tenkaichi3Decomp's list of link stubs had. Requests built at run time (VIF codes, GS packets, service commands not passed as constants) are not covered by it; the coverage files (step 6) report them.

### 5. General kernel and hardware handlers

- Each request is answered by its system call number or register address, whatever the caller: threads, semaphores, event flags, interrupts and alarms by number; timers, DMA channels, INTC, VIF and GS registers by address. One implementation per behavior, with a unit test each. References: PCSX2 and PS2SDK for behavior, BT3-Recomp's `Kernel/Syscalls` and the PS2Recomp runtime for the layout.
- The startup handlers gated on guest PCs, stack offsets and call counts are replaced, one subsystem at a time: the state hash of the gate runs and the scripted fight stays identical, and the original comparisons keep passing.
- Waits are recognized in general (a loop polling a status with an idle point or a VBlank wait), not by the caller's address.
- Order: what step 4 lists and the sweeps (step 6) hit, most frequent first.

### 6. Game states without input, and the coverage corpus

- **Find BT2's own state machinery** (the evidence is gathered on BT2; names stay neutral): the routine the overlay's entry uses to choose a screen and its mode field, the battle setup the character and map selection fill, the start of the CPU-vs-CPU demo fight (the idle demo fights already show the game has one), story episode selection, and an unlock routine if one exists. The user's 100% save is the fallback for unlocked content.
- **Starter:** a tool-only replacement of that routine through the function table (switches such as `BT2_START_BATTLE=<character>,<character>,<map>`, `BT2_START_SCREEN=<mode>`, `BT2_START_EPISODE=<n>`). It writes the same fields the menus write and lets the game's own code run from there. Not built into the player programs.
- **Sweeps:** headless Release runs, one after another (the AGENTS.md rule on runs holds): CPU vs CPU for every character, form and map (the AI uses special moves and ultimates without input), every menu screen by its mode, every story episode. Each run has a frame budget and the stall detector (`BT2_STALL_FIELDS`).
- **Coverage file:** every run, sweep or player session writes the functions entered, the interpreted entries, the targets inside functions and the requests the runtime refused, with their names (`BT2_COVERAGE=<file>`; the player programs write `user/coverage.tsv`).
- **Corpus:** a tool under `tools/ps2recomp/` keeps everything seen in ignored `work/coverage/`, discards what it has seen before and plans the next batch: new entries go to `discovery_loop.py` in one regeneration, refused requests go to step 5 grouped by request. Each sweep that stopped keeps its switches as a reproducer and is rerun after the fix.

### Order

1 → 2 → 3 → 4 → the starter and the battle sweep of 6 → then 5 and the rest of 6 in turns, until the overnight sweeps run without a stop and without interpreted code → the [performance plan](PERFORMANCE_PLAN.md).

## Limits and stop conditions

- The interpreter runs only code the map does not hold. It is not a fallback for refused requests (those stop, as now) and not a replacement for compiled code; a gate run that interprets anything fails.
- No fixed game addresses in the general handlers. The starter is the one place that writes the game's own state fields, and only in tool runs.
- The starter's fields and routines are established from BT2's code and checked in runs (the sweep's frames are kept for the user to look at); BT3's layout is not assumed.
- Coverage files, the corpus, sweep frames and the starter's address notes stay in ignored `work/`. Documents record counts and descriptions only.
- Stop and plan separately if a sweep reaches a new subsystem (for example network or a device the game drives directly), or if code loaded at run time needs a new generated program.

## Progress

Planned 2026-10-18.

### Step 1 done (2026-10-18)

- **Entries inside functions were already labelled.** Each generated function begins with a switch on `ctx->pc` whose labels are the points it can be entered at: resume points after calls, targets the generator found from other functions, and every instruction of a function with an unresolved `jr`. 66,784 such labels in the main program, 8,389 in the overlay. The runtime registered only function starts, so none of them could be dispatched to. `expand_startup.py` now registers each label with the function that holds it (`EeRuntime::registerInteriorEntry`; overlay entries are checked against their owner's bytes); a dispatch there runs the owner, which resumes at the label, and the owner counts as entered. 66,244 entries in the main program and 8,388 in the overlay after the map changes below.
- **Of the addresses inside functions that code builds or data holds,** all but 32 in the main program are labels or local switch targets; the 32 (most look like round constants such as `0x200000`) became entry hints (`entry-hints.json`, passed by `generate_game.py` as `entry_points`), and the generator made labels of them (21 functions changed).
- **Unmapped code:** the 16 gaps with non-zero words held 17 routines nothing names (small getters and setters, a system call stub, two larger functions) and the thread exit stub at `0x100250`, which ends in a system call that does not return (`add_entries` takes such a stub up to the next function). The map grew from 11,122 to 11,139 functions. The exception handlers the game installs with system calls 13 and 14 (`0x33AC00`-`0x33AF58`) and the unreachable word after an `ERET` (`0x338B40`) are kernel-mode code, reported and not mapped; the runtime does not handle those two system calls yet (step 4).
- **Closure check:** `map_gaps.py --check` reports nothing left in either program and is a gate step.
- **Checked:** unit tests (Python: entry labels, bindings, the new scans, the system call stub; C++: interior registration and dispatch, overlay check); the scripted fight gives the baseline hash `5a4960667d4a3dff` with 3,732 functions entered and 0 interior dispatches; with and without interior entries (`BT2_NO_INTERIOR_ENTRIES`) its state hash agrees on all 6,534 fields; the gate passed.

### Step 6 begun (2026-10-18): BT2's own state machinery

Found on BT2's code and checked in runs (scripted fight and idle runs); names stay neutral.

- **Main loop** (`0x100680`-`0x1006C0`): load the overlay (`0x1002D8(0)`), call its entry (`0x3B7FA8(0)`), which runs the menus and returns; then, if flag `0x200` of the progress structure is set, clear it and call `0x2A8A38`, otherwise call `0x103000`, which runs a battle; repeat. The same shape as Tenkaichi3Decomp's notes describe for BT3.
- **Progress structure:** pointed to by the word at `0x3876A4` (at `0x76BDC0` in both runs). Flags at `+0x18` (`0x40` on the first run, cleared by the entry after its first-run work; `0x02` while a battle runs), mode at `+0x1C`. The entry dispatches modes 1 to 64 through the table at `0x421950`; the modes fall into groups sharing a routine: 1 (`0x100D58`), 2, 4-5, 6-8 (`0x41BF08`), 10-16 (`0x3F9300`), 24-27 (`0x3CB1D0`), 30-35 (`0x418410`), 38-41 (`0x3C0700`), 43-47 (`0x3DF9E8`), 48-51 (`0x3E3B98`), 53-58 (`0x3DB6A8`), 61-64 (`0x3EC690`); the other values share a default case. Another system structure, which the battle routine works on, is pointed to by the word at `0x3876A0` (flags at `+0x4D8`).
- **Modes seen in the scripted fight:** 1 from the start through the title, New Game and the main menu; 30 in the Dueling menu (by flip 2,700); 31 in character selection (3,300-3,900); 34 in map selection (4,380); the battle (`0x103000`, from flip 4,403) runs with mode 31 and flag `0x02`. Group 30-35 is the versus-battle setup.
- **Idle demo fight:** with no input at all (the opening film plays to its end), the entry returns at flip 10,130 with flags `0x06` and mode 1 and main starts the battle routine: the game's own CPU-against-CPU demo. It hung in its first frame (no display flip after flip 10,205) in `0x145DF0`, the update of an object only the demo has (the structure behind `gp-18076`, absent in the scripted battle; called from `0x146AA0` through the object tree walk at `0x1F2F90`). It places points at random and moves each back inside a box by ±80 until a VU0 `VCLIP` against `w` = 40 reports it inside. **The generated `VCLIP` swapped the above and below bits** (and compared with `w`, not `|w|`), so a point outside was moved further out until adding 80 no longer changed it (z = 1.07e9 in the trace). Fixed in `prepare_runtime.py` (`fix_clip`; the only generated `VCLIP` site); the demo fight now plays and the game returns to the title (idle run to flip 13,000: 4,687 functions entered, nothing interpreted). Not a regression: whether the loop runs away depends on the random points, so earlier demo fights could pass. It is the first finding of the sweep approach.
- **Battle setup** (found by comparing memory between scripted runs that differ in one choice, then reading the code that writes and reads it):
  - The menus keep the selection in the progress structure: per side five slots of grid positions (`+0x390` for 1P, `+0x3A4` for the other side; the grid has 7 columns), character ids (`+0x3E0`, `+0x3F4`; Goku 0, Kid Gohan 4) and the map's list position (`+0x45C`). `0x102360` resets that block.
  - The battle reads its setup from the system structure: `0x101A10` clears it, `0x101A60` writes the settings (`+184`: kind, map, two values, 4, music; the versus menus leave kind 0 and the map id, for example 9 for Wasteland and 1 for Rocky Area), `0x101A88(side)` returns a side's record (`+224 + side × 464`), `0x101AB0(side, …)` sets its control (byte `+452`: 1 for a CPU fighter, 0 for the player), and `0x101AF8` fills a fighter (`+0`: character × 9 + variant, then values from the character table the common files load).
  - **The demo:** after the title's countdown, `0x3B9710` seeds the random numbers from the clock, picks one of 21 entries (two characters, two variants, map + 1, music) other than the previous one (`+0x20`; `+0x24` counts demos), and makes the setter calls above with kind 6; the title routine then returns 1 and the entry returns into the battle routine.
  - **What the entry's first run does that a battle needs:** mount the second archive (`0x100950(1)`, waited for by `0x100990(1)`; file numbers from 13,234 up are its entries) and load the common files (`0x102450`: files 15401, 15402 and 15404, the character table among them). Without the mount, its entries were opened in the first archive and the load retried for ever.
- **Starter built:** `BT2_START_BATTLE=<character>[:<variant>],<character>[:<variant>],<map>[,<music>]` (`runtime/probe/battle_starter.cpp`, tool runs only) replaces the overlay's entry on its first call, does those two first-run steps, makes the demo's setter calls with the given values, CPU on both sides, and the versus settings, and returns into the game's battle routine. Checked: character 0 variant 2 against 70 on map 22 loads without input, the two CPUs fight (the opponent transforms), and the battle ends by K.O. at about flip 4,000, after which the game waits in its post-battle menu (3,624 functions entered, nothing interpreted). With the demo's kind 6 the same battle ends after about 1,000 flips and returns to the title.
- **Runtime rules the starter exposed** (startup-phase special cases, generalized; the scripted fight's state hash agrees on all 6,534 fields before and after): a busy main thread receives fields before the first presented frame as after it, the runaway guard restarts its count at each field before the first frame too, the no-flip limit counts once a frame was presented or main was busy past a field, and main lowering its own priority below a ready worker yields to it as a worker's priority change already did.

### Step 4 done (2026-10-18): the work list for step 5

`python tools/ps2recomp/runtime_census.py` reads the mapped code of all programs and the runtime's sources (per-site lists in ignored `work/bt2-generation/runtime-census.json`). Measured on the map of 2026-10-18:

- **System calls:** 144 numbers have a SYSCALL site; 75 of them are library stubs nothing in the code refers to. Of the 69 the code reaches, 41 are handled by the runtime without a gate, 4 are the game's own numbers it installs with `SetSyscall` (0x56, 0x5A, 0x5B, 0x83; the runtime runs installed handlers for any number, and the report now lists them as `installed_syscalls`: 14 in the scripted fight), 5 are handled only under a gate (`GsGetIMR`, `SetVSyncFlag`, `SifSetDChain` and its interrupt form, `Deci2Call`), and 19 are not named at all:
  - threads: `ExitThread`, `ExitDeleteThread` (7 references), `DeleteThread`, `TerminateThread`, `RotateThreadReadyQueue`, `iSuspendThread`;
  - interrupt-context forms: `_iEnableIntc`, `_iDisableIntc`, `_iEnableDmac`, `_iDisableDmac`;
  - exception handlers: `SetVTLBRefillHandler`, `SetVCommonHandler` (the kernel-mode handlers of step 1; the port has no CPU exceptions to deliver to them);
  - leaving the game: `Exit`, `LoadExecPS2`, `ExecPS2` (a checked stop by name is the right answer);
  - `GetMemorySize`, and the numbers 0x7B, 0x80 and 0x82 (one reference each; not installed in the recorded run).
- **Hardware registers:** 85 addresses in 24 blocks are loaded, stored or built as pointers; the runtime names 80 of them without a gate and 5 only under gates (one GIF, two VIF1 and two DMAC registers). This is a lenient measure: an address counts as named when one condition names it without a gate, although other paths to it may be gated.
- **The runtime's gated conditions:** 163 lines compare the guest pc, a branch source or target, a startup phase, a call count or the stack offset: 77 in `ps2_runtime_syscalls.cpp`, 66 in `ps2_runtime_memory.cpp`, 7 in `ee_runtime.h`, 5 in `ps2_runtime_dispatch.cpp`, 4 in `ee_sound_dispatch.cpp`, 4 in services and VU0 support. These are the special cases step 5 turns into general handling.
- **VU1:** all 1,946 instruction pairs of `.vutext` use instructions the VU1 model implements.
- **Not covered statically:** SIF RPC requests (their function numbers are mostly built at run time), VIF codes and GS packets; the coverage files of step 6 report what the runtime refuses.

### Step 3 done (2026-10-18)

- **Handlers from the recompiler:** instead of a second, hand-written meaning of every instruction, `tools/ps2recomp/generate_interpreter.py` takes every distinct instruction word of the mapped code that is not a branch or jump (82,667 words: 203 instruction forms appear in the game, among them about 60 VU0 macro forms and 25 MMI forms), lays them out as a synthetic program of one-instruction functions, generates it with the pinned recompiler and the same preparation as the game's functions (`prepare_runtime.prepare_text`, refactored out of `prepare_runtime.main` without changing its output), and turns each body into a handler taking the program counter (41 units, local and ignored). The VU0 flag reports `route_cop2_condition` inserts between a status flag write and read are carried as handler flags and applied in execution order.
- **Core** (`runtime/ee/ee_interpreter.cpp`): branches and jumps as the generated code has them (conditions and targets read before the delay slot, 64-bit comparisons, BC0F never taken, BC1 from FCR31, BC2 from the runtime), a backward jump within the function counted as a loop iteration, calls and jumps to other functions through dispatch, `jr ra` and `ERET` ending the frame, unmapped code that runs into compiled code handing over to it. The extents of all generated functions are registered for this (`registerFunctionExtent`; the 71 functions `startup_bindings.cpp` registers got their interior entries in the same change).
- **A general fix found on the way:** the generator emits `Store128` for a quadword store whose address it knows and `WRITE128` (main RAM only) otherwise, so the same store reached a FIFO or failed depending on the context. `WRITE128` now goes through `guestWrite128`, which sends the EE register window to `Store128`, as `guestWrite64` already did for doublewords.
- **Checked:** the whole scripted fight run with `BT2_EE_INTERPRET=all` (every generated function interpreted, 2,475,969,625 instructions, 92.5 s against 32.5 s compiled) agrees with the compiled run on all 6,534 fields of the state hash, with the same VU1 hash and 3,732 functions entered; unit tests for the core (unmapped code, a counted loop, calls, running into compiled code, `ERET`, the explicit stops, a function within its extent) and for the handler generator; the normal run interprets nothing.
- **Tooling:** the report lists `ee_interpreted_entries`; `discovery_loop.py` maps them (also from the log of a played session) and regenerates the handlers when the map gains words; the battle run fails when anything was interpreted; the game program tells the player when a session ran untranslated code and names `discover.cmd`.

### Step 2 done (2026-10-18)

`BT2_STATE_HASH=<file>` writes, at every VBlank, the hash of EE RAM, the scratchpad, VU0 data memory and VU1 data and program memory (per 4 KB page, then the field's hash of the page hashes); `BT2_STATE_HASH_PAGES=<field>` writes every page hash at that field. `tools/ps2recomp/compare_state_hash.py` names the first field at which two runs differ and the pages that differ there. Unit tests for the hash and the comparison. Its cost per field was not measured separately (both 5,100-frame runs of the comparison above wrote it).
