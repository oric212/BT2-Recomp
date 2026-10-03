# Reverse-engineering record

Record conclusions with their source, method, and reproducible evidence. Distinguish observations from hypotheses. Do not assign function names, structures, addresses, or subsystem behavior without evidence. Preserve unknown names such as `FUN_00123400`; this is an example, not a discovered BT2 function.

## Executable

Planned target: North American SLUS-21441. Preliminary validation of local `rom/SLUS_214.41` passed on 2026-10-03 using `tools/verify_game/verify_game.py`:

- Size: 2,675,936 bytes.
- SHA-256: `f7e5ba8e74657b8533c3a0fedff45de0d052be3a23fe4c475059ef888a81f03e`.
- Basic header: ELF32, little-endian, MIPS, executable.
- Sibling `SYSTEM.CNF` boot reference: `cdrom0:\SLUS_214.41;1`.

These observations establish a candidate match. No trusted reference hash has been supplied to authenticate the exact retail revision. ELF tables and code have not been analyzed.

## Memory Map

No memory regions or addresses have been confirmed.

## Functions

No functions have been analyzed or named.

## Subsystems

No BT2 subsystem behavior has been established.

## File Formats

No game file formats have been inspected.

## Runtime Calls

No PS2 runtime calls used by BT2 have been identified.

## Confirmed Facts

The native host builds and prints its startup message without loading game data. This is a host observation, not a conclusion about the retail game.

## Hypotheses

None recorded.

## Unknowns

Exact retail revision, ELF table metadata, entry point, memory layout, function boundaries, file formats, and required PS2 runtime behavior remain unverified.
