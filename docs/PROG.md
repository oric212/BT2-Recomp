# Current progress

*Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441) as a native Windows x64 port: recompiled game code on a minimal native layer ([implementation plan](project/IMPLEMENTATION_PLAN.md)). How each part was checked: [VALIDATION.md](progress/VALIDATION.md). Dated detail: [November](progress/2026-11.md), [October](progress/2026-10.md).

<!-- progress-summary:start -->
![BT2 port status](progress/dashboard.svg?v=bd120b04)

| Milestone | Status |
| --- | --- |
| Startup | ✅ Verified — boot to the frame loop; key states match captures of the original |
| Controller and card checks | ✅ Verified — native services; the no-card prompt matches the original |
| Films | 🟢 Operational — logo and opening films with sound |
| Title and menus | 🟢 Operational — every main-menu mode; all Ultimate Battle Z courses, tournaments and training lessons run without input |
| Versus battle | 🟢 Operational — playable with a controller; all 130 fighters and 16 stages sampled; the enemy AI matches PCSX2 exactly at the calls compared |
| Story mode | 🟢 Operational — all 200 parts and 73 towns run without input; not yet played through |
| Audio | 🟢 Operational — music, voices and effects; confirmed by listening |
| Saves | ✅ Verified — loading, saving and overwriting; the saves byte-identical to the original's in PCSX2 |
| Enhancements | 🟢 Operational — HD textures, super-sampling to 16x, widescreen, 60 fps battles, fast loading, beam struggles by buttons; full speed with all of them |
| Online battle | 🟢 Operational — lockstep over UDP with connection codes, no port to open; played between two homes; lock-on and prompts left for later |

**Code:** 11,139 EE functions plus 494 in the `DBZP.BIN` overlay, all compiled natively; nothing interpreted in the gate's runs · **Gate:** unit tests, Python suites, 31/31 comparisons with the original, the battle run against its baseline.
<!-- progress-summary:end -->

Status: ✅ Verified · 🟢 Operational · 🔧 Incomplete · ⛔ Blocking · ⏳ Not Reached ([definitions](project/PROGRESS_TRACKING.md)).

## Latest (2026-11-10)

- **Online input delay:** Auto chose 3 fields at an 8 ms round trip because a player's game read the handshake only between drawing its window; both sides now keep reading while it runs, and Auto chooses 2 there (a test; the rule itself measured over simulated links from 8 to 60 ms and kept) ([online plan](plans/online/ONLINE_BATTLE_PLAN.md), "Left for later").
- **60 fps through heavy super attacks:** after a super attack that destroyed the stage the user's online fight stayed at 30 fps (the host's log: turned off as "too slow" after one heavy scene). A heavy stretch now pauses the in-between pictures for 3 seconds and they resume ([60 fps plan](plans/graphics/HIGH_FRAME_RATE_PLAN.md), 4d).
- **Sharper picture on a 4K screen:** the game's picture is enlarged to the screen by a sharper filter (Lanczos) instead of a bilinear stretch, which had made the fighters look soft ([image quality plan](plans/graphics/IMAGE_QUALITY_PLAN.md) step 6).
- **Own button prompts online:** the opponent's recovery and beam struggle prompts no longer show on your screen.
- **Online split line gone:** dashing or charging ki no longer flickers a line in the middle of an online fight.
- **Online radar:** each player sees their own radar again, at the bottom right as in a normal fight, to keep track of the opponent (the user: easy to lose online).
- 2026-11-09: saves verified byte-identical to the original's; the HD faces and the RX 9070 XT's fade fixed; online without an open port, played between two homes ([November](progress/2026-11.md)).

## Waiting for the user's play

Built and measured, not yet seen in play: online battles on two PCs (the release package, `dist/`), with Auto's input delay and 60 fps after a stage-destroying super attack; the 60 fps aura fix, Variable refresh rate, beam struggles by buttons, the HD outline and dialogue shadow, the light depth of field, the second controller, the programs' icons; on the RX 9070 XT the fade fix and the HD faces. Open report: a texture glitch in the first fight with HD (not reproduced headless).

## Measurements

One PC (Core i9-10900K; Radeon RX 6800 XT, since 2026-11-09 an RX 9070 XT with about the same GPU time), Release, a fight with every enhancement ([performance findings](reference/PERFORMANCE_FINDINGS.md)).

| Item | Value |
| --- | --- |
| Fight | 29.97 flips a second, super attacks included; an in-between picture for nearly every flip, 6.3 ms each |
| Game thread | 10 to 14 ms of a 33 ms frame |
| Graphics card | 6 ms a frame plain, 8 to 10 with 16x, HD and widescreen |
| Latency | 10 ms flip to screen in menus; 7.5 ms in a 60 fps fight with VRR, 97% of pictures a field apart within 1 ms |
| Memory | 0.7 to 0.85 GB; 0.8 to 1.2 GB of video memory |

## Subsystems

| Subsystem | Status | Scope |
| --- | --- | --- |
| EE CPU and kernel | 🟢 Operational | Every mapped function compiled; EE floating-point rounding; threads, semaphores, interrupts |
| IOP services | 🟢 Operational | Native disc, file, controller, memory card, sound and CRI services; no IOP code runs ([plan](plans/iop/NATIVE_SERVICES_PLAN.md)) |
| GS and display | 🟢 Operational | paraLLEl-GS on its own threads, native passes for the outline, glow and depth of field |
| GIF / VIF / VU | 🟢 Operational | DMA as the game uses it; VU1 programs native, checked in the gate |
| IPU / video | 🟢 Operational | Both films decode through the game's Sofdec library |
| Audio | 🟢 Operational | ADX streams and a native effects sampler, 48 kHz stereo ([plan](plans/sound/SOUND_OUTPUT_PLAN.md)) |
| Input | 🟢 Operational | Keyboard, XInput and DualSense; mapping in the launcher and the in-game menu |
| Memory card | ✅ Verified | A host folder per card; saves byte-identical to the original's ([plan](plans/memory-card/MEMORY_CARD_PLAN.md)) |
| Disc / filesystem | ✅ Verified | ISO reads; archive table and overlay byte-identical to the original |

## Next

In the [implementation plan](project/IMPLEMENTATION_PLAN.md)'s order:

1. Online: the lock-on lost too often and each player seeing only their own prompts ([online plan](plans/online/ONLINE_BATTLE_PLAN.md), "Left for later"); then the user's play of the items above.
2. Final validation: the exhaustive battle sweep; 3D frames compared with the original.
3. Picture: an edge filter ([image quality plan](plans/graphics/IMAGE_QUALITY_PLAN.md)); 60 fps load work ([native fight rendering plan](plans/graphics/NATIVE_FIGHT_RENDERING_PLAN.md)).
4. The user's decisions: a matchmaking server and rollback for online; publishing the release.

**Not compared with the original yet:** GS readback pixels, 3D frames, VU1 memory.

[Active handoff](progress/ACTIVE_PASS.md) · [components.json](progress/components.json) · [reference architecture](reference/REFERENCE_ARCHITECTURE.md) · [runtime layout](guides/RUNTIME_LAYOUT.md) · [tools](guides/TOOLS.md)
