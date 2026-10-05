# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-05.

## Current status

The optional EE probe completes **thirteen native IOP module loads**, including CRI_ADXI. LIBSD `sceSdInit` returns 0 after the bounded initial sound RAM transfer and register setup. CRI_ADXI returns resident result 2; its relocated code, full normalized data/BSS and two waiting workers match a local retail capture. The first SDRDRV bind/command executes through native IOP code and generated EE cleanup. No displayed frame, audio playback or complete boot yet.

Latest verification: MSVC builds, **14/14 CTests**, **13/13 IOP tooling tests**, **106/106 capture tests**, **45/45 recompilation tooling tests**, independent heap and embedded-module/cleanup checks. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

The copied embedded IRX (`ck01`) executes natively and returns **1 (nonresident)**. Its CDVDMAN version query uses the disc-matched **0x225** header value. The loaded image is released, then the translated FILEIO handler frees the owned source block. **Fourteen module-load RPC paths complete; thirteen modules remain resident.** Three reached global DMAC setup writes complete without inventing interrupt events. Latest checkpoint: **1,228 EE calls / 1,220 returns**, **32,482 IOP instructions / 144 physical entry targets / twelve waiting workers and one ready helper**.

The bounded VIF setup completes: **seven configuration commands / eight words / two FIFO writes**, one VU reset pulse and one idle GIF reset. Independently captured register readbacks match. The second GS reset/IMR/CRT setup also returns, preserving existing VRAM. No VU microprogram executes.

The third normal GIF request completes: packed A+D retains PABE/TEX0 state without drawing. Disabled Timer 1 setup, main-thread priority 2, generated EE heap helpers and four pool allocations complete. The four pool pointers independently match retail captures; seven EndOfHeap queries and eight lock/unlock pairs execute.

The second IOP allocation now completes through real translated FILEIO and generated EE RPC cleanup: **0x81800 bytes at 0xCE500**, independently matching retail. Five native images were relocated out of that heap interval; the four resident modules still match their normalized retail startup captures. All module, heap and stack placements pass provenance/overlap checks. Only the two captured allocations are supported.

SDRDRV service **0x80000701** binds using owned native addresses. Command **0xE620** runs its registered handler, returns sixteen zero bytes and starts a priority-24 IOP helper. Independent retail captures validate the result and thread parameters. The helper is ready natively versus running in retail; scheduling equivalence and its outgoing callback bind are unverified.

The [EE thread-start scope](EE_THREAD_START_PLAN.md) now completes: owned ID 3 queries dormant state, clears its saved context and becomes ready with the captured GP/SP/exit handler. Only the reached IE/EIE handoff is supplied; main priority 2 continues ahead of child priority 5. No child instructions or general scheduling yet. SOUNDS service **0x02000004** binds to native owned addresses; command **0x10** executes natively and returns the independently captured 64-byte result. Both sound RPC cleanup paths complete.

DBCMAN's **four binds / four commands** complete through real native handlers and generated cleanup: 0x80001363, 0x80001304 and two 0x80001301 requests. The bounded **128-byte IOP-to-EE copy / one completion poll** completes using owned native source bytes; an immutable payload record verifies EE readback. Independent retail transport evidence uses an explicitly redirected diagnostic source copy, so live device bytes and physical scheduling are not equated. Both related request/result shapes match independent captures. See [completed reverse DMA scope](DBCMAN_DMA_PLAN.md).

Current blocker: a new **MCSERV bind to 0x80000400**, RPC ID 39 / client 0x3A6F80, EE SifSetDma 0x77 / PC 0x32F028. Generated helpers create two local semaphores and hold their lock; the bind remains pending without a fabricated reply. This memory-card protocol is a separate subsystem scope. Next: capture its bind and first reached request, consult reference contracts, and establish a bounded implementation plan. No assets loaded, displayed frame or audio playback.

The [heap capacity/layout scope](IOP_HEAP_CAPACITY_PLAN.md) records completed second-allocation evidence. General allocation, arbitrary buffer modules, file reads and physical DMA timing remain incomplete.

## Key counts

- **8,088** provisional mapped/generated EE functions; **8,091** raw generated C++/header files (8,089 C++ files include the stub unit). Safe structural filtering now admits isolated unreachable branch-padding NOPs; boundaries remain provisional.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- Fourteen EE module-load helper paths now complete (one nonresident); updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered at its initial resident checkpoint**, with one worker parked then (now active at the transfer blocker). MCMAN: 167 provisional ranges / 35 imports; resident result 2 validated against a local reference return and normalized startup data/BSS. MCSERV and DBCMAN resident entries/worker waits are validated; SIO2D resident entry and Vblank wait are validated locally. DS2U_D: 40 provisional ranges / 25 imports; resident return and owned worker start/dormant state are verified. Live controller data/BSS is not equated across Vblank phases. These startup counts do not represent working gameplay functions.
- Added sound maps: LIBSD **57 ranges / 19 imports**, SDRDRV **7 / 52**, MODHSYN **174 / 24**, MODSESQ2 **46 / 9**. These are provisional discovery counts, not restored gameplay functions. Their resident startup paths work; substantive sound processing remains unverified.
- CDVDSTM **14 ranges / 34 imports** and SOUNDS **14 / 35** have verified resident startup. CRI_ADXI **217 / 45** has verified resident startup; these provisional map counts do not indicate functioning audio/streaming.
- Three GS packets (two draws, one configuration) / three logical DMA completions / **573,440** sprite pixels processed, including depth writes. No displayed frame or complete game boot.

## Milestones

- [x] Native host executable.
- [x] Preliminary local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools and synthetic translation.
- [x] Provisional BT2 function map and local C++ generation.
- [x] Generated routines compiled, linked and executed; game entry reached.
- [x] Local kernel capture and isolated generated scan validation.
- [x] Bounded generated platform initialization returned to entry.
- [ ] Full generated-code/runtime integration and complete startup.
- [ ] First rendered frame.
- [ ] Title screen and main menu.
- [ ] Character select and battle load.
- [ ] Playable versus battle.
- [ ] Audio and full match.

## History and references

A plain "continue" automatically invokes the permanent pass rule: [continue through multiple blockers](IMPLEMENTATION_PLAN.md#continue-through-multiple-blockers), gathering available evidence before stopping under the documented conditions.

- [October 2026 detailed history](progress/2026-10.md): all previous progress entries, evidence, counts and historical milestone snapshots preserved. Statements about "current" work inside that archive describe their original point in time.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [tool workflow](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

Keep this page limited to current status. Append detailed pass results to `progress/YYYY-MM.md`, linking new monthly logs here. Update the blocker, counts and checklist after verification.
