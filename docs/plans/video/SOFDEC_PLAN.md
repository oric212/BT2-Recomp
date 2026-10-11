# Opening movie: Sofdec video and ADX audio

Play the game's Sofdec movies natively. Step 5 of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). Audio output follows the [native services plan](../iop/NATIVE_SERVICES_PLAN.md) (sound and CRI services).

## Evidence (2026-10-11)

- **Files** (ISO, `DATA/`): `LOGO_O.SFD` (2.9 MB), `ZS2USOP.SFD` (opening, 97 MB), `ZS2USED.SFD` (ending, 162 MB). Audio streaming uses the IOP modules `CRI_ADXI.IRX` and `CDVDSTM.IRX`, both replaced by the native CRI service.
- **Video:** the game sets up the IPU (decoder tables, thresholds, reset) before the movie; no IPU decoding command has been reached yet (`runtime/ee/ipu_device.*`).
- **Current stop:** after GIF chain mode, startup runs to 1,259 entered functions and stops at the native CRI service's checked boundary (selector `0x14`, pc `0x32EB88`): the EE's CRI stream buffer (IOP `0x191740`) carries its first commands for the object at `0x1B5E2C`:

  | Offset | Words |
  | --- | --- |
  | `+0x00` | `2` (command count, presumably) |
  | `+0x10` | `8`, `0x1B5E2C`, `1` |
  | `+0x20` | `5`, `0x1B5E2C`, …, `0x65` (at `+0x2C`) |

  Until then every returned stream buffer was empty (sequence counter only). The IOP's answer was captured on 2026-10-12 (see Progress).
- **Capture:** PCSX2 and the BIOS used by the existing captures are available locally, so the original's answer can be captured with `capture_ee.py`.

## Progress (2026-10-12)

- **CRI stream replies:** captured in PCSX2 (`capture_ee.py --capture-point rpc-cri-stream-reply`, unattended: scripted LEFT/CROSS presses answer the absent-card prompt). The IOP returns the buffer with the command count cleared and the records unchanged; the native CRI service now does the same for commands 8 and 5 (`runtime/services/cri_stream.h`) and refuses other codes.
- **Movie path:** the run then reaches 1,590 functions and stops at the first **toIPU DMA** (channel 4, `0x1000B400`, pc `0x2CB384`): the game's own Sofdec library feeds the IPU. Step 3's choice is therefore **native IPU commands** (the game code stays unchanged).
- **Next:** write `docs/plans/video/IPU_PLAN.md` (toIPU/fromIPU DMA, the IPU commands the movie issues, MPEG-1/2 macroblock decoding and CSC), then implement it.

## Approach

1. **CRI stream replies (done 2026-10-12):** capture EE and IOP RAM in PCSX2 at the first VBlank after these commands are uploaded, and at the next point the EE reads the returned buffer. Derive what the returned buffer holds for commands 8 and 5 (object state, play position, status). Implement those replies in `runtime/services/cri_service.cpp`, one test per command, and keep unknown commands at the checked boundary.
2. **Find the movie path (done: the IPU):** run to the next boundary and record whether the movie reaches IPU commands (EE-side Sofdec decoding through the IPU) and which EE library functions drive it (neutral names).
3. **Choose the boundary** from that evidence:
   - **IPU commands natively** (MPEG-1/2 macroblock decoding, CSC) if the game's own Sofdec library drives the IPU directly: smallest change, game code unchanged.
   - **Replace the Sofdec library entry points** through the function table with a native SFD demuxer and decoder, if that boundary is clear and the IPU path is costly.
4. **ADX audio:** decode ADX natively (CRI ADPCM) behind the CRI service and play it through a host audio backend; keep the stream timing the game observes (returned buffers, positions).

## Verification

- Unit tests per CRI command reply and per IPU command (or per replaced library call).
- Compare EE-visible state with PCSX2 captures at the movie start; compare decoded frames with PCSX2 frame captures.
- The run reaches the title screen with the movie shown, or skipped through the game's own skip path with identical state.

## Stop conditions

A reply or decoding step that cannot be derived from captures or public format knowledge stops at a checked boundary. Never fabricate stream status.
