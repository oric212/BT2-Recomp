# Sound output

Make the game audible. Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). How each step is checked is in the [sound verification plan](SOUND_VERIFICATION_PLAN.md). Revised 2026-10-13 against the [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md) and the two BT3 references' sound code and notes.

## What the references say

Reference behaviour is adapted, never copied, and nothing about BT3 is assumed for BT2 without BT2 evidence (no addresses, formats, constants or code).

### Tenkaichi3Decomp (native port of the next game on this engine)

From its port log (`docs/port/README.md`) and the names of its port sources (`port/src/gs/snd_se.c`, `snd_adx.c`), snapshot `80e0dd42`:

| Topic | What it reports | Use here |
| --- | --- | --- |
| Effects boundary | `SOUNDS.IRX` is replaced behind the SIF RPC call, the SIF DMA upload and the sound driver's remote call. It captures banks into a 2 MB sound memory and keeps a per-frame command queue: play, stop by number, stop by handle, pause, stop bank, reset, mono, bank volume | The same boundary as our native SOUNDS and sound driver services. The command list is a checklist for decoding BT2's requests |
| What the original uses | Sony's sequencer and synthesizer modules (`modsesq2`, `modhsyn`); the play call takes port, sequence, loop, volume, pan, pitch | BT2's disc holds the same module names and the chunk names `Vagi`, `Setb`, `Sesq` (checked locally, 2026-10-13), so BT2 very likely uses the same stack |
| How little is needed | Over 361 bank files (18,924 effects): every effect is one note-on, timbre *i* uses sample *i*, all timbres share the same parameter bytes. Conclusion: a sampler is enough — PS2 ADPCM decoding, pitch, pan, 48 voices, one 48 kHz output | The target for BT2's effects, **if BT2's banks show the same regularity**. That survey is to be repeated on BT2's banks before the sampler is written |
| Sample data | Rates from 8,000 to 24,000 Hz, almost all 16,000; three samples loop | What to expect; BT2's banks decide |
| Left out there | The envelope's release (an 8 ms fade), the chip's interpolation, reverb | Candidates to leave out here too, but only where a comparison with the original shows no difference above the limits |
| Effects level | 0.25 of full scale, explained as three volumes of 100/127 and the chip's half-scale voice volume (0.244). Chosen with the user's ear: 0.55 was too loud | A lead for our levels, see below. Our own level is measured, not tuned by ear |
| Music and voices | ADX players for music **and voices**, each on its own output stream: stop, pause, volume in 0.1 dB steps, pan per channel, mono, status. Files are plain ADX at 16, 24 or 48 kHz | Confirms that voices are expected on the stream path, and reads our stream commands: a volume in 0.1 dB steps and a pan per channel are what commands `5` and `9` look like |
| Verification | The ADX decoder against ffmpeg; effects by the user's ear only. Not verified: individual effects against the console, pitch accuracy, timed and looping effects | Shows the gap our verification plan is meant to close |

### BT3-Recomp (recompilation of the next game)

Its runtime (`ps2xRuntime/src/lib`, snapshot `e22a34be`) routes the sound library's RPC calls to an audio backend (`ps2_iop_audio.cpp`), with a separate ADPCM decoder (`ps2_audio_vag.cpp`), a mixer (`ps2_audio.cpp`) and a host output (`ps2_host_audio.cpp`). Use here: the same separation — service handlers, a decoder, a mixer, a host device — in a recompilation project, which is the structure our `runtime/services/` and `runtime/host/` folders already follow.

### Behavioural references

| Reference | What it gives |
| --- | --- |
| PCSX2 SPU2 sources, pinned in the reference document (`RegTable.cpp`, `spu2sys.cpp`, `Mixer.cpp`) | The sound chip's behaviour: ADPCM blocks of eight halfwords giving 28 samples, 4.12 fixed-point pitch, end/repeat/loop-start flags, the volume chain from voice to master |
| PS2SDK `libsd` (`freesd.c`, `spu2regs.h`), pinned | Register layout and the library calls the game's sound driver is built on |
| Chronicle's PCSX2 practice | Every comparison records the emulator version, input hash, scenario and observations |
| This project's earlier sound work ([SPU2 voices](SPU2_VOICE_PLAN.md), [SPU2 initialization](SPU2_INITIALIZATION_PLAN.md), [sound driver RPC](SDRDRV_RPC_PLAN.md)) | Written while the IOP was emulated: which SPU2 registers the game's modules write, with comparisons against local PCSX2 captures. The code is gone; the observations about the original remain |

