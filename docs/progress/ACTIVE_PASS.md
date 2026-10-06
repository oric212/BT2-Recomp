# Resume handoff: EE priority ownership

Updated: 2026-10-06. **Pass verified and ready to commit.**

Normal checkpoint: **302 entered / 295 completed**, kernel 2/2, 3,903 calls / 3,897 returns, 81,640 IOP instructions, 37 CRI calls, six heaps. Main ChangeThreadPriority(1,24), selector 0x29 / PC 0x32EAF8 remains pending at priority 2.

Queued SOUNDS completion now runs the original sound child at main's real bind wait, preserves its parked owned stack and resumes main once. The IOP reverse bind completes and its helper parks at the original first sleep. Separate --startup-workers diagnostic reaches 312/298 and helper 5's ChangeThreadPriority(5,1); separate --sound-worker retains its earlier diagnostic scope.

## Next implementation

Read ../PROG.md, ../EE_RPC_SCHEDULING_PLAN.md and ../REFERENCE_ARCHITECTURE.md. Admit main's priority 24 with owned previous-priority return before eligible dispatch. Implement observed helper critical/restore ownership (5:16→1→16; 6:18→1→18). Native helper first-wait guards are provisional/unreached. Helper 6 entry 0x2EE050 remains unmapped; use existing discovery tooling at the observed target. Do not skip eligible helpers or fabricate successful scheduler returns.

Helper 6 reads RAM flag 0x34BC84; its conditional hardware store targets BGCOLOR 0x120000E0. It does not read GS CSR. Both flag captures are zero. Original restore requests are captured; restore API returns are not.

## Verification and evidence

Serialized MSVC entry/ALL builds; 17/17 CTests; 153 capture, 55 recompilation and 13 IOP tests; 16 existing comparisons plus queued sound and two diagnostic comparisons passed. Final reports: ignored work/bt2-generation/{entry-stop,sound-worker-stop,startup-workers-stop}.json. Logs: work/iop/queued-final-*.log. Original helper evidence: work/pcsx2-capture/rpc-helper{5,6}-*; queued sound comparisons use the existing reverse-bind and first-wait captures.

Map: 8,585 entries / 8,588 raw files / 242 selected extras. Monthly history retains full findings. No startup completion, native audio playback or presented game frame is verified.
