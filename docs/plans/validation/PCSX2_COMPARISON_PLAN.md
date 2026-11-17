# The comparison with PCSX2

This page explains how the port was checked against the original game running in PCSX2, and what the check found.

The method: PCSX2 runs the original game and pauses at a chosen moment. The port loads PCSX2's exact state from that
moment and runs the same piece of game code. Then the memory and registers both leave behind are compared byte for
byte.

The comparison started on 2026-11-12 at the project owner's request. It is the comparison half of the "final
validation" milestone ([implementation plan](../../project/IMPLEMENTATION_PLAN.md), step 6). The day-by-day detail
is in the [record](#record-by-date) at the end.

## At a glance (2026-11-16)

**The comparison was closed on 2026-11-16 by the project owner's decision**, because the original plan went further
than the port needs. What it set out to show has been shown:

- A fight runs exactly as in PCSX2, frame by frame.
- The menus compared so far (main menu, Dueling, character selection) run exactly as in PCSX2.
- Everything the game loads, and every request it makes to the PS2's services, is the same as in PCSX2, or the
  difference is explained.

Dropped from the plan: the graphics and memory of the other screens, the services after the title demo, Ultimate
Battle Z, the tournament, and more input-lag measurements. Two small items remain, both optional: comparing the
story map and a town frame by frame, and adding the stored fight comparisons to the gate.

| Step | What was compared | Status | Result |
| --- | --- | --- | --- |
| 1. Harness | Sessions, checkpoints, replays | ✅ Done | Many sites per run; button scripts; sites in the overlay |
| 2. Fight frame | The fight's logic and draw steps | ✅ Done | Every call equal, at every frame compared |
| 3. Trajectories | 300 consecutive fight frames | ✅ Done | All 300 equal; a full versus battle to its K.O. |
| 4. Other screens | The menus' own per-frame steps | ✅ Done | Main menu, Dueling, character selection: 27 calls, all equal |
| 5. Loaded data | All RAM at a fight's first frame | ✅ Done | Fighters and stage byte-equal; every other difference explained |
| 6. VU1 | A fight frame's VU1 programs and memory | ✅ Done | VU1's memory equal after every chain |
| 7. GS and pictures | Graphics memory and the drawn fight frame | ✅ Done | Textures and uploads exact; pictures within the renderers' rounding |
| 8. Services | Every request to the disc, sound, card and controller services | ✅ Done | Boot to the title demo: equal or explained |
| 9. Input lag | The game's own delay; press to screen | ✅ Done | Delay equal; press to screen 104 ms (port) against 108 ms (PCSX2) |
| Optional | The story map's and a town's logic; the fight comparisons in the gate | ⏳ Not started | — |

**Beyond PCSX2:** the project owner has also played the port on several PCs besides the development machine, and in
online battles between two homes.

### Faults found and fixed

| Found by | Fault | Fix |
| --- | --- | --- |
| Step 2 | `sqrt.s` took the root of the wrong register | The recompiler's rewrite reads `ft` |
| Step 2 | `sqrt.s` rounded toward zero; PCSX2 rounds to nearest | FPU and VU0 roots apart |
| Step 2 | `movn` / `movz` copied 128 bits; the EE copies 64 | 4,657 sites rewritten |
| Step 6 | VU1: a pair with the I bit gave its upper instruction the new I | Interpreter, translator and hand-written clipper |
| Step 8 | LOADFILE numbered modules from 1; PCSX2's IOP from 0x19 | Module IDs from 0x19 |
| Step 8 | CRI's handles were 0xEF0D0 too high | The original's work-area base |
| Step 8 | FILEIO's first file descriptor was 1 | The lowest free slot from 2 |
| Step 5 | The film decoder rounded as the MPEG specification, PCSX2 otherwise | PCSX2's integer IDCT and rounding (the user's choice) |

### Differences that are not faults

- **Pictures:** the port's renderer (paraLLEl-GS) and PCSX2's software renderer round texture filtering and
  blending slightly differently. Most pixels are equal or one shade apart; where glows stack up, the port's picture
  is a little brighter. Which of the two rounds like a real PS2 is not known.
- **Timing:** the port loads faster than PCSX2. From boot it reaches a fight 600 fields (about 10 seconds) sooner.
  So wherever each program reached a moment at its own pace, frame counters, random numbers and stream buffers
  differ.
- **The PS2's system software:** PCSX2 runs the console's own kernel and IOP modules, while the port answers their
  requests itself. Their memory therefore differs: the system's own data, leftovers on other threads' stacks, IOP
  addresses, and bytes the game never reads.
- **The analog stick at rest:** PCSX2 reports `0x7F` and the port `0x80`. Measured: the game treats both the same.

## How it works

**Why not just run both programs side by side?** Loading and the PS2 services take different amounts of time in the
two programs, so frame counters and random numbers drift apart within seconds of the boot. Instead, **the port
starts from PCSX2's state**. Then nothing can drift, and every difference found is the port's own.

- **A session** is one PCSX2 run with **checkpoints** (`tools/pcsx2_compare/session.py`): call sites in an
  instrumented copy of the executable whose stub publishes every register, waits while the host reads RAM, VU
  memory and (when asked) a save state for GS memory, and continues. Sites can be any call, any function's entry, a
  function's every call (`--callees-of`), and since 2026-11-16 also code in the overlay `DBZP.BIN`.
- **Steering:** `--set` changes a call's arguments without stopping (the title's demo becomes a versus battle);
  `--pad-script` presses buttons in the controller's raw data, so a session reaches menus and screens by itself.
- **The replay** (`BT2_REPLAY_CALL`, `compare_calls.py`): the port loads a checkpoint's state, runs the same function
  and writes what it leaves. A call that reaches a system call stops at that boundary by design.
- **The verdict:** the words the port wrote equal PCSX2's after the same call, and the return registers equal.
  Words only PCSX2 changed (interrupts and other threads while it waited) are listed apart.
