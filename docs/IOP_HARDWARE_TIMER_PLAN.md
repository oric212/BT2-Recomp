# IOP hardware timers and the SOUNDS worker

## Why this is a new subsystem

Main's first sound-bank load (`0x297A60`) ends with SOUNDS command `0x20` (256-byte request from `0x3B0E00`: bank handle `0x5010` and its two IOP blocks). The original handler starts a higher-priority SOUNDS worker, wakes it (SleepThread/WakeupThread), converts a period with USec2SysClock and then calls `timrman` ordinal 4 (AllocHardTimer) to drive a periodic tick from an IOP root counter interrupt. The runtime has no IOP root counters, no timer interrupts and no periodic IOP time; the command therefore stops before the IOP is entered (`sounds_hardware_timers_supported` is false). Executing it already covers the worker start, sleep/wakeup and clock conversion in the sound-command scope.

## Bounded design

1. Trace the worker after AllocHardTimer: timer source/prescale, SetTimerCompare/SetupHardTimer/StartHardTimer calls, the registered handler and what each tick does (sequencer step, SPU2 register writes, iWakeupThread).
2. Model allocated IOP root counters in logical time derived from the EE logical VBlank (one NTSC field per VBlank), with compare interrupts delivered to the registered handler at EE scheduling points, as Timer 2 is on the EE side.
3. Run woken IOP workers after each tick until they park; fail closed on any wait the scheduler cannot own.
4. Enable SOUNDS command `0x20`; compare the request result and SOUNDS state with `rpc-sound-bank-after`; capture the original after a few frames if tick-driven state is consumed by the EE.

## Limits and stop conditions

No audio output, voice playback or sequencer timing is claimed. Stop if a tick requires a suspended RPC frame or hardware outside the owned SPU2/timer model.
