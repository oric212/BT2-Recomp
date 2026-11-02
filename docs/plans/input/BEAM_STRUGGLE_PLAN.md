# Beam struggle by buttons

The user's request (2026-11-02): in a beam struggle, mash Square or Cross instead of rotating both sticks, with the
on-screen prompt showing Square/Cross; a setting, on by default. The original controls stay available (setting off).

## What is known

- **The port controls what the game reads.** Each VBlank the native pad service writes the whole DualShock 2 report
  (buttons, the four stick bytes, pressures) into the game's two pad buffers (`services/pad_service.cpp`,
  `services/pad_block.h`). Measured 2026-11-02 (two runs of the same CPU fight, the left stick held left and the right
  stick held down against both at rest, EE memory compared): the buffers are at 0x3B0B00 and 0x3B0B80 (sticks at
  +0x1E to +0x21 as in the report), and the pad library's copy is at 0x3B0C00 (buttons at +0x14, RX, RY, LX, LY at
  +0x16 to +0x19). The code that addresses 0x3B0C00 is the cluster at 0x333B80 to 0x334668 (the pad library).
- **Game functions can be replaced** by host code with a check of the guest code they stand for
  (`ee/native_functions.*`): the place for a struggle input that counts presses.
- **Not known yet:** the code that turns stick rotation into the struggle's push, its state (which player, the
  gauge), and how the prompt is drawn. Static searches for the stick offsets found sprite builders that use the same
  offsets for colours, not input; the struggle's code has to be found in a run that reaches a struggle.

## Order

1. **Input that reaches a struggle, repeatably.** Pad scripts and `BT2_PAD_RECORD` hold stick positions (built
   2026-11-02: `lx=HH`, `ly=HH`, `rx=HH`, `ry=HH` in a step; `host_input_tests.cpp`). The user plays into a beam
   struggle with `run_startup_probe.py --config Release --play --record <file>`; the recording replays it.
2. **Find the struggle's code.** Replay to just before the struggle and into it; the reports' `ee_entered_addresses`
   differ by the functions the struggle runs. Among them, the one that reads the player's sticks (call trace,
   `BT2_EE_CALL_TRACE`) and what it writes (the push per field).
3. **Replace the input part.** A native replacement of that function (checked code hash) that, with the setting on,
   counts new presses of Square or Cross where the original measures rotation, and writes the same result. Its scale is
   set so steady mashing matches steady rotation (measured in the original function: the push per field at a steady
   rotation speed), so struggles are neither easier nor harder; the CPU side is untouched. With the setting off the
   generated function runs. Tests: the press counting, the scale, the fallback.
4. **The prompt.** Find what draws the rotation prompt (texture name in `BT2_TEXTURE_NAMES` during the struggle) and,
   with the setting on, draw Square/Cross art of the project's own in its place (as a texture replacement the
   renderer applies with or without an HD pack).
5. **Setting** "Beam struggle by buttons" (Controls, on by default; `beam_struggle_buttons` in the settings file),
   applied from the next start like the other game patches, or at once if the replacement reads it each call.

## Verification

Replays of the recorded struggle with the setting off: the game state identical to the generated code
(`BT2_STATE_HASH`). With it on: the struggle won by mashing at the measured rate, lost without input; the user's play.
