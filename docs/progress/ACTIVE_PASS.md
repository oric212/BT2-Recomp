# Resume handoff: queued RPC scheduling

Updated: 2026-10-06. **Retained sound-worker diagnostic and reverse-bind pass verified.**

## Verified checkpoints

Normal startup remains **296 entered / 292 completed**, kernel 2/2, 3,892 calls / 3,887 returns, 61,612 IOP instructions, 37 CRI calls and six heap allocations. Main ChangeThreadPriority(1, 24), selector 0x29 / PC 0x32EAF8, remains pending.

Separate `--sound-worker` diagnostic: **302 / 295**, 3,903 calls / 3,897 returns, **81,641 IOP instructions**. Generated sound queue/server/loop reaches first EE SleepThread at 0x32EB88. One actual native reverse bind completes and SDRDRV parks at its original first IOP sleep, 0xA1304 / RA 0xA0F24. The EE diagnostic stack is cancelled while child return ownership is still selected; main does not resume. Six extra generated entries are selected, now 232.

## Important ordering finding

The original child's first-wait capture still contains the earlier SOUNDS bind (RPC ID 29 / client 0x6351C0 / service 0x02000004). Original main-priority-before already has the completed reverse-bind client. Our synchronous RPC completion prevented earlier dispatch. Do not enable the late diagnostic as the normal scheduler or acknowledge priority 24 by skipping eligible helpers.

## Next pass

Read `../PROG.md`, `../EE_RPC_SCHEDULING_PLAN.md`, `../EE_SOUND_SCHEDULING_PLAN.md` and `../REFERENCE_ARCHITECTURE.md`. Implement persistent per-thread invocation ownership and queued EE/IOP RPC completion at the earlier blocked-main bind phase. Preserve the main continuation and actual sound wait. Follow subsequent helpers 5/6 and execution blockers; their bodies remain unverified. Do not invent command callbacks or wake ownership.

The new design changes transport scheduling. The current diagnostic is verified validation tooling, not an interrupted unverified scheduler patch.

## Verification and private evidence

- MSVC entry/ALL builds; 17/17 CTests; 146 capture tests; 52 recompilation tests; 13 IOP tooling tests.
- Sixteen existing normal-startup comparisons and separate `verify_ee_sound_dispatch.py` comparison.
- Actual bind and first-sleep captures under `work/pcsx2-capture/sound-init-before-absent-cards-ee-reverse-bind[-wait]/`.
- Reports: `work/bt2-generation/entry-stop.json` and separate `sound-worker-stop.json`, with ignored RAM sidecars.
- Logs under `work/iop/ee-sound-*` and `ee-reverse-bind-*`. Callback-entry observation timed out; no payload/lifecycle conclusion.

```powershell
python tools/ps2recomp/run_startup_probe.py --disc "PATH_TO_YOUR_BT2_USA_ISO"
python tools/ps2recomp/run_startup_probe.py --disc "PATH_TO_YOUR_BT2_USA_ISO" --sound-worker
python tools/capture_ee/verify_ee_sound_dispatch.py --native work/bt2-generation/sound-worker-stop.json
```

After a verified pass, update normal and diagnostic scopes separately, append monthly evidence, review, commit and push. No presentation or native playback is verified.
