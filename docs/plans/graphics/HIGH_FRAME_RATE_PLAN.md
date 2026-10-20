# 60 fps battles

Requested by the user on 2026-10-20 (the second of two enhancements; [HD textures](HD_TEXTURES_PLAN.md) come first): battles at 60 pictures per second, although the game shows 30, as an optional toggle in the in-game overlay's Display entry ([launcher plan](../launcher/LAUNCHER_PLAN.md), Overlay).

## What is known (2026-10-20)

- **Where the 30 comes from.** The fight's main loop (`0x102E80`, `0x102F30`) starts Timer 1 at the top of a frame, and before waiting for the vertical blank calls `0x28C790`, which spins until the timer has counted 400 lines (about 1.5 fields) and stops it. So a fight frame takes two fields. The menus' loops have no such wait and run at 60 ([performance plan](../ee/PERFORMANCE_PLAN.md), finding 3; the user confirmed 30 in fights on the original).
- **Nobody has patched it.** PCSX2's patch database has only widescreen patches for the Tenkaichi games (BT2's PAL release among them), none for frame rate, and requests for a BT2 60 fps code on the PCSX2 forums went unanswered. That suggests what the code also suggests: the fight advances a fixed step per frame, so removing the wait alone would run the fight at double speed.
- **The port's own budget.** A fight frame currently takes 18 to 27 ms of work in most stretches and up to 31.5 ms in the heaviest on this machine (performance plan, finding 7). 60 needs every frame under 16.7 ms. Whatever the method, the fight has to get about twice as fast first, or the toggle only gives an uneven rate.

## Ways to get there

1. **Half steps (the preferred way, if the game allows it).** Run the fight's frame twice as often with each frame advancing half as far. That needs every speed, timer and animation step of the fight to be scaled. Fighting games often have one place for this: slow motion (BT2 slows the action for some finishes) usually means a global time scale that the fight's systems read. If that scale exists and is honoured everywhere, 60 fps is: the 400-line wait skipped, the scale halved, and the step counters that are integers (frame counts) checked.
2. **Generated in-between pictures.** Leave the game at 30 and make a picture between each two from the rendered images (motion estimation, as frame generation does). It does not touch the game, but it adds a frame of latency, which matters in a fighting game, and the HUD and fast effects smear. A fallback only.
3. **Scaling each system by hand.** If there is no common time scale: find each per-frame step (movement, animation playback, effects, camera, timers, AI) and halve it. This is a large reverse-engineering effort with a long tail of things that run twice as fast.

## Order of work

1. **Measure the uncapped fight (tool runs).** A switch that skips the 400-line wait in a scripted CPU battle: does the fight run at double speed (the results' "Battle Time" against fields elapsed, character movement per field), and does anything break?
2. **Look for a time scale.** Find what slow motion changes (capture memory around a finish that slows the action; compare the per-frame step of movement and animation in slow and normal frames). Decide between ways 1 and 3 from what is found; stop and report if neither is in reach.
3. **Performance.** The fight under 16.7 ms per frame in its heaviest stretch (performance plan).
4. **The toggle:** the overlay's Display entry, battles only (menus already run at 60), off by default; compared with the 30 fps fight for game speed (equal results' battle times for the same fight) and checked by the user playing.
