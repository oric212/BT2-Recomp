# Active pass: CRI second-channel setup and startup workers

Updated: 2026-10-06. **Status: implemented locally, final verification incomplete.**
This is an unfinished implementation pass, not a completed pass. A later documentation-only commit does not verify its code.

## Last measured checkpoint

User-confirmed startup run: **291 EE entries entered / 286 completed**, kernel **2 / 2**, 3,855 calls / 3,848 returns, 61,612 IOP instructions, 37 CRI calls, six heap allocations. Boundary: worker ReferThreadStatus, selector `0x30`, PC `0x32EB68`. Report: ignored `work/bt2-generation/entry-stop.json`; this path is overwritten by later runs. The code baseline before the unfinished pass is `2eb87cb`; `ab4baa4` records the measured progress only. The measured binary includes local changes, so neither commit alone reproduces that checkpoint.

## Local implementation and scope

| Work | Files / evidence | State |
| --- | --- | --- |
| Second-channel heap capacity and owned module placements | `config/iop_layout.json`, `runtime/iop/compiled_modules.cpp`, `runtime/iop/ee_loadfile_bridge.cpp`, layout tools | Verified at the measured 37-call startup checkpoint; pass uncommitted |
| Five later second-channel constructor requests | `runtime/iop/service_tests.cpp`, `tools/capture_ee/verify_cri_second_channel.py` | Verified in isolated service testing; full EE path has not executed them |
| Distinct entry/return counting and repeatable startup CLI | `runtime/probe_abi/ps2_runtime.h`, `runtime/entry_probe.cpp`, `runtime/bootstrap_tests.cpp`, `tools/ps2recomp/run_startup_probe.py` | Counts observed in startup; latest header edits still require rerun |
| Main status query | Same runtime header; `tools/capture_ee/verify_ee_main_status.py` | Only caller-consumed status/priority fields verified |
| New worker status, ready transitions and priorities | Same runtime header and probe reports | Implemented locally, **unverified**; worker execution/scheduling is not implemented |
| Original worker/main evidence | `tools/capture_ee/capture_ee.py`, capture tests, ignored `work/pcsx2-capture/` | Original matching workers use IDs 6/7; owned native sequence is 4/5 |

Worker neutral entries are `0x2EE050` and `0x2EE140`. The implementation intends to query/start them and change their priorities to 18/25 while main priority remains 2. It does not execute their bodies. Original SetupThread exit `0x100250` is used only under checked ownership. Do not assume retail/native thread IDs match.

## Remaining verification / resume order

- Check whether the entry build finished: ignored `work/iop/startup-workers-entry-build.log`. Rebuild if uncertain:
  ```powershell
  cmake --build build/probe --config Debug --target bt2-entry-probe --parallel 4 -- /nodeReuse:false
  ```
- Add meaningful tests for worker query/start/priority ordering, context ownership and rejection without mutation. Existing new tests cover distinct accounting and the main query, not this latest worker group.
- Run the authored startup probe against the local ISO; preserve the resulting report before another test overwrites it:
  ```powershell
  python tools/ps2recomp/run_startup_probe.py --disc "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso"
  ```
- Continue through actual mapping/link/runtime blockers using the documented recomp-first workflow. Do not count intended worker transitions until observed.
- Update `tools/capture_ee/verify_ee_main_status.py`: it currently expects the previous worker-query boundary. Compare the actually reached state against original worker contexts; do not loosen unrelated assertions.
- Rebuild the current ALL targets and run the CTest suite using the build instructions. Rerun isolated CRI service validation, layout checks and applicable startup comparison tools. Last completed pass had 16/16 CTests; that result does not certify latest worker edits.
- Tooling checks already passed during this unfinished pass: 142 capture tests, 52 recompilation tests and 13 IOP tooling tests. Rerun affected checks after edits.
- Rerun the full startup probe last because CTests can overwrite its report. Record final distinct counts and boundary in `PROG.md`, synchronize README, append the detailed monthly outcome, review all staged files, commit and push the verified implementation.

## Next design/evidence boundary

The expected later main priority change from 2 to 24 would make the ready priority-5 EE child eligible. Actual retained EE thread execution and reverse RPC service `0x80000704` need context ownership and first sleep/wait evidence. Consult `EE_SOUND_SCHEDULING_PLAN.md`, `EE_THREAD_START_PLAN.md` and `REFERENCE_ARCHITECTURE.md`. Do not acknowledge a scheduling transition by returning success without implementing its effects. This expected boundary has not been established by the latest unverified run.

No complete startup, native audio, presented frame or gameplay claim is made. Keep proprietary inputs, generated translations and capture reports ignored.