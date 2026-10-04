# Current progress

Target: SLUS-21441, native Windows x64 static recompilation. Updated: 2026-10-04.

## Current status

The C++20 host and automated map/recompilation pipeline work locally. Generated startup now completes the first CD-init mode-0 RPC using a validated local reply checkpoint, returns through cleanup and CD semaphore/command-handler setup, then enters the next service bind. This is bounded startup compatibility; complete boot, disc access and gameplay remain incomplete.

Latest verification: MSVC build/full checkpoint pipeline, seven CTests, 42 recomp-tool tests and 24 capture-tool tests passed. Game files, BIOS, captures, generated sources and automatic binding selections remain ignored. Retail revision is confirmed; function boundaries remain provisional.

## Current blocker and next step

The next generated RPC bind reaches CreateSema (`0x40`), resume `0x0032EC68`, SP `0x01FFEC40`. Descriptor: `[0x00370000,1,0,0,2,0x003838D8]`. The previous first-bind evidence does not establish this new call's semaphore result or service reply.

Next: capture this reached bind's semaphore and request/reply effects locally, validate the actual service identity and queue, then rerun. No general CD service or disc result is fabricated.

## Key counts

- **7,262** locally generated functions; **7,271** raw generated C++/header files.
- **76** distinct functions entered / **72** completed across native probes (startup/helpers, not gameplay).
- This pass completes `FUN_00332970`, `FUN_0031F8B0`, `FUN_0031EDB0`, `FUN_0031F010` and `FUN_0032ECB0`. Entry, `FUN_00100388`, `FUN_00320488` and `FUN_0031F6A8` remain partial. Previously completed helpers can stop on new inputs.

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

- [October 2026 detailed history](progress/2026-10.md): all previous progress entries, evidence, counts and historical milestone snapshots preserved. Statements about "current" work inside that archive describe their original point in time.
- [Implementation plan](IMPLEMENTATION_PLAN.md), [tool workflow](TOOLS.md), [reverse-engineering evidence](REVERSE_ENGINEERING.md).

Keep this page limited to current status. Append detailed pass results to `progress/YYYY-MM.md`, linking new monthly logs here. Update the blocker, counts and checklist after verification.
