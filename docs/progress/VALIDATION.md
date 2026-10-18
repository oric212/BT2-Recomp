# Validation record

What has actually been checked, how, and when. Only checks that were run are listed; anything else is not validated. Details are in the [monthly history](2026-10.md); the current status is in [PROG.md](../PROG.md).

**Kinds of check:** *Original*: compared with the original game (PCSX2 captures or an independent decoder). *Exact*: byte-exact comparison between two port runs (state hash of every guest memory per field, VU1 output hash). *Ran*: the run reached its frame budget with no stop and nothing interpreted; the picture was not judged by the user. *User*: the user looked or listened and confirmed.

## Checked

| Area | What was checked | Kind | Date |
| --- | --- | --- | --- |
| Gate (every commit) | Debug and Release builds, 21 unit tests, the Python suites, the probe runs, 32 comparisons and renders, map closure, and the scripted 1P-vs-COM fight against its baseline (5,100 frames, VU1 hash `5a4960667d4a3dff`) | Exact, Original | last passed 2026-10-18 |
| Startup | 46 comparison scripts against PCSX2 captures: 42 pass; `cri_rpc`, `second_heap`, `cri_preemption` and `spu2_initialization` fail | Original | 2026-10-13 |
| Native services | The same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes | Exact | 2026-10-11 |
| Movies | The logo and opening films' first pictures match an independent decode; the boot films' sound level, channel order and timing match a PCSX2 recording | Original | 2026-10-13 |
| Interior entries | The scripted fight with and without them: same state on all 6,534 fields | Exact | 2026-10-18 |
| Interpreter | The scripted fight with every function interpreted against compiled: same state on all fields, same VU1 hash | Exact | 2026-10-18 |
| Runtime rules for the battle starter | The scripted fight before and after the change: same state on all 6,534 fields | Exact | 2026-10-18 |
| Idle demo fight | Plays (after the `VCLIP` fix) and returns to the title; run to flip 13,000 | Ran | 2026-10-18 |
| Battles without input | Characters 0 to 10 (each against the next) and 100 to 129 (against 0), variant 0, Rocky Area: each ran 4,500 flips, nothing interpreted. Characters from 130 are not valid | Ran | 2026-10-18 |
| Stages | Goku against character 1 on stages 0 to 15 (16 distinct stages): each ran 4,500 flips, nothing interpreted; stage numbers from 16 give Wasteland again | Ran | 2026-10-19 |
| Screens without input | The ten main-menu screens started by mode (`BT2_START_SCREEN`): Dragon Adventure intro, Ultimate Battle Z, Dragon Tournament, Dueling, Ultimate Training, Evolution Z, Item Shop, Data Center, Options, Dragon Library; each ran 1,200 flips, nothing interpreted | Ran | 2026-10-19 |
| Story episode starts | Episodes 0 to 24 started by `BT2_START_EPISODE`: 24 load their 3D map with their own lead character and ran 1,500 flips, nothing interpreted; **episode 23 draws no 3D** (grey screen, part of the map's interface; open). From 25 the game reads past its tables (28 and up stop presenting frames). Story battles not reached | Ran | 2026-10-19 |
| Runtime rules for the screen starter | The scripted fight before and after the general SetVSyncFlag and INTC_STAT rules: same state on all 6,534 fields | Exact | 2026-10-19 |
| Frames | Demo fights, title dialogues, main and Dueling menus, character and map selection, loading screen, battle | User | 2026-10-13 |
| Sound | A WAV of menus and a fight | User | 2026-10-13 |
| Saves | The user's 100% save loads from a card folder | User | 2026-10-13 |

## Not validated yet

- Characters 11 to 99, variants 1 to 8, other character and stage pairings (left for the final validation); the screens beyond their first page (they wait for input); story battles.
- 3D frames, menu frames, memory card replies and GS readback pixels against the original.
- Writing a save; special moves and ultimates in play; live sound in play.
