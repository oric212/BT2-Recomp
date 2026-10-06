# CRI RPC subsystem boundary

## Observed handoff

After the owned directory-sector read, generated parsing, five listed file lookups and further generated initialization, EE execution submits a bind at selector 0x77 / PC 0x32F028. RPC ID is 52, client 0x355C70, service 0x90000200. No native bind response or command completion is supplied.

The resident CRI_ADXI module already owns that server. Existing validation checks its child RPC worker (entry +0x7D4, priority 48, 0x800-byte stack), queue +0xED80, descriptor +0xED30, original handler +0x720, buffer +0xE7C0 and module GP. Its parent waits for Vblank-end. These registrations establish ownership, not request execution, streaming or native audio playback.

## Design and evidence required

- Capture the original EE bind/semaphore/result and identify the first actual CRI command, input/output extents and completion mode. Verify retail module code, GP and worker/server ownership.
- Consult REFERENCE_ARCHITECTURE.md and its pinned BT3 runtime/IOP routing patterns. Adapt the retained native worker pattern; do not copy game-specific implementations or assume command semantics.
- Define request lifecycle, buffer ownership and synchronization with the parked parent/worker. Inspect actual reached imports before deciding whether existing kernel providers suffice or streaming/timer/SPU2 behavior is required.
- Execute the original translated handler for a bounded observed command; validate primary/secondary transfers and generated EE cleanup. Preserve unknown function names and reject unsupported commands or completion conditions.
- Follow subsequent blockers within the bounded subsystem. Keep stream contents, captures and generated code ignored. Test, update PROG.md/history, commit and push at the end of the pass.

The command protocol and required streaming/scheduling scope remain unknown. Do not return successful initialization merely because a server was registered. The completed disc scope is documented in DISC_COMMAND_PLAN.md.
