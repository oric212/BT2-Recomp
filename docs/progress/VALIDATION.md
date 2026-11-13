# Validation record

What has been checked, how, and when. Only checks that were actually run are listed; anything not listed is not
validated. The current status is in [PROG.md](../PROG.md); the details of each check are in the monthly history
([November](2026-11.md), [October](2026-10.md)).

**Kinds of check**

| Kind | Meaning |
| --- | --- |
| **Original** | Compared with the original game running in PCSX2 (or an independent decoder) |
| **Exact** | Two port runs compared byte for byte (every guest memory per field, or VU1's output hash) |
| **Ran** | The run reached its end with no stop and nothing interpreted; nobody judged the picture |
| **Measured** | A number taken on the developer's PC (Core i9-10900K, Radeon RX 9070 XT) |
| **User** | The user looked, listened or played and confirmed |

## At a glance

| Area | Strongest check so far | Kind | Date |
| --- | --- | --- | --- |
| Fight logic and AI | Every replayable step of 300 consecutive fight frames equal to PCSX2 | Original | 2026-11-12 |
| 3D pipeline (VU1) | VU1 memory after a fight frame's chain equal to PCSX2 | Original | 2026-11-12 |
| Pictures (GS) | Eight fight frames, fighters and effects included: textures exact, picture within the renderers' rounding | Original | 2026-11-13 |
| Loaded data | Fighters and stage in memory equal to PCSX2 at a fight's start | Original | 2026-11-12 |
| Input delay | Same delay in fields as PCSX2; press to screen as fast as PCSX2 | Original, Measured | 2026-11-12 |
| Saves | A new save byte-identical to the one the original wrote in PCSX2 | Original | 2026-11-09 |
| Startup | 42 of 46 boot comparisons pass; 31 run in every gate | Original | 2026-10-13 |
| Films | First pictures and sound timing match | Original | 2026-10-13 |
| Whole game reachable | All modes, 200 story parts, 73 towns, 130 fighters, 16 stages run without input | Ran | 2026-10 to 2026-11 |
| Sound | Menus, fights and story heard by the user | User | 2026-10-13 |
| Online | Played between two homes | User | 2026-11-09 |

## Against the original (PCSX2)

### How the comparison works

Running both programs side by side does not work: the port loads faster, so the two drift apart within seconds.
So the port is **started from PCSX2's own state** ([plan](../plans/validation/PCSX2_COMPARISON_PLAN.md)):

1. PCSX2 runs the game with **checkpoints**: chosen calls where the game pauses while a tool copies out its memory,
   registers, VU and VIF1 state and, when asked, a save state (GS memory).
2. The port loads that state, runs the same function, and the results are **compared byte for byte**.
3. Where the timing itself is the question, **logging sites** record without pausing, and a black-box tester
   measures press to screen on the real display.

Every difference is therefore the port's own, and each one found is either fixed or explained below.

### Results

| What | Compared | Result | Date |
| --- | --- | --- | --- |
| Fight logic and AI | The logic step `0x102AA0`, its 34 calls, their 148 callees, the AI's `think`; 292 calls in 5 sessions | Equal, after 3 fixes (below) | 2026-11-12 |
| Fight logic over time | Frames 600 to 899 of a versus battle, every replayable call of the fight loop (1,500 calls) | 1,459 equal; 41 stop at the sound requests' system call (a service, compared separately) | 2026-11-12 |
| Draw step | `0x102C40` at frames 150, 450, 750 and every 600 frames of a full battle | Equal | 2026-11-12 |
| Loaded data | All 32 MB of memory at the first frame of the same battle | Fighters and stage (22 MB) equal but 2.7 KB of positions and leftovers; the rest explained (below) | 2026-11-12 |
| VU1 | VU1 memory after the fight frame's VIF1 chain, frames 300, 600, 900 | Equal (16 KB), after 1 fix; also against PCSX2's VU1 interpreter | 2026-11-12 |
| GS memory | Fight frames 300, 900, 1500, 1800, 2100, 2400, 2700, 3000: GS memory after the frame, PCSX2's software renderer on one thread | Textures, palettes, uploads equal; depth equal; picture: 36 to 59% of pixels equal, most others 1 to 2 levels apart, 1 to 53 of 229,376 more than 16 (thin lines); the port 0.1 to 0.5 levels brighter on average, up to 16 where glows stack; the blur buffers up to 7 levels apart | 2026-11-13 |
| Services | Every SIF RPC request and synchronous reply from boot to the title demo, the same card's save | Memory card (the save read), disc control, CD init, controller and IOP heap equal; LOADFILE, FILEIO, CRI, sound driver, SOUNDS and disc reads differ, not yet explained (timing or the port's replies) | 2026-11-13 |
| Input delay in fields | A press at fight frame 480 (Cross, d-pad up) | The display lists change in the frame that read it, VU1 a frame later: same in both | 2026-11-12 |
| Press to screen | 30 unpauses per setup, windowed, screen read every 7 ms | PCSX2 108 ms; port 104 ms (Low input lag, default), 111 ms (60 fps), 133 ms (Low input lag off); 105 and 117 ms with HD, 16x, widescreen | 2026-11-12 |
| Saves | A new save written by the port's title New Game | Byte-identical to the original's | 2026-11-09 |
| Startup | 46 scripts at points of the boot | 42 pass; `cri_rpc`, `second_heap`, `cri_preemption`, `spu2_initialization` fail | 2026-10-13 |
| Films | Logo and opening films | First pictures match an independent decode; sound level, channels and timing match a PCSX2 recording | 2026-10-13 |

