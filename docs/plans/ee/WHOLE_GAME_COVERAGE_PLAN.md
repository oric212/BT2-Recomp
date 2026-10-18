# Whole-game coverage

**Goal:** the whole game recompiled and running without stops, found by work that does not depend on someone steering the game to each state. Decided by the user on 2026-10-18: playing or scripting button input to find what is missing does not scale (too many states, different buttons in each). This plan goes ahead of the [performance plan](PERFORMANCE_PLAN.md).

**The user's decisions:** an EE interpreter is accepted as a logged bridge for code the map does not hold yet (compiled natively at the next build); a tool-only starter of game states is accepted (never in the player programs); exhaustive sweeps are the final validation, not the work now (2026-10-19).

References consulted (BT3-Recomp's interpreter and orchestrator, Tenkaichi3Decomp's headless starter and per-blank state hash): [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#whole-game-coverage-2026-10-18).

## Steps and status

| Step | What | Status |
| --- | --- | --- |
| 1. Close the map statically | Every address the code calls, jumps to, points at or builds is a function or a registered interior entry; `map_gaps.py --check` is a gate step | Done 2026-10-18 (11,139 functions + 494 in the overlay) |
| 2. State hash per field | `BT2_STATE_HASH` and `compare_state_hash.py`: the first field at which two runs differ; checks that a runtime change alters nothing | Done 2026-10-18 |
| 3. Interpreter bridge | Unmapped code runs through handlers the recompiler makes from each instruction word, is logged and mapped by `discovery_loop.py`; gate runs must interpret nothing | Done 2026-10-18 (a fully interpreted scripted fight equals the compiled one) |
| 4. Census | `runtime_census.py`: the system calls, hardware registers and VU1 instructions the code uses against what the runtime handles; the work list for step 5 | Done 2026-10-18 |
| 5. General runtime handlers | Replace the runtime's special cases (guest PC, stack offset, call count, startup phase) by general behavior, as runs reach them; the scripted fight's state hash must stay identical | In progress: 6 generalized so far (VBlank fields for a busy main before the first frame, the runaway guard per field, the no-flip limit, main's priority change, `SetVSyncFlag`, INTC_STAT) |
| 6. Game states without input | Starters (`runtime/probe/game_starter.cpp`) for battles, screens and story episodes; sweeps; then the final validation | In progress: battles and screens done, story mode next ([story mode plan](STORY_MODE_PLAN.md)) |

What each step was checked on: [VALIDATION.md](../../progress/VALIDATION.md). How it was done: [the October history](../../progress/2026-10.md).

## Step 5: what is left

From the census (2026-10-18): 163 runtime conditions on guest PCs, stack offsets, call counts or startup phases; of the 69 system calls the code reaches, 5 handled only under a gate and 19 not at all (thread exit and deletion, interrupt-context forms, exception handler setup, leaving the game, `GetMemorySize`, three unnamed numbers). Most are startup replays that work as they are; each is generalized when a run reaches it, with a unit test. The unhandled thread calls are reached only through thread teardown (the sound library's shutdown at `0x315248`, a worker's exit at `0x31EB88`).

## Step 6: BT2's state machinery (the starters rely on it)

- **Main loop** (`0x100680`): load the overlay, call its entry `0x3B7FA8` (title and menus); when it returns, run `0x2A8A38` (story) if progress flag `0x200` is set, else the battle routine `0x103000`.
- **Progress structure** (pointer at `0x3876A4`): flags `+0x18` (`0x40` first run), mode `+0x1C` (modes 1 to 64, table `0x421950`), story episode `+48` and part `+52`. The main menu's ten entries set modes 6/7 (Dragon Adventure), 10, 24, 30 (Dueling), 61, 53, 5, 38, 48, 43 (table `0x4217B0`).
- **The entry's first run** that later states need: mount the second archive (`0x100950(1)`, wait `0x100990(1)`; files from 13,234 up) and load the common files (`0x102450`; the character table among them). The card check, logos and films are skipped by the starters.
- **Battle setup** (system structure, pointer at `0x3876A0`): `0x101A10` clears it; `0x101A60(kind, music, …, stage)` (kind 0 versus, 6 demo; the stage passes through `0x24AF50`); `0x101AB0(side, …)` sets control (byte `+452`: 1 CPU); `0x101AF8` a fighter (character × 9 + variant). The title's demo picker `0x3B9710` makes the same calls from a 21-entry table.
- **Ranges:** characters 0 to 129; stages 0 to 15 (from 16 the game shows Wasteland); variants 0 to 8 (not every character has all).

## Limits

- The interpreter runs only code the map does not hold; it is not a fallback for refused requests, and a gate run that interprets anything fails.
- No fixed game addresses in general handlers; the starters are the one place that writes the game's own fields, in tool runs only.
- Sweep results, pictures and address notes stay in ignored `work/`; documents record counts and descriptions only.
- A new subsystem reached by a sweep (such as the story script machine) gets its own plan before work on it.
