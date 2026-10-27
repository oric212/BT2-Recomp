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
2. **GS and display startup** (GS CSR reset, GsPutIMR, SetGsCrt, the display's VSync waits): one reset behavior and one CRT behavior, wherever they come from. **Done 2026-10-19** except the two VSync waits (see below).
3. **Threads** (CreateThread, StartThread, ChangeThreadPriority, ReferThreadStatus, SuspendThread, the startup workers): a general thread table with the kernel's states; then thread exit and deletion (the sound library's shutdown, a worker's exit), which the census lists as unhandled. **Done 2026-10-19** (see below), except blocking semaphore waits, which nothing reaches.
4. **Hardware stores of the startup** (DMAC control and priority, Timer 1's delay helpers, the startup GIF transfers, VIF startup): the general register behavior the frame phase already has.
5. **SIF and IOP replays after the IOP reset** (the post-reset LOADFILE, SCMD and SIF register sequences): general SIF register and DMA behavior.
6. **The rest** (dispatch, sound dispatch, services, VU0 macro): case by case.

**Groups 4 to 6 paused (the user, 2026-10-20):** they are replays of the startup that run the same way on every boot; making them general rebuilds the startup's hardware model and most of the 32 startup comparisons, without changing anything the game can reach. Coverage comes first; these groups wait until after the final validation sweep. **Resumed 2026-10-27 (the user):** group 4 done, group 5 in progress (below).

## Group 1 result

- CreateSema: 24 conditions that admitted a nonzero `attr` removed; any `attr` is accepted. The four that count the startup's own semaphores (the first CD-init call's, the CD semaphores, the disc and NCMD bind semaphores) remain as counters.
- DeleteSema: the 9 cleanup conditions removed; any existing semaphore is deleted, its id freed (the newest is removed; an older one is marked deleted, and CreateSema reuses the lowest free id). Polls, waits and signals on a deleted id return -1.
- Unit tests rewritten from refusals to the general behavior (and a deleted older id reused). The gate passed (32/32 comparisons, battle hash unchanged); a story part, the shop town and the Dragon Tournament ran as before. The census went from 160 lines to 118.

## Group 2 result

- One behavior each (`runtime/ee/ee_gs_privileged.cpp`), from any pc and stack: a CSR reset (`0x200`) resets the GS drawing registers; the CSR read gives revision `0x1B`, ID `0x55`, the latched status bits, FINISH and FIELD, all read as clear after a reset until the next VBlank (what every capture of the read after a reset shows); GsGetIMR reads the mask and GsPutIMR sets it and returns the previous one (the mask starts with every interrupt masked, `0xFF00`, as the first call's captured return shows); SetGsCrt records interlace, mode and FFMD. Mask bits outside `0xFF00` and modes other than NTSC stay explicit stops.
- 18 branches keyed on pcs, stack offsets, return addresses and step numbers replaced by these four functions, and the evidence flags that admitted the first calls removed. The startup's sequence counters (the first reset's steps, the graph startup's, main's two display re-initializations, the frame-phase resets) advance from their own state after the general behavior; the later startup steps read them, but they decide nothing.
- Left for group 4: the startup's two VSync waits (the CSR read at `0x3104F4` during main's display re-initialization), because the first of them also starts the repeated clear transfer (`clear_repeat_gif`) that the startup's GIF replay checks.
- Unit tests of the general behavior; the gate passed (32/32 comparisons, battle hash unchanged); a story part (cleared, saga 411), a town and the Dragon Tournament (four battles to Champion, 4,197 functions) ran as before, nothing interpreted. The census counts 117 lines: 9 conditions fewer, 8 counter steps in the new file that it cannot tell from conditions.

## Group 3 result, first part

- The startup's thread calls go through the same handler as the scheduled ones (`runtime/ee/ee_thread_scheduler.cpp`, `handleThreadSyscall`), from main from SetupThread on and from any worker the scheduler runs: CreateThread, StartThread, ChangeThreadPriority, ReferThreadStatus, SuspendThread/ResumeThread, SleepThread/WakeupThread, GetThreadId. 9 startup branches keyed on pcs, return addresses, stack offsets and call counts removed, with the table of the four workers' captured identities and the separate startup dispatch.
- CreateThread writes SetupThread's root function as every thread's return address (the startup captures of threads 3 to 7 show it; thread 2 had an unfounded default before). StartThread passes its argument in a0 and the caller's interrupt state. ReferThreadStatus writes the kernel's full descriptor for a created thread (status, entry, stack, size, gp, priorities, attr, option, wait type, wakeups), as the startup captures show it; for main, its status and current priority (the other fields of the startup's main thread are not known).
- Main's admission is now what it is: main's first priority change that lets ready workers above it run. They run before the call returns, and the scheduled part of the run begins. The startup's counters count the same calls before it and the scheduled counters after it.
- Left in this group: the sound thread's first SleepThread (it is the startup's sound dispatch, in `ee_sound_dispatch.cpp`) and blocking semaphore waits.

## Group 3 result, second part: thread exit and deletion

