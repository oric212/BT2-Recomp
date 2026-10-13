# Sound verification

How to tell that the port's sound matches the original without relying on listening. Companion to the [sound output plan](SOUND_OUTPUT_PLAN.md); part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step).

## Why (2026-10-13)

The first two checks were by ear and by one loose recording. Listening found a real fault (the level) but cannot confirm a fix: after the correction the user heard an improvement and could not say whether it is right. The recording comparison settled level, channel order and timing for two movies, but not the waveform, because PCSX2's output was time-stretched, and not the menus or the battle, because the input differed on the two sides.

The rule from here: **each stage of the sound path is compared where it is exact, with numbers and a pass limit.** Listening stays as a last sanity check only.

## Stages and their checks

Sound passes through four stages. Each has its own reference in the original, and the earlier a difference is caught, the easier it is to explain.

| Stage | What is compared | Reference | Exact? |
| --- | --- | --- | --- |
| 1. Requests | The sound requests the game makes, in order, with their arguments and the field they occur in | PCSX2 call logs (`capture_ee.py --sounds-log`, `--cri-stream-log`) | Yes |
| 2. Sample data | The PCM the game uploads for streams; the bank data it uploads for effects | PCSX2 IOP memory at the same point | Yes, byte for byte |
| 3. Voice parameters | For every started voice: pitch, left/right volume, envelope settings; the master volume | SPU2 registers read from PCSX2 at the moment a voice starts | Yes |
| 4. Output | The mixed signal | A PCSX2 recording made without time stretching, with the same input | Within stated limits |

### 1. Requests

The port answers every CRI stream command and SOUNDS call already, and both can be logged on the original. Compare the two logs: same calls, same arguments, same order. Fields may differ slightly where loading time differs, so order is checked exactly and timing within a tolerance. This catches "the wrong sound" and "a sound too many or too few" without any audio.

### 2. Sample data

Stream PCM is produced by the game's own decoder, so it must be identical on both sides. Capture the IOP buffer a stream announces (the existing `rpc-cri-stream-reply` capture point holds the address and size) and compare it with the bytes the port passes to its mixer. One match per stream type (48 kHz movie, 24 kHz music) is enough to close the question the recording left open: if the PCM is identical, the waveform difference lies in PCSX2's output stage. The same comparison applies to the bank blocks the game uploads for effects.

### 3. Voice parameters

This is where the stream level of 25/64 should come from instead of a measurement. The original's IOP modules set SPU2 voice and master volumes; PCSX2 exposes the SPU2 register block in IOP memory. Add a capture that reads those registers when a voice is keyed on. For streams it gives the volume the CRI player uses; for effects it gives pitch, volume and envelope per request, which is exactly what the port's voice player has to reproduce. The port then logs the same values for its own voices and the two lists are compared.

### 4. Output

Only after stages 1 to 3 agree. Requirements for a usable recording:

- **No time stretching:** PCSX2's audio synchronisation set to none, so samples are not warped.
- **Same input:** the same button presses on both sides. Input-free stretches (boot movies, the idle demo fight) need nothing. For menus and battles, play back one input recording in PCSX2 and convert it to a pad script for the port (a small converter in `tools/audio/` or `tools/capture_ee/`).
- **Numbers, with limits,** from `tools/audio/compare_audio.py`, extended as needed:
  - offset drift over the stretch: below 5 ms;
  - level ratio per frequency band: within 1 dB;
  - short-window coherence: above 0.95 where the signal is not silent;
  - for effects, per event: onset time, level and pitch of each detected sound.

The limits are starting values, to be tightened once a clean recording shows what the original's own run-to-run variation is.

## Order of work

1. Stream PCM against PCSX2 memory (stage 2): small, closes the waveform question.
2. SPU2 register capture (stage 3) for a playing stream: replaces the measured level by the original's setting.
3. Request-log comparison (stage 1) for the battle run, before any effect is made audible.
4. Effects: bank data (stage 2) and voice parameters (stage 3) as the voice player is written.
5. Output recordings without time stretching (stage 4), first the input-free stretches, then with a shared input recording.

Each comparison that passes becomes a gate comparison where its reference capture is small enough to keep locally.

## What the user is asked for

- A PCSX2 recording with audio synchronisation set to none, when stage 4 is reached.
- An input recording for a menu-and-battle stretch, when the converter exists.
- Listening only at the end, as a check that nothing obvious was missed.

## Limits

- Captures and recordings are proprietary-derived and stay local; only numbers are recorded in the documents.
- Stage 4 cannot be bit-exact: the original's SPU2 resamples and interpolates, and its reverb is not modelled yet. The limits above are the claim, not identity.
