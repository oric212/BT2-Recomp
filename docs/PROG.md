# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-05.

## Current status

The optional EE probe completes **thirteen native IOP module loads**, including CRI_ADXI. LIBSD `sceSdInit` returns 0 after the bounded initial sound RAM transfer and register setup. CRI_ADXI returns resident result 2; its relocated code, full normalized data/BSS and two waiting workers match a local retail capture. No displayed frame, audio playback or complete boot yet.

Latest verification: MSVC builds, **14/14 CTests**, **11/11 IOP tooling tests**, **94/94 capture tests**, **43/43 recompilation tooling tests**, independent heap bind/allocation/copy checks. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

The IOP heap service binds, its translated FILEIO handler allocates **864 bytes**, and the first EE-to-IOP copy/completion finishes. Native destination bytes match the local reference capture. Correction: server **0x80000003** is the heap service hosted by FILEIO, not ordinary file I/O.

Startup now requests **LOADFILE operation 6 (module from memory)** through `FUN_003373F0`, using the copied IRX at **0xCE500**. Current stop: **CreateSema selector 0x40 at EE PC 0x32EC68**, parameters at **0x1FFEB70**. Full checkpoint: **736 calls / 729 returns**, **thirteen residents**, **30,889 cumulative IOP instructions / 126 physical entry targets / twelve waiting workers**. Boot FILEIO heap-handler execution adds 50 IOP instructions; its boot entry/worker is not run natively or counted as another resident.

Follow the new [memory-backed module loading plan](MODULE_BUFFER_STARTUP_PLAN.md). The [heap scope](IOP_HEAP_STARTUP_PLAN.md) supports only the first captured allocation and copy; general allocation/free, file reads, physical DMA timing and buffer-module residency remain unsupported. Assets, Vblank production, audio playback and presentation remain incomplete.

## Key counts

- **7,262** locally generated EE functions; **7,271** raw generated C++/header files.
- **111** distinct EE functions entered / **107** completed at the last measured unique-count checkpoint across native probes (startup/helpers, not gameplay).
- Thirteen EE module-load helper paths now complete; updated distinct EE totals have not been measured. Main and entry remain partial.
- SIO2MAN: **47 provisional function ranges / 28 imports / 239 relocation sites** validated locally. All relocation words and code ranges match the reference loaded module; **six native IOP entry targets entered at its initial resident checkpoint**, with one worker parked then (now active at the transfer blocker). MCMAN: 167 provisional ranges / 35 imports; resident result 2 validated against a local reference return and normalized startup data/BSS. MCSERV and DBCMAN resident entries/worker waits are validated; SIO2D resident entry and Vblank wait are validated locally. DS2U_D: 40 provisional ranges / 25 imports; resident return and owned worker start/dormant state are verified. Live controller data/BSS is not equated across Vblank phases. These startup counts do not represent working gameplay functions.
- Added sound maps: LIBSD **57 ranges / 19 imports**, SDRDRV **7 / 52**, MODHSYN **174 / 24**, MODSESQ2 **46 / 9**. These are provisional discovery counts, not restored gameplay functions. Their resident startup paths work; substantive sound processing remains unverified.
- CDVDSTM **14 ranges / 34 imports** and SOUNDS **14 / 35** have verified resident startup. CRI_ADXI **217 / 45** has verified resident startup; these provisional map counts do not indicate functioning audio/streaming.
- Two drawing requests / two logical DMA completions / **573,440** sprite pixels processed, including depth writes. No displayed frame or complete game boot.

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
