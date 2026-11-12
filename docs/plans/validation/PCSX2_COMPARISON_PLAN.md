# One comparison with PCSX2, across the game

The user's request (2026-11-12): a cohesive comparison of the port with PCSX2, planned and started. It is the
"final validation" milestone's comparison half ([implementation plan](../../project/IMPLEMENTATION_PLAN.md), step 6).

## Where the comparisons stand

- **Startup:** 46 scripts compare single trap points of the boot with PCSX2 captures; 31 of them in the gate.
- **The AI:** three calls replayed on PCSX2's RAM, byte-exact after four rounding fixes
  ([AI plan](../ee/AI_VERIFICATION_PLAN.md)).
- **Saves:** byte-identical to the original's; **films:** first pictures and sound timing.
- **Not compared:** the fight beyond those three calls, menus and the story map's logic, loaded data after the boot,
  VU1 memory, GIF packets, GS memory and pictures, the services' replies after the boot.

Each of these was its own tool with its own trap. The comparison below is one method for all of them.

## Why not compare two runs frame by frame

PCSX2 and the port do not take the same time: loads finish sooner in the port, its services answer sooner, so the
game's frame counters and random numbers drift apart within seconds of the boot. A comparison over time compares
two different games. The method is therefore **the port started from PCSX2's state**: PCSX2 halts at a chosen point,
its state is read out, the port runs the same code from that state, and what both leave is compared byte for byte.
Drift cannot enter, and every difference is the port's.

## The method

**A session** is one PCSX2 run with **checkpoints**: call sites in an instrumented copy of the executable (as the
existing captures) whose stub publishes the full register state, waits until the host has read the state out, and
continues. One session yields many checkpoints, and "before" and "after" a call come from the same run.

