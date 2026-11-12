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
  16 VI registers and its Q, R, I and clip registers. Not readable without changing them: the FPU's ACC and VU0's
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

**Next:** trajectories (step 3: consecutive frames), the input lag (step 9), loaded data (step 5).

## Unknowns

- Whether `VUmem` is VU0 micro, VU0 data, VU1 micro, VU1 data in that order (checked on the first session against
  the game's VU1 programs).
- How many of the fight's steps can be replayed without reaching a service or DMA (step 2 finds out).
- Whether the save state's GS memory can be taken while the stub spins without changing what is drawn next.
- Whether the desktop duplication API sees a fullscreen program's pictures when they bypass the compositor (if not,
  fullscreen press-to-screen needs a camera, and the user's).
- Whether PCSX2's software renderer and paraLLEl-GS agree exactly on BT2's draws (paraLLEl-GS documents some
  interpolation rounding as not bit-exact).

## Not in this plan

Sound output against a recording (optional, [sound plan](../sound/SOUND_OUTPUT_PLAN.md), Verification); the
enhancements (they are not the original's); PCSX2's own accuracy against the console (it is the reference).
