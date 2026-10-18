# Resume handoff

Updated: 2026-10-18. **No pass in progress.** Steps 1 to 3 of the [whole-game coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md) are done: the map closed statically with interior entries registered, a state hash per field, and the EE interpreter bridge (code the map does not hold runs interpreted through recompiler-made handlers instead of stopping; a fully interpreted scripted fight matches the compiled one on every field). Step 4 is done too: `runtime_census.py` gives the work list for step 5 (19 reached system calls the runtime does not name, 5 handled only under gates, 163 gated runtime conditions; VU1 microcode fully implemented; details in the plan). Next, in the plan's order: the tool-only starter of game states and the battle sweep (step 6: find BT2's own menu mode field, battle setup and demo fight start), then step 5 and the rest of 6 in turns.

- **For the user's play:** a session that runs untranslated code no longer stops for it (unless something else is missing); at its end the game says so and names `work\scripts\discover.cmd`, which maps that code (`discovery_loop.py --player-stop`, which also reads the log of a session closed with its window). The build needs the handlers: `python tools/ps2recomp/generate_interpreter.py` after a fresh generation (the discovery loop reruns it).

The verified checkpoint (gate run, 1,000-frame budget) is **2,007 entered / 1,998 completed**. Every mapped function (11,139, plus the 494 of the `DBZP.BIN` overlay) is generated and compiled, with its interior entries registered. History of how this state was reached: [2026-10.md](2026-10.md).

## Where the work stands

The user plays the game with `bt2-game.exe` / `bt2-launcher.exe` and reports; gameplay is checked by their play, not by scripted input. Order of work set by the user on 2026-10-13: the player's programs and saves (done, in test), the hand-written VU1 programs (done), 3D verification against PCSX2 (deferred to a later pass).

**Waiting for the user's test** (all built, gate passed, none confirmed in play yet):