### Faults found and fixed

| Fault | Effect in the game | Fixed |
| --- | --- | --- |
| `SQRT.S` took the root of the wrong register (110 of 300 sites) | Wrong rotations from packed quaternions in the fight | 2026-11-12 |
| `SQRT.S` rounded toward zero; PCSX2 rounds to nearest | Last-bit differences in the fight's floats | 2026-11-12 |
| `MOVN`/`MOVZ` copied 128 bits instead of 64 (4,657 sites) | Upper register halves wrong after many calls | 2026-11-12 |
| VU1: in an instruction pair that loads I, the other instruction saw the new I; it must see the old one | Triangles clipped at the screen's edges were pushed out from a wrong centre | 2026-11-12 |

### Differences that remain, with their reason

| Difference | Reason |
| --- | --- |
| Picture pixels 1 to 2 levels apart; rare larger ones on thin lines; the port slightly brighter where additive layers stack | paraLLEl-GS rounds some interpolation, blending and filtering differently from PCSX2's software renderer (documented as not bit-exact); which agrees with the console is not known |
| Memory below `0x100000` | The kernel: PCSX2 runs the BIOS's, the port answers it natively |
| Stream and display-list buffers, stack, counters | Timing and route: the port loads faster and its starter skips the logos, films and title |
| 23 KB of game globals | Same route difference; not explained one by one |
| Sticks at rest `0x7F` (PCSX2) against `0x80` (port) | Each emulator's rest value; the fight is the same with either (1,427 frames compared) |

### Not compared with PCSX2 yet

