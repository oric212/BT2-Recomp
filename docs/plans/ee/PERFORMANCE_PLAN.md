# Performance: real-time speed in the window

Opened on 2026-10-13 after the user reported the window as laggy (about 15 frames per second) with the graphics card at about 16% load. The goal is that every part of the game holds its own speed on the user's machine, with sound, so play-testing is meaningful.

## How it is measured

`BT2_VIEWER_STATS=1` with the viewer prints every 120 flips: flips and fields per second of wall-clock time, and the time per flip spent on reading the picture back from the GPU, on showing it, on VIF1 unpacking, on VU1 (with the GS packets it sends), on GS packet handling from all sources and on the GPU renderer's flush. The rest of a flip is the recompiled game code and the services. Timers are in `runtime/ee/frame_profile.h`.

**Scene:** the scripted path into a 1P-vs-COM fight (`pad-fight.txt`), Release build, the user's 10-core machine. One scene on one machine; other fights and stages are not measured.

**Equality of output:** the report's `vu1_kick_hash` is a hash over every GS packet VU1 sends, in order. Equal hashes on the same scripted run mean a change to VU1 or VIF1 left the output bit-identical. Baseline for 5,100 frames of the fight script: 784,729,536 instruction pairs, hash `f3da74e43010e423`.

## Findings (2026-10-13)

**1. Menus and movies were slow because of waiting, not work.** The window and the audio device both paced the run, each with sleeps at Windows' default granularity of about 15 ms, so a flip took about 28 to 31 ms although the work needed under 10 ms. Fixed: the audio device is the only pacer when it is on, and sleeps are 1 ms. Measured after: menus hold 60 flips per second (16.7 ms per flip); the movies' 30 flips per second are the movies' own rate.

**2. The fight is limited by the processor, not the graphics card.** Time per flip in the fight, three windows of 120 flips:

| Part | Before | After the first VU1 change |
| --- | --- | --- |
| Whole flip | 58.7 to 73.1 ms | 48.3 to 57.8 ms |
| VU1 with its GS packets | 35.9 to 50.3 ms | 25.4 to 35.0 ms |
| GS packet handling (all sources; mostly inside the VU1 figure) | 7.3 to 8.0 ms | 7.2 to 7.8 ms |
| VIF1 unpacking | 4.5 to 4.9 ms | 4.5 to 4.7 ms |
| Picture readback and showing | 2.8 to 3.9 ms | 2.8 to 3.8 ms |
| GPU flush | 0.1 to 0.2 ms | 0.1 ms |
| Game code and services (the remainder) | about 12 to 14 ms | about 12 to 14 ms |

VU1 runs about 1.1 to 1.3 million instruction pairs per flip in this scene. The GPU's low load is consistent with this: it waits for the processor.

**3. How many pictures per second the fight should show.** The fight's main loop (the functions at `0x102E80` and `0x102F30`) starts Timer 1 on the H-BLANK clock at the top of each frame and, before it waits for the vertical blank, waits until that timer has counted 400 (`0x28C790`). 400 lines are about 1.5 fields, so each frame takes two fields: 30 pictures per second in NTSC. The menu loops call a frame end without this wait and run at 60. This is read from the game's code, not measured on the original; the user's recollection is that the original targets 60 in fights. Open until it is checked on the original (a fight in PCSX2 with its internal frame rate shown). Under the reading from the code the budget is 33.4 ms per flip; under the user's it would be 16.7 ms.

## Done

- **Pacing:** one pacer, 1 ms sleeps (`runtime/host/audio_device.h`, `viewer_window.h`).
- **VU1 arithmetic with SSE:** the float mode of a run (round toward zero, denormals as zero) is set in MXCSR and the four lanes of an arithmetic instruction are computed at once, with the model's clamp of infinities and NaNs as integer operations. The XGKICK packet buffer is reused. Same instruction count and the same output hash on the fight run; the VU1 unit tests pass.

## Next, in order of expected gain

1. **VU1 instruction dispatch.** After the arithmetic change VU1 still costs about 18 to 28 ms per flip for its own work, roughly 15 to 20 ns per instruction pair, most of it decoding each pair again every time it runs. Two options:
   - **Predecoded programs:** decode a microprogram once into a table of operations and operands when VIF1 loads it, and run from the table. Moderate change inside `runtime/vu/vu1.cpp`; expected to roughly halve the cost.
   - **Native translation of the microprograms** to C++ (generated locally from the game's own programs, like the EE code, with the interpreter as fallback for unknown programs). The largest gain and the largest change; it needs its own tool and tests.
   Start with predecoding; translate only if the measurement after it still misses the budget.
2. **GS packet handling** (7 to 8 ms): in GPU-only mode the CPU model still parses every packet to validate it before the renderer gets it. Find what of that is needed per packet.
3. **Game code and services** (12 to 14 ms): not profiled yet. The branch dispatch of recompiled calls and the per-call bookkeeping are the first suspects.
4. **VIF1 unpacking** (about 4.6 ms): the code list is checked as a whole and then run, two passes.
5. **Presenting without readback** (about 3 ms): show the renderer's image through a swapchain instead of copying it to the processor and drawing it with GDI.

Each step: measure with `BT2_VIEWER_STATS`, compare `vu1_kick_hash` and the run's counters with the baseline, run the gate, record before and after in the monthly history.

## Verification

- Unit tests for VU1 and VIF1 (`bt2-vu1-tests`), the gate's frame comparisons and renders.
- The output hash and instruction count on the fight script.
- The user's observation in the window, with sound: no gaps where the run holds the game's speed.
