# Resume handoff

Updated: 2026-10-12. **No pass in progress.**

The verified checkpoint is **1,829 entered / 1,819 completed** (1,000 presented frames, the probe's frame budget) on native IOP services and the native IPU. The opening movie decodes and presents its first three pictures, then waits. History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Opening movie playback ([Sofdec plan](../plans/video/SOFDEC_PLAN.md#progress-2026-10-12-later), [IPU plan](../plans/video/IPU_PLAN.md#progress-2026-10-12-later)).

1. Find how the movie player learns audio playback progress: trace the EE writers of the object fields after the second stream channel (`0x366458`...) in the generated code, and compare two PCSX2 captures taken some fields apart (`capture_ee.py --capture-point rpc-cri-stream-reply --cri-stream-code 2 --cri-stream-delay N`).
2. Supply that progress from the native CRI service (logical time, one field per VBLANK), with a test per reply.
3. Compare decoded movie frames with PCSX2 (the first picture is a flat green field in a black letterbox in both renderers; not yet confirmed against the original).
4. GPU rendering: a PCSX2 GS dump still decides the one-step rounding differences (user).

## Notes

- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-12) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/` (audio start, audio data, play, five seconds into playback).
