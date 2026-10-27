# Responsiveness and loading

Opened 2026-10-27 at the user's request: the game should feel more responsive and load a bit faster; more native code where it improves either. Builds on the [performance plan](PERFORMANCE_PLAN.md), whose rate targets are met (30 flips per second in fights, 60 in menus).

## Requirements

- **Nothing the game computes changes.** Every step keeps the game's state per field identical (the gate's battle hash and comparisons): only when a picture reaches the screen, and how long the waits take on the wall clock, may change.
- **Even pictures:** a cut in latency must not make the pictures uneven (judder) at the display's rate.
- **Choices that change the look are the user's** (tearing, loading screens flashing by): such a step comes as a setting, with its measurement, and the user decides its default.

## What was measured (2026-10-27)

Release, headless, the gate's path (films, title, menus, character and map select, a 1P-vs-COM fight; 6,534 fields), with `BT2_FIELD_LOG` (one line per field: its time, flips, disc and file requests, load-wait idle points, the audio's peak).

- **The work is small:** a field takes 0.9 ms of the game's thread (median), 10.7 ms at the 99th percentile, against 16.7 ms. Only 4 fields went over a field period: 70 ms entering the fight (field 5,063), 60 ms after a menu confirm (1,702), 37 ms at a menu change (1,038), 21 ms at boot (118). These are the hitches at screen changes ([performance plan](PERFORMANCE_PLAN.md) item 6).
- **Loading is the game's pacing:** disc reads complete when asked; the stretches with disc or file activity add up to about 10 s of the 109, mostly fades and loading screens timed in fields as on the console. The boot's black wait is 2.1 s (silent, 3 pictures), the fight's load about 1.6 s (mostly silent).
- **Presentation** (from the code, not yet measured): the game flips early in a field; the port then waits for the field's deadline (`FramePacer::wait`) and presents; Windows shows the picture at the display's next refresh after that, in a window through the compositor. On the console the flipped picture is scanned out from that VBlank. With 60 fps a fight's picture is shown a field later (the in-between picture needs the next frame).

