# Resume handoff

Updated: 2026-10-12. **No pass in progress.**

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. The long Release run (`run_startup_probe.py --config Release --frames 9000`) plays the logo movie and the opening movie to the end (7,902 frames, 2,016 / 2,005 functions) and stops at the native SOUNDS service: function `0x50`. History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Goal of the next pass

Reach the title screen.

1. Capture the reply to SOUNDS (`0x02000004`) function `0x50` in PCSX2: 256-byte command (words `0, 0, 0, 0x1F`), 64-byte reply at `0x635180`. It is issued after the opening movie (the unattended pad presses skip the movies in PCSX2, so it is reached quickly); add a capture point at the SOUNDS call's return for a given function number, then answer it in `runtime/services/sound_service.cpp` with a test.
2. Continue the selection loop from there (use `--frames 9000`; every round replays both movies, so batch map discoveries where possible).
3. Movie verification: compare decoded frames with ffmpeg frame by frame (`work/movie-ref/` holds the locally extracted `.SFD` files and reference frames).
4. GPU rendering: a PCSX2 GS dump still decides the one-step rounding differences (user).

## Notes

- The user knows how the game should look and offers to inspect frames saved under `work/frames/`.
- The full gate's generic verifier loop omits per-script inputs; see [PROG.md](../PROG.md#verification-2026-10-12) for which checks need phase stops or their own captures.
- Still captured, not native: the CD init, S-command media mode and LOADFILE bind/version replies around the IOP reset.
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/` (audio start, data, play, five seconds into playback, and the two stream logs).
- Time between the logo movie's end and the opening's start is about 850 fields in our run; PCSX2 starts the opening much sooner. Not investigated (logical time during loading).
