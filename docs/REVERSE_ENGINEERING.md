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

No functions have been analyzed or named.

## Subsystems

No BT2 subsystem behavior has been established.

## File Formats

Only executable container metadata has been inspected. The section table includes `.text`, `.vutext`, `.data`, `.rodata`, `.bss`, and `.DVP.*` names; their game-specific purpose has not been established. No SHT_SYMTAB or SHT_DYNSYM section was observed. Other game file formats remain uninspected.

## Runtime Calls

No PS2 runtime calls used by BT2 have been identified.

## Confirmed Facts

The native host builds and prints its startup message without loading game data. This is a host observation, not a conclusion about the retail game.

## Hypotheses

None recorded.

## Unknowns

Exact retail revision, actual runtime memory behavior, the empty load record's purpose, function boundaries, game file formats, and required PS2 runtime behavior remain unverified. No instructions or functions have been analyzed. Raw metadata output stays in ignored `work/elf_report.json`.