- **Registers published:** all 32 GPRs at 128 bits, HI/LO and HI1/LO1, SA, the 32 FPRs and FCR31, VU0's 32 VF and
  16 VI registers and its Q, R (the game's random numbers), I and clip registers. Not readable without changing them: the FPU's ACC and VU0's
  ACC (a function that reads them before writing them shows as a difference). `t8` and `t9` are the stub's own.
- **Memory read:** EE RAM (32 MB, `EEmem`), VU0 and VU1 micro and data memory (`VUmem`), and when a picture is
  compared, GS memory from a PCSX2 save state (PINE), with the software renderer.
- **Resume:** the stub spins on a word the host clears with a process write; the host also writes how many calls
  to let pass before the next checkpoint. Interrupt handlers and other threads keep running meanwhile: words they
  change are told apart as before (a difference counts where the port wrote).
- **Input:** a pad hook in the game's reader plays a script keyed to the game's own pad reads, so a session reaches
  menus and fights without a user, and the port's starters are not needed in PCSX2.

**The replay** loads a checkpoint's RAM, registers and VU memory into the probe, runs the recompiled function to a
return sentinel, and writes the RAM and registers it leaves (`BT2_REPLAY_CALL`). A function that reaches a service,
a DMA channel or the kernel stops at that boundary with its name (fail explicitly); such functions are compared at
the level the plan's later steps give them.

**The verdict** per checkpoint: the words the port wrote equal PCSX2's after the same call, the return registers
equal, and no word PCSX2 wrote in the function's own data is missing in the port. Differences are listed by address
range with the function that wrote them (store watch).

## Steps

1. **The session harness** (started 2026-11-12): resumable checkpoints with the full register block, several sites
   per run, EE RAM and VU memory per checkpoint, the full-context replay, and the comparison. Check: the AI's three
   calls again from one session, equal as in November; two sessions with the same script give the same fighter data
   (PCSX2 repeats itself).
2. **The fight frame, step by step:** checkpoints at each call of the fight loop (`0x102F30`, `0x102E80`): the logic
   step `0x102AA0` (AI, motion, collisions, damage), the camera and effects steps, the draw builders `0x102C40` and
   `0x102D20`. Each step replayed for 100 frames spread over the demo battle and a scripted fight with super attacks,
   beam struggles and a stage destroyed. Steps that reach a boundary are split into their callees.
3. **Trajectories:** one session takes the logic step at consecutive frames (K = 300); each frame replayed from
   PCSX2's state of that frame; then the port run free for K frames from the first and the frame where it first
   differs found. This is the claim "the fight plays as the original's", frame by frame.
4. **Other screens' logic:** the same for the menus' frame steps, character and stage selection, the story map, a
   town, Ultimate Battle Z's course logic, the tournament's draw.
5. **Loaded data:** at events that follow a load (a fight's first frame, a story map shown, a menu entered) EE RAM
   compared in whole, by region: every loaded and unpacked file, the stage's and fighters' data, the tables. Regions
   that differ by timing (counters, the sound and stream buffers, the stacks, the kernel's own memory) are listed
   once with their reason; anything else is a finding.
6. **VU1 and the GIF stream:** at a frame's VIF1 kick, PCSX2's chain (in RAM) and VU memory; the port's VIF1 and
   native VU1 programs run the same chain; VU1 memory at the chain's end and the GIF packets compared with PCSX2's
   (VU memory at each `XGKICK`, or a PCSX2 GS dump of the frame).
7. **GS memory and pictures:** a PCSX2 save state at a frame's start and at the next (software renderer) gives GS
   memory; the port's renderer, started from the first with the frame's packets, is compared with the second:
   textures and palettes byte for byte, the drawn frame by pixel statistics, the two pictures saved to
   `work/frames/pcsx2-compare/` for the user. Differences of paraLLEl-GS's own rounding are measured, not judged by
   eye alone.
8. **Services:** the requests and replies of the sound, CRI, card and disc services over a session (the PCSX2 side
   logged by hooks in the game's call wrappers, as `--sounds-log` does) against the port's log for the same
   checkpoints' state.
9. **Input lag and latency** (the user, 2026-11-12). Three parts, each measured the same way in both programs:
   - **The game's own delay, in fields.** The port's (2026-11-11, [responsiveness plan](../ee/RESPONSIVENESS_PLAN.md)):
     a press changes the game's state in the field whose pad read saw it, VU1's memory a frame later, the picture
     7 fields after the read. In PCSX2 the same: a pad hook in the game's reader presses at a chosen read, a
     checkpoint at the frame's end (`0x2711E8`) every frame for 20 frames records the game's field counter, the
     fighter's state, the display-list buffer (`0x430000` to `0x457000`) and VU memory, and the field of the first
     changed picture comes from the flips (the display buffer switched to the frame drawn after the change). Both
     must give the same count for a stick push, Cross, Circle and Square (on its release), in either field of a
     fight frame. A difference is a finding about the port's timing of pad reads, kicks or flips.
   - **Press to screen, in milliseconds.** One black-box tester for both: a key sent to the program's window
     (`SendInput`), the screen read with the desktop duplication API (each frame's present time), the first
     picture whose fighter area changed. 100 presses spread over a fight frame's phase, in a CPU fight on the same
     stage, windowed and fullscreen; PCSX2 at its defaults and at its lowest-latency settings (its frame pacing
     and vsync options), the port with and without Low input lag and 60 fps. Report median and spread per setup.
     The tester sees what the player sees, so it also checks the port's own flip-to-screen numbers.
   - **Press to sound, and the pictures' evenness.** The same tester records the audio device (WASAPI loopback)
     and finds the first sound of the move (a swing or a hit) after each press; and from the presents' times it
     gives each program's picture intervals (a field apart or not) over the same fight. The game's own sound delay
     (the field its sound request is made) comes from the same checkpoints as the game's picture delay.
10. **In the gate:** every comparison whose captures exist locally runs in the gate from the stored captures (no
   PCSX2 run), as the startup comparisons do.

Each difference found gets its fix, a test of its own, and the gate's battle baseline rerecorded when the fight
changes (as the rounding fixes did).

## Results

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

**Next:** the globals' 152 ranges by route (or the port through the same route); then step 6 (VU1 and the GIF stream). Press to sound and fullscreen when wanted.

## Unknowns


- Whether `VUmem` is VU0 micro, VU0 data, VU1 micro, VU1 data in that order (checked on the first session against
  the game's VU1 programs).
- How many of the fight's steps can be replayed without reaching a service or DMA (step 2 finds out).
- Whether the save state's GS memory can be taken while the stub spins without changing what is drawn next.
- Which word of the display shows when a drawn frame reaches the screen in PCSX2 (GS memory and its privileged
  registers are not exported; the tester sees the screen instead).
- Whether the desktop duplication API sees a fullscreen program's pictures when they bypass the compositor (if not,
  fullscreen press-to-screen needs a camera, and the user's).
- Whether PCSX2's software renderer and paraLLEl-GS agree exactly on BT2's draws (paraLLEl-GS documents some
  interpolation rounding as not bit-exact).

## Not in this plan

Sound output against a recording (optional, [sound plan](../sound/SOUND_OUTPUT_PLAN.md), Verification); the
enhancements (they are not the original's); PCSX2's own accuracy against the console (it is the reference).
