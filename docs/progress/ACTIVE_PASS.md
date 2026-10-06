# Resume handoff: retained EE scheduling and reverse RPC

Updated: 2026-10-06. **Previous capacity/worker pass verified; next major subsystem requires continuation/delivery design.**

## Verified starting checkpoint

Full native startup: **296 EE entries entered / 292 completed**, kernel **2 / 2**, **3,892 calls / 3,887 returns**, **61,612 IOP instructions**, **37 CRI calls**, six heap allocations. Stops at **ChangeThreadPriority(1, 24)**, selector `0x29`, PC `0x32EAF8`. Main remains priority 2; the requested transition has not been accepted.

Four helpers have completed owned dormant queries, ready transitions and priority changes. Helpers 4/7 are suspended at priorities 8/25; helpers 5/6 are ready at priorities 16/18. The sound child 3 is ready at priority 5. No child EE instructions have executed. All reset exits are owned SetupThread exit `0x100250` under checked signatures.

Previous unfinished changes are included in the verified implementation pass; there is no remaining unverified worker-status/start/priority patch to resume. Full EE coverage remains 37 CRI calls; five later constructor pairs are verified only in the isolated native service test.

## Evidence and completed checks

- MSVC entry and ALL builds; bootstrap ownership/rejection tests; **16/16 CTests**.
- **145 capture tests**, **52 recompilation tests**, **13 IOP tooling tests**.
- **16 original/native startup comparisons**, including four-worker state/reset contexts, both ready queries/suspensions, first AutoDMA setup and second-channel service-only objects.
- **22 ownership rows** checked; relocated MODSESQ2/CDVDSTM startup comparisons passed during the capacity portion.
- Private reports: `work/bt2-generation/entry-stop.json` and RAM sidecars. Logs: `work/iop/resumed-final-startup.log`, `resumed-final-all-build.log`, `resumed-final-ctest.log`, `final-comparisons.json`, `resumed-final-service.log`. Public counters: `latest.json`.
- Original first RPC child wait captured at `0x333090`: SP `0x3A6C80`, GP `0x38BDF0`, queue `0x3A6D00`, original pre-observer Status `0x70030C11`, saved exit `0x100250`. Queue owner 3; server `0x3A6D18`, service `0x80000704`, handler `0x3153F0`, buffer `0x3A6CC0`.

## Next pass

1. Read `../PROG.md`, `../EE_SOUND_SCHEDULING_PLAN.md`, `../REFERENCE_ARCHITECTURE.md` and `../SDRDRV_RPC_PLAN.md`. Follow the permanent subsystem-scale rule.
2. Define per-thread retained invocation/return validation and continuation ownership before accepting the main priority transition. The current synchronous dispatcher cannot safely substitute a new CPU or unwind a sleeping child as a successful return.
3. Establish actual reverse-bind delivery and callback lifetime from local original captures/tooling. Registration/first-wait RAM proves these fields, not the complete cross-CPU transaction.
4. Execute the generated child registration/loop path on its owned context, preserve the main continuation, park the child at the evidenced wait and complete the pending reverse bind using owned registered state. Do not replay entry code on resume or fabricate completed RPCs.
5. Follow related execution blockers, test each meaningful change, then run full build/tests and capture comparisons. Run the full startup probe last because CTests overwrite its report. Refresh PROG/README/component scopes and snapshot, append monthly evidence, review, commit and push.

To rerun the prepared local checkpoint from your checkout:

```powershell
cmake --build build/probe --config Debug --target bt2-entry-probe
python tools/ps2recomp/run_startup_probe.py --disc "PATH_TO_YOUR_BT2_USA_ISO"
```

The original first-wait snapshots are available. The unresolved scope is a new scheduler/continuation and reverse-RPC delivery implementation, not another worker descriptor stub. Archive payload loading, DMA consumption/IRQ, native audio and frame presentation remain unverified.
