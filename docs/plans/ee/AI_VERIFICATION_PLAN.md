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

## Result (2026-11-03): the AI matches PCSX2 exactly, after four rounding fixes

**Verified:** at three calls in the demo battle the port's recompiled AI, run on PCSX2's RAM, leaves exactly the
words PCSX2 leaves and returns the same `v0`:

- `input` 400 (0x1214F8, side 1): nothing written, `v0` 0 in both.
- `think` 200 (0x11E988, the frame's AI for every CPU side; side 2's turn): 66 words, all equal.
- `think` 900 (the fight under way): 59 words, all equal.

PCSX2 repeats itself: two "before" captures of call 400 differ only in sound buffers, pad and timer counters and
library state, never in the fighters' data, so pairing "before" and "after" from separate runs is sound.

**What was wrong, found by the comparison** (each a last-bit difference in the AI's vectors and distances):

1. VU0 macro add, subtract and multiply (`PS2_VADD/VSUB/VMUL`, 1,700 sites) used the host's rounding to nearest;
   now toward zero per lane (`ps2_vu_add`, `ps2_vu_sub`, `ps2_fpu_mul`). 17 differing words at `think` 200 became 12.
2. Four operations the recompiler writes inline (not through the runtime's helpers) used host arithmetic: the FPU's
   `div.s` (1,836 sites, also giving infinities on a zero divisor, which the EE has not), VU0's `vdiv` (34),
   `vsqrt` (19) and `vrsqrt` (2; the recompiler also ignored its numerator). `tools/ps2recomp/ee_rewrite.py`
   rewrites them after each generation (`generate_game.py`, `generate_interpreter.py`; the discovery loop's receipt
   check applies it to the old output first): `ps2_fpu_div`, `ps2_vu_div`, `ps2_fpu_sqrt`. 12 became 5.
3. Division rounds differently by unit: the FPU's `div.s` to nearest (as PCSX2's separate FPU division rounding
   setting does), VU0's `vdiv` toward zero. Measured: with both toward zero `think` 200 had 5 differing words, both
   to nearest 2, split 0. Square roots toward zero: to nearest broke `think` 200.
4. The EE FPU's adder keeps one guard bit: the operand of the smaller exponent loses its low (difference - 1) bits
   before the addition, and is a signed zero from a difference of 25 (PCSX2's FPU recompiler `FPU_ADD_SUB`, always
   on there; read for the rule, not copied). `add.s`, `sub.s` and `adda.s` (`ps2_fpu_add`, `ps2_fpu_sub`); VU0's
   add has no such alignment (`think` 200 matches without it). This was `think` 900's last two words: a distance of
   195.19 whose subtrahend lost bits.

Tests: `fpu_tests.cpp` (each operation against a reference written separately: the alignment rule, division to
nearest for the FPU and toward zero for VU0, VU0's add without alignment), `test_ee_rewrite.py`. The gate's battle
run fights slightly differently with the original's rounding and has a new baseline (`tools/gate/battle_run.py`).

Tools: `capture_ai_call.py --site input|think` and `compare_ai_call.py --site` (replaced 2026-11-12 by the sessions of the
[PCSX2 comparison plan](../validation/PCSX2_COMPARISON_PLAN.md), sites `input` and `think`), the probe's `BT2_REPLAY_CALL`, and
`BT2_EE_WATCH_WORD=<file>:<address>` (which call a scalar changed across; it found the distance's writer).

Not checked: whether PCSX2 itself matches the hardware in these operations (it is the reference here); other
floating-point code than the AI's (the fixes are general, so the whole game now rounds this way); VU1 (its own
programs, checked against the interpreter in the gate).

## Checks

The listed comparisons; the gate unchanged (the port's code is not changed by this plan unless a difference is
found, which then gets its own fix and test).
