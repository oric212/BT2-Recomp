# Reverse-engineering record

Record conclusions with their source, method, and reproducible evidence. Distinguish observations from hypotheses. Do not assign function names, structures, addresses, or subsystem behavior without evidence. Preserve unknown names such as `FUN_00123400`; this is an example, not a discovered BT2 function.

## Executable

Planned target: North American SLUS-21441. Preliminary validation of local `rom/SLUS_214.41` passed on 2026-10-03 using `tools/verify_game/verify_game.py`:

- Size: 2,675,936 bytes.
- SHA-256: `f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e`.
- Basic header: ELF32, little-endian, MIPS, executable.
- Sibling `SYSTEM.CNF` boot reference: `cdrom0:\SLUS_214.41;1`.

These observations establish a candidate match. No trusted reference hash has been supplied to authenticate the exact retail revision.

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

## Subsystems

No BT2 subsystem behavior has been established.

## File Formats

Only executable container metadata has been inspected. The section table includes `.text`, `.vutext`, `.data`, `.rodata`, `.bss`, and `.DVP.*` names; their game-specific purpose has not been established. No SHT_SYMTAB or SHT_DYNSYM section was observed. Other game file formats remain uninspected.

## Runtime Calls

Entry contains syscall instructions at `0x001001C8` and `0x001001E4`, each with encoded code field zero. The preceding Ghidra instructions load register `v1` with `0x3C` and `0x3D`, respectively. These observations do not assign syscall names or establish the required runtime ABI. No runtime implementation has been added.

The direct callee at `0x0032EEE0` loads `v1` with `0x64`, executes a syscall with encoded code field zero at `0x0032EEE4`, and returns through `ra`. Its meaning remains pending primary-source ABI confirmation.

## Confirmed Facts

The native host builds and prints its startup message without loading game data. This is a host observation, not a conclusion about the retail game.

## Hypotheses

None recorded.

## Unknowns

Exact retail revision, actual runtime memory behavior, the empty load record's purpose, function boundaries, game file formats, and required PS2 runtime behavior remain unverified. Ghidra reported a conflicting `rom0` block, unsupported byte mapping for DVP overlays, and decompiler flow errors. Candidate maps, disassembly, logs, and projects stay under ignored `work/ghidra/`; raw ELF metadata stays in ignored `work/elf_report.json`. Boundaries require manual review before generation.
