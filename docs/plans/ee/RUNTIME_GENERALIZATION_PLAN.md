# Runtime special cases made general

Step 5 of the [whole-game coverage plan](WHOLE_GAME_COVERAGE_PLAN.md). Written 2026-10-19.

## Why

The runtime reached the game's frame loop by replaying its startup step by step: many system calls and hardware stores are accepted only at the guest pc, stack offset, call count or startup phase where the captures of the original saw them, and refused elsewhere. That made every step checkable against the original, but it is the opposite of the project's rule (one implementation per behavior, generalized verified special cases). The census (`tools/ps2recomp/runtime_census.py`) counts **160 such lines**: 76 in the system calls, 64 in hardware memory access, the rest in dispatch, sound dispatch and services. The sweeps of 2026-10-19 (all story parts, towns, menus, the other modes' battles) stopped at none of them, so none is urgent; each is still a place where a path the sweeps did not take would stop.

## How

- One group at a time, smallest kernel behavior first; each change replaces a set of conditions with the general behavior the kernel or hardware has (from the references: Play!'s kernel model, PCSX2's hardware behavior, the PS2SDK's declarations), and deletes the conditions.
- Counters the startup checks and the comparisons with the original read stay counters (diagnostics), but no longer decide whether a request is accepted.
- Each change: unit tests of the general behavior (replacing tests that asserted a refusal), the gate (the 32 comparisons with the original must still pass, the battle run's state hash unchanged), and a sample of the sweeps (a story part, a town, a menu screen, one mode battle).

## Groups, in order

1. **Kernel semaphores** (CreateSema, DeleteSema, WaitSema, PollSema, SignalSema): CreateSema accepts any `attr` (the kernel ignores it: it keeps the count, maximum and option only); DeleteSema deletes any existing semaphore and its id is reused (lowest free id first); a wait on a zero count still stops (no other thread can signal in this model) until threads (group 3) can block. **Done 2026-10-19:** see below.
2. **GS and display startup** (GS CSR reset, GsPutIMR, SetGsCrt, the display's VSync waits): one reset behavior and one CRT behavior, wherever they come from.
3. **Threads** (CreateThread, StartThread, ChangeThreadPriority, ReferThreadStatus, SuspendThread, the startup workers): a general thread table with the kernel's states; then thread exit and deletion (the sound library's shutdown, a worker's exit), which the census lists as unhandled.
4. **Hardware stores of the startup** (DMAC control and priority, Timer 1's delay helpers, the startup GIF transfers, VIF startup): the general register behavior the frame phase already has.
5. **SIF and IOP replays after the IOP reset** (the post-reset LOADFILE, SCMD and SIF register sequences): general SIF register and DMA behavior.
6. **The rest** (dispatch, sound dispatch, services, VU0 macro): case by case.

## Group 1 result

- CreateSema: 24 conditions that admitted a nonzero `attr` removed; any `attr` is accepted. The four that count the startup's own semaphores (the first CD-init call's, the CD semaphores, the disc and NCMD bind semaphores) remain as counters.
- DeleteSema: the 9 cleanup conditions removed; any existing semaphore is deleted, its id freed (the newest is removed; an older one is marked deleted, and CreateSema reuses the lowest free id). Polls, waits and signals on a deleted id return -1.
- Unit tests rewritten from refusals to the general behavior (and a deleted older id reused). The gate passed (32/32 comparisons, battle hash unchanged); a story part, the shop town and the Dragon Tournament ran as before. The census went from 160 lines to 118.

## Limits

- No behavior that the comparisons with the original check may change; where a general behavior would, the plan records it and the change waits for a new capture.
- Diagnostics keyed on pcs (for example the memory card lock's waits) are counters only and may stay.
