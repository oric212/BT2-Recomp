# The CPU opponent's AI against the original

The user's request (2026-11-03): make sure the enemy AI works as it should, verified with PCSX2.

## What the AI is in the port

The AI is the game's own code, recompiled: no part of it is replaced. Each frame the input reader (0x207630) asks it
for a CPU fighter's buttons and sticks (`jal 0x1214F8` at 0x207760, `a0` the side, `a1` a 4-byte stick buffer on the
caller's stack; the buttons come back in `v0`). So the AI behaves as the original's exactly when the recompiled code
computes what the original computes from the same state: its arithmetic (integer, FPU, VU0 macro instructions), its
random numbers (state in memory), and every function it calls. A difference would come from the runtime, not from
the AI.

## Approach: one AI call, exact

Fights cannot be compared frame by frame between PCSX2 and the port (their timing differs, so the states drift
apart). One AI call can be compared exactly:

1. **PCSX2, before:** the title's demo battle (CPU against CPU; in the port from flip 10,301 of a boot with a card
   and no input) runs in an instrumented copy of the executable whose call at 0x207760 goes through a stub that counts
   the calls and, at the Nth, publishes the registers the call uses (`a0`, `a1`, `sp`, `gp`) and halts before it.
   EE RAM is read out of the PCSX2 process (`tools/capture_ee/capture_ee.py`, the existing mechanism).
2. **PCSX2, after:** the same with the halt after the call returns (`v0` published too).
3. **Determinism:** two "before" captures of the same N are equal (else PCSX2's run is not repeatable and the method
   changes).
4. **Port:** the "before" RAM loaded, the recompiled 0x1214F8 run with those registers to a return sentinel, and the
   RAM and `v0` compared with "after" byte for byte, except the stack below `sp` (the callee's own frame, whose
   contents depend on registers not captured). Differences outside that are listed with the function that wrote
   them (store watch, call trace).
5. **Several calls:** N spread over the demo battle (approach, combos, guard, a super if one happens), both sides.

## Unknowns

- Whether the demo battle runs unattended in PCSX2 as in the port (the boot needs a card or the absent-card answer
  the capture tool already has).
- Whether the AI reads anything outside RAM (hardware timers for randomness): a difference there would show as a
  `v0` or RAM difference traced to such a read.
- Interrupt handlers running during the captured call in PCSX2 (VBlank) may write RAM: such words are told apart by
  address (the handlers' counters).

## Status (2026-11-03, pass ended by the user before the first comparison)

Built, not yet run to a result:
- `tools/capture_ee/capture_ai_call.py --pcsx2 <exe> --bios <bios> --disc <iso> --card <PCSX2 card with a BT2 save>
  --call <N> --when before|after`: the stub at 0x207760, PCSX2 unthrottled (`FrameLimitEnable = false`) with the
  null renderer and a copy of the card, so the boot has no prompts; RAM and registers to
  `work/pcsx2-capture/ai-call-<when>-<N>/`.
- The probe's `BT2_REPLAY_CALL="<RAM in>|<function>|<a0>|<a1>|<sp>|<gp>|<RAM out>"` (`runtime/probe/call_replay.cpp`):
  one function on a captured RAM from main's prepared context to a return sentinel; RAM written, v0 printed.
- `tools/capture_ee/compare_ai_call.py --disc <iso> --call <N>`: runs the replay on the "before" capture and lists the
  words that differ from the "after" capture (outside the AI's stack frame and the stub), marking those the port wrote.

Found on the way:
- On this PC: PCSX2 in `D:\Emulation\PCXS2`; the BIOS of the earlier captures is `scph39001.bin` (US; its hash in
  the captures' records); `memcards\Mcd001.ps2` holds a BT2 save.
- The capture tool's stub segment at 0x01000000 is game heap by a fight: the first capture's stub was overwritten (the
  counter read garbage, PCSX2 then ran unknown instructions). The stub now sits at 0x01D00000: 0x1BF4000 to 0x1EFD000
  stayed zero in all 13 port RAM dumps at hand (title, fights, the demo battle); in PCSX2 the counter stayed clean.
- In the port the boot with a card and no input reaches "Press START button" by flip 8,500 and the demo battle at flip
  10,301 (2,728 AI calls; then the title again), so N = 400 falls in the demo battle.
- PCSX2's log still says "target speed: 1" with `FrameLimitEnable = false`: whether it runs unthrottled is not
  confirmed (the second capture was stopped 45 s in, at the films).

Next: the "before" and "after" captures of call 400 (a run takes up to the demo battle, minutes), a second "before"
for PCSX2's repeatability, the comparison; then calls across the demo battle and both sides.

## Checks

The listed comparisons; the gate unchanged (the port's code is not changed by this plan unless a difference is
found, which then gets its own fix and test).