- Whose rounding the console has: single draws (a modulated texture, an additive blend, a bilinear sprite) on both renderers.
- Other screens' logic and pictures: menus, character select, the story map, towns.
- The services' differences: LOADFILE's load reply, FILEIO's seeks, CRI, the sound driver, SOUNDS, the disc's reads (timing or the port's replies).
- Fullscreen press to screen; press to sound.

## The port against itself (Exact)

| Check | Result | Date |
| --- | --- | --- |
| **Gate**, every commit: Debug and Release builds, unit tests, Python suites, probe runs, 31 comparisons with the original, map closure, the scripted 1P-vs-COM fight against its baseline (VU1 hash `aeff76be1011da3b`) | Passed | 2026-11-12 |
| Native services against the emulated IOP | Same stop and memory, except two regions of stale IOP bytes | 2026-10-11 |
| Interior entries on and off; every function interpreted against compiled; the battle and screen starters' runtime rules | Same state on all 6,534 fields of the scripted fight | 2026-10-18 to 19 |
| Native VU1 programs against the VU1 interpreter (`BT2_VU1_VERIFY`) | 9,061,301 starts equal over a 2,000-flip fight | 2026-11-12 |
| 60 fps in-between pictures (`BT2_SIXTY_FPS=check`) | Pixel-equal to the next main picture at flips 700, 999, 1,200, 3,300 (also 16x, HD, widescreen); Vulkan validation silent | 2026-10-26 |
| Online lockstep over loopback | 84 drift checks alike, audio byte-identical | 2026-11-04 |

## Coverage runs (Ran)

Each ran to its budget with no stop and nothing interpreted.

| Area | What ran | Date |
| --- | --- | --- |
| Fighters | Characters 0 to 10 and 100 to 129 in CPU battles, 4,500 flips each (from 130 not valid) | 2026-10-18 |
| Stages | All 16 stages, 4,500 flips each | 2026-10-19 |
| Story | All 200 startable parts to their battle's end (70 cleared, 130 lost by the CPU); all 73 optional markers: 32 towns, the shop, 41 fights | 2026-10-19, again 2026-11-02 |
| Story details | 621 scene scripts decode; battle rules 0 to 3, saga items 392, 395, 401, 408, 411 unlocked, objects broken in 112 parts, a Dragon Ball picked up | 2026-10-19 |
| Main menu | All ten screens; 1,476 button paths through eight of them | 2026-10-19 |
| Other modes | Ultimate Battle Z: all 15 courses (175 battles); Dragon Tournament to Champion and 130 matches; Ultimate Training: all 21 lessons | 2026-10-20 |
| New Game and Item Fusion | New Game's unlocks; fusion of two items and of a character item; a wrong pair refused | 2026-10-19 |
| Idle demo fight | Plays and returns to the title (flip 13,000) | 2026-10-18 |
| Kernel generalizations | Semaphores, GS reset path, thread calls, thread exit: unit tests, gate comparisons and battle hash unchanged | 2026-10-19 |

## Graphics and enhancements

| Check | Result | Kind | Date |
| --- | --- | --- | --- |
| HD textures (pack 2) | 57,784 draws replaced in 2,000 flips, no renderer errors; names match PCSX2's texture and palette hashes | Ran | 2026-10-20 |
| HD texture names, PCSX2's scheme | 22.2% of a versus fight's draws found (4.5% before) | Ran | 2026-10-21 |
| Super-sampling 4x and 16x | 1,280 × 896 pictures, no renderer errors | Ran | 2026-10-20 |
| Widescreen | Wider 3D view at the same proportions; the fight itself unchanged (same moves, hits, health, positions at 4 points over 5,000 flips) | Ran | 2026-10-20 |
| 60 fps battles | 30 flips and 60 pictures a second, also with 16x, HD, widescreen | Ran | 2026-10-21 |
| Pictures in the window | Super-sampling 1 to 16 and pack 2, no errors, 30 pictures a second | Ran | 2026-10-20 |
| Settings | Picture, enhancement and launcher settings read back, wrong values refused | Unit-tested | 2026-10-20 |

## Sound

| Check | Result | Kind | Date |
| --- | --- | --- | --- |
| Menus and a fight | Heard and confirmed | User | 2026-10-13 |
| Channel balance | No 0.1-second stretch with one side far below the other (boot, menus, Ultimate Battle Z, story) | Ran | 2026-10-20 |
| One-channel streams | Heard on both sides, left and right within 1% | Unit-tested, Ran | 2026-10-20 |

## Saves, launcher, online

| Check | Result | Kind | Date |
| --- | --- | --- | --- |
| New save | Byte-identical to the original's (see above) | Original | 2026-11-09 |
| The user's 100% save | Loads from a card folder | User | 2026-10-13 |
| Launcher folders and screens | Folders made once and never emptied; discs and cards found; game and launcher start | Ran | 2026-10-19 |
| Online between two homes | Connected without an open port and played | User | 2026-11-09 |

## Measured performance

| Measurement | Value | Date |
| --- | --- | --- |
| Fight speed | 29.97 flips a second with every enhancement, super attacks included | 2026-11 ([PROG.md](../PROG.md)) |
| Processor and memory, paced fight | 0.42 cores and 680 MB plain; 0.55 cores, 843 MB with 16x, HD, widescreen | 2026-10-20 |
| GPU time a frame | 5.9 ms plain, 7.7 ms at 16x, 10.1 ms with everything | 2026-10-20 |
| Fast forward | 48.2 flips a second (normal 30.0) | 2026-10-20 |

## Confirmed by the user

- **Pictures** (2026-10-13): demo fights, title dialogues, main and Dueling menus, character and map selection,
  the loading screen, a battle.
- **Sound** (2026-10-13): menus and a fight.
- **Saves** (2026-10-13): the user's 100% save loads.
- **Online** (2026-11-09): played between two homes.

## Not validated yet

- **Against PCSX2:** see "Not compared with PCSX2 yet" above.
- **Coverage:** characters 11 to 99, variants 1 to 8, all character and stage pairings (the exhaustive battle sweep);
  screens beyond their first page; story parts after a lost battle; the chase itself; buying in the shop; Dragon
  Balls inside a fight; sagas unlocked by a fight's condition.
- **In play, by the user:** special moves and ultimates, live sound in play, the second controller, Variable
  refresh rate, beam struggles by buttons, Low input lag, the enhancements on the RX 9070 XT (see
  [the handoff](ACTIVE_PASS.md), "Waiting for the user").