## Evidence from BT2 (2026-10-13)

- **Streams (movies, music):** the game's own CRI ADX library runs on the EE. It decodes there and uploads finished PCM to IOP memory, then announces each piece on a stream channel (command `0x100` {source, IOP address, bytes}). The IOP side only plays: 512 output samples per block at 48 kHz, 16-bit mono per source, two sources per player; a 24 kHz player uses half-size blocks. The native CRI service models this exactly, because the game paces movies and music on the "played" reports ([Sofdec plan](../video/SOFDEC_PLAN.md), `runtime/services/cri_audio.h`). So BT2 needs no native ADX decoder; the reference port needs one only because its game code was rebuilt for the PC.
- **Stream commands** (the report's `cri_stream_commands`; boot run and battle run):
  - `9` {object, channel, pan}: always channel 0 with −15 and channel 1 with +15, the ends of the ADX pan range, so channel 0 is left and channel 1 right.
  - `5` {object, value}: 0 in all 69 uses. Read as the ADX volume in 0.1 dB steps, the game plays every stream without attenuation.
  - `4` {object, rate}: 48000 and 24000. `2` {object, 0 or 1}: stop and start. `3` and `8` {object, 0 or 1}: meaning unknown.
  - The third argument word often holds leftovers of earlier records, so only the arguments named above are evidence.
- **Effects:** a separate path through the SOUNDS module and the sound driver (`runtime/services/sound_service.cpp`). The game uploads sound banks (SOUNDS `0x20` with two IOP blocks; 5 banks loaded by the battle) and requests sounds (`0x40` and others; 399 SOUNDS calls in the 9,000-frame battle run). The driver transfers blocks to sound RAM (`0x80D0`). The replies are answered as logged; the requests' contents and the bank layout are not decoded. The disc contains the module names `modhsyn` and `modsesq` and the chunk names `Vagi`, `Setb` and `Sesq`.
- **Comparison with the original** (a PCSX2 recording by the user: boot without input through both movies, against the port's boot run; `tools/audio/compare_audio.py`):
  - Timing: constant offset over each movie, no drift over 80 s.
  - Channels: the left/right balance tracks the recording with correlation 0.994.
  - Level: the unscaled PCM was 2.53 to 2.58 times too loud at every frequency, a level of 0.388 to 0.395. With a stream voice output at 25/64 (0.391) the RMS ratio is 1.00 to 1.08 and the peaks agree.
  - Waveform: alike in short windows only (coherence about 0.97, sample correlation about 0.5), as a time-stretched recording would be. Not settled.
  - The recording's emulator version and audio settings were not noted. Later references record them.
- **A lead for the stream level:** one volume of 100/127 times the chip's half-scale voice volume is 0.394, inside the measured range; the reference port explains its effects level with the same two factors (three such volumes there). This is a hypothesis: it is confirmed only when the values the original's modules write are read (order of work, step 1).
- **The user's listening checks:** before the level correction, music too loud, voices too quiet, effects too loud; after it, better, but not something the ear can confirm. Bank effects are not played at all yet, so what was heard as effects is stream audio.
- **Not known:** where the original sets the stream level; whether BT2's voices are streams, as in the reference port; the layout of BT2's banks and whether its effects are as regular as BT3's; the meaning of the SOUNDS request arguments.

## Design

Sound is produced where the game asks for it. Two producers feed one mixer.

1. **Streams — done.** The CRI flow passes on each stretch of PCM a playing player consumed (`on_audible`). Data that a stop only reports back, and data of a player destroyed while playing, is not heard. Each source is a mixer voice: mono, routed left (the player's first buffer) or right (its second), upsampled to 48 kHz by an integer factor with linear interpolation, at 25/64 of its PCM level. A voice starts with one block of silence, because data arrives in 512-sample blocks while a field takes 800.8 samples; with that its queue cannot run dry while the player plays. Still to add when a run needs them: a stream's volume and pan other than the values seen so far (commands `5` and `9`), refused by name until then.
2. **Effects — a native sampler behind SOUNDS,** the shape both BT3 references use:
   - **Banks:** keep what the game uploads (SOUNDS `0x20`: a header block and a sample block) and decode the header's tables into sounds: sample offsets and rates, pitch, volume, pan, envelope, loop points. The sample data is expected to be SPU2 ADPCM. The layout is established from BT2's own blocks.
   - **Bank survey first:** count, over the banks the game loads, how many effects are a single note-on with one sample and shared parameters. If BT2 is as regular as BT3, the sampler needs no sequencer; every exception found is listed and decides what else is needed.
   - **Requests:** a SOUNDS request starts, changes or stops sounds (expected kinds: play, stop by number, stop by handle, pause, stop bank, reset, mono, bank volume). Each kind is implemented when BT2's arguments for it are understood; until then it stays silent and is listed as such.
   - **Voices:** ADPCM decoding, pitch, per-side volume and looping as the sound chip does them, with PCSX2's mixer as the behavioural reference. The release envelope, the chip's interpolation and reverb are added only if a comparison with the original shows a difference above the verification limits without them.
   - The sampler sits at the request boundary. It does not emulate the sequencer and synthesizer modules or their voice allocation unless the game can observe them (the SOUNDS replies carry a status block; so far nothing in it depends on voices).
3. **Mixer** (`runtime/services/audio_mixer.h`): 48 kHz stereo, one field of output per VBLANK (800.8 samples, the fraction carried), saturating. Streams and sampler voices add here, each at the level the original's volume chain gives it.
4. **Output:** the runtime hands each field's samples to `on_audio_field`. The probe writes them to a WAV file (`BT2_AUDIO_WAV=<path>`). A host audio device comes with real-time play in the viewer; there the device's clock paces playback, so the mixer needs a queue with drift control between game time and device time. User volume settings for music and effects, as the reference port has, belong to that step.

## Order of work

1. **Explain the stream level.** Read the volume chain in the pinned PCSX2 mixer and the values the original's modules write (from the earlier SPU2 evidence, or a new capture of the SPU2 registers while a stream plays). Test the 100/127 times one-half hypothesis; replace the measured 25/64 by the original's settings.
2. **Stream PCM against PCSX2 memory,** byte for byte: closes the waveform question the recording left open.
3. **Log the SOUNDS requests** of the battle run with their arguments, on the port and on the original; group them by kind against the expected list.
4. **Bank layout and survey** from the uploaded blocks; a decoder with tests on synthetic banks.
5. **Sampler voices,** request kind by request kind, each checked against the original's voice parameters at key-on.
6. **Host audio device** with the viewer, with volume settings.

Steps 1 to 4 need no new playback code and decide what step 5 has to build.

## Verification

Per the [sound verification plan](SOUND_VERIFICATION_PLAN.md): requests, sample data and voice parameters are compared exactly against PCSX2; the mixed output within stated limits; listening only as a last check. Both BT3 references set their effects by ear; this plan does not.

- Unit tests so far: the flow's audible stretches (stop and destroy cases), the mixer (start silence, interpolation, routing, level, field length, saturation, a dry voice, refused rates), the comparison tool.
- Measured (7,000-frame battle run): 10,334 fields give 172.4 s of audio from 19.4 MB of stream PCM; 5,939 samples (0.03%) were missing where a voice ended or ran dry; no clipping at the corrected level (peak 21,315).
- Every reference capture or recording is logged with the PCSX2 version, its audio settings, the disc hash and the scenario (input used, where it starts and ends).

## Stop conditions

A sampling rate that does not divide 48 kHz, PCM outside IOP memory, a bank layout or a SOUNDS request whose meaning BT2's evidence does not settle: stop at a checked boundary, or stay silent and say so. No invented sound, no level chosen by ear, and no BT3 constant used without BT2 evidence.
