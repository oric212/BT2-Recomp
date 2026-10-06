# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-06.

## Verified checkpoint

<!-- progress-summary:start -->
**Verified checkpoint: native startup probe; complete startup remains unfinished.**
Last measured: **2026-10-06**, using the local game ISO and prepared startup inputs.

![Measured native startup dashboard](progress/dashboard.svg)

| Area | Verified result |
| --- | --- |
| EE execution | **291 distinct entries entered / 286 completed**; kernel **2 / 2** |
| Runtime activity | **3,855 calls / 3,848 returns**; **61,612 IOP instructions** |
| Sound setup | **37 CRI calls**, **six IOP heap allocations**; first AutoDMA half pending |
| Disc access | Directory-sector read and initial metadata lookups work |
| Graphics | Two startup clears consumed; **no presented game frame** |
| Current blocker | Worker `ReferThreadStatus`: selector `0x30`, EE PC `0x32EB68` |
| Active work | Worker status/start/priority changes **implemented locally, not yet verified** |
| Next gate | Finish worker tests/build, rerun startup, compare captures and record the new boundary |

Counts describe observed execution, not fully restored gameplay functions or a percentage of completion. Audio playback, complete startup and gameplay are not verified.

```mermaid
flowchart LR
    A["Local validation<br/>Verified"] --> B["Map and generation<br/>Working; provisional map"]
    B --> C["Native startup execution<br/>Verified to checked boundary"]
    C --> D["Worker scheduling<br/>Active; latest changes unverified"]
    D --> E["Archive payload loading<br/>Not verified"]
    E --> F["Game draw and presentation<br/>No presented frame"]
    F --> G["Title/menu and battle<br/>Not reached"]
    classDef verified fill:#173c2c,stroke:#4ade80,color:#fff
    classDef active fill:#493b16,stroke:#fbbf24,color:#fff
    classDef future fill:#242b36,stroke:#94a3b8,color:#fff
    class A,B,C verified
    class D active
    class E,F,G future
```

This is the main startup path, not a strict dependency graph; audio and other runtime services develop alongside it.

| Subsystem | Evidence / measurement | Status |
| --- | --- | --- |
| Mapping / generation | 8,584 provisional EE entries; 8,587 raw generated files; 221 extra entries selected | Tooling working; map remains provisional |
| IOP execution | 213 physical entry targets; 14 module-load paths / 13 resident modules | Bounded execution verified |
| EE memory / threads | Four startup pools; one child ready but unexecuted | Scheduling incomplete |
| Disc loading | One two-sector read; original completion callback; six metadata lookups/transfers | Initial metadata verified; archive payload pending |
| Memory cards | Six DMA pairs; five timed FIFO probes | Absent-card path verified; mounted filesystem pending |
| Audio transport | First owned 2,048-byte AutoDMA half programmed | Consumption / IRQ / native playback unverified |
| GS sink | Three packets; two startup draws; 573,440 sprite pixels | Startup clears verified; presentation absent |
| End-to-end game | No title screen, input-driven battle or completed match | Not reached |

**Change from the preceding measured checkpoint:** CRI calls **35 → 37**, heaps **4 → 6**, EE call events **3,415 → 3,855**, IOP instructions **61,054 → 61,612**. The older **111 / 107** distinct count is from an earlier checkpoint; it is not a same-pass delta.
<!-- progress-summary:end -->

## Verification

The last completed pass passed MSVC builds and **16/16 CTests**. Current tooling checks passed **142 capture tests**, **52 recompilation tests** and **13 IOP tooling tests**. Later worker changes are still under verification; their final build/test result is pending. Proprietary inputs, captures and generated code remain ignored.

## Current blocker and next step

Latest user-confirmed run stops at **ReferThreadStatus, selector 0x30 / EE PC 0x32EB68**, querying the next startup worker. The fifth and sixth heap requests now pass this checkpoint.

Next: verify the bounded worker status/start/priority changes, then measure the resulting scheduling boundary. See [EE sound scheduling plan](EE_SOUND_SCHEDULING_PLAN.md) and [second-channel evidence](CRI_SECOND_CHANNEL_PLAN.md). Pending changes are not counted as working.

Remaining: SDRDRV reverse bind to 0x80000704, general scheduling/timer delivery, mounted cards, archive payload loading, audio consumption and presentation. Logical IOP time remains 1900 us; the fifth card alarm is armed for 2000 us. An EE child is ready but has not executed.

Run the measured checkpoint using the [startup probe instructions](BUILDING.md): `python tools/ps2recomp/run_startup_probe.py --disc <local-ISO-path>`.

## Milestones

- [x] Native host executable.
- [x] Local executable validation and ELF metadata inspection.
- [x] Trusted retail revision confirmation (Redump disc match; local ELF equals the embedded executable).
- [x] Pinned PS2Recomp tools, provisional EE map and local generation.
- [x] Generated routines compiled/linked/executed; game entry reached.
- [x] Local kernel capture and bounded generated platform initialization.
- [x] Initial absent-card query through native IOP and generated EE callback.
- [x] Owned disc metadata lookup through original native IOP and generated EE cleanup.
- [x] First directory-sector read, generated parsing and all five listed file lookups.
- [ ] Complete startup and general required runtime integration.
- [ ] First rendered/presented frame; title screen and main menu.
- [ ] Character select and battle load.
- [ ] Playable versus battle.
- [ ] Audio and full match.

## Active pass and tracking

See [active-pass handoff](progress/ACTIVE_PASS.md) for pending files, verification steps and resume instructions. See [tracking rules](PROGRESS_TRACKING.md) for status definitions and README synchronization. New changes enter the verified checkpoint only after their required checks pass.

## History and working rules

- [October 2026 history](progress/2026-10.md): detailed evidence, old counts and the previous current-status summary are preserved.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [reference architecture](REFERENCE_ARCHITECTURE.md), [tools](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

A plain "continue" invokes the permanent subsystem-scale pass rule. Follow multiple blockers, gather available evidence, verify, update history, commit and push. Reduce scope only when actual remaining usage is low. Keep this page short; detailed pass results belong in monthly history.
