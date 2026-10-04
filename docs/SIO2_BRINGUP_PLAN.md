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

## Reached device-transfer boundary (2026-10-04)

DS2U_D executes registered compiled exports using the registering module GP. Its startup acquires a positive semaphore count, copies owned RAM, sets/clears real event flags and resumes the actual SIO2MAN worker. That worker reads its previously written control value and writes command/port configuration. These values are retained without producing a device response. At native PC 0x10148 it writes byte 1 to 0x1F808260 (SIO2 transmit FIFO); the write remains unsupported. Five residents are acknowledged, DS2U_D remains pending, and six other workers wait. Current SIO2MAN state is active, so the earlier parked startup snapshot is historical.

This is a new device/protocol scope beyond module initialization. Before accepting FIFO traffic, establish the complete observed packet and configured port/device, command queue lengths, transmit/receive ownership, and completion ordering from local evidence. Consult pinned PCSX2 Sio2.cpp/IopHwWrite.cpp and BT3 runtime structure. Choose the bounded device model needed by this actual startup request, retaining unknown fields rather than default replies. Validate response bytes and native event/IRQ/return effects against local reference evidence before completing DS2U_D. Do not equate a retained configuration write with a completed transfer.

Current scheduling is cooperative and scoped: FIFO event-wait satisfaction, priority order among woken threads and actual saved-context execution. Nested worker wakes, blocking native-export call frames, general preemption, device responses, SIO2 DMA/IRQ completion and Vblank timing remain unsupported.
