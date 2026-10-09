# Resume handoff

Updated: 2026-10-08. **No pass in progress.**

The verified checkpoint is **1254 entered / 1243 completed** (650 frames). History: [2026-10](2026-10.md). Source layout: [RUNTIME_LAYOUT.md](../guides/RUNTIME_LAYOUT.md).

## Next implementation

Strategy change (2026-10-09): follow "Current next step" in [the implementation plan](../project/IMPLEMENTATION_PLAN.md#current-next-step-change-of-strategy).

1. **Profile** the 650-frame Release startup run per subsystem and record the baseline in the monthly history.
2. **Audit and generalize** address-gated handlers in `runtime/ee/` and `runtime/iop/`, starting with behavior verified at more than one site; add a regression test for each.
3. **GIF DMA chain mode** (current blocker: `0x1000A030`, pc `0x312084`) as a general DMA chain walker shared with VIF1. References: PCSX2 `Dmac.cpp`/`Gif.cpp`, PS2Recomp's GIF arbiter, paraLLEl-GS for the GS side.
4. **Opening movie:** IPU MPEG-2 decoding needs `docs/plans/video/IPU_PLAN.md` first.
5. **After the movie:** the title screen waits for Start; without input the game runs a CPU-vs-CPU demo fight (user report).
6. **Frame field:** handler-delivered VBlanks while main sleeps do not toggle `display_field`; check against an original capture if a field-dependent path diverges.
7. **Memory card, host input (XInput/SDL, keyboard), 16:9:** unchanged.

## Comparisons outside the gate script

Run these with their own inputs (`$C=work/pcsx2-capture`, outputs to a scratch folder): `verify_cd_init.py $C/cd-init-after/capture.json --semaphores-capture $C/cd-semaphores/capture.json`; `verify_rpc_bind.py $C/sif-bind-after/capture.json --call-sema-capture $C/rpc-call-sema/capture.json`; `verify_heap_bind.py`, `verify_heap_allocation.py` and `verify_heap_copy.py` with `--capture $C/fileio-bind-after|heap-alloc-after|heap-copy-after --disc <ISO> --output <scratch>`; `verify_spu2_transfer.py --disc <ISO>`; `verify_disc_read.py --disc <ISO> --native work/bt2-generation/sound-worker-stop.json --workspace 0x1D4B00`. `cri_rpc`, `second_heap`, `cri_preemption` and `spu2_initialization` currently fail.

## Possible follow-ups (not started)

- **GS rasterizer:** about 75% of a Release run.
- **Map-gap iterations:** these rebuild everything, because the generated declaration header changes.
- **Tests:** `tests/bootstrap_tests.cpp` could be split by subsystem.
- **Loop tooling:** the selection-loop driver and receipt refresh still live in scratch scripts; move them into `tools/ps2recomp/`.
