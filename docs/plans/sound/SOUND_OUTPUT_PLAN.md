# Sound output

Make the game audible. Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). The game has two independent sound paths, and they need different amounts of work.

## Evidence (2026-10-13)

- **Stream audio (movies, music; possibly voices):** the game's own CRI ADX library runs on the EE. It decodes there and uploads finished PCM to IOP memory, then announces each piece on a stream channel (command `0x100` {source, IOP address, bytes}). The IOP side only plays: 512 output samples per block at 48 kHz, 16-bit mono per source, two sources per player; a 24 kHz player uses half-size blocks. The native CRI service already models this exactly, because the game paces movies and music on the "played" reports ([Sofdec plan](../video/SOFDEC_PLAN.md), `runtime/services/cri_audio.h`). **No ADX decoder is needed natively.**
- **Sound effects:** a separate path through the SOUNDS module and the sound driver (`runtime/services/sound_service.cpp`). The game uploads sound banks (SOUNDS `0x20` with two IOP blocks, 5 banks loaded by the battle) and requests sounds (`0x40` and others; 399 SOUNDS calls in the 9,000-frame battle run). The driver transfers blocks to sound RAM (`0x80D0`). The replies are answered as logged; the requests' contents and the bank format are not decoded yet.
- **Stream commands seen in the runs** (the report's `cri_stream_commands` lists each distinct {code, argument 1, argument 2} with its count; boot run and battle run, 2026-10-13):
  - `9` {object, channel, pan}: always channel 0 with −15 and channel 1 with +15. That matches the ADX pan range (−15 left to +15 right), so channel 0 is left and channel 1 right. That the player's first buffer is channel 0 is still an assumption.
  - `5` {object, value}: the value is 0 in all 69 uses. If it is the ADX output volume (0 = no attenuation), the game plays every stream at full volume and sets no per-stream balance.
  - `4` {object, rate}: 48000 and 24000. `2` {object, 0 or 1}: stop and start. `3` and `8` {object, 0 or 1}: meaning unknown.
  - The third argument word often holds leftovers of earlier records (values such as `0x65`, `0x13`), so only the arguments named above are evidence.
- **The user's listening check (2026-10-13, battle run WAV):** music too loud, voices too quiet, effects too loud. No stream command explains a balance, and the output often reaches full scale where two players overlap (the mixer adds them at full level). The original plays streams through SPU2 voices with their own volume and the SPU2's master volume, which the mixer does not model. This needs the original as reference, not a guess.
- **Not known:** the level at which the original's IOP player and the SPU2 put a stream on the output; the bank format and the meaning of the SOUNDS request arguments; whether voices use streams or banks.

## Design

1. **Stream mixer** (`runtime/services/audio_mixer.h`), done: the CRI flow passes on each stretch of PCM a playing player consumed (`on_audible`: source, side, rate, IOP address, bytes). Data that a stop only reports back, and data of a player destroyed while playing, is not heard. Each source is a voice: mono, routed left (the player's first buffer) or right (its second), upsampled to 48 kHz by an integer factor with linear interpolation. One field of output is 800.8 stereo samples (the fraction is carried). A voice starts with one block of silence, because data arrives in 512-sample blocks while a field takes 800.8 samples; with that its queue cannot run dry while the player plays.
2. **Output:** the runtime hands each field's samples to `on_audio_field`. The probe writes them to a WAV file (`BT2_AUDIO_WAV=<path>`). A host audio device comes with real-time play (the viewer), where the device's clock, not the field count, has to pace playback.
3. **Sound effects** (next): decode what the SOUNDS requests ask for. Steps: log the requests of the battle run with their arguments; identify the bank layout from the uploaded blocks (header block and sample block; the samples are expected to be SPU2 ADPCM); then a voice player (ADPCM decode, pitch, volume, pan, envelope) for the request types the game uses, mixed with the streams. The original's SPU2 output is the reference: a PCSX2 recording of a known sound.

## Verification

- Unit tests: the flow's audible stretches (stop and destroy cases), the mixer (start silence, interpolation, routing, field length, saturation, a dry voice, refused rates).
- Measured (7,000-frame battle run, 2026-10-13): 10,334 fields give 172.4 s of audio; 19.4 MB of stream PCM consumed; 5,939 samples (0.03%) were missing when a voice ended or ran dry. The output is audio-like (sample-to-sample correlation about 0.98, left and right partly correlated). **Not compared with the original.**
- Listened to by the user: the audio is recognizable; the balance is wrong (see the evidence above).
- **Next: comparison with PCSX2.** Reference run without input: boot to the title screen (logo movie, opening movie), recorded with PCSX2's capture (audio). The port's audio of the same run is `work/audio/boot-run.wav` (116.8 s, 6,200 frames, default pad script). Compare the level (peak and RMS per second), channel order, length and drift; then set the stream level from the measured ratio and look for where the original gets it (the driver's voice volume or the SPU2 master volume).

## Stop conditions

A sampling rate that does not divide 48 kHz, PCM outside IOP memory, or a SOUNDS request whose meaning the evidence does not settle stops at a checked boundary or stays silent and documented; no invented sound.
