# Performance: real-time speed in the window

Opened on 2026-10-13 after the user reported the window as laggy (about 15 frames per second) with the graphics card at about 16% load.

## Requirements (set by the user, 2026-10-13)

- **Fights: a flat 30 pictures per second.** The user confirmed in PCSX2 that the original shows 30 in fights; the fight loop's two-field frame, read from the code, agrees.
- **Menus: a flat 60, without dips.**
- **Never faster than the game:** the run is held at the PS2's field rate, 59.94 per second.
- **A mid- to low-end PC must hold these too,** not only the user's machine. The measure for that is the work per picture, not the rate on this machine: a processor half as fast needs the work here to stay under half the budget (about 16 ms of 33.4 ms in fights, about 8 ms of 16.7 ms in menus). Optimizations must remove work or spread it over cores; nothing may rely on instruction sets beyond SSE2.

## How it is measured

`BT2_SAMPLE_PROFILE=<path>` (with `BT2_SAMPLE_FROM=<flip>`) samples where the runner's thread executes and writes module offsets; `python tools/perf/profile_report.py <profile> build/probe/Release/bt2-entry-probe.map` names the functions. For the parts of a flip:

`BT2_VIEWER_STATS=1` with the viewer prints every 120 flips: flips and fields per second of wall-clock time, the slowest single flip and how many took more than 20.9 ms, the work per flip (its time without the pacer's waiting), and the time per flip spent on reading the picture back from the GPU, on showing it, on VIF1 unpacking, on VU1 (with the GS packets it sends), on GS packet handling from all sources and on the GPU renderer's flush. The rest of a flip is the recompiled game code and the services. Timers are in `runtime/ee/frame_profile.h`.

**Scene:** the scripted path into a 1P-vs-COM fight (`pad-fight.txt`), Release build, the user's 10-core machine. One scene on one machine; other fights and stages are not measured.

**Equality of output:** the report's `vu1_kick_hash` is a hash over every GS packet VU1 sends, in order. Equal hashes on the same scripted run mean a change to VU1 or VIF1 left the output bit-identical. Baseline for 5,100 frames of the fight script: 784,729,536 instruction pairs, hash `f3da74e43010e423`.

## Findings (2026-10-13)

**1. Menus and movies were slow because of waiting, not work.** The window and the audio device both paced the run, each with sleeps at Windows' default granularity of about 15 ms, so a flip took about 28 to 31 ms although the work needed under 10 ms. Fixed: the audio device is the only pacer when it is on, and sleeps are 1 ms. Measured after: menus hold 60 flips per second (16.7 ms per flip); the movies' 30 flips per second are the movies' own rate.

**2. The fight is limited by the processor, not the graphics card.** Time per flip in the fight, three windows of 120 flips:

| Part | At the start | After the changes below |
| --- | --- | --- |
| Whole flip | 58.7 to 73.1 ms | 31.7 to 37.7 ms |
| VU1 with its GS packets | 35.9 to 50.3 ms | 16.8 to 23.5 ms |
| GS packet handling (all sources; mostly inside the VU1 figure) | 7.3 to 8.0 ms | 5.8 to 6.5 ms, of it 3.9 to 4.5 ms in the GPU renderer |
| VIF1 (unpacking and direct GS data) | 4.5 to 4.9 ms | 2.8 to 3.1 ms |
| Picture readback and showing | 2.8 to 3.9 ms | 2.7 to 3.9 ms |
| GPU flush | 0.1 to 0.2 ms | 0.1 to 0.3 ms |
| Game code and services (the remainder) | about 12 to 14 ms | about 8 ms |

The three windows are the same 360 flips of the run each time; the scene gets lighter across them, so the ranges are comparable between columns but not within one. With the first figure of the right column the fight ran at 53 fields per second of wall-clock time, with the last at 63 (60 is the game's speed).

VU1 runs about 1.1 to 1.3 million instruction pairs per flip in this scene. The GPU's low load is consistent with this: it waits for the processor.

**3. The fight shows 30 pictures per second.** The fight's main loop (the functions at `0x102E80` and `0x102F30`) starts Timer 1 on the H-BLANK clock at the top of each frame and, before it waits for the vertical blank, waits until that timer has counted 400 (`0x28C790`). 400 lines are about 1.5 fields, so each frame takes two fields. The menu loops call a frame end without this wait and run at 60. The user confirmed 30 in fights on the original in PCSX2.

**4. Menus averaged 60 but were not flat.** With the sound device's queue as the pacer, 15 to 20 of every 120 pictures took more than 20.9 ms (the slowest 26 to 30 ms), because the device releases its queue in coarse steps. With the clock-based pacer (below) 0 or 1 of 120 are late and the slowest steady picture is about 20 ms; the work per picture is 6 to 7.5 ms.

**5. Single hitches at screen changes** (0.11 to 0.3 s, and 0.2 s entering the fight): the game loads data and the port does all of it within one field. Not addressed yet.

**6. A longer fight is heavier than the first 20 seconds.** Over about 100 seconds of the scripted fight the work per picture is 25 to 31 ms in most stretches and 35 to 39 ms in the heaviest (flips 5,280 to 5,880), where the rate falls to 23 to 28 pictures per second. In the heaviest stretches VU1 with its packets takes 23 to 24.5 ms, of which about 6 ms are GS packets (5 ms in the GPU renderer). So the fight does not hold a flat 30 yet, even on the user's machine.

## Done

All with the same instruction count, call count and output hash on the fight run as the baseline.

- **Pacing:** one pacer, 1 ms sleeps (`runtime/host/audio_device.h`, `viewer_window.h`).
- **VU1 arithmetic with SSE:** the float mode of a run (round toward zero, denormals as zero) is set in MXCSR and the four lanes of an arithmetic instruction are computed at once, with the model's clamp of infinities and NaNs as integer operations. The XGKICK packet buffer is reused.
- **VU1 pairs decoded once:** an instruction pair is decoded into a table entry the first time it runs (an entry is current while its words equal program memory, so nothing has to invalidate it). Arithmetic, the plain loads and stores and the no-operations run from the entry; everything else goes through the general path, which also refuses what is not modelled.
- **No CPU copy of texture uploads in GPU-only mode:** the CPU GS model no longer writes host-to-local image data into its own memory when the GPU renderer draws alone.
- **Main RAM first in word loads and stores:** the game's 32-bit memory accesses reach RAM before the chain of device and checkpoint cases, none of which is a RAM address.
- **Call bookkeeping:** the sets of entered and completed functions are skipped for a function recorded just before (a small table in front of them).
- **A sampling profiler** (`BT2_SAMPLE_PROFILE`; `tools/perf/profile_report.py` by function with the callers of library code, `profile_lines.py` by source line) to find the next places.
- **The VIF1 list buffer is reused** instead of being built in a new zero-filled vector for every list.
- **The EE FPU helpers set the rounding mode in MXCSR** for their one operation instead of through the C library.
- **The CPU GS model follows only A+D registers in GPU-only mode** (transfers, FINISH); vertices, primitives and CLUT loads are the renderer's. Its primitive counters stay at zero in that mode.
- **Frame timers cost nothing unless the statistics are on.**
- **A clock-based pacer** (`runtime/host/frame_pacer.h`): each field has a deadline one period after the last and is waited for once, when its picture is shown or at its end, with a high-resolution timer. The period follows the sound device's queue within 1%, so the device no longer paces the run and its queue stays near 50 ms.

## Next, in order of expected gain

Against the requirements the fight needs its work cut from 25 to 39 ms per picture to about 16 ms.

1. **VU1 as native code.** VU1's own work is 11 to 18 ms per picture, spread evenly over the interpreter's loop (no single hot line), so the interpreter is at its limit. Translate the game's microprograms to C++ (generated locally from the programs the game uploads, like the EE code, with the interpreter as fallback and as the reference for the output hash).
2. **Graphics on its own thread.** VIF1, VU1 and the GS packets do not feed back into the game except at a few points (path synchronization, readback, the flip). Running them on a second thread takes about two thirds of the fight's work off the game's thread. Needs an ordered queue for all GS-bound data and those synchronization points.
3. **Presenting without readback** (about 3.5 ms per picture): show the renderer's image through a swapchain instead of copying it to the processor and drawing it with GDI.
4. **Game code and services** (about 5 to 7 ms): the other access widths and the per-call checks, guided by a new profile.
5. **Loading hitches at screen changes:** spread the load over fields or do it off the game's thread.

Each step: measure with `BT2_VIEWER_STATS` over the extended fight run (7,400 frames of `pad-fight.txt`), compare `vu1_kick_hash` and the run's counters with the baseline (5,100 frames), run the gate, record before and after in the monthly history.

## Verification

- Unit tests for VU1 and VIF1 (`bt2-vu1-tests`), the gate's frame comparisons and renders.
- The output hash and instruction count on the fight script.
- The user's observation in the window, with sound: no gaps where the run holds the game's speed.
