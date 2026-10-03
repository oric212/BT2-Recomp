# Reverse-engineering record

Record conclusions with their source, method, and reproducible evidence. Distinguish observations from hypotheses. Do not assign function names, structures, addresses, or subsystem behavior without evidence. Preserve unknown names such as `FUN_00123400`; this is an example, not a discovered BT2 function.

## Executable

Planned target: North American SLUS-21441. Preliminary validation of local `rom/SLUS_214.41` passed on 2026-10-03 using `tools/verify_game/verify_game.py`:

- Size: 2,675,936 bytes.
- SHA-256: `f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e`.
- Basic header: ELF32, little-endian, MIPS, executable.
- Sibling `SYSTEM.CNF` boot reference: `cdrom0:\SLUS_214.41;1`.

Retail revision is now confirmed: the local 2,998,829,056-byte ISO matches the official [Redump PS2 datfile](http://redump.org/datfile/ps2/) USA entry (MD5 `37393b158051af0ab5cb3c86d04d51b7`, SHA-1 `0b11f7a2a4c178b555c45a58ce8ee725e57a1423`). Its root `SLUS_214.41;1` extent at byte offset 624,640 has the same size and SHA-256 as the local executable above. `config/retail_revision.json` records reference provenance; `verify_disc.py` repeats the read-only comparison. No independent published ELF hash is claimed.

ELF metadata inspected with `tools/inspect_elf/inspect_elf.py` on 2026-10-03 using the same SHA-256 above. The header declares entry point `0x00100008`, four program headers, and 33 section headers. The declared entry lies within an executable load region. This does not mean the entry has executed.

## Memory Map

Declared ELF load regions (end addresses are exclusive; these are file metadata, not a confirmed hardware memory map):

| Program index | Virtual start | Memory end | File offset | File bytes | Memory bytes | Flags |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `0x00100000` | `0x0033f390` | `0x1000` | 2,356,112 | 2,356,112 | 7 |
| 2 | `0x0033f400` | `0x003b3038` | `0x240400` | 297,500 | 474,168 | 6 |
| 3 | `0x00000000` | `0x00000000` | `0x288ed0` | 0 | 0 | 7 |

Program 2 declares 176,668 bytes beyond its file payload. Program 3 is empty and declares physical address `0x003b3180`; its file offset and virtual address do not satisfy the generic ELF alignment relationship for its declared `0x1000` alignment. Its purpose is unknown and no loader behavior is inferred.

## Functions

Ghidra 12.1.4 with the pinned R5900 extension analyzed the recorded local executable. It discovered 9,541 unreviewed function candidates; 7,241 have contiguous aligned bodies exported to the local CSV. These counts describe analyzer output, not verified boundaries or purposes. The entry candidate is `FUN_00100008`, with proposed exclusive end `0x00100248` (576 bytes). No semantic function name has been established.

### Entry boundary review, 2026-10-03

For the executable hash above, `tools/ghidra/review_entry.py` cross-checked raw ELF control instruction words against the Ghidra export. All 144 instruction addresses cover `[0x00100008, 0x00100248)` without gaps. Sixteen control transfers were audited, including two syscall instructions. Direct targets agree with the export, conditional branches stay within the body, and all delayed transfers include their following instruction.

The final transfer at `0x00100240` jumps to `0x00324398`; its delay slot at `0x00100244` is included. This supports the exclusive end `0x00100248` for a static entry-body map. It does not establish that the external target never returns, certify all opcode semantics, or prove behavior during execution. No full-game reviewed map exists.

| Direct call site | Neutral target |
| --- | --- |
| `0x001001E8` | `FUN_00338520` |
| `0x001001F0` | `FUN_0032EEE0` |
| `0x00100220` | `FUN_003242D0` |
| `0x00100238` | `FUN_00100388` |

These are encoded targets, not established purposes or reviewed callee boundaries. An indirect call through register 16 occurs at `0x00100208`; its reachability and target require separate analysis.

### Direct entry target boundary review, 2026-10-03

The exporter now includes complete instruction listings for the five direct targets. `tools/ghidra/review_entry_targets.py` checks their file-backed ranges, raw control targets, complete instruction coverage, internal conditional branches, and included delay slots against the same executable hash. All five passed the limited static audit:

| Neutral name | Exclusive end | Instructions | Transfers | Final transfer |
| --- | --- | --- | --- | --- |
| `FUN_00338520` | `0x00338574` | 21 | 9 | Jump to `0x0033A238` |
| `FUN_0032EEE0` | `0x0032EEF0` | 4 | 2 | Return through `ra` |
| `FUN_003242D0` | `0x00324370` | 40 | 5 | Return through `ra` |
| `FUN_00100388` | `0x001006D8` | 212 | 72 | Return through `ra` |
| `FUN_00324398` | `0x00324444` | 43 | 8 | Jump to `0x00100248` |

These extents are supported for static body mapping; runtime semantics and deeper callees remain unverified. `FUN_00324398` includes branch-likely instructions and two indirect calls through `v0`; the audit records likely branches without claiming to execute their annulled delay-slot behavior.

At `0x00100688`, `FUN_00100388` directly calls `0x003B7FA8`, outside the ELF's file-backed executable sections. Its origin and contents are unknown. Do not create a function body or stub for it without evidence. Ghidra assigned a semantic label to `0x0032EEE0`; retain the neutral name because the label's provenance has not been confirmed.

### Unmapped target investigation, 2026-10-03

`tools/ghidra/audit_startup.py` independently checked the raw entry constants and mapped sections for the recorded hash. `0x003B7FA8` is outside all declared PT_LOAD memory extents, including zero-fill regions. The entry constructs bounds `0x00387E80` and `0x0043058F`; its observed byte/quadword zero-store loops appear to clear that interval before the first direct callee. The unresolved target lies within this larger interval. This is static evidence, not an observed memory write during execution.

An aligned scan of allocated file-backed sections found no exact 32-bit literal equal to the target. Executable-section scanning found one encoded JAL to it, at `0x00100688`. These scans do not exclude split address constants, calculated pointers, encoded data, or code copied/produced later. The target's contents and the writer remain unknown. A future memory layer must account for observed startup accesses beyond ELF load extents rather than assuming those extents describe every runtime access.

## Subsystems

No BT2 subsystem behavior has been established.

## File Formats

Only executable container metadata has been inspected. The section table includes `.text`, `.vutext`, `.data`, `.rodata`, `.bss`, and `.DVP.*` names; their game-specific purpose has not been established. No SHT_SYMTAB or SHT_DYNSYM section was observed. Other game file formats remain uninspected.

## Runtime Calls

Entry contains syscall instructions at `0x001001C8` and `0x001001E4`, each with encoded code field zero. The preceding Ghidra instructions load register `v1` with `0x3C` and `0x3D`, respectively. These observations do not assign syscall names or establish the required runtime ABI. No runtime implementation has been added.

The direct callee at `0x0032EEE0` loads `v1` with `0x64`, executes a syscall with encoded code field zero at `0x0032EEE4`, and returns through `ra`.

### Startup ABI reference, 2026-10-03

PS2SDK revision `ac92a9f657d2e531dd8f060250b07f2a5ac6dea5` defines selectors `0x3C` as SetupThread, `0x3D` as SetupHeap, and `0x64` as FlushCache. Its assembly wrapper puts the selector in register 3 (`v1`) and executes syscall; its startup code consumes the SetupThread return from register 2 (`v0`) as the stack pointer. These primary-source conventions match the reviewed local instruction sequences. This supports the following ABI interpretation, while actual retail kernel behavior remains untested. Sources: [selector definitions](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/include/syscallnr.h), [assembly wrapper](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/src/kernel.S), and [startup](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/startup/src/crt0.c).

| Local syscall site | Selector / public ABI | Observed arguments |
| --- | --- | --- |
| `0x001001C8` | `0x3C` / SetupThread | `a0=0x0038BDF0`, `a1=0xFFFFFFFF`, `a2=0x00100000`, `a3=0x0038B980`, `t0=0x00100250`; returned `v0` copied to `sp` |
| `0x001001E4` | `0x3D` / SetupHeap | `a0=0x0043058F`, `a1=0xFFFFFFFF` |
| `0x0032EEE4` | `0x64` / FlushCache | Entry call sets `a0=0` in its delay slot |

PS2SDK declares SetupThread parameters as gp, stack, stack size, args, root function; SetupHeap takes heap start and size. Its FlushCache operation zero is WRITEBACK_DCACHE. This identifies `FUN_0032EEE0` as a matching FlushCache syscall wrapper; retain its neutral working name in maps. Pointer contents, sentinel handling for `0xFFFFFFFF`, alignment, stack placement, heap limits, and cache behavior require further evidence before implementation. [Public declarations and cache modes](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/kernel/include/kernel.h).

Reference revision and downloaded file hashes are recorded in `config/startup_abi.sources.json`. Reference files remain ignored under `work/abi-reference/ps2sdk/`; no SDK implementation is linked or copied into the runtime.

## Confirmed Facts

The native host builds and prints its startup message without loading game data. This is a host observation, not a conclusion about the retail game.

The optional native generated probe executes PS2Recomp's local translation of `FUN_0032EEE0` from the inspected executable through an authored adapter. It reaches a recorded FlushCache request and returns to a host sentinel. It performs no guest memory access and establishes no whole-game boot or cache-equivalence behavior. The broader provisional EE map and generation results are recorded in [the detailed progress history](progress/2026-10.md); [PROG.md](PROG.md) summarizes current status.

On 2026-10-03, the generated entry also compiled/linked and executed through an authored bounded-RAM bootstrap adapter. With the observed uninitialized region poisoned for verification, it cleared `[0x00387E80, 0x0043058F)` with 43,120 quadword and 15 byte stores. Execution reached the first syscall with selector `0x3C`, resume PC `0x001001CC`, and argument registers matching the static review above. The runtime deliberately rejected that unsupported syscall. Its return value, stack behavior, later code and whole-game boot remain unobserved. Local evidence is in ignored `work/bt2-generation/entry-stop.json` and test logs.

## Hypotheses

The unresolved target may acquire executable contents after the startup zero-store loops. This is an investigation hypothesis; no writer, source bytes, or executed contents have been identified.

## Unknowns

Actual runtime memory behavior, the empty load record's purpose, general function boundaries, game file formats, and required PS2 runtime behavior remain unverified. Ghidra reported a conflicting `rom0` block, unsupported byte mapping for DVP overlays, and decompiler flow errors. Candidate maps, disassembly, logs, and projects stay under ignored `work/ghidra/`; raw ELF metadata stays in ignored `work/elf_report.json`. Experimental generation uses structurally filtered provisional maps; focus further manual review on actual build and execution blockers.

### Bootstrap stack model (not retail BIOS confirmation)

[Play! kernel source](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/PS2OS.cpp) and [context reserve](https://github.com/jpd002/Play-/blob/83700b2c31e593bc94e845b4b31b797be84dda59/Source/ee/PS2OS.h) provide the current default-stack model. The authored adapter supports only the observed sentinel and empty launch arguments. Execution now reaches SetupHeap; its implementation and retail BIOS stack comparison remain pending.

### SetupHeap bootstrap model and executed call boundary

The pinned Play! `sc_SetupHeap` returns main-thread stack base for size `0xFFFFFFFF`. Our bounded adapter records the requested lower boundary without changing its alignment and returns `0x01EFF000`. This model has not been compared against retail BIOS behavior. Generated entry now reaches JAL at `0x001001E8` to `FUN_00338520`, return PC `0x001001F0`; the callee has not executed yet.

### Executed first callee prologue

Generated `FUN_00338520` now executes its stack allocation and saves RA=`0x001001F0` at guest address `0x01FFED50`. Runtime then observes the JAL at `0x00338528` to `FUN_003382D0`, resume `0x00338530`. This establishes execution of the prologue only; no subsystem purpose has been assigned and no deeper target is yet executed.

### Executed nested startup syscall

`FUN_003382D0` builds a stack frame and calls `FUN_0032EC60` at `0x00338304`, with a0=`0x01FFED00` and resume `0x0033830C`. The generated wrapper sets v1=`0x40` and executes syscall at `0x0032EC64`, resume `0x0032EC68`. Pinned PS2SDK `syscallnr.h` identifies selector `0x40` as CreateSema; no subsystem purpose is inferred for the enclosing unknown functions. The syscall has not been implemented or returned yet.

### Executed semaphore creation and return paths

Generated startup now stores CreateSema IDs 0 and 1 at `0x0036AB58` and `0x0036AB5C` under the bounded compatibility model. Both objects initialize count/max_count to 1, with option values `0x00383BF0` and `0x00383C00`. Three generated JR return paths complete, and the outer caller reaches JAL at `0x00338530` to neutral `FUN_00338410`, return PC `0x00338538`. No semantic name has been assigned to the enclosing functions. Wait/signal/scheduler behavior and retail BIOS equivalence remain unverified.

### Executed SetSyscall registrations

Pinned PS2SDK names selector `0x74` SetSyscall and declares a void result. Generated startup now records `0x83` -> `0x00338388` and `0x5A` -> `0x00338350` from loaded guest globals. Invocation of custom handlers remains unsupported. Execution next reaches `FUN_003383C8` from JAL `0x00338464`, return `0x0033846C`, with a0/a1/a2=`0x80000000`/`0x80080000`/`0x00338388`. Handler purposes and memory behavior remain unknown; no semantic function names assigned.

### Registered handler discovery blocker

Execution enters `FUN_003383C8` and invokes selector `0x83` at `0x003383CC`, resume `0x003383D0`. The table contains handler `0x00338388`, which has no function entry in the current Ghidra export or provisional map. No handler boundary or purpose is established. `inspect_blocker.py` records range membership and exclusions in ignored `blocker-map.json`; it never guesses a boundary.

### Discovered handler's actual memory blocker

Focused Ghidra discovery produced provisional `[0x00338388, 0x003383C8)`. The regenerated handler compiles and is entered, but its first word read at `0x80000000` is rejected by the current main-RAM-only adapter. Required address mapping and kernel-memory contents have not been established. The callback adapter is a host execution model, not confirmed BIOS exception behavior; no missing memory was synthesized to advance the scan.

### Kernel address evidence and fixture limits

[PCSX2 memMapKernelMem](https://github.com/PCSX2/pcsx2/blob/81526d4dc7cc70e4ae75abb35a789417456c6d43/pcsx2/Memory.cpp) maps `0x80000000` to physical zero. Our adapter exposes only a read-only captured window; it does not treat ELF zero-fill as kernel contents. Generated handler match/no-match behavior passes authored synthetic fixtures. Real capture comparison at PC `0x00338388` is pending, as are kernel table mutation and BIOS exception semantics.

### Snapshot-driven entry execution

With the explicitly supplied local capture, generated `FUN_00338388` returns twice through `FUN_003383C8`. `FUN_00338410` completes and stores `0x80014D00` at `0x0036AB40`; the two searches converge on the same value after their emitted offsets. Execution then attempts `FUN_00338A00` from `0x00338538`, resume `0x00338540`. This target is already provisionally mapped/generated, but is not registered in the bounded adapter. The capture window remains read-only and separate from main RAM; no kernel writes or BIOS exception semantics are established.

### First observed Timer 3 MMIO boundary

Generated `FUN_00338A00` now compiles/links/registers. It saves four doublewords in its 64-byte frame, then attempts a word read at `0x10001810`, PC `0x00338A1C`, through emitted `Load32`. The authored forwarding API uses the same checked read path and rejects MMIO. Pinned PS2SDK `common/include/ee_regs.h` (`A_EE_T3_MODE`) and `ee/kernel/include/timer.h` (`T3_MODE`) identify this register; hashes are recorded in `config/startup_abi.sources.json`. No value is supplied and no timer state or game-specific purpose is inferred. The emitted following instructions mask bit `0x100`; they have not executed in the native probe yet.

### Captured Timer 3 mode read and explicit replay

An instrumented local ELF copy reads Timer 3 mode once at the `0x00338A1C` boundary, stores the result beside an authored marker, and spins. PCSX2 2.9.81 with the local scph39001 BIOS returned `0x83`; the original ELF remains unchanged. The capture preserves PCSX2/BIOS/ELF/RAM hashes under ignored output. Pinned PCSX2 `HwRead.cpp` dispatches the counter read, `Counters.cpp` returns the low 16 bits of mode, and `Counters.h` identifies bit 8 as TargetInterrupt; hashes are in `config/startup_abi.sources.json`. The native probe now replays this explicitly supplied single read. It follows the clear-bit branch and reaches unregistered `FUN_00338998` from `0x00338A40`, resume `0x00338A48`, with arguments a0 `0x5A`, a1 `0x003389B8`. No general timer behavior or function purpose is claimed.

### Discovered registered handler reaches a kernel store

Generated `FUN_00338998` executes selector `0x74`, registers `0x003389B8` for selector `0x5A`, and returns. Generated `FUN_003389A8` then invokes `0x5A`; the observed handler was a map gap. Focused discovery produced provisional contiguous `[0x003389B8, 0x003389F0)`, 56 bytes, and regenerated the local map/output. The handler executes a word-count shift (`0x740 >> 2 = 0x1D0`), reads the first source word from `0x0036B328`, and reaches a word store to `0x80076000`, PC `0x003389D8`. The adapter rejects the write; no kernel bytes changed. The handler's complete loop, copied payload purpose, writable kernel behavior and BIOS/cache equivalence remain unverified. The function keeps its neutral name.

### Bounded patch overlay and returned generated copies

The entry probe explicitly enables aligned word stores only for `[0x80076000, 0x80076740)` after loading the captured kernel. Stores populate a separate overlay; KSEG0 word reads consult it, and baseline bytes remain immutable. No general KSEG1/physical alias coherency, guest kernel execution or instruction-cache invalidation is implemented. Generated `FUN_003389B8` copies 464 words from `0x0036B328`, returns through `FUN_003389A8`, then copies ten words from `0x0036BA68` to main RAM `0x00082000` and returns again. The probe compares every destination word against its source and checks the full baseline against its pre-run copy. Linked `FUN_0032EEE0` records an operation-0 FlushCache request and returns, then a second invocation stops at unsupported operation `2`, selector `0x64`, PC `0x0032EEE8`. This establishes returned copy paths, not cache/BIOS equivalence or a full boot.

## 2026-10-03 - Cache boundary and kernel handler registration

Pinned PS2SDK `kernel.h` defines FlushCache operation `2` as instruction-cache invalidation and declares a void return. The probe now accepts only observed operations `0`/`2`, preserves return registers, and records separate invalidation/total request counts. Its stores are immediately visible and dispatch is fixed native C++; no guest cache exists and no patched kernel instructions are translated by this call. This is a bounded compatibility behavior, not hardware cache validation.

Generated entry completes both requests, then SetSyscall (`0x74`) attempts to register `0x80076000` for `0x5B` at resume PC `0x003389A0`. Main-RAM registration validation rejects that address before installing it. This is handler-address validation, not a guest load fault. Captured baseline/copy comparisons remain unchanged. Handler meaning, execution and aliases remain unknown.

## 2026-10-03 - Copied kernel registration and dispatch

The bounded adapter now accepts aligned SetSyscall destinations in the enabled kernel overlay only if the entry word was actually written. This registers a pointer without translating or executing its bytes. Baseline-only, unwritten, adjacent, unsupported-alias and wrong-memory-pointer destinations remain rejected; null unregister still works and the void ABI preserves return registers.

Actual generated `FUN_00338998` registers `0x80076000` for `0x5B` and returns. Linked neutral wrapper `FUN_003389F0` invokes `0x5B` with observed `a0=0xFC`; dispatch rejects the uncompiled target before entering it, at resume PC `0x003389F8`. All previous copies/cache requests remain verified. The kernel destination is outside original file-backed ELF `.text`; it needs a separate payload map with source/copy provenance and runtime-address relocation. No purpose is assigned to the handler or argument.