**Step 1 measured (2026-10-27, `BT2_LATENCY_LOG`, `tools/perf/latency_report.py`; the gate's path at 1x, Release):**

| From the game's flip | Window (5,099 flips) | Fullscreen (2,499 flips) |
| --- | --- | --- |
| Pacer hold to the handover (the game's own field, as on the console) | 15.4 ms median | 15.3 ms |
| Handover to present (the presenter's thread) | 0.6 ms (p90 1.6) | 0.6 ms (p90 2.2) |
| Present to the refresh that showed it (DXGI frame statistics) | 11.4 ms (p90 13.9) | 4.3 ms (p90 7.1) |
| Whole | 27.2 ms | 20.1 ms |

The pad is read at a field's deadline and the flipped picture is handed over at the next deadline, one field as on the console, so the port adds the last two rows: about 12 ms in a window (the desktop compositor) and about 5 ms in fullscreen (Windows flips it directly). Step 2's options are worth about 3 to 4 ms more; fullscreen is the setting that cuts latency.

**The hitches (step 3) are loading work:** each of the three slow fields makes 75,000 to 130,000 guest calls (a field's median is 3,200), main polling a worker with GetThreadId, ReferThreadStatus and ChangeThreadPriority (the wrappers at `0x32EB50`, `0x32EB60`, `0x32EAF0`, about 4,000 to 6,500 times each) while the worker loads the next screen (`0x31A6C8`, `0x31A338`); logical time does not advance meanwhile, so the load lands in one field. Spreading it over fields would change the game's timing; making the code faster does not. In a profile of the whole path about a third of the game thread is the runtime's own overhead per call (dispatch, function bookkeeping, overlay checks: about 17%) and per memory access (about 17%).

## Steps

1. **Measure the latency the port adds.** For each picture: when the game flipped it, when it was handed to the swapchain, and when the display showed it (DXGI's frame statistics: the refresh at which each present appeared). Report the distribution of flip-to-display in a window and in fullscreen, with and without 60 fps. The input side needs no measurement: the pad is read at the field's deadline, where the console samples it.
2. **Cut presentation latency.** In order of expected gain, each measured with step 1 and its pictures checked for evenness:
   a. **Present when the picture is ready, timed to the display:** the pacer's deadline waits for the display's refresh instead of a clock of its own, so a picture is not held for a deadline and then for a refresh (up to a refresh saved, about 8 ms on average).
   b. **Fullscreen without the compositor** (independent flip): check that fullscreen gets it and that the window's size and the swapchain's do not block it.
   c. **A "low latency / variable refresh" setting:** present at once with tearing allowed; on a variable-refresh display the picture appears when presented, without tearing. The user's choice.
   d. **60 fps:** the field of delay stays (the in-between picture needs the next frame); measure and report it.
3. **Remove the hitches at screen changes.** Profile the four slow fields (`BT2_SAMPLE_PROFILE` from the field before), find whether the work is the game's (a decompression, a copy), a service's or the renderer's (uploads, pipeline creation), and move it off the game's thread or spread it over fields. Target: no field of the path over a field period.
4. **Faster loading (the user's choice of default).** Automatic fast forward through loading stretches: fields where a disc or file request is pending or the game waits in a load idle point, its audio is silent and no input is held, run without waiting for their deadlines (the existing fast forward, which keeps the game's state per field). Saves the boot's 2 s and most of each load's silent part; the loading screens' animations pass faster.
5. **Native code where it pays:** only where step 3's profiles show recompiled game code doing the work of a hitch (for example a decompression or a large copy as a native function).

## Results

**Steps 3 and 5, first part (2026-10-27): the game's code faster.** Measured on the headless Release path (4,400 flips, 5,137 fields) with `BT2_FIELD_LOG`; every change keeps the guest calls per field and the guest memories of every field (`BT2_STATE_HASH`) identical.

| | Before | Dispatch cache | + native memcpy | + inline RAM accesses |
| --- | --- | --- | --- | --- |
| The path's time on the game's thread | 6.03 s | 5.80 s | 5.66 s | 4.41 s (−27%) |
| A field's median | 914 µs | 869 µs | 854 µs | 693 µs |
| Hitch entering the fight | 69.5 ms | 70.7 ms | 58.7 ms | 49.8 ms |
| Hitch after a menu confirm | 59.8 ms | 59.6 ms | 51.2 ms | 43.0 ms |
| Hitch at a menu change | 35.2 ms | 35.3 ms | 30.3 ms | 26.0 ms |

- **Dispatch cache** (`ps2_runtime_dispatch.cpp`): a direct-mapped table in front of the function maps; the overlay and kernel binding checks only for addresses they cover.
- **Native memcpy** (`ee/native_functions.cpp`): the C library's memcpy copied bytes one at a time when either address was not 16-byte aligned, each through the runtime's checked accessors. The host version copies the same chunks in the same order, leaves the same registers (all 128 bits) and takes the same checkpoints (`EeRuntime::takeCheckpoints`); it runs the generated code for addresses outside main RAM and where its checkpoints would stop the run or deliver main's busy-field VBlank. Installed only when the code at its address hashes as expected. A unit test runs it against an instruction-by-instruction model of the guest function.
- **Inline RAM accesses** (`ps2_runtime_macros.h`): generated code reads and writes aligned main RAM directly; every other address and every unaligned access goes through the checked accessors as before (halfword stores always, which a startup check counts).

Left in the hitches (a profile at 20,000 samples a second): the IPU's picture decoding about 22%, the loop guard (`eeCheckpointDue`, one call per loop iteration) about 6%, the game's start-code scan (`0x2DCFE8`, byte by byte) about 5%.

**Step 2, pictures ahead of their deadline (2026-10-27):** a picture was ready about 1 ms into its field and then held to the field's deadline (15 ms); the game read its input at the field's start, so that hold was delay the console does not have in it. Now the picture goes out at the same moment in every field, when 99% of the last 120 pictures were ready plus 1 ms (`FramePacer::waitToShow`), and the deadline is waited for after it: the game's pace, input and state are unchanged. With 60 fps outside fights the in-between pictures go out at the deadlines, so pictures keep the deadline there; fights with 60 fps below. `BT2_PICTURE_AT_DEADLINE=1` keeps the deadline everywhere.

| From the game's flip to the refresh that shows it (the gate's path, 1x) | Window | Fullscreen |
| --- | --- | --- |
| At the deadline | 27.2 ms median (p90 30.4) | 20.1 ms (p90 23.4) |
| Ahead of it | 17.0 ms (p90 23.4) | 10.3 ms (p90 18.7) |

Evenness, from the presents' times (5,097 intervals in a window): the median distance from a whole number of fields 0.19 ms (0.26 at the deadline); 24 intervals off by more than 4 ms (4 at the deadline), late pictures at screen changes. The rates per 120 flips are the same (60 in menus, 30 in the fight). A unit test checks the pacer (`frame_pacer_tests.cpp`).

**Step 2, with 60 fps (2026-10-27):** a fight frame's commands come a frame ahead, so its in-between picture is usually drawn before the flip. At the flip the game's thread (which waits for the field's deadline anyway) waits for the in-between picture, shows it when such pictures are usually ready, and hands the flip's own picture to the presenter scheduled exactly a field later (`WindowPresenter::presentAt`; the presenter keeps two pictures and waits for a scheduled one on its thread). Measured in a window at 1x on the gate's path with 60 fps, the fight's flips: flip to screen 44.8 to 35.3 ms median (p90 49.3 to 42.3); the same pictures shown (1,125 in-between of 1,221 drawn); presents off a whole field by more than 4 ms 57 to 25. The field the in-between picture needs before the flip's own stays.

**Step 2, a field less with 60 fps: tried and left (2026-10-27).** The delayed fights cost a field: the flip's picture waits behind its in-between picture. Without the delay (`BT2_SIXTY_FPS_DELAY=0`, the flip's picture ahead in its own field, the in-between picture handed over at the field's end and scheduled a field after it) a fight at every enhancement (16x, HD, widescreen, 4x scanout) went from 50.1 to 21.0 ms flip to screen in fullscreen, with the same pictures shown, but the cadence broke: 91.6% of pictures a field apart instead of 98.5%, nearly all an in-between picture handed over after it was due. Its scanout needs the flip's display registers, so it is done only after the flip, 12 to 16 ms into the field at 1x and at 16x (the delayed path's hand-over waits for it: 29 and 33 ms from the flip). A choice per fight (undelayed while in-between pictures could be handed over in time) chose delayed in every run, at 1x too, so it was not kept. What would gain the field: an in-between picture finished early in the flip's field, for example its scanout prepared before the flip.

## Verification

Each step: the gate (game state unchanged), step 1's numbers before and after, `BT2_FIELD_LOG` for hitches and loading, and the user's feel in play.
