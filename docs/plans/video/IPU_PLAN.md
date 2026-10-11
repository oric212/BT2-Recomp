# Native IPU

Decode the opening movie's MPEG video with a native implementation of the IPU's commands, driven unchanged by the game's own Sofdec library. Part of the [Sofdec plan](SOFDEC_PLAN.md) and the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step).

## Evidence (2026-10-12)

- **Setup is modelled:** `runtime/ee/ipu_device.*` handles the registers, the input FIFO and the setup commands (BCLR, SETIQ, SETVQ, SETTH). Decode commands and IPU DMA starts are refused.
- **Current stop:** the first toIPU start (channel 4) at pc `0x2CB384`: CHCR `0x30000105` (source chain from memory; the upper half is the read-only TAG field carried back), TADR `0xBC9880`. The chain is a ring of REF tags, each pointing at a 2 KB bitstream chunk (`0xB49880`, `0xB4A080`, …).
- **The stream:** the first chunk starts with an MPEG sequence header (`00 00 01 B3`): 512×448, frame-rate code 4 (29.97 Hz), with loaded quantiser matrices. Sofdec parses headers itself through the IPU's bit-level commands.

## Progress (2026-10-12)

- **Done (order step 1, input side):** `runtime/ee/ipu_input.*` pulls toIPU source chains on demand into the FIFO and bit buffer (IPU_BP with BP/IFC/FP, IPU_TOP); FDEC; commands without enough input stay pending with BUSY set and complete when a transfer or FIFO write supplies it. Tests in `bt2-ipu-tests` (chain pacing, FDEC across tags, the BUSY wait).
- **Map:** the movie code reached a function whose jump-table cases Ghidra had split off; the candidate export now keeps functions whose holes no other function owns, and the switch prover accepts calls inside case blocks and a last block falling through into the following span (four more functions proven, 9,504 mapped).
- **Also:** a running toIPU channel keeps the FIFO full (filled on start and after every consumption), and clearing STR stops it with MADR/QWC/TADR and the FIFO kept (the movie stops and restarts the chain); 64-bit reads of IPU registers return BUSY in bit 63.
- **Reached:** the movie starts the chain and parses the MPEG sequence header with FDEC (start code, then 32, 32 and 30 bits). It then issues no further command; its decoder thread (7) polls IPU_BP, IPU_CMD and IPU_CTRL (ECD, bit 14) in the wait loop at `0x31A3C8` until the probe's execution budget ends.
- **Next:** find what the wait loop at `0x31A3C8` expects after the header (likely the next FDEC field, an end-code condition or the IPU interrupt), then VDEC and the macroblock commands.

## IPU behavior to implement

Public IPU semantics (register layout, command set, MPEG-1/2 decoding from ISO/IEC 11172-2 and 13818-2). Adapt reference behavior; do not copy PCSX2's or libmpeg2's code.

| Part | Behavior |
| --- | --- |
| Input | toIPU DMA (normal and source chain, via `runtime/ee/dma_chain.*`) feeds the 8-quadword input FIFO **on demand**: as commands consume bits, the next quadwords are taken from the running transfer. CHCR STR, MADR, QWC and TADR advance as on hardware, so the game sees the same refill points. |
| Bit reader | IPU_BP (bit pointer, FIFO quadword count, FP), IPU_TOP (peek of the next 32 bits), BCLR. |
| FDEC / VDEC | Fixed-length and variable-length header fields: macroblock address increment, macroblock type (I, P, B; MPEG-1 and MPEG-2 tables), motion code, DMV, coded block pattern. Results in IPU_CMD. |
| IDEC / BDEC | Intra and block decoding: DC/AC coefficient VLC (tables zero and one), inverse quantisation with the SETIQ matrices and QSC, alternate scan, IDCT, saturation; output RAW8/RAW16 blocks or, for IDEC, CSC to RGB32/RGB16 with optional dithering (DTE) and the threshold alpha (SETTH). |
| CSC / PACK | Colour conversion of RAW8 macroblocks to RGB32/RGB16 and the 4-bit indexed PACK through the SETVQ table. |
| Output | fromIPU DMA (channel 3, normal mode) drains the output FIFO to main RAM. |
| Status | Commands complete synchronously (BUSY never observed set); ECD/SCD and error bits in IPU_CTRL as the stream defines them; the IPU interrupt only if the game enables it. |

## Order

1. **toIPU input and the bit reader:** on-demand FIFO filling from the chain, IPU_BP/IPU_TOP, then FDEC and VDEC with the header tables, until the game reaches its first macroblock command. Unit tests with synthetic bitstreams.
2. **Macroblock decoding:** the command the movie uses first (IDEC or BDEC), with IDCT and dequantisation, and fromIPU output. Tests against reference blocks computed from the MPEG specification.
3. **CSC / PACK** as reached.
4. **Frames:** compare decoded movie frames with PCSX2 frame captures (visual first, then per-pixel where the IPU is exact).

Each refused command or mode names itself (command code, field) at a checked boundary.

## Verification

- Unit tests per command and VLC table, with synthetic streams (no game data).
- The run advances through the movie; the presented frames match PCSX2 captures.
- Release timing of the decode on the movie (time per frame), reported separately from startup.

## Stop conditions

A command or mode the specification and captures cannot pin down stops at a checked boundary.