- ExitThread and ExitDeleteThread from a worker the scheduler runs: the worker's stack parks once more, the scheduler sees the thread exited and releases it (the stack is unwound, the thread is dormant, with 0x24 its record goes). DeleteThread removes a dormant thread; TerminateThread makes another thread dormant wherever it is and unwinds a parked stack. A deleted thread's id is free and CreateThread takes the lowest free id. Main's exit (the program's end) and the sound thread's (outside the scheduler) stay explicit stops. The four CRI workers end this way when their library shuts down (a stop flag, then ExitDeleteThread); no sweep reached that.
- Blocking semaphore waits (a WaitSema on a zero count from main or a worker) stay explicit stops: no run reached one, since the native services complete their requests at once; the kernel's wait-queue order would be a guess without a capture.
- Unit tests (a worker exiting with ExitDeleteThread and its id reused, a sleeping worker terminated and deleted, main's exit refused); the gate passed (32/32 comparisons, battle hash unchanged).
- Unit tests rewritten from refusals to the general calls (from any pc and buffer); the gate passed (32/32 comparisons, battle hash unchanged); a story part, a town and the Dragon Tournament ran as before, nothing interpreted. The census counts 108 lines.

## Group 4 result

- **Registers** (`runtime/ee/ee_hardware_registers.cpp`), from any pc outside interrupts: Timer 1 (one MODE write, H-BLANK or BUSCLK/256 with CUE; one COUNT write; a COUNT read of the counting H-BLANK clock on main's thread waits one line, other reads do not advance time), Timer 2 (MODE before it counts is the boot's value, written with flag acknowledgement; then the logical timer), Timer 3's MODE, the DMAC's D_CTRL (DMAE only), D_PCR and D_STAT (status bits write-one-to-clear, masks toggle), VIF1 FBRST and ERR, GIF_CTRL's reset, the VU reset. The startup's VIF codes go through the VIF1 FIFO into the general VIF1 interpreter; a GIF reset records VIF1 as the setup left it, for the startup comparison.
- **GIF** (`ps2_runtime_memory.cpp`): one GIF channel (normal and source-chain mode, complete when it starts) and one sink path for every transfer, from the first boot's clears and the texture setup on; the captured-packet replays (`GifDmaStaging`, `TextureGifStartup`) and the sink's startup-only paths are removed. The GS registers at the first display flip are recorded for the texture comparison.
- **VBlank**: INTC_STAT is write-one-to-clear; main polling for VBLANK_START receives the next VBlank, delivered with its handlers when cause 2 is enabled and latched otherwise (the first boot's wait included). The CSR read always goes through `gsCsrRead`; FIELD starts at 0 so both of the original's field samples (the first boot's wait and 0x271178) match. AddIntcHandler and RemoveIntcHandler for cause 2 as the kernel keeps them (a chain, ids not reused). VPU_STAT reads idle whoever asks.
- **One change the game sees in timing:** the first boot's VSync wait now advances logical time like every other wait, so Timer 2 counts one field more from the boot and the random seeds shift; the gate's battle run has a new baseline (`7f996c1524a22bea`). Skipping that one field gives the old hash again (checked), so nothing else moved.
- The startup checks keep their effect checks (the first boot's GS setup and clears, the VIF setup's registers, the texture registers against the capture) and lose the replays' internal counts. The census went from 108 lines to 44; every hardware register the game reaches is handled without a gate.
- The gate passed (31/31 comparisons); samples: story episode 0 (17 parts; the long one cleared at 25,882 flips), the Dragon Tournament (7 battles), a CPU battle; nothing interpreted.

## Group 5, first part: the SIF

- `runtime/services/sif_system.cpp`: one SIF register file (SMFLG set by the IOP and cleared by EE writes; SMCOM, SIFCMD's buffer per boot; the 32 software registers), SifSetDChain, the SIF stop, AddDmacHandler/RemoveDmacHandler (lowest free id) and _EnableDmac/_DisableDmac for any channel, and the SIF system commands (CHANGE_SADDR, INIT_CMD, RESET_CMD) the native IOP answers; the IOP finishes a reboot when the EE first polls SMFLG after asking. D_STAT reads its channel status bits (set by GIF and VIF1 completions) and masks; SIF0's CHCR is a register (the reboot stops its receive, SetDChain starts it). The uncached RAM alias is valid at any time.
- `PostResetSif` keeps only the captured handshake as evidence; `post_reset_sif_tests.cpp` checks the general SIF against it word by word.
- Unit tests rewritten to the general behavior (registers read any number of times, handlers and channels for any channel, ids reused); the gate passed (31/31 comparisons, battle hash unchanged). The census counts 38 lines.

## Group 5 result, second part: the CD and LOADFILE requests around the reset

- The seven captured replays (`receiveFirstRpcBind`, `receiveFirstCdInit`, `receiveScmdBind`, `receiveScmdMmode`, `receiveFreshCd`, `receiveFreshScmd`, `receiveFreshLoadfile`), keyed on the transfer count and stack offsets, are removed. The requests go through the native RPC path like every other service: the server table has one set of handles per IOP boot (the BIOS's CD-init and S-command servers before the reset, IOPRP300's after it); the disc service answers sceCdInit (mode 0) and the media mode with each boot's captured reply, the module service LOADFILE's version.
- One rule replaces the post-reset CD replay's first empty bind: a rebooted IOP's modules register their servers from their own threads, so the first bind request after a reboot finds no server (the original's CD-init bind then binds again).
- One transfer id for every SIF transfer (`submitSifDma`): the kernel's numbering with the descriptor count in the low byte, which gives all three captured ids (the first CHANGE_SADDR 0x701, the first bind 0x801, the CD-init call 0x902). It replaces 23 per-service tokens, among them the memory card's and the sound services' first-call cases.
- The captured bundles stay as evidence: the probe checks them against the answers the native services give. Deci2Call kputs (16) is accepted from any pc and parameter block (group 6).
- The gate passed (31/31 comparisons, battle hash unchanged); samples: story episode 0, the Dragon Tournament and a CPU battle, part by part as after group 4, nothing interpreted. The census counts 35 lines, all of them diagnostic counters except the dispatch's idle points and the sound thread's start (group 6).

## Limits

- No behavior that the comparisons with the original check may change; where a general behavior would, the plan records it and the change waits for a new capture.
- Diagnostics keyed on pcs (for example the memory card lock's waits) are counters only and may stay.