- **Other comparisons:** whole RAM by region (`loaded_data.py`), GS memory and pictures (`gs_compare.py`), the
  service log (`rpc_compare.py`), timing logs (`input_delay.py`, `press_to_screen.py`).
  The [tools guide](../../guides/TOOLS.md#comparing-with-pcsx2) has the commands.

## Steps

As planned on 2026-11-12. Closed on 2026-11-16 with steps 4 to 9 done on the screens and moments listed in
"At a glance"; the wider scope below (other screens, step 10) is no longer planned.

1. **The harness:** sessions, checkpoints, replays, comparisons.
2. **The fight frame:** each call of the fight loop's logic and draw steps replayed at frames spread over a battle;
   a step that differs is split into its callees until the fault shows.
3. **Trajectories:** every frame of a long stretch, to claim "the fight plays as the original's" frame by frame.
4. **Other screens' logic:** the same for the menus, character and stage selection, the story map, a town,
   Ultimate Battle Z's course logic and the tournament's draw.
5. **Loaded data:** all of RAM at events that follow a load (a fight's first frame, a story map shown, a menu
   entered), by region; regions that differ for a known reason are listed once
   (`config/pcsx2_compare_regions.json`), anything else is a finding.
6. **VU1:** a frame's VIF1 chain run through the port's VIF1 and VU1 from PCSX2's state; VU1's memory compared.
7. **GS memory and pictures:** GS memory from PCSX2's save states against the port's renderer after the same
   frame: textures and palettes byte for byte, the frame by how far pixels differ.
8. **Services:** every request and synchronous reply of the game's IOP services, logged in both programs.
9. **Input lag:** the game's own delay in fields (logging sites, a scripted press), press to screen in milliseconds
   (one black-box tester for both programs), and if wanted press to sound.
10. **In the gate:** every comparison whose captures exist locally runs in the gate from stored captures, without
    PCSX2, as the startup comparisons do.

Each fault found gets its fix and a test of its own; the gate's battle baseline is rerecorded when the fight changes.

## Record, by date

**Step 1, the harness (2026-11-12).** One session took the AI's `think` calls 200, 550 and 900 and the fight's
logic step `0x102AA0` (fight loop `0x102E80`, the one the title's demo battle runs) at frames 150, 450 and 750, all
before and after, in 232 s (the old captures took about 200 s each). Replayed in the port: the three `think` calls
write exactly PCSX2's words (as in November; 550 is new), so one run gives what three did. PCSX2 needs the stub's
data on a page of its own: it write-protects the pages of code it has compiled.

**Step 2, the fight's logic step, split down to its faults:** the step differed in about 500 ranges per frame, each
a float's last bits. Its 41 calls (34 reached by the demo battle) taken in one session: 29 equal on both frames
taken, 5 different. Their 148 callees: 145 equal, 3 different (`0x25F2A0`, `0x26A3B0`, `0x23DE40`); theirs: 2
different (`0x25ED10`, `0x26BCB0`); theirs: `0x29A840`, the unpacking of a 20-bit-per-component quaternion, whose
fourth component is `sqrt(1 - x² - y² - z²)`. Three faults found:

1. **SQRT.S read the wrong register.** The EE takes the root of the `ft` field; the recompiler read `fs` (0 in the
   game's words), so 110 of the 300 `sqrt.s` sites took the root of `f0`. `tools/ps2recomp/ee_rewrite.py` (the
   post-generation rewrite, until now `float_rewrite.py`) now writes `ft`, checked against each word.
2. **SQRT.S rounds to nearest** in PCSX2 (its divide rounding mode covers the root), VU0's root toward zero (as
   November found). The quaternion's root, recomputed step by step from the captured inputs, gives PCSX2's word only
   so (`0x3F5D8EBF` → `0x3F6E2830`; chopped `…282F`). `ps2_fpu_sqrt` (SQRT.S) and `ps2_vu_sqrt` (VU0) apart now.
3. **MOVN and MOVZ copied 128 bits;** the EE copies the low 64 and keeps the upper half (v0's upper half differed
   after every compared call). 4,657 sites rewritten as 64-bit copies.

**After the fixes every compared call is equal:** 292 calls in five sessions, among them the whole logic step at
frames 150, 450 and 750 (about 10,000 words written each), its 34 reached calls, their 148 callees, the AI's
`think` at 200, 550 and 900, the 4x4 matrix product `0x29DEB8` and VU0's `vsub`, `vmula`/`vmadd` blend. The gate's
scripted fight goes slightly differently (VU1 hash rerecorded; the same VU1 work). Not reached by the title's demo
battle: 7 of the step's calls (`0x102B58` to `0x102BA0`) and other fight loop `0x102F30`.

**The rest of the fight frame (2026-11-12, later):** the draw step `0x102C40` equal at frames 150, 450 and 750
(16,000 to 30,000 words of the frame's lists each) and its 16 reached calls. The fight loop's other calls: three
equal (`0x2A8678`, `0x297978`, `0x1057F8`); four stop at their boundary, by design: `0x2711A0` and `0x28B090`
(hardware registers), `0x28BDA0` (the pad read's system call), `0x2711E8` (the frame end's VBlank wait). These are
the services' and timing steps' to compare.

**Steering a session:** sites at a function's entry (`%<function>`, any caller: the demo picker is in the overlay)
and `--edit` (registers changed at a checkpoint; the stub loads them back). `battle_set` (`0x101A60`) given the
versus kind (`a0=0, a2=0, a3=1`, as the port's starter) makes the title's demo a full battle. PCSX2's null renderer
stopped at an assertion (`IsCoverageAlphaSupported`, a dialog) once that battle drew; sessions use the software
renderer now. The fighters stay the demo's (the picker also passes three of its character's values on the stack).

**A full versus battle (2026-11-12):** the demo turned into a versus battle by `--edit` ran to its K.O. in PCSX2
(seen by the user: won, then the after-battle screen waiting for player 1). The logic and draw steps every 600
frames from 600 to 6,000: all 20 equal (3,000 to 45,000 words written each). Sessions end with `--until SITE` and
minimize PCSX2's window (the software renderer shows one).

**Step 3, consecutive frames (2026-11-12).** One session took frames 600 to 899 of that versus battle without a
gap: every call of the fight loop `0x102E80` the replay can run (`0x2A8678`, `0x297978`, `0x1057F8`, the logic step
`0x102AA0` and the draw step `0x102C40`), before and after, 3,001 checkpoints in 644 s. Checkpoints now keep the
4 KB pages changed since the previous one (`ram_store.py`; about 110 of 8,192 pages a call, a whole RAM every 64th):
2.3 GB instead of 96. Replayed: 1,459 of the 1,500 calls equal; the other 41 are `0x297978` reaching system call
0x77 (`SifSetDma`, its sound requests to the IOP), a service boundary for step 8, listed as such. One finding, in
the replay, not the port: the logic step differed at frame 600, the one frame of the 300 that drew random numbers
(a burst of ten particles, each at angle −π in the port). The game's random numbers are VU0's R register (`VRNEXT`);
the replay loaded PCSX2's R into the context's unused field instead of the runtime's (`EeRuntime::vu0_random_`).
With R loaded and saved where the runtime keeps it, and compared, all 300 frames are equal.

What this shows: from PCSX2's state at any of 300 consecutive frames, every step of the fight frame the game's own
code computes gives PCSX2's words. A free run of the port over those frames differs from PCSX2 only through what
the replay does not cross (the pad read, the VBlank wait, the SIF DMA, hardware registers), which steps 8 and 9
compare; chaining the port's own results from frame to frame adds nothing while every frame is equal.

**Step 9, first part: the game's own delay (2026-11-12).** A checkpoint's wait lets PCSX2's VBlanks pass, so the
fields are measured with **logging sites** instead (`session.py --log`): after each call the stub writes the call's
number, COP0 Count (the EE's cycles), two words and three checksums of RAM ranges to a ring and goes on at once, and
it can press a button itself by ORing it into the word the game reads (player 1's buttons in the pad library's
record, `0x3B0C2C`, after the pad update `0x28BDA0` returns; the fight's reader `0x207630` takes them from there).
The battle is set up by `--set` (registers changed inside the stub at a function's entry, without waiting): the
title's demo turned into a versus battle with side 0 on the controller (`battle_set` `0x101A60` as before,
`side_control` `0x101AB0` third argument 0, `fighter_set` `0x101AF8` CPU level 0, the port starter's settings). The
demo's fight is character 8 against 120, stage index 9 (game stage 15), music 23 (captured from the setters).
Logged per fight frame: the pad update (cycles, buttons) and the frame end (`0x2711E8`'s return: cycles, checksums
of the two display-list buffers the frames alternate between, `0x430000` and `0x530000`, 160 KB each, and of VU1's
data memory). The port logs the same at the same points (`BT2_EE_CALL_SUMS`), and `input_delay.py` compares each
run with a run without the press.

- **PCSX2 repeats itself:** every pair of sessions was identical in every frame before the press (up to 600 frames).
- **The intro ignores input:** the controlled and CPU sides split at fight frame 474 in PCSX2, 471 in the port
  (their starts differ: save data, load timing, random state); a press at frame 300 changed nothing in either.
- **A press at fight frame 480**, Cross and d-pad up each held two frames, in both programs: the display lists
  change in the frame whose pad update returned the press (at its end, 2.0 fields from the start of the read's
  field), VU1's memory a frame later (4.0 fields). Identical in PCSX2 and the port, and as the port's own
  measurement of 2026-11-11 found. PCSX2's frames are 2.0 fields apart throughout (1.97 to 2.03 by the cycles), and
  its pad update runs 0.1 to 0.2 field into a frame's first field, as the port's.
- A press at frame 600 in the port's run found the fighter under the CPU's attack and changed nothing; in PCSX2's
  run (other random numbers) it acted as at 480. The two runs' fights differ from the start, so the comparison is
  each program's own response to the same press, not the same frames.

**Not covered by this part:** from VU1's memory to the picture (the GS and the display), and the port's reading of
a press that arrives in a fight frame's second field (the game reads the pad once per frame in both programs; when
the host samples it is the port's). Both are the black-box tester's (press to screen, the second part).

**Step 9, second part: press to screen (2026-11-12).** `press_to_screen.py` starts either program in that fight
(PCSX2 through a realtime session: full speed, its default renderer, the keyboard as pad 1 with the port's keys; the
port through the probe's live play), brings its window to the front, presses Start with `SendInput` and reads the
window's middle from the screen (GDI, about every 7 ms) for the first picture that differs from the one before.
The keys reach PCSX2's game about 30 ms after `SendInput` (the button word read from its memory). Pausing is not a
clean stimulus (the fight freezes, then the menu fades in), so each sample is an **unpause**: the paused picture is
still and the first change is the game taking the press. 30 samples a setup, all seen, on this PC (Core i9-10900K,
RX 9070 XT), windowed through the desktop compositor:

| Setup (windowed) | Median | 10% to 90% | Range |
| --- | --- | --- | --- |
| PCSX2, its defaults | 108 ms | 91 to 126 ms | 90 to 139 ms |
| Port, 30 fps, Low input lag off | 133 ms | 118 to 140 ms | 112 to 141 ms |
| Port, 30 fps, Low input lag on (the game's default) | 104 ms | 91 to 119 ms | 91 to 126 ms |
| Port, 60 fps | 111 ms | 98 to 126 ms | 97 to 126 ms |
| Port, 30 fps, Low input lag, HD textures, 16x, widescreen, light depth of field | 105 ms | 92 to 126 ms | 91 to 127 ms |
| Port, 60 fps, the same picture settings | 117 ms | 104 to 132 ms | 97 to 134 ms |

With Low input lag on (the game's default) the port shows a press as soon as PCSX2 does (4 ms sooner at the median,
within the 7 ms sampling); without it about a frame later (25 ms), the wait for the field's deadline that Low input
lag removes; at 60 fps 4 ms later than PCSX2 (the in-between picture comes first). The samples fall on steps of
about 7 ms (the sampling) spread over a frame (33 ms), as the press lands anywhere in it. With the user's picture settings (HD textures, 16x, widescreen, light depth
of field) the 30 fps fight is unchanged (105 ms: the picture is drawn well within its frame) and the 60 fps one 6 ms
later (117 ms: the in-between picture costs more to draw). Not measured: fullscreen, the window at scale 4, a move in the fight (the fight's own motion hides it from this detector), sound.
A first try read the wrong part of the screen (the tester was not DPI-aware on a scaled display); the tester now
is, and saves the region it reads (`work/frames/latency/<label>-region.png`) to check.

**Step 5, loaded data: a fight's first frame (2026-11-12).** PCSX2's session `loaded` took the versus battle
(the demo turned into one by `--set`, as for the input delay; fighters 8 and 120, stage index 9, music 23) before
the first call of its logic step (`step-b` call 1, at 200.7 s); the port ran the same battle from its starter
(`BT2_START_BATTLE=8,120,9,23`, `BT2_START_BATTLE_PLAYER=1`) and wrote its RAM before the same call
(`BT2_EE_CALL_RAM`, field 531). `loaded_data.py` lists the words that differ, merged into ranges, against the regions
of `config/pcsx2_compare_regions.json` (each with its reason) and the session's patched call words.

- **803 KB of 32 MB differ; 777 KB of them in explained regions:** the kernel's memory (below `0x100000`: PCSX2's
  BIOS kernel, the port's native one; 177 KB), the two display-list buffers (the previous screen's lists: PCSX2's
  title demo, none after the starter; 50 KB each), a 416 KB streaming buffer (`0x64BBC0`, at the same heap place in
  both, full in PCSX2 at the first frame, filled in the port by frame 600: the stream's timing), two ADX stream
  buffers of a pool (CRI headers; PCSX2 and the port had different ones current), the main stack, the session's
  stubs and patched calls.
- **The fighters' and stage's data are equal:** from `0x700000` to the end of RAM (22 MB, the loaded and unpacked
  files) 2.7 KB differ, none of it file data: positions shaped (x, −10, z, 1) with counters beside them
  (`0x19CE154`, `0x19CF414`, `0x1BB2D90` to `0x1BBF3BC`; most likely objects placed with random numbers, which differ
  by route, not yet shown), and two records PCSX2 has and the port leaves zero (`0x8C6080`, `0xD23F00`), left by
  earlier screens: PCSX2's fight never writes them (frames 600 to 2,400 of the `versus` session), the port's never
  sets them (a word watch to field 800).
- **23 KB in the game's globals (`0x340000` to `0x3C0000`, 152 ranges)** differ by route: frame counters (`0x2E17`
  against `0x213`), the file service's path buffers ("cdrom0:\", the logo file's name), heap positions of the title's
  allocations, menu tables PCSX2 filled and the starter never reached. Not yet explained one by one.
- **A finding for the controller service:** the sticks at rest read `7F 7F 7F 7F` in PCSX2's pad record
  (`0x3B0B1C`) and `80 80 80 80` in the port's. Both rest values are inside a real pad's. The game treats them
  alike: the same battle with side 0 on the pad, all sticks held at `0x7F` from field 1 against at rest, gives the
  same display lists and VU1 memory in all 1,427 fight frames to field 1,500 (`BT2_EE_CALL_SUMS` at the frame's end).
  The port's value stays; the difference is the pad record's only.
- **The title's demo is not the same battle in both programs:** without the starter or `--set` (session
  `loaded-demo`, the port to field 11,201 with PCSX2's save on a folder card) 11.8 MB differ, the fighter and stage
  blocks from `0x8D0000` to `0x15EC860`: the demo draws its fighters with random numbers, which drift between the
  two. The same-battle comparison above is therefore the one that counts.

What this shows: at the first frame of the same battle, everything the loads brought in and unpacked is byte-equal
to PCSX2's; the differences are the route (the port's starter skips the logos, films and title), the streams'
timing and the kernel. **Not covered:** the route's own state, which needs the port to reach the battle by PCSX2's
route (the title's demo with the same draw); other events (a story map shown, a menu entered).

**Step 6, VU1: a frame's chain (2026-11-12).** The fight frame's VIF1 kick is `0x28B090` (call `@102ED8`): it ends
the current display list with an END tag, starts it (`0x28B318`) and switches to the other buffer. Its replay now
runs the chain through the port's VIF1 and VU1 from PCSX2's state, which needed three things from PCSX2 beyond RAM:

- **VU1's two memories,** already in each checkpoint (`vu-mem.bin`), now loaded by the replay.
- **VIF1's registers,** which a chain relies on from the one before (CYCLE, BASE, OFST, TOPS, TOP, ITOP, ROW, COL,
  MASK, MODE, DBF): the stub reads them with `lw` from `0x10003C00` to `0x10003D70`, with GIF_STAT and VIF1's DMA
  channel registers (`hardware.bin`). Without them the first replay stopped at an UNPACK with a filling write cycle
  (CYCLE at reset).
- **VU1's registers,** which the EE cannot read: `vu_registers.py` finds PCSX2's `VURegs` array in its writable
  sections by VU0's VF registers (published by the stub), takes VU1's entry beside it and checks VF0
  (`vu1-regs.bin`). They turned out not to matter for these chains.

Comparison point: PCSX2's VU1 memory once the chain has finished. With microVU it already has at the kick's "after"
checkpoint; with PCSX2's interpreter (`session.py --vu1-interpreter`) not yet, so the next call's checkpoint
(`@102EE0`, D1_CHCR idle) is the reference.

**One fault found, in the port's VU1:** an instruction pair with the I bit loads I from its lower word, and the
port let that pair's upper instruction read the new I; PCSX2, its interpreter and microVU alike, gives it the I
before the pair. Before the fix VU1's memory after the chain differed at frames 300, 600 and 900 (20, 38 and 23
ranges): the unused fourth word of the strip program's STQ quadwords, and at 600 vertex positions and the clip
polygon's data. The strip program (`1657b0`, `a728e0`, hand-written; `81d8a7`, translated) computes a clipped
triangle's centre with `ADDA` (I = 1), `MADDi` (loading I = 1/3), `MULi`: (a + b + 1·c) / 3 with PCSX2's reading,
(a + b + c/3) / 3 with the port's, so clipped triangles were pushed out from a wrong point. Fixed in the
interpreter, the translator (`vu.i` set after the upper instruction) and the hand-written clipper; the unit test
asserts the rule. After the fix VU1's whole data memory equals PCSX2's after all five chains compared (frames 300,
600, 900 against microVU; 300, 600 against the interpreter). A fight with `BT2_VU1_VERIFY=1` (2,000 flips, 9,061,301
starts, 7,124,333 of them hand-written) has every native program equal to the interpreter. The gate's battle
baseline was rerecorded (5,886 more VU1 instructions: the clipped polygons change).

**Not compared yet:** the GIF packets the chain sends (XGKICK and DIRECT): PCSX2 hands them to its GS, which a
session cannot read; step 7 compares GS memory instead. Other screens' chains.

**Step 7, GS memory and the picture: one fight frame (2026-11-12).** PCSX2's GS state comes from its save states:
the session asks for one through PINE (`pine.py`, PCSX2's local control socket, enabled in the session's profile)
while the stub waits (`session.py --save-state SITE`; `state.p2s` in the checkpoint). Its `GS.bin` (freeze version
9) holds the general registers (the environment's from offset 4, the two contexts' from 124, RGBAQ, ST, UV from
316), the transfer state, and GS memory at offset 425 (`gs_memory.py`; found from textures: read from 424 every
texel was a byte off). The port gets that state as GIF data before the replay (BT2_REPLAY_CALL's eighth part): GS
memory as one host-to-local upload (a 512 × 2048 PSMCT32 rectangle covers the 512 pages once; checked word for
word, synthetic and PCSX2's), then the registers by A+D. Its GS memory after the call is read back from
paraLLEl-GS (`<RAM out>.gs`). `gs_compare.py` compares it with PCSX2's GS memory by region (the frame and Z buffers
FRAME_1 and ZBUF_1 name, the rest) and the frame's pixels by how much they differ, and saves both pictures.

The comparison point must be the frame's end (`@102F08`): just after the chain (`@102EE0`) PCSX2's GS was still
drawing the frame's last strips (76,136 Z words changed between the two).

Fight frame 300 of the versus battle, the kick's chain replayed (2,777 VU1 starts, 3,200 XGKICKs, 3,695 draws,
2,421 with Z writes, about 130 uploads), the port's native passes off and one sample per pixel:

| Region | PCSX2 changed | Port changed | Differ |
| --- | --- | --- | --- |
| Textures, palettes, work buffers (the rest) | 42 words | 42 words | 0 |
| Z buffer | 16 | 15 | 1 |
| Frame (512 × 448) | 226,757 | 226,959 | 146,212 |

Of the frame's 229,376 pixels 83,164 are equal, 130,475 differ by one level, 14,690 by two, 1,021 by 3 to 16 and
26 by more than 16 (largest 52): the ±1 and ±2 spread evenly over the shaded surfaces (paraLLEl-GS's interpolation
rounding against PCSX2's software renderer), the larger ones on thin features (rain streaks, a few edges,
sparkles at the waterfall), where the two rasterize differently. Pictures in
`work/frames/pcsx2-compare/gs3-102ED8-300-*.png`. Everything the chain uploads and every texture-side write is
exact, and the frame is the same picture within the GS renderers' rounding. **Not compared yet:** a frame with the
fighters in view (frame 300 is the intro's stage pan), frames with the port's native passes on (they are the
port's, not the original's), super-sampling and the enhancements (not the original's), other screens.

**Step 7 at frames with the fighters and effects (2026-11-13).** The same comparison at seven more frames of the
versus battle: 900 (a fighter far off, the radar shown), 1500 (both fighters close, a combo), 1800, 2100, 2400 (a ki
aura), 2700 (a ki blast and a large glow), 3000 (a hit with its lights). Sessions `gs4` (900, 1500) and `gs5b`
(the rest), PCSX2's software renderer on one thread:

| Frame | Rest differ | Z differ | Pixels equal | by 1 | by 2 | 3 to 16 | over 16 | largest |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 900 | 0 of 0 | 7,514 of 15,671 | 85,388 | 121,985 | 21,304 | 698 | 1 | 26 |
| 1500 | 0 of 61 | 0 of 10,496 | 124,292 | 100,784 | 3,339 | 942 | 19 | 52 |
| 1800 | 0 of 118 | 4,434 of 28,061 | 121,855 | 99,954 | 6,744 | 786 | 37 | 189 |
| 2100 | 0 of 181 | 4,466 of 18,309 | 135,535 | 88,103 | 4,505 | 1,194 | 39 | 222 |
| 2400 | 3,177 of 2,348 | 4,565 of 23,934 | 109,801 | 97,530 | 15,513 | 6,502 | 30 | 61 |
| 2700 | 0 of 4,171 | 5,278 of 21,192 | 102,133 | 76,379 | 16,586 | 34,225 | 53 | 134 |
| 3000 | 0 of 74 | 5,599 of 47,198 | 91,863 | 115,291 | 16,920 | 5,285 | 17 | 138 |

("of" is the words PCSX2 changed.) What the differences are:

- **The Z buffer's pages are also a work area.** The game composes the radar in the first 32 lines of the Z buffer's
  pages (four 128 × 32 pieces, as colour) and copies them into the frame; the Z words that differ are there, a level
  or two per channel as in the frame, and on the radar's thin lines (its ring and grid fall a pixel apart). The
  depth itself is equal where only depth was written (frame 1500: none of 10,496 differ).
- **The blur pyramid (frame 2400).** The frame is filtered down to 256 × 256 (block `0x2A00`), 128 × 128
  (`0x2E00`) and 64 × 64 (`0x3EC0`), blurred back and forth between `0x3EC0` and `0x3F00` with one-texel offsets and
  added back up. Later in the frame textures are uploaded over the 256 × 256 and 128 × 128 levels, so only the
  64 × 64 buffers and the 256 × 256 level's last rows survive to the frame's end: the 64 × 64 buffers differ by up to
  7 levels in their lit lower rows (mean 0.75 in the port, 0.48 in PCSX2), the bilinear passes' rounding summed.
  That is the frame's 2,348 and 3,349 "rest" words.
- **Stacked additive layers.** Where glows overlap (2700) pixels differ by 3 to 16 levels, the port brighter; over
  each frame the port is brighter by 0.09 to 0.51 levels on average. Every texture, upload and palette word is
  equal, so this is how paraLLEl-GS and PCSX2's software renderer round blending, modulation and filtering. Which
  of the two the console agrees with these programs cannot tell; a single-draw test of each rule would.
- **PCSX2's software renderer with threads is not a reference for this.** In the first sessions (`gs5`, PCSX2's
  default threads) the radar's strips at 2400 and 3000 were drawn from the work area half done (horizontal bands of
  the background), pixels up to 245 levels apart, and two sessions with different sites gave different GS memory at
  the same frame. PCSX2 had finished (its frame did not change up to the next frame's kick, VIF1's DMA idle). With
  `extrathreads = 0` (now set by `session.py`) every radar is whole and the large differences are gone. `gif_dump_draws.py`
  now also lists a replay's draws (a dump without a final flip), which is how the targets above were found
  (`BT2_GIF_DUMP=0:2:<file>` with `gs_compare.py`'s replay).

Pictures in `work/frames/pcsx2-compare/` (`gs4-102ED8-*`, `gs5b-102ED8-*`, `gs5b-*-side.png`, `gs5b-2400-pyramid.png`).
The fight's frame is the same picture as PCSX2's at every frame compared, within the two renderers' rounding.

**The port's rounding rules (2026-11-13).** Measured by one crafted GIF packet through `BT2_REPLAY_CALL` (function 0,
no game code: a texture whose first two texels per row are pairs such as 0 and 1, 255 and 0, then a 16-pixel bilinear
sprite per row with the filter's weight stepping by 1/16): every result is the exact weighted mean **rounded to
nearest, ties to even** (0 and 1 at 8/16 give 0; 1 and 2 give 2; 255 and 0 give 128), where truncation would give
0, 1 and 127. paraLLEl-GS filters with the GPU's sampler and rounds (`roundEven(255 · c)`), blends as
`floor((A - B) · C / 128) + D` and modulates as `floor(Cf · Ct / 128)`, the GS manual's integer formulas. PCSX2's
rule could not be measured here: the pyramid's intermediate levels are overwritten before the frame's end and a
checkpoint cannot stop inside the chain. **Whose rounding the console has is not known** and not decidable here:
paraLLEl-GS's author notes that PCSX2's software renderer is not bit-exact with the console either and that no
public hardware tests of the GS's interpolation exist. The port keeps paraLLEl-GS's rules; the differences above
stay measured (0.1 to 0.5 levels a frame).

**Step 8, the services: the log and a first comparison (2026-11-13).** Every service call the game makes goes
through the library's `sceSifCallRpc` (0x332970: client a0, function a1, mode a2, send buffer and size a3 and t0,
receive buffer and size t1 and t2; the services are bound by `sceSifBindRpc`, 0x332790, client a0, service a1).
`session.py --rpc-log COUNT` patches it twice (`rpc_log.py`): its entry logs each request, and 0x332B3C, reached only
after the semaphore wait of a synchronous call that was sent, logs the reply (s1 the client, s6 the function, fp the
mode, s4 the receive buffer, the packet's +44 its size). Each record holds the function, client, mode, the buffer's
size, address and checksum; the stubs use only registers the function does not hold there, so calls on any thread
are logged, and the host drains the ring while PCSX2 runs (37,311 records from boot to the title demo's first
fight frame, none lost). The reply mark is bit 31 of the mode, as the controller's function numbers use bit 31
(0x80001363). The port's side is `BT2_RPC_LOG=<file>` (`runtime/probe/rpc_log.cpp`): the same function runs there
as recompiled code, so the same points are logged (its call from 0x332B34 right after the wait), and binds as `B`
lines name each client's service. `rpc_compare.py` compares the two per client in order (the programs' timing
differs, each service's order does not).

Session `rpc2` (PCSX2, boot to the demo's first fight frame) against the port booted with the same card's save
(`card-pcsx2`, no input, 3,000 flips; without the save the game polls the card and the route differs):

| Service | Calls compared | Result |
| --- | --- | --- |
| Memory card (`0x80000400`) | 14 | equal: card info, the save opened and read |
| Disc control (`0x80000593`), CD init (`0x80000592`) | 706, 4 | equal |
| Controller (`0x80001300`), IOP heap (`0x80000003`) | 18, 22 | equal |
| Disc commands (`0x80000595`) | 1,056 | equal to call 562; then 165 sector-read requests differ by small amounts in one word |
| Controller ports (`0x8000131E`, `0x8000131F`) | 2,991 each | all but the first differ: the polls carry the pads' timing |
| LOADFILE (`0x80000006`) | 32 | 18 differ: the load reply's 8 bytes (checksum 0x64 in PCSX2, 0x4 in the port), then the next request, which shares its buffer |
| FILEIO (`0x80000001`) | 12 | 4 seek requests (28 bytes) differ in one word |
| CRI (`0x90000200`) | 90 | 65 differ, from the first reply of function 2 (4 bytes) |
| Sound driver (`0x80000701`), SOUNDS (`0x2000004`) | 6, 10 | requests 0x80D0 and 0x80F0 differ; SOUNDS's reply to function 0x20 (64 bytes) differs |
| Disc (`0x80000597`) | 12 | 1 request (300 bytes) differs |

**Step 8, the differences explained (2026-11-13).** The records now carry their buffers' first 24 words (PCSX2's
records 128 bytes, the port's lines the same words; `rpc_compare.py` compares the words inside each buffer). Session
`rpc4` (PCSX2, boot to the demo's first fight frame, 37,291 records) against the port's log `work/rpc/port-rpc6.txt`
(the same card, 3,000 flips):

| Service | Found | Now |
| --- | --- | --- |
| LOADFILE | The port numbered modules from 1; PCSX2's IOP from 0x19 (24 modules registered by the boot and the IOP reboot) | Fixed (`services/module_loads.h`, with a test): every reply equal |
| CRI | Every handle the port gave was PCSX2's plus 0xEF0D0: the port's base for CRI_ADXI's work area came from an older trace; the original's is 0xB7F30 (October's notes had its first handle, 0xC67F0) | Fixed: all 90 calls equal |
| FILEIO | The port's first file descriptor was 1; the IOP's file manager takes the lowest free slot, 0 and 1 being the console's input and output | Fixed: all 12 calls equal |
| SOUNDS | Status block words 8 and 13 (a thread ID and an address in SOUNDS): 0xF9F239 and 0xA5870 in every reply of this session; the October capture without cards had SOUNDS at another base (0xA3430). They follow where the IOP placed the module, which depends on the cards' state | Explained: module-local; the port keeps its own module base's values (this session's broke the comparison without cards). All other words equal |
| Disc commands | Sector reads of one stream in different chunks | Timing: the same 13,403 sectors in the same order |
| Sound driver 0x80D0, 0x80F0 | Request words the caller does not set: the request builder (`0x314DC0`, variadic) stores a2, a3 and t0 to t3; the callers at 0x297B58 and 0x297B74 leave t3, and t0 to t3, as earlier code left them | Explained: leftover registers, not read by the driver |
| LOADFILE requests, disc search | Bytes past a reply up to its 16-byte unit: PCSX2's SIF DMA moves whole quadwords, so the IOP's buffer follows the reply ({receive address, an IOP word, 0x10} after every short reply of every service; the IOP word 0x8001F930, for disc control also 0x8001F970, 0x8001F9B0, 0x8001A1F0). The port leaves EE memory there unchanged. The first search's name has `;1` after its NUL in PCSX2 (the IOP's buffer) | Explained: IOP leftovers the game does not read; not reproduced |
| Controller ports | The polls are asynchronous; the port echoes each at once, PCSX2's DBCMAN replies later; request words 5 to 12 differ | **Explained (2026-11-16)**, below: stack leftovers and a dropped motor command; nothing to model |

Memory card, disc control, CD init, controller main, IOP heap: equal as before.

**The controller ports explained (2026-11-16).** Each field `0x28C890` sends both ports' motor (actuator) values
through `0x3206F8` and the pad library's sender `0x313F58`: the request is {port, command 0x0103400B, length 0x28,
40 bytes from the caller's stack}, of which the caller writes only the first 11 (the counts 1 and 2, then 2 + 1 data
bytes). Requests alternate between two buffers and clients (`0x3A2640` with client `0x3A0438` for port 1, `0x3A2700`
with `0x3A0460` for port 0), the RPC number 0 or 1 by turn. Found:

- **Counted bytes equal:** words 0 to 4 and word 5's low three bytes are equal in all 2,991 polls of each port
  (session `rpc4` against `work/rpc/port-rpc6.txt`). Every other word is stack left by earlier calls (return
  addresses of the SIF library, floats, pointers; in PCSX2 more than six patterns that follow the scene); the
  differences in words 5 to 12 are these leftovers, not DBCMAN's reply.
- **The reply is not read:** the sender writes words 0 to 12 afresh each time and reads nothing back; it only asks
  whether the client's previous call is done (`0x332B70`) and, when it is not, prints a message and skips the send.
- **Timing:** PCSX2 polls once per field as the port does (gaps of 4.92 million cycles), and in 662 of 10,236 fields
  (6.5%) its DBCMAN had not answered yet, so that field's motor command was dropped. The caller ignores the result and
  clears its motor words either way, so no EE state differs; the port has no vibration output.

**Step 5, the route run (2026-11-16).** The port now takes PCSX2's own route to the `loaded` checkpoint: it boots
with a copy of the session's card (`card-pcsx2`), plays the logos, films and title into the demo, and at the demo
picker's calls (`0x3B9710`) sets the same arguments as the session's `--set` (`BT2_EE_CALL_SET`, the same text). It
also pins the picks the port's demo makes otherwise by its own random numbers: both `fighter_set` calls' character
(a1 72 and 1,080), a2 and t0 to t3 (the starter's 0, 0, 0, 1, 1, which matched PCSX2 on 11-12), `stage_choice`'s a0 9,
`battle_set`'s music a1 23. RAM was written before the same call (`step-b` 1). The port reaches it at field 11,199,
PCSX2 at 11,799 (the fields-since-boot counters `0x34AED8` and others: 0x2BBF against 0x2E17): **PCSX2 takes 600
fields (10 s) longer along the route**, the loads' timing (the port completes disc reads in logical time).

| Compared with PCSX2 | Starter (11-16) | Route |
| --- | --- | --- |
| Display lists A and B | 50,696 bytes each | 4,288 and 4,136 |
| Stream buffer | 414,116 bytes | 6,144 |
| Outside the explained regions | 163 ranges, 25,280 bytes | 118 ranges, 16,308 bytes |

With the regions added below, 100 ranges (10,672 bytes, 1,078 differing words) are left outside. 65 of those words
equal PCSX2's in the starter run: the demo's own picks the register edits cannot set (the picker passes three
per-character values on the stack, from a table at `s5 + character × 16`, so `0x76BB3C` and `0x17E9E58` hold the
port's own pick's 50,000 against 40,000; `0x76BDE0` is written by the picker too). The 1,013 that differ in both
runs, by cause, each found by its writer (`BT2_EE_WATCH_WORD`, now several addresses at once):

- **Time:** the fields-since-boot counters and their half-field and IOP-tick forms (`0x346C00`, `0x34AED8` to
  `0x34BBC0`, `0x36057C`, `0x36643C`, `0x369F44`, `0x3A2890`, `0x3AAFC0`, `0x3B0A84`, `0x3B0B7C`, `0x3B0BFC`); the
  random generator's state (`0x36A3B8`, changed about 450,000 times along the route, before calls of `0x325388`) and
  what the fight placed with it (`0x19CE154`, `0x19CF414`, `0x1BB2D90` to `0x1BBF3BC`: positions and counters); the
  stream and sound engines' state machines, caught mid-cycle (`0x353490`, 40,306 changes by `0x2EDC64`; `0x353F90`,
  16,912 by the library at `0x2F1xxx`; `0x355338`, 4,460 by `0x30A6B4`: the port's values are among PCSX2's cycle),
  their buffer positions and current pool buffers (`0x347C40`, `0x347E48`, `0x353624`, `0x354F48`, `0x354FC0`,
  `0x355390` to `0x355C74`, `0x35D580` to `0x35E080`, `0x35FD00`, `0x364B1C` to `0x3694C8`), the CRI and disc
  services' progress words (`0x3A03C4`, `0x3A5C84`, `0x3A6F84`, `0x3AC744` to `0x3ACC04`).
- **Other threads' stacks** (added as regions, with the random state and the session's program name; before them
  118 ranges, 16,308 bytes): threads 2 to 7 at the addresses the game gives CreateThread, read from the port
  (`BT2_EE_CALL_RAM` writes `<file>.threads.txt`): residue below their frames and where each waits (PCSX2's BIOS
  kernel, the port's native one); PCSX2's own thread stack pointers at the checkpoint are not read.
- **IOP-side words:** the 16-byte tails after short replies (`0x3683F4`, `0x369580`, `0x39DD84`, `0x3A8544`,
  `0x3A8744`, `0x3ABC84`, `0x3ACA08`; as in step 8), the SIF command buffers PCSX2's SIF builds and the port's native
  SIF does not (`0x3A94C0` to `0x3A9FC0`, `0x3AC124`, `0x356B30`, `0x39D800`, `0x3A7480`), the last request's text in
  the file service's buffer (`0x3A8624`), IOP addresses the port's services give from their own module layout
  (`0x3A2814` DBCMAN's handles; `0x3B0E04`, `0x6350B0`, `0x6351A0` SOUNDS's, as in step 8).
- **Session and controller:** the session's program name `host:capture.elf` (`0x38B980`, now a region); the sticks'
  rest value (`0x3B0B1C`, step 5's finding).
- **Picture data one step apart (resolved, below).** The 16 KB at `0x38FD40` are a saved copy of the scratchpad
  (`0x2B7828` sends the scratchpad there by DMA channel 8 and loads 16 KB from `0x38BD40` back by channel 9; the last
  swap at flips 7,966 to 7,974). 668 of its 16,384 bytes differed, by +1 (475), −1 (181) or +2 (12); the colour at
  `0xD23F78` (computed by `0x317964` at the same flips) the same way (`80B3B2B5` against `80B4B3B6`).

**The film decoder's rounding (2026-11-16).** The field log's new column (the IPU's decoded macroblocks) places the
opening film at flips 1,502 to 7,972 (2,419,844 macroblocks; the logo film at 748 to 1,047): the scratchpad copy and
the colour are the film decoder's data at its last picture. PCSX2's IPU computes the inverse DCT with the integer fast
transform (16-bit storage between passes) and rounds its inverse quantisation its own way (escapes scaled signed,
MPEG-1 made odd only for intra table codes and escapes, residuals not clamped); the port computed the exact transform
rounded to nearest and followed the specification. With PCSX2's arithmetic in `runtime/ee/ipu_block.cpp` (the
user's decision) the same route run gives the scratchpad copy and the colour **byte-identical to PCSX2's** (668 → 0
bytes), and outside the explained regions 81 ranges (5,732 bytes) are left, all of the causes listed above. The IPU
tests pass, with two new vectors of the integer transform.

**Step 4, input for other screens (2026-11-16).** Sessions can now play a pad script: `session.py --pad-script`
(the port's syntax, `N:cross+up,N:-`, or `@file`) puts a stub at the entry of the pad update `0x28BDA0` (every screen's
loop calls it, about once a field) that changes port 1's raw controller block before the game parses it: both
receive buffers the game registers in its slot-open request (`0x3B0B00`, `0x3B0B80`; PCSX2's blocks have the port's
layout), the buttons cleared in word 7 (active low) and each pressed button's pressure byte set, so the game derives
its held, new and repeated buttons itself. (A first try ORed the game's processed button word `0x3B0C2C` after
`0x28BF54`: the word held the buttons, but no new press reached the logos.) The stub's table takes 198 word changes.

`config/pcsx2_pad_main_menu.txt` takes PCSX2 from boot to the **main menu** with the card's save: Start every 30
calls from 250 to 780 (logos and opening film skipped), Start at 900 ("press start"), the title from call 918 (its
object at `0xAA11C0`), Start every 30 calls from 1,100 (the title's menu shown at 1,129, flags 0xC, the cursor on
Continue), Cross at 1,500, the title released at 1,539. At field 2,806 the progress flags are 0xC08 (the save loaded,
as the port's quick start at its main menu) and the mode 4. Timeline found with `--log "@28BF54:N,word=4213B0,
word=<title+0xCC>"`; a session's timing repeats from run to run with the same script.

**Step 4, overlay sites and the main menu (2026-11-16).** Sites may now lie in the overlay (DBZP.BIN at
`0x3B3100`, loaded by main's first frame step): `session.py` builds their stubs from the local overlay image
(`work/overlay/dbzp`, its pinned identity checked) and an installer stub on main's one call into the overlay
(`jal 0x3B7FA8` at `0x100688`) writes their jumps with EE stores before that code first runs, so PCSX2 has none of
it compiled. `--callees-of` takes overlay functions (the overlay's generation map). `compare_calls.py` puts the
overlay's original words back at those sites in the replay's RAM (the port runs overlay code only while RAM holds
the bytes it was generated from) and leaves every patched word out of the comparison (now both words of a call
site, its delay slot too, also in `loaded_data.py`).

The main menu (`0x3B4250`) loops once a frame through the frame's work in the executable (the pad update, kicks,
the flip) and two overlay steps, `0x3B3E50` (at `0x3B42B0`) and `0x3B37A8` (at `0x3B42B8`), found with
`BT2_FIELD_CALLS` at a main-menu field of the port's quick start. Session `menus7` (the main-menu route above, both
steps before and after at the menu's frames 60 to 62): **all six calls equal** in the port's replay (28 to 69 words
written; registers equal). PCSX2 changed 5,000 to 12,000 other words during each call (interrupts, DMA and other
threads while it waited at the checkpoint), none of them written by the port.

**Step 4, Dueling and character selection (2026-11-16).** Each screen's loop was found in the port (quick start, the
same presses, `BT2_FIELD_CALLS` at a field of the screen, a picture to see it) and its once-a-frame overlay steps
checkpointed in PCSX2 at the screen's frames 60 to 62:

| Screen | Route (calls of the pad update, after the main menu's) | Steps | Result |
| --- | --- | --- | --- |
| Dueling (mode 30, cursor on 1P VS COM) | Down at 1,700, 1,720, 1,740; Cross at 1,780 | `0x41B6A8` (at `0x41BDA8`), `0x41AE58` (at `0x41BDB0`) | session `menus8`: 6 of 6 equal |
| Character selection (1P VS COM, Single Battle) | then Cross at 1,950 and 2,050 (`config/pcsx2_pad_character_select.txt`) | `0x405898`, `0x407AC8`, `0x406CA0`, `0x409698`, `0x407BE0` (at `0x409C20`, `0x409C48`, `0x409C50`, `0x409C60`, `0x409F8C`) | session `menus9`: 15 of 15 equal |

On the selection screen three steps wrote nothing in those frames on either side (no button pressed): equal, but
little exercised; frames with the cursor moving are left to check.

**Next:** none required; the plan was closed on 2026-11-16 (see "At a glance"). Optional: the story map's and a
town's logic (their routes from the main menu's first entry) and the stored fight comparisons in the gate.

## Unknowns

- Whether a save state taken while the stub waits changes what PCSX2 draws afterwards (states are taken there and
  the session goes on; a change to later frames has not been looked for). Sessions with different sites reach
  different GS memory at the same frame; with PCSX2's renderer on one thread whether they still do is not checked.
- Which word of the display shows when a drawn frame reaches the screen in PCSX2 (GS memory and its privileged
  registers are not exported; the tester sees the screen instead).
- Whether the desktop duplication API sees a fullscreen program's pictures when they bypass the compositor (if not,
  fullscreen press-to-screen needs a camera, and the user's).
- Whether the CLUT buffer matters: PCSX2's freeze does not hold it, so a chain that draws with a CLUT loaded in an
  earlier frame (CLD 0) would draw differently in the replay (not seen in fight frame 300: its draws load theirs).

## Not in this plan

Sound output against a recording (optional, [sound plan](../sound/SOUND_OUTPUT_PLAN.md), Verification); the
enhancements (they are not the original's); PCSX2's own accuracy against the console (it is the reference).
