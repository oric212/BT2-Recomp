# Scoped SIO2 startup and scheduling

## Evidence and scope

The first native SIO2MAN entry writes 0x3BC to control 0x1F808268. Pinned PCSX2 identifies SIO2MAN_RESET and START_TRANSFER bit 0 separately. The observed reset write records owned control state; any other control value, FIFO/status access or transfer remains unsupported. No ready status, DMA completion or IRQ is invented.

Public PS2SDK supplies event/semaphore/thread parameter layouts, first thread startup, interrupt registration/suspend/resume and SIO2 DMA priority/enable semantics. BT3's module/import/scheduler separation informs structure; its permissive success defaults are not adopted. Only reached providers are installed.

## Implemented checkpoint

- Actual entry returns resident result 0 after 151 native instructions.
- Library registration, one event, one semaphore, one dormant-then-started thread, IRQ 17 registration/enabling and DMA channels 11/12 priority 3/enabling retain owned state.
- A separate thread stack and context retain PC/GP/HI/LO/branch/load state. The created thread runs to WaitEventFlag for mask 0x4155, OR mode, and yields. No event completion is supplied.
- The owned diagnostic kernel begins with interrupt dispatch enabled, matching public THREADMAN startup convention. Nested suspend/error handling and general scheduling are explicitly unsupported.
- Started worker execution is scoped to this observed initialization path. It does not establish general priority/preemption/timing fidelity. Synthetic native tests verify parked import resumption, register/return state, result bits and WEF_CLEAR behavior.
- Local capture comparison validates normalized module BSS and reference semaphore/event/thread wait ownership. Retail kernel record offsets are observations of this scenario, not a portable SDK structure. Opaque options differ because the reference caller stack contains prior data; no semantic equivalence is claimed for those options. Full reference IOP snapshot remains live.

## Continue from here

Follow [the EE LOADFILE lifetime bridge plan](IOP_LOADFILE_BRIDGE_PLAN.md) to connect the completed entry and retained worker. Then gather reached SIO2 accesses/requests and before/after state. Implement only observed control, command, FIFO, DMA and IRQ behavior with explicit known/unknown fields. Resume waiting native threads only when a real modeled event satisfies their masks. Validate transfer results against local captures before acknowledging completion. Controller/memory-card protocol work is a separate scope when actually reached.