- **Special moves and ultimates** (R2 + triangle, full charge): the stop was an unmapped function; since then everything the programs name is mapped (see Open, Unmapped code).
- **Saving:** write, create, flush, set entry data and delete on the card folder follow the public library's layout and pass the unit test; no run has saved yet. A request beyond those stops the run by number. Ask the user to save, quit and start again.
- **The film after the logos** (black screen): fixed by flipping the GS field flag at every VBlank; checked on scripted runs and with the pause test.
- **The opening film after a skipped first film** (Start during the logos): it never started, because a film's sound player destroyed while playing dropped its announced data instead of reporting it back; it is reported back now, as a stop does. Checked on scripted runs with Start pressed at four moments.
- **Folders with names outside the system's code page** (the user's card folder had one and was stored as question marks): the folder chooser, the settings, the menus and the card folder passed to the runtime are UTF-8 / wide now. The stored broken entry must be removed from the list and the folder added again. The disc image path still goes through the code page.
- **Window size:** up to the multiple of 640 x 448 nearest to the screen (one more than fits when that is closer; a little of the picture's edge is cut then), and fullscreen with integer scaling uses that multiple.
- **Launcher and in-game menu:** card list (cards of `user/memcards`, folders added from elsewhere, new empty card, "Remove a card from the list" for any listed card: only the entry goes, kept as `hidden_card` or by dropping the `card_folder` line), a new card made automatically when none is in the slot, folder chooser for disc images, Controls with "Button mapping" and "Stick sensitivity", window size up to the screen, the picture keeping its proportions in a resized window, sliders "Music" and "Effects and voices".
- **Camera in fights** (fixed earlier the same day: sign extension of float moves): the user was asked to confirm.

**Open:**

- **"Mono sound happens sometimes"** (the user; they want stereo only). Not reproduced: the music is as wide in stereo with the user's save as without, the port's mix has no mono mode (SOUNDS request `0x60`, value 1 with the save, is recorded and not acted on), and no one-channel stream occurs (`audio_single_stream_bytes` is 0; the port would play one on the left only). Ask where and when it happens, and whether it is the music or the effects, before changing anything.
- **Performance, next step:** the GPU renderer's intake of packets is the largest graphics cost (3.7 s of the 14.7 s scripted fight, `BT2_PROFILE_TOTALS=1`). It is the library's own work; the step is a graphics thread, which needs its plan first ([performance plan](../plans/ee/PERFORMANCE_PLAN.md)). The fight's frame rate in the window with sound has not been measured since the VU1 work.
- **Unmapped code:** `map_gaps.py --check` (a gate step) reports nothing uncovered in the main program (11,139 functions) or the overlay (494): every call, jump, data pointer and built address is a function or a label, and every label is a dispatch entry. A stop at an unmapped function can now only come from a pointer computed at run time; `work\scripts\discover.cmd` (`discovery_loop.py --player-stop`) then maps that target and everything it leads to in one go. After a change to the map rules run `python tools/ps2recomp/discovery_loop.py --map-all`.
- **Not compared with the original:** the memory card replies, GS readback buffers, a 3D frame, VU1 memory (the deferred 3D verification).

## How the user runs things

- The user's helper scripts are in `work\scripts\` (local, ignored; `work\WORKING_WITHOUT_AI.md` describes them): `play.cmd`, `discover.cmd`, `rebuild.cmd`, `map-all.cmd`, `check-map.cmd`, `gate.cmd`, `show-log.cmd`, `make-report.cmd` (a dated report in `work\reports\`), `play-record-sound.cmd` (a WAV of the session in `work\audio\`), `play-with-stats.cmd`, `import-save.cmd`, `backup-cards.cmd`. `play.cmd` starts the launcher; `build\probe\Release\bt2-game.exe` starts the game with the stored settings. Settings: `user/settings`; the last run's messages: `user/last-run.log`.
- **A stop at untranslated code** ("Branch target" in the log; the game's message says so): the user double-clicks `work\scripts\discover.cmd`, which runs `python tools/ps2recomp/discovery_loop.py --player-stop`. "map did not grow" in `work/bt2-generation/discovery-loop.log` means the map prover refused the function and needs work.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## Rules learned the hard way

- **Build the player programs, not single targets, before handing a build to the user:** `python tools/build/msvc.py cmake --build build/probe --config Release`. `bt2-game` and `bt2-launcher` depend on `bt2-entry-probe` since a settings change left the runtime behind and the game refused its settings file. A build fails to link while a game window is open.
- **Runs with a window appear on the user's screen.** Use `BT2_VIEWER_CLOSE=1` so they close at their end, and never leave one open. The in-game menu can be opened without key presses with `BT2_PAUSE_TEST` ([TOOLS.md](../guides/TOOLS.md)).
- **Every VU1 or VIF1 change keeps the baseline, which the gate now checks** (`tools/gate/battle_run.py`, with `BT2_VU1_VERIFY`; a change that alters the 3D output on purpose updates `BASELINE` there): 5,100 frames of `tools/gate/pad-fight.txt`, headless: `vu1_kick_hash` **`5a4960667d4a3dff`**, 724,698,589 instruction pairs, 2,140,195 starts; the same with `BT2_VU1_INTERPRET=1` and `BT2_VU1_NO_HAND_WRITTEN=1`, and `BT2_VU1_VERIFY=1` passes with `vu1_verified_runs` equal to `vu1_runs`.
- **Native VU1 translations are local build inputs:** `game/generated/vu1/vu1_native.cpp` is generated from `work/perf/vu1-census/` by `tools/vu1/translate_programs.py`; without it the build interprets what the hand-written programs do not cover, and the baseline hash is the same.
- **After a change to `prepare_runtime.py`:** run it, then build; the prepared sources are rewritten and recompiled.
- **Shell heredocs corrupt backslashes:** write files that contain them with the file tools.

## How to run (automated)

- **Scripted fight:** `run_startup_probe.py --config Release --frames 5100 --pad-script "<contents of work/bt2-generation/pad-fight.txt>"`. The pad scripts are local and ignored. Scripts count fields; snapshots count flips and print `Snapshot: flip N at field M`.
- **With a memory card:** `BT2_CARD_FOLDER=<folder>`; the default pad script answers the no-card dialogue, which does not appear then.
- **Frames:** `BT2_GPU_GS_SNAPSHOT="<flip>:<path>;<flip>:<path>"` writes those flips as PPM. **Audio:** `BT2_AUDIO_WAV=<absolute path>`.
- **Did a change alter what the game sees:** run both builds with `BT2_STATE_HASH=<file>` and compare with `tools/ps2recomp/compare_state_hash.py` (first differing field; `BT2_STATE_HASH_PAGES=<field>` for the pages).
- **Following a value:** `BT2_EE_CALL_TRACE`, `BT2_EE_WATCH_QUADWORD`; **a hang:** `BT2_STALL_FIELDS=120`; all in [TOOLS.md](../guides/TOOLS.md).
- **Build times:** a runtime header (`ee_runtime.h`) about 16 s, one file about 12 s, everything about 6 minutes (only after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`). **Gate:** `python tools/gate/run_gate.py --disc <iso>` (about 75 s plus the builds; it includes the 30 s battle run).

## Notes

- The 3D frames exist only on the GPU renderer: the CPU GS model refuses perspective texturing and fog, so `--gs cpu` and `--gs mirror` stop at the first 3D frame; the gate's CPU run ends before any. The user decided that 3D on the CPU renderer is an optional extra for the end of the project.
- VU1 counts one cycle per instruction pair without stalls; `vu1_early_q_reads` and `vu1_early_clip_reads` are 0 in the fight.
- The gate runs 22 comparisons on the full run and 9 at phase stops; the others are listed in `NOT_IN_GATE` in `tools/gate/run_gate.py` with their reasons ([PROG.md](../PROG.md#verification-2026-10-13)).
- Kept local captures: `work/pcsx2-capture/cri-stream-kept/`, `work/pcsx2-capture/sounds-log/` and `sounds-function-*`; reference movie frames in `work/movie-ref/`; the user's launcher reference pictures in `work/launcher_ui_ref/`.
