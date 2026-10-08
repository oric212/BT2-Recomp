# Developer tooling

Start with [building and game-file preparation](BUILDING.md). This guide describes development tools and successive checkpoint evidence; it is not a player setup wizard. Older probe descriptions below document the bounded tests introduced at that stage. Use [PROG.md](PROG.md) for the current boundary.

## Paths in examples

Run commands from your own repository checkout. Paths such as `rom/SLUS_214.41` and `work/` are relative local input/output locations. They do not contain game data in a fresh clone. For PCSX2 examples, set the following PowerShell variables to your own installed executable and locally supplied BIOS; replace the placeholder strings before running:

```powershell
$Pcsx2Path = 'PATH_TO_PCSX2_EXE'
$BiosPath = 'PATH_TO_YOUR_PS2_BIOS'
```

Disc-backed captures must also pass `--disc rom/BT2-USA.iso` (or your actual ISO path). Tool defaults may reflect a development environment; pass explicit paths when your setup differs.

## Preliminary validation

Requires Python 3.10 or newer; uses only the standard library. From the repository root:

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41
```

The tool reads the local executable without changing it and prints JSON containing size, SHA-256, and preliminary validation results. It checks the filename and basic ELF32 little-endian MIPS executable header. A sibling `SYSTEM.CNF`, when present, must name `SLUS_214.41` in its boot reference. Use `--system-cnf PATH` to supply it explicitly.

Use `--expected-sha256 HASH` to compare against an independently trusted reference. Without that reference, successful preliminary checks do not authenticate the retail revision. This tool does not parse ELF tables, inspect functions, or load the game. Failures return a nonzero exit code.

Keep game inputs under ignored `rom/` or `game_data/` and temporary reports under ignored `work/`.

## ELF metadata inspection

```powershell
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41
```

The inspector reports entry point, program headers, declared load regions, and section headers/names as JSON. Addresses in JSON are decimal integers. It checks table and payload bounds, load sizes, allocated address overflow, and section-name references. SHT_NOBITS sections need no file payload. Generic load alignment anomalies are warnings and are retained in the report. Extended numbering is explicitly unsupported. This is a metadata inspector, not a loader, disassembler, or full ABI conformance checker.

To save a local report:

```powershell
New-Item -ItemType Directory -Path work -Force | Out-Null
python tools/inspect_elf/inspect_elf.py rom/SLUS_214.41 | Set-Content work/elf_report.json -Encoding utf8
```

Run synthetic tests without game data:

```powershell
python -m unittest discover -s tools/inspect_elf -p "test_*.py"
```

Field layouts and generic semantics follow the ELF specification: [header](https://gabi.xinuos.com/elf/02-eheader.html), [program headers](https://gabi.xinuos.com/elf/07-pheader.html), and [section headers](https://gabi.xinuos.com/elf/03-sheader.html). Processor-specific values are recorded without guessing their meaning.

## Pinned PS2Recomp tools

Requires Python 3.10+, Git, CMake 3.21+, and Visual Studio 2022/MSVC x64. Internet access is needed to fetch the open-source sources on the first build. The host's normal CMake build stays independent.

```powershell
python tools/ps2recomp/build_tools.py
python tools/ps2recomp/smoke_test.py
python -m unittest discover -s tools/ps2recomp -p "test_*.py"
```

The build checks immutable revisions from `config/ps2recomp.lock.json`, stores clean checkouts in ignored `work/dependencies/`, and builds tools in ignored `build/ps2recomp-tools/`. Upstream runtime, studio, and test targets are disabled. Supplying dependency paths directly to CMake avoids Git submodule shell helpers in the Windows sandbox. Builds run serially with MSBuild node reuse disabled. Local provenance records the lock-file and recompiler hashes.

The smoke test creates our own ELF containing `addiu v0, zero, 42; jr ra; nop` and our own function map, then checks the emitted C++. It does not compile or execute that output and establishes no BT2 function boundaries.

## Local BT2 generation

Automatically prepare a provisional map from our local Ghidra export with `tools/ps2recomp/prepare_map.py`. It retains contiguous aligned file-backed `.text` candidates, neutralizes names, and quarantines overlapping, oversized, noncontiguous or out-of-section candidates. It does not treat VU code as EE code or certify boundaries. Keep maps and exclusion/provenance manifests under ignored `work/`. Manual investigation follows actual build/execution blockers.

```powershell
python tools/ps2recomp/prepare_map.py
python tools/ps2recomp/generate_game.py --candidate-map work/bt2-generation/candidates.csv
```

CSV columns are `Name,Start,End,Size` (case insensitive), with exclusive ends. The driver checks executable identity, ranges/overlaps, candidate manifest, and pinned tool provenance. `--function-map PATH` remains available for explicitly reviewed maps. Matching the hash establishes consistency with our inspected file, not authenticity against a retail reference. Configuration/logs/manifests stay under `work/bt2-generation/`; C++ stays under ignored `game/generated/bt2/`, with automatic instruction patches disabled and no manual stubs or skips. Prior generated output is archived locally before regenerating, preventing stale source files from joining the new set.

These checks cannot certify map correctness. Generated output can include upstream heuristic discoveries and fallback entrypoints; retain warnings as diagnostics. The emitted code uses PS2Recomp runtime APIs. The probe adapter supports one routine; broader integration remains unfinished.

## Generate, build and execute

```powershell
python tools/ps2recomp/run_pipeline.py
python tools/ps2recomp/run_pipeline.py --reuse-generated
```

The first command prepares the map, generates code, hashes all source/header output, prepares runtime input, builds the host and `bt2-generated-probe` with MSVC/C++20, runs tests, then builds and executes `bt2-entry-probe`. Reuse verifies executable/map/tool-lock/recompiler/output hashes and file-set identity before rebuilding. Stages record logs under ignored `work/bt2-generation/`.

The probe registers and executes the actual generated `FUN_0032EEE0` from the broad generation set. It verifies selector/operation, syscall resume PC, return target and preserved register bits. Unsupported syscalls, cache modes, guest targets and returns throw errors. It records the cache request; it does not model caches, DMA, MMIO, graphics, or boot the game. No upstream runtime is linked.

Entry preparation copies ELF loads into a bounded main-RAM image and creates an ignored compiler-facing entry source under `game/generated/bt2-runtime/`, routing emitted direct tail calls through runtime dispatch. The original generated set is unchanged. `runtime-preparation.json` records source/image hashes and hook count. Out-of-range guest memory, unaligned word/quadword accesses, unsupported dispatch and unsupported syscalls throw errors; backedges have an execution budget.

The entry probe poisons its observed uninitialized zero-store interval, executes generated CPU initialization/zeroing, and verifies memory coverage and boundary bytes. It currently expects to stop at SetupThread (`0x3C`) and writes `entry-stop.json` with selector, PC, arguments and store counts. This verifies actual entry execution, not successful boot or a syscall implementation. FPU support is deliberately restricted to entry's zero operands.

`bt2-entry-probe` is excluded from the default build and built explicitly by the pipeline. Its build result and execution stop are reported separately in `pipeline-result.json`, `integration-build.log` and `entry-test.log`; a successful pipeline result means the checked probes passed. The normal host build remains independent.

## Installed Ghidra analysis

Install Ghidra in a location you choose and launch it with its own launcher. The tested version is Ghidra 12.1.4 with a compatible JDK (the development setup used JDK 25). Versions and archive hashes are recorded in `config/ghidra.lock.json`. No Ghidra installation is included in this repository.

The R5900 extension v2.1.37 was built from pinned source against this Ghidra version. Gradle 9.8.0 is installed beside Ghidra. These tools remain outside the repository.

```powershell
python tools/ghidra/analyze_game.py --ghidra-home "PATH_TO_GHIDRA_DIRECTORY" --java-home "PATH_TO_JDK_DIRECTORY"
```

The driver imports the recorded local candidate using `r5900:LE:32:default` and runs our authored exporter. Override installations with `--ghidra-home PATH --java-home PATH`. Projects, logs, JSON, instruction listings, and candidate CSV stay in ignored `work/ghidra/`. An existing project is reopened without repeating analysis. Remove or archive it explicitly when a fresh analysis is needed.

Exports use neutral address names and remain provisional. Ghidra reports overlay and decompiler limitations. Use `prepare_map.py` for structural filtering and provenance before experimental generation; do not commit either the raw or prepared map.

## Entry control audit

```powershell
python -m unittest discover -s tools/ghidra -p "test_*.py"
python tools/ghidra/review_entry.py
```

The audit compares raw ELF control encodings with the entry export, checks instruction coverage and delay-slot inclusion, and writes ignored `work/ghidra/review/entry-control.json`. It supports only the control encodings needed by the current entry; unsupported branch families stop the audit. It verifies neither full instruction semantics nor callees, and never automatically marks a map reviewed. Static review findings are recorded in `REVERSE_ENGINEERING.md`.

After rerunning `analyze_game.py` to export the direct entry targets, run:

```powershell
python tools/ghidra/review_entry_targets.py
```

This checks the five target bodies and writes ignored `work/ghidra/review/entry-targets-control.json`. Integer branch families, including branch-likely target encodings, are supported; coprocessor branches remain unsupported. Delay-slot inclusion is checked, but slot execution/annulment is not simulated. Direct targets outside file-backed executable sections are reported for investigation, never stubbed automatically.

## Startup evidence audit

```powershell
python tools/ghidra/audit_startup.py
```

For the recorded executable hash, this independently checks three raw selector/syscall pairs and the entry's zero-loop bound constants. It reports whether `0x003B7FA8` lies in a declared load extent or the apparent zero-loop interval, plus aligned exact literal and encoded JAL matches. Output stays in ignored `work/ghidra/review/startup-evidence.json`. The scan does not resolve split constants, later writers, or executable runtime contents.

`config/startup_abi.sources.json` records the pinned PS2SDK reference revision and file hashes used for the ABI interpretation in `REVERSE_ENGINEERING.md`. These are research references, not build dependencies.

### Reached target diagnostics

`python tools/ps2recomp/inspect_blocker.py` verifies generated provenance and classifies the latest registered handler or branch target against the provisional map and Ghidra candidates. The pipeline runs it automatically after a verified stop. Read ignored `work/bt2-generation/blocker-map.json` before adding discovery targets; range membership does not certify a function boundary.

`python tools/ghidra/analyze_game.py --discover-blocker` performs focused provisional discovery at the latest observed map-gap address and re-exports candidates. It rejects an address inside an existing function and checks that the target appears in the fresh export. Then run `python tools/ps2recomp/run_pipeline.py` to regenerate from the updated automated map. Discovery is not correctness certification.

### Local kernel capture diagnostic

Capture with the installed PCSX2 and your own BIOS/game:

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath
```

The tool copies PCSX2 and the selected BIOS into ignored `work/pcsx2-capture/`, with a separate settings profile. It instruments only a copy of the validated local ELF: 24 bytes at `0x00338388` write marker `0xB702` at `0x00387E80` and spin before the handler's first read. Read-only Windows process access captures 32 MiB of physical EE RAM from the exported `EEmem` pointer in the process it launched. It checks the marker, trap bytes, isolated data folder and stability of the kernel prefix across two reads, then terminates only that process. Hashes, instrumentation and timing are recorded in `capture.json`. Original game/BIOS files and the user's PCSX2 profile are preserved. This is an instrumented standalone ELF diagnostic; the IOP remains active during capture.

After building the pipeline, run:

```powershell
python tools/ps2recomp/probe_kernel.py work/pcsx2-capture/ee-ram.bin --capture-note "PCSX2 2.9.81; instrumented ELF handler entry, stable kernel prefix"
```

This extracts a read-only kernel window and invokes the generated scan handler in isolation. Hashes and notes are saved under ignored `work/bt2-generation/`. The result does not establish a full boot, exact BIOS exception behavior or writable kernel-patch compatibility. No capture is downloaded or bundled.

### Retail disc revision confirmation

```powershell
python tools/verify_game/verify_disc.py rom/BT2-USA.iso
```

This streams the local ISO against the official Redump size/MD5/SHA-1 recorded in `config/retail_revision.json`, reads bounded ISO9660 root records, and compares the embedded executable's size/SHA-256 with the local ELF. Reports stay under ignored `work/disc-validation/`. Only reference checksum metadata is stored in Git.

### Snapshot-driven entry pipeline

```powershell
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin
```

The snapshot requires its `.json` sidecar from `probe_kernel.py`; size, SHA-256 and capture note are checked before the run. The entry probe verifies both successful scans, their converged table base, restored stack and the next dispatch boundary. CTest retains the no-snapshot boundary. Supplying a snapshot is explicit; there is no automatic kernel-memory loading.

### Local Timer 3 boundary capture and read replay

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point timer3-mode
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json
```

This capture uses a 40-byte authored trap in an ELF copy at `0x00338A1C`: read `0x10001810`, publish its value at `0x00387E84`, publish marker `0xB703`, then spin. It uses a separate ignored capture folder/profile. The pipeline verifies ELF source, capture point, RAM size/hash, trap bytes, marker and payload before extracting the word for explicit replay. The native probe currently accepts the observed `0x83` and permits one mode read; timer writes, repeated reads, counter progression and interrupts are not implemented. Omitting `--timer3-capture` preserves the Timer 3 MMIO stop.

The snapshot + Timer 3 pipeline now enables aligned word writes only to the observed kernel patch range `[0x80076000, 0x80076740)`. Values live in an overlay used by KSEG0 word reads; the capture file and in-memory baseline are preserved. The entry probe compares all 464 kernel words and ten main-RAM words with their generated-code sources before reporting the next cache syscall blocker. Kernel-only mode keeps the snapshot read-only. This does not enable general writable aliases or guest kernel execution.

The startup cache adapter accepts only observed FlushCache operations `0` and `2`; pinned PS2SDK ABI evidence is recorded in `config/startup_abi.sources.json`. Operation `2` records instruction-cache invalidation; fixed native function dispatch has no guest instruction cache. It does not execute patched kernel bytes. The captured entry stop now reports `access: "handler registration"`, syscall selector `0x74`, handler destination `0x80076000` and arguments selecting syscall `0x5B`; the address is rejected before registration. The stop remains under unsupported guest memory because the supported registration-address range check produces that exception.

Copied-kernel registration now accepts only aligned entry words written in the explicitly enabled overlay. Current entry diagnostics report syscall `0x5B`, `access: "callback dispatch"`, handler `0x80076000`, PC `0x003389F8`. Dispatch stops before kernel execution because no native function is compiled for the target. Blocker classification sets `requires_kernel_payload_mapping`; `analyze_game.py --discover-blocker` rejects this target before launching Ghidra against the original ELF. A separate relocation-aware payload path is required rather than inserting a kernel address into the original `.text` map.

### Copied kernel payload generation

With `--timer3-capture`, `run_pipeline.py` now runs `generate_kernel.py` and sets `BT2_BUILD_KERNEL_PAYLOAD=ON`; without that capture it explicitly sets the option off. The tool hash-checks the main generation/retail ELF/tool lock, extracts the observed copy source through PT_LOAD bounds, packages unchanged bytes at the observed runtime address, and runs a provisional automatic discovery pass. It regenerates only the observed entry using the discovered range and a neutral map. Remaining bytes are preserved as data for scoped translation, not certified as data-only in the retail payload. Scoped generation errors stop the pipeline.

Maps, packaged ELF, exploratory/scoped logs and hash manifests stay under ignored `work/kernel-generation/`; raw/prepared source stays under ignored `game/generated/bt2-kernel*`. Separate prepared header names avoid collisions with the main ELF's generated headers. `pipeline-result.json` records kernel provenance. Compiled kernel binding checks the complete copied payload before each dispatch; missing/tampered words fail, and other kernel entries remain uncompiled.

Current checked stop: SetSyscall (`0x74`), extended selector `0x12C`, handler `0x80076488`, PC `0x003389A0`. The generated kernel lookup entry and its wrapper have returned; this request is rejected before registration. Synthetic tests cover payload preservation, mapped addresses/code-data separation, unsafe extraction/mapping, missing/tampered copied words, invalid/duplicate kernel bindings and return validation.

The captured pipeline now records one bounded extended `0x12C` Timer 3 registration/enable request using the pinned Play! reference; it does not run that handler or deliver interrupts. Ordinary slots remain separate and other extended selectors/repeated requests are rejected. AddIntcHandler now returns shared-pool ID 2 for cause `11`, handler `0x00339118`, next/arg zero; Timer 3 reserves ID 1. Without Timer 2 capture, the stop remains mode read `0x10001010` at PC `0x00338C58`. The optional Timer 2 continuation is described below.

The optional generated-probe build includes `generated-buffer-fill`, bringing the suite to five CTests. It checks real generated `FUN_003251DC` with nonzero byte data, vector-aligned, unaligned, short and zero-size inputs, return value and complete outside-memory guards. The adapter supports signed low-word reads and the reached vector operations. Entry zeroing is verified at the first SetupThread call, with saved zero-store counts; later buffer/list initialization is checked separately. This avoids requiring cleared memory to stay zero after subsequent guest writes.

### Timer 2 read capture and continuation

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point timer2-mode
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json
```

The isolated 40-byte authored trap at `0x00338C58` reads `0x10001010`, publishes payload/marker `0xB704`, then spins. Observed mode: `0x80`. Capture remains under ignored `work/pcsx2-capture/timer2-mode/`. Pipeline validates exact expected trap instructions, source hash, RAM hash/size, marker and payload for both timers. `--timer2-capture` requires Timer 3 capture and a kernel snapshot. The native probe accepts the validated startup word once; wrong RAM pointers, duplicate reads and writes fail. Omitting it retains the Timer 2 read stop. With it, generated code completes one observed write of `0x382` to `0xB0001010` and reaches EnableIntc, as described below. This is captured startup replay, not a running timer implementation.

The Timer 2 continuation now accepts one observed `0x382` mode store to `0xB0001010` after capture consumption, records the register bit update and rejects other values/repetitions. It advances to EnableIntc (`0x14`), cause 11, PC `0x0032E9A8`. `generated-timer-write` executes the actual generated helper with EIE clear/set, checks ErrorEPC return, Status restoration, LL-bit clearing, preservation of status flags and unchanged main RAM. Counter synchronization, gating, rate/event processing and interrupt delivery remain unimplemented.

### INTC mask checkpoint and EnableIntc continuation

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point intc-mask
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json --intc-capture work/pcsx2-capture/intc-mask/capture.json
```

The isolated authored trap at `0x0032E9A4`, immediately before EnableIntc, reads `0x1000F010` and publishes marker `0xB705` plus mask `0x1002`. Output stays ignored under `work/pcsx2-capture/intc-mask/`. Exact trap, source/RAM hashes, size, marker and payload are validated through the shared word-capture validator. `--intc-capture` requires Timer 2 capture; this checkpoint applies to the reached syscall, not initial BIOS state. EnableIntc supports only cause 11, returns changed/already-enabled `1`/`0` and preserves other mask bits. It records `0x1802` and does not process pending interrupts. Omitting it retains the EnableIntc stop. With it, startup reaches the second Timer 2 read at `0x00338DD8`; repeated reads remain unsupported pending validation.

### Validated Timer 2 readback

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point timer2-readback
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json --intc-capture work/pcsx2-capture/intc-mask/capture.json --timer2-readback-capture work/pcsx2-capture/timer2-readback/capture.json
```

The isolated trap at second read PC `0x00338DD8` reads `0x10001010` and publishes marker `0xB706` plus observed mode `0x382`. Exact trap/source/RAM/payload provenance stays checked; outputs are ignored. Readback requires all earlier explicit inputs. The runtime compares its recorded post-write mode to the validated expected word, then returns its recorded state once. Missing writes, mismatch, wrong RAM pointers and further reads fail. Omitting this validation preserves the second-read stop. With it, generated `FUN_00338DB8` returns and startup creates semaphore ID 2 before stopping at CreateThread (`0x20`, PC `0x0032EA68`). This does not validate counter timing, future status flags or interrupt delivery.

The full checkpoint pipeline now creates dormant thread ID 2 and reaches ChangeThreadPriority (`0x29`, main ID 1 / priority 1, PC `0x0032EAF8`). Creation validates the 36-byte SDK descriptor and consumed fields, records bounded stack/GP/priority/heap metadata and writes only the four low context words established by pinned Play! ThreadReset. IDs 1/256 are reserved for the authored main/idle model; IDs are not validated against retail BIOS allocation. No scheduler or native worker is launched. Bootstrap tests verify invalid descriptors, preserved RAM/upper registers, ID allocation/capacity, exact context writes and current-thread query. The later generated helper returns -1 through its IE-clear guard; other helper paths have not run.


The full checkpoint pipeline now passes the observed main ID 1 / priority 1 ChangeThreadPriority request and stops at GetOsdConfigParam (`0x4B`, PC `0x0032ED18`). Bootstrap tests cover previous-priority return, repeated requests, rejected arguments and state preservation. Only the main thread is runnable; no scheduler is implemented. OSD configuration remains unsupported pending evidence.


### First OSD configuration checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point osd-config
```

Add `--osd-capture work/pcsx2-capture/osd-config/capture.json` to the full pipeline command above. It requires Timer 2 readback and all earlier explicit inputs. The trap reads the first returned OSD stack word at `0x003385F4`; output stays ignored. Verified provenance supplies one native read; startup then stops at SetOsdConfigParam (`0x4A`). Omitting it retains the earlier OSD read stop. No host preferences or broader BIOS OSD behavior are implemented.


### OSD same-value setter/readback checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point osd-readback
```

Add `--osd-readback-capture work/pcsx2-capture/osd-readback/capture.json` alongside `--osd-capture` in the full pipeline. This trap at `0x00338620` publishes both original and returned configuration words. Validation requires their equality and the first setter expression to equal the original. The adapter accepts the first same-value setter, second getter and same-value restore, then reaches the next registered handler. Omitting this checkpoint retains the first setter stop. Configuration mutation remains unsupported.


The full OSD readback pipeline now accepts the ordered same-value restore, completes the generated OSD helper and enters `FUN_0033A820`. It stops at selector `0x5A` with registered handler `0x0033A7C8`, currently a map gap. Stop reports preserve the exception handler field for correct classification. No handler implementation or kernel payload copy is supplied. Earlier commands without readback retain their original stop.


The reached `FUN_0033A7C8` map gap has now been discovered with the existing focused Ghidra command, regenerated and linked. The full pipeline dispatches it and stops at its first store to `0x80075000`; writable support for that payload is pending. `generated-copy-handler` is included in the pipeline tests and verifies aligned copies/zero size/strict return in isolated RAM. Generated code and discovery artifacts remain ignored.


The full OSD-readback pipeline now enables a second fixed expected-word overlay at `0x80075000` for the observed `0x330`-byte payload. Generated copy execution fills it; SetSyscall accepts its entry only after complete matching stores. Startup stops before executing that uncompiled relocated entry through selector `0x5B`, argument `0x55`. Earlier inputs preserve their existing boundaries. The original kernel baseline and first compiled binding remain checked.


### Startup kernel lookup generation

`python tools/ps2recomp/generate_kernel.py --payload startup` scopes the reached relocated entry using direct-branch leaf closure; full payload bytes remain unchanged. The full OSD-readback pipeline runs it automatically, uses `BT2_BUILD_STARTUP_KERNEL_PAYLOAD=ON`, binds with complete payload verification, and executes the copied table lookup; exact returned selector/address registrations complete after payload validation. Separate startup output/header names keep the earlier kernel payload intact. Registration diagnostics now classify their handler addresses, and Ghidra discovery rejects kernel addresses requiring relocated payload analysis.


### Explicit DMA startup checkpoints

Capture each point from an independent local ELF boot:

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point dma-status
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point dma5-control
```

Add `--dma-status-capture work/pcsx2-capture/dma-status/capture.json` and `--dma5-control-capture work/pcsx2-capture/dma5-control/capture.json` to the full pipeline with all earlier OSD-readback/checkpoint inputs. The first requires OSD readback; the second requires DMA status. Full 32-bit payload/provenance validation supplies only the observed one-time reads. The bounded adapter records channel 5 handler/enable requests; it has no DMA transfers or interrupt delivery. Without the optional SIF capture, continuation stops at SifGetReg selector 0x80000000. Omitting channel input stops at CHCR; omitting both stops at D_STAT. Detailed earlier results are historical; see [current status](PROG.md).

## First SIF query checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point sif-command
```

Add `--sif-command-capture work/pcsx2-capture/sif-command/capture.json` to the full pipeline with all earlier captures, including channel 5. The trap at `0x00331A34` publishes returned `v0` and selector `a0`; strict source/RAM hashes, trap, marker and selector validation precede replay. Observed selector `0x80000000` returns `0x00019600` once. Prepared `FUN_00331F38` replaces nine omitted CACHE 0x18 sites with aligned, RAM-bounded line checks; raw generated source remains unchanged. Coherent host RAM has no guest cache model. The executed path checks one line and reaches SifSetDma `0x77`; no DMA/IOP transfer is performed.

## First SIF command receive and RPC state checkpoints

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point sif-dma-before
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point sif-dma-after
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point sif-rpc-state
```

Append `--sif-dma-before-capture work/pcsx2-capture/sif-dma-before/capture.json --sif-dma-after-capture work/pcsx2-capture/sif-dma-after/capture.json` to the full pipeline including `--sif-command-capture`. Both are required together. The tool also captures the first 2 MiB of IOP RAM through PCSX2's IOPmem export; source/RAM hashes, traps, descriptors, packets, table fields and observed receive effects/return are verified. Packet/table windows are checked twice; unrelated kernel and IOP memory continue running. The ignored `sif-dma.bin` bundle contains the before IOP state and observed return, enabling only a synchronous logical CHANGE_SADDR receive model.

Append `--sif-rpc-state-capture work/pcsx2-capture/sif-rpc-state/capture.json` to continue through the reached RPCINIT query. It requires the paired receive captures and matching PCSX2/BIOS provenance. Observed selector `0x80000002` returns 1 once. Omit this input to retain that query stop; omit the paired inputs to retain the first SifSetDma stop.

Checked coherent main-RAM byte/word aliases `[0x20000000,0x22000000)` support the reached packet allocator; other access widths and address regions remain unsupported. Full continuation stops at the second submission: RPC bind command `0x80000009`, server ID `0x80000592`, packet source `0x203A97C0`, size 64. No general transfer scheduler, IRQ delivery or RPC service response is implemented. All EE/IOP/capture/generated output stays ignored.

## First RPC bind response and isolated callback

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point sif-bind-after
cmake --build build/probe --config Debug --target bt2-rpc-bind-probe
python tools/capture_ee/verify_rpc_bind.py work/pcsx2-capture/sif-bind-after/capture.json --run-probe
```

Use the existing generated-probe CMake configuration. The verifier checks the first CD initialization bind's request, registered IOP service/queue, reply, completed client and packet release. It writes the checked image and provenance to ignored `work/bt2-generation/`. The probe reconstructs only the explicit pending callback fixture and runs three original generated functions with strict returns. Negative interrupt-only SignalSema is scoped to this probe callback. This command validates the callback independently. The full pipeline below now connects bounded logical delivery with caller-context preservation; hardware interrupt timing, CD initialization execution and disc access remain incomplete.

## Startup RPC bind delivery and first service-call checkpoint

Capture the call semaphore boundary independently, after the first bind returns:

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 $Pcsx2Path --bios $BiosPath --capture-point rpc-call-sema
```

Append `--sif-bind-capture work/pcsx2-capture/sif-bind-after/capture.json --rpc-call-sema-capture work/pcsx2-capture/rpc-call-sema/capture.json` to the full pipeline including all earlier RPC-state and paired DMA inputs. Hash/trap/publication, descriptor and reference-environment validation precede preparation of the ignored bind bundle. The bounded model accepts only the first captured bind, delivers the checked reply through the generated handler on a copied CPU context, restores its 64-byte scratch window, then lets generated code wait/delete its temporary semaphore and construct the first CD initialization call. Hardware DMA/interrupt timing and service execution are not implemented.

Omit the bind input to retain the previous second-SifSetDma boundary. Omit only the call-semaphore input to retain the explicit CreateSema boundary in the RPC call. Public PS2SDK leaves that unused attribute field uninitialized; the adapter permits it only for this exact call descriptor with a checked checkpoint, and does not overwrite guest stack contents to match the capture. Full continuation reaches the third submission, with two descriptors and RPC function number 0.

For standalone bundle preparation, `verify_rpc_bind.py` accepts `--call-sema-capture work/pcsx2-capture/rpc-call-sema/capture.json` in addition to `--run-probe`. The isolated callback probe remains useful independently; the entry pipeline now also supports bounded logical delivery.

### Reached startup expansion and CD-init evidence

`python tools/ps2recomp/expand_startup.py --report work/bt2-generation/entry-stop.json` selects a reached exact map entry, verifies generated provenance and writes ignored `extra-startup.cmake`, `bt2_extra_startup.h` and a hash receipt. Reconfigure/rebuild after selection. It rejects interior, duplicate, kernel or unmapped targets. Runtime behavior still requires evidence.

The pipeline accepts `--cd-init-capture work/pcsx2-capture/cd-init-after/capture.json` after bind/call-semaphore inputs, and optionally `--cd-semaphores-capture work/pcsx2-capture/cd-semaphores/capture.json`. Capture points of those names are available in capture_ee.py. Validators check request/reply/descriptor effects, instrumentation and matching reference environments. Current full continuation uses three ignored selections (0x00320488, 0x0031F6A8, 0x0032ECB0); omit the semaphore checkpoint to retain the earlier setup boundary. No disc data is supplied by these captures.

The next checked continuation adds `--scmd-bind-capture work/pcsx2-capture/scmd-bind-after/capture.json`, then optionally `--scmd-mmode-capture work/pcsx2-capture/scmd-mmode-after/capture.json`. It requires CD-init/semaphore evidence and matching reference environments. Omit media-mode evidence to retain its CreateSema boundary. Bindings for nine subsequent reached entries remain in the ignored selection receipt. The full path stops at SifStopDma; no IOP reset or disc load is claimed.

Reset preparation adds `--sif-stop-capture`, then `--sif-reset-subaddr-capture`, then the paired `--reset-smflag-before-capture` / `--reset-smflag-after-capture` manifests from equally named capture folders. Each requires the previous evidence and matching reference environment. Stop/query and SMFLAG validators are in `tools/capture_ee/verify_sif_reset.py`. Omit each new input to preserve the earlier checked boundary. Full continuation stops before the disc-backed reboot submission; it supplies no IOPRP image or reset readiness.

## Disc-backed reboot submission and local IOP inventory

```powershell
python tools/iop/inspect_image.py --disc rom/BT2-USA.iso
```

The tool compares the existing local `rom/IRX/IOPRP300.IMG` with its exact ISO9660 path and writes bounded ROMDIR/ELF-header inventory to ignored `work/iop/image.json`. It does not extract, load or execute modules.

Use `capture_ee.py --disc rom/BT2-USA.iso` with existing PCSX2/BIOS arguments for each capture point: `reset-submit-after`, `reset-cmd-clear`, `reset-flags-after`, `reset-rpc-clear`, `reset-subaddr-clear`. Each launches an independently instrumented local ELF with that local disc mounted. Publications record the submission return and subsequent register clears; they are EE evidence, not post-reset IOP snapshots.

Append `--reset-submission-captures work/pcsx2-capture` to the full pipeline after all reset-preparation inputs. It verifies image identity, capture hashes/traps/publications, packet/path, matching disc/PCSX2/BIOS and retains provenance in the ignored pipeline receipt. Omitting this new input preserves the pre-submission boundary. Earlier standalone checkpoints and these disc-backed observations remain separate experiments. The bounded runtime acknowledges the exact request, invalidates old service state and stops at the first boot-readiness poll; it does not execute IOP code or fabricate readiness.

### Post-reset ready-branch diagnostic

Capture `reset-ready` with the explicit local `--disc`, PCSX2 and BIOS arguments above. The eight-word trap preserves the not-ready return path and publishes the 0x40000 mask only after the original conditional selects the ready branch. It does not publish the entire SMFLAG value. EE publication is checked twice; the fresh two-MiB IOP RAM dump is live diagnostic data, not atomically frozen or fully stable.

Append `--reset-ready-capture work/pcsx2-capture/reset-ready/capture.json` after the submission-chain inputs. The verifier checks the original ready-test prefix digest, trap/publication, both RAM hashes, sizes and matching disc/PCSX2/BIOS. The native probe replays only the tested bit for this exact one-shot caller; it keeps IOP boot pending and old services invalid. Omit the ready input to retain the prior SifGetReg boundary.

### GS startup checkpoints

Use capture points `gs-csr-reset`, `gs-imr-after`, `gs-crt-after` with the explicit local disc/PCSX2/BIOS arguments. Publications retain a 64-bit value at marker+8 and SP at marker+16. Pipeline flags `--gs-csr-capture`, `--gs-imr-capture`, `--gs-crt-capture` require the preceding checkpoint. The verifier checks preceding instruction digests, trap/publication, RAM hash and matching disc/PCSX2/BIOS. Omit each input to preserve its unsupported boundary.

Diagnostic points `gs-intc-after` and `gs-csr-field` capture the completed interrupt wait and subsequent field read. `verify_gs.verify(..., ready_manifest, point)` validates them; the completed-helper sample remains diagnostic; the field sample can now be supplied explicitly after the interrupt evidence bundle. Do not interpret independent snapshots as a continuous event trace.

Selected extra entries now compile from ignored prepared sources. `prepare_runtime.py` routes emitted direct tail calls uniformly through checked dispatch, records raw/prepared hashes, and retains unchanged file timestamps. Original generated sources remain untouched.

### Interrupt ordering checkpoints

Capture points `gs-intc-before`, `gs-intc-cleared`, `gs-intc-event` and `gs-intc-acknowledged` require the explicit local disc/PCSX2/BIOS arguments. Each publishes four words at marker+8: status, mask, SP and preceding v0. The event trap leaves the original polling loop intact. These independent halted runs do not establish elapsed cycles.

Append `--gs-interrupt-captures work/pcsx2-capture` after the CRT checkpoint. verify_gs.verify_interrupts checks all four full RAM hashes, authored trap, preceding instruction digest, publications and matching reference environment. This appends 64 bytes to the ignored startup bundle (316 bytes total), enabling a deliberately bounded diagnostic wait: acknowledge, unsatisfied poll, one event checkpoint, poll success, acknowledge. No timed video or callback scheduling is claimed.

Append `--gs-csr-field-capture work/pcsx2-capture/gs-csr-field/capture.json` to enable the one-shot later CSR field sample (324-byte bundle). Without it the probe stops at that read; without the interrupt bundle it retains the first acknowledgement boundary. Full continuation currently stops at the GIF DMA control read, before any transfer is accepted. Pipeline receipts retain all new provenance.

### First GIF request evidence

Capture points `gif-idle`, `gif-qwc`, `gif-after` require the explicit local disc/PCSX2/BIOS arguments. The idle trap publishes the read result, SP, CHCR, QWC, MADR, DMAC_CTRL, DMAC_STAT and DMAC_ENABLER, and verifies that publication and the first source packet twice. The later traps publish QWC-derived value and helper return respectively. Instrumentation halts before later paths; the earlier busy-timeout target is preserved. These independent runs do not establish elapsed transfer timing or completion.

Append `--gif-captures work/pcsx2-capture` after the CSR field checkpoint. verify_gif validates all identities/hashes/traps/prefixes/publications, matching environment, and the entire source packet across the three reference captures. Its bounded inspector admits only a complete single PACKED A+D tag without PRE and writes packet metadata into the ignored pipeline receipt. The ignored startup bundle grows from 324 to 616 bytes, including the local 240-byte packet. No packet bytes are committed.

The runtime stages only the reached idle/count/address operations, validates source bounds and semantics, then stops before the first DMA start. Only unused bits in PRMODECONT/COLCLAMP/DTHE are ignored during comparison. A meaningful packet mismatch fails the probe and writes `.gif-expected.bin` /`.gif-actual.bin` alongside the caller's diagnostic report (the pipeline uses ignored work/). Those files contain proprietary-derived data and must remain local. Omitting GIF evidence retains the first channel read boundary. A reference helper return does not constitute native DMA completion or a rendered frame.

### Scoped GS consumption and later synchronization

`tools/ps2recomp/run_pipeline.py --gif-sync-captures work/pcsx2-capture` requires `--gif-captures` and the preceding GS/checkpoint chain. Five ignored disc-backed points (`gif-sync-before`, `gif-sync-poll`, `gif-sync-after`, `gif-vu-idle`, `gif-path-after`) validate the first wait, idle status and second source packet; their extension produces the 988-byte SCMD bundle. Use the capture tool with the user's existing local PCSX2/BIOS/disc paths. Do not commit captures, packet bytes or patched executables.

The authored GS CTest validates page addressing, color/depth masks, clipping, unknown bits, transactional rejection, source immutability and ordered two-request consumption/completion. Completion is a bounded logical diagnostic; no video clock or displayed game frame is asserted. Generated VPU_STAT accesses are routed through a checked runtime hook with preparation hashes, preserving raw output.

## Fresh SIF transport checkpoint

Capture `post-sif-before` and `post-sif-after` with the user's existing local PCSX2, BIOS and disc paths. These unique caller traps preserve both earlier startup and the reboot path; all patched files and EE/IOP captures remain ignored. Selected ownership windows are stable twice, while full IOP RAM remains a live snapshot.

Append `--post-sif-captures work/pcsx2-capture` to the pipeline after `--gif-sync-captures` and its prerequisites. `verify_post_sif.py` validates identity, environment, full RAM hashes, trap/prefix/publication, EE/IOP ownership, opt-1 packet and SET_SREG effects. The 136-byte extension produces a 1124-byte bundle. The scoped runtime now reaches the fresh service bind described in [the service lifetime plan](POST_RESET_IOP_SERVICES_PLAN.md); it does not execute the IOP or reuse old service addresses.

## Post-reset CD and SCMD services

Capture `post-cd-bind-after`, `post-cd-bind-success`, `post-cd-after`, `post-scmd-bind-success` and `post-mmode-after` with the existing local PCSX2/BIOS/disc paths. Successful-bind traps use an authored conditional stub in a separate load record at 0x01000000: earlier helpers resume their displaced branch and conditional delay slot; later helpers halt only at the selected ownership/success condition. Captures, appended segments and patched executables stay ignored. ReadProcessMemory remains the only process-memory operation.

Append `--post-cd-captures work/pcsx2-capture` after `--post-sif-captures` and its prerequisites. `verify_post_cd.py` validates identities, trap/stub/prefix, RAM hashes, reference environment, fresh service/client/queue ownership and returned values. A 208-byte CD extension yields a 1332-byte diagnostic prefix; adding 132 SCMD bytes yields the full 1464-byte bundle. Epoch-owned callbacks deliver the verified replies; no native IOP boot, module execution or general disc access is implemented.

## LOADFILE and local IRX inspection

`tools/capture_ee/capture_ee.py` provides disc-backed `loadfile-bind-after`, `loadfile-version-after`, `loadfile-module-before` and `loadfile-module-after` checkpoints. Short traps preserve the earlier execution and do not overlap the module caller. `tools/capture_ee/verify_loadfile.py` validates the bind/version reply bundle and the first module path. Add `--loadfile-captures work/pcsx2-capture` to the checkpoint pipeline after `--post-cd-captures`.

Inspect a provided module without executing it:

```powershell
python tools/iop/inspect_irx.py rom/IRX/SIO2MAN.IRX --disc rom/BT2-USA.iso --output work/iop/sio2man.json
```

Optional `--base 0x4BF30 --iop-capture work/pcsx2-capture/loadfile-module-after/capture.json` prepares a private relocated image and compares module ownership, relocation words and code ranges against that recorded scenario. The explicit base is reference evidence for this capture, not a native allocator address. Output/maps/images must stay in ignored `work/` or `game_data/`. No imports or module entries are executed.

## Isolated native IOP diagnostic

With your local disc/module already provided:

```powershell
python tools/iop/generate_native.py rom/IRX/SIO2MAN.IRX --disc rom/BT2-USA.iso --output work/iop/sio2man_native.cpp
python tools/iop/generate_native.py rom/IRX/MCMAN.IRX --disc rom/BT2-USA.iso --base 0x20000 --symbol bt2_iop_mcman --prefix iop_mcman --output work/iop/mcman_native.cpp
python tools/iop/generate_native.py rom/IRX/MCSERV.IRX --disc rom/BT2-USA.iso --base 0x50000 --symbol bt2_iop_mcserv --prefix iop_mcserv --output work/iop/mcserv_native.cpp
python tools/iop/generate_native.py rom/IRX/DBCMAN.IRX --disc rom/BT2-USA.iso --base 0x60000 --symbol bt2_iop_dbcman --prefix iop_dbcman --output work/iop/dbcman_native.cpp
python tools/iop/generate_native.py rom/IRX/SIO2D.IRX --disc rom/BT2-USA.iso --base 0x70000 --symbol bt2_iop_sio2d --prefix iop_sio2d --output work/iop/sio2d_native.cpp
cmake -S . -B build/probe -DBT2_BUILD_IOP_PROBE=ON
cmake --build build/probe --config Debug --target bt2-iop-tests bt2-iop-probe
build/probe/Debug/bt2-iop-tests.exe
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/native-stop.json
```

The authored standalone MIPS-I translator emits native instruction expressions and checked import dispatch. It is separate from the pinned EE PS2Recomp pipeline. Outputs, relocation images and provenance remain under ignored work/iop/. CMake also generates authored synthetic programs locally. No game code is needed in the repository.

The module probe validates its image against compiled generation, owns separate RAM/stack/arguments and reports a diagnostic stop. Exit zero means a report was produced, **not module-load or game success**; inspect its PC/import/memory fields. Bad input/report paths fail nonzero. Current expected report: entry returned with result 0 after 151 instructions, six entry targets entered and one native worker waiting for event bits 0x4155. Reports and companion private RAM output must be directly under work/iop; run from the project root. The isolated probe and optional EE adapter now share NativeIopService. Enable BT2_LINK_NATIVE_IOP with the existing generated-entry probe configuration to execute the compiled modules through the persistent EE loader. The ordinary host remains independent of private code.

Validate scoped native initialization against your recorded local checkpoint:

```powershell
python tools/iop/verify_native.py --native work/iop/native-stop.json --reference work/pcsx2-capture/loadfile-module-after/capture.json --module rom/IRX/SIO2MAN.IRX --output work/iop/startup-validation.json
```

The checker validates capture hashes/trap, module ownership/code, normalized BSS, semaphore/event state and native thread parameters/wait. It checks reached reset/DMA/IRQ state against the public behavior consulted. It does not equate opaque options or capture hardware MMIO, and does not complete EE LOADFILE. Runtime handles use separate event/semaphore/thread namespaces. Kernel record offsets are specific to this reference scenario.

Generated IOP functions are partitioned into 4 KiB address buckets to bound MSVC switch compilation. Synthetic tests exercise branch delay slots across bucket boundaries. `--symbol` and `--prefix` give separately linked modules distinct generated names.

## Scoped MCMAN resident comparison

Capture `--capture-point mcman-module-after` with the existing explicit local PCSX2/BIOS/disc arguments. The authored trap is at unique main caller return 0x100434, so the preceding SIO2MAN load completes normally. Result/SP, LOADFILE buffers/service windows are checked stable; full IOP RAM remains a live diagnostic.

```powershell
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/mcman-stop.json --mcman
python tools/iop/verify_mcman.py --native work/iop/mcman-stop.json --reference work/pcsx2-capture/mcman-module-after/capture.json --module rom/IRX/MCMAN.IRX --output work/iop/mcman-validation.json
```

The checker guards capture/source hashes, trap, scenario-specific module record/base/text extent, result 2, relocated code and full initialized data/BSS. Only unequal module-local address values and two owned semaphore handles are normalized; equal scalar words stay unchanged. Semaphore record tags/attributes/count/max are guarded. Kernel offsets are observed for this scenario, not a portable ABI. Opaque options, authentication and card transfer behavior remain unverified. Generated entry-target counts now use physical addresses so different modules cannot collide through identical local offsets.

## Native RPC worker startup comparisons

Capture `mcserv-module-after` / `dbcman-module-after` with the explicit local PCSX2/BIOS/disc arguments. Unique EE main return traps 0x100454 / 0x100474 preserve preceding module loads. Full IOP RAM remains live; stable service/publication windows are checked separately.

```powershell
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/mcserv-stop.json --mcserv
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/dbcman-stop.json --dbcman
python tools/iop/verify_rpc_startup.py --profile mcserv --native work/iop/mcserv-stop.json --reference work/pcsx2-capture/mcserv-module-after/capture.json --module rom/IRX/MCSERV.IRX --output work/iop/mcserv-validation.json
python tools/iop/verify_rpc_startup.py --profile dbcman --native work/iop/dbcman-stop.json --reference work/pcsx2-capture/dbcman-module-after/capture.json --module rom/IRX/DBCMAN.IRX --output work/iop/dbcman-validation.json
```

The checker guards identity/hash/trap, resident result, module ownership/text extent, relocated code, full normalized data/BSS, queue thread identity and reference worker sleep/parameters. Native kernel handles and module-local address values are normalized. Opaque options, physical timing/DMA and real RPC requests remain unvalidated. The four completed module entries do not represent complete device functionality.

## SIO2D startup and DS2U_D native generation

Capture `sio2d-module-after` with the explicit local PCSX2/BIOS/disc arguments. The unique EE caller trap is 0x100494, marker B73E. Compare the scoped startup and generate the next supplied IRX locally:

```powershell
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/sio2d-stop.json --sio2d
python tools/iop/verify_rpc_startup.py --profile sio2d --native work/iop/sio2d-stop.json --reference work/pcsx2-capture/sio2d-module-after/capture.json --module rom/IRX/SIO2D.IRX --output work/iop/sio2d-validation.json
python tools/iop/generate_native.py rom/IRX/DS2U_D.IRX --disc rom/BT2-USA.iso --base 0x80000 --symbol bt2_iop_ds2ud --prefix iop_ds2ud --output work/iop/ds2ud_native.cpp
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/ds2ud-stop.json --ds2ud
```

The SIO2D checker guards identity, trap, result, relocated code, normalized data/BSS, Vblank waiter and semaphore ownership. Offset 0x1BD8 is an explicitly unvalidated live counter, reported separately; non-end Vblank phase bits are not compared. DS2U_D generation has 40 provisional ranges / 25 imports and is linked into the optional native targets. Its resident entry and two initial polls now complete; later controller packets remain unsupported. All generated code, captures, images and reports stay ignored.

## Direct first-controller poll capture

Use the existing explicit local PCSX2/BIOS/disc capture command with `--capture-point sio2d-module-after --capture-first-pad-poll --pad-poll-read-count 10`. This uses a separate isolated interpreter profile under `work/pcsx2-capture/sio2d-module-after-pad-poll-10/`. Count 5 captures only the first port. The transient process hook resumes the restored EE setup, reads real FIFO bytes once, publishes reply/control/status and halts the IOP worker after the requested read count. It does not freeze other workers or validate physical timing. Regular `ds2ud-module-after` captures the unique EE caller return at 0x1004B4 / marker B73F.

```powershell
python tools/iop/verify_ds2ud_startup.py --native work/iop/ds2ud-stop.json --reference work/pcsx2-capture/ds2ud-module-after/capture.json --module rom/IRX/DS2U_D.IRX --poll work/pcsx2-capture/sio2d-module-after-pad-poll-10/pad-poll.json --output work/iop/ds2ud-validation.json
```

The verifier compares the resident return, relocated code, worker ownership/parameters/Vblank-start wait/dormant state and guarded direct first two poll publications. It does not equate live DS2U_D data/BSS across Vblank phases. Regenerate each local native IRX after the backend update: generated module dispatch now returns to the owned service on cross-module jumps, which preserves the actual callback PC/RA/context rather than treating the other module as an unmapped local target.


## Sound-module startup probes

The optional native targets now require these additional locally generated sources before configuration/build:

```powershell
python tools/iop/generate_native.py rom/IRX/LIBSD.IRX --disc rom/BT2-USA.iso --base 0x90000 --symbol bt2_iop_libsd --prefix iop_libsd --output work/iop/libsd_native.cpp
python tools/iop/generate_native.py rom/IRX/SDRDRV.IRX --disc rom/BT2-USA.iso --base 0xA0000 --symbol bt2_iop_sdrdrv --prefix iop_sdrdrv --output work/iop/sdrdrv_native.cpp
python tools/iop/generate_native.py rom/IRX/MODHSYN.IRX --disc rom/BT2-USA.iso --base 0xB0000 --symbol bt2_iop_modhsyn --prefix iop_modhsyn --output work/iop/modhsyn_native.cpp
python tools/iop/generate_native.py rom/IRX/MODSESQ2.IRX --disc rom/BT2-USA.iso --base 0x180000 --symbol bt2_iop_modsesq2 --prefix iop_modsesq2 --output work/iop/modsesq2_native.cpp
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/modsesq2-stop.json --modsesq2
```

Stages `--libsd`, `--sdrdrv`, `--modhsyn`, `--modsesq2` load all earlier modules in observed order. Capture points and `verify_rpc_startup.py --profile` names are respectively `libsd-module-after`/`libsd`, `sdrdrv-module-after`/`sdrdrv`, `modhsyn-module-after`/`modhsyn`, `modsesq2-module-after`/`modsesq2`. Use the explicit local PCSX2/BIOS/disc capture command and the checker's existing native/reference/module/output arguments. Resident validation does not establish playback or successful sound RPC requests. MODHSYN's final two untouched allocator-fill bytes are explicitly guarded/excluded in the comparison.

The direct initial poll publication now includes DPCR1/2, using BF80 for their positive 16-bit offsets. Regenerate that scoped capture after the hook update before running the DS2U_D comparison; its hook/hash and seven published register words must match.


## Disc-streaming and sound-service startup probes

Generate these supplied local sources before building the optional native targets:

```powershell
python tools/iop/generate_native.py rom/IRX/CDVDSTM.IRX --disc rom/BT2-USA.iso --base 0x188000 --symbol bt2_iop_cdvdstm --prefix iop_cdvdstm --output work/iop/cdvdstm_native.cpp
python tools/iop/generate_native.py rom/IRX/SOUNDS.IRX --disc rom/BT2-USA.iso --base 0x150000 --symbol bt2_iop_sounds --prefix iop_sounds --output work/iop/sounds_native.cpp
python tools/iop/generate_native.py rom/IRX/CRI_ADXI.IRX --disc rom/BT2-USA.iso --base 0x168000 --symbol bt2_iop_cri_adxi --prefix iop_cri_adxi --output work/iop/cri_adxi_native.cpp
build/probe/Debug/bt2-iop-probe.exe work/iop/sio2man_native.bin work/iop/cri-adxi-stop.json --cri-adxi
```

Ordered stages `--cdvdstm` and `--sounds` complete residency; `--cri-adxi` completes validated resident startup. Local capture points `cdvdstm-module-after` and `sounds-module-after` have generalized checker profiles `cdvdstm` and `sounds`. Reports now include each thread's owned stack base. The CDVDSTM check normalizes its external event after guarded object-state comparison; SOUNDS checks the stack-owned queue/server and tagged reference worker. These checks do not establish successful sectors, sound RPC requests or playback.


## Bounded LIBSD transfer and CRI residency checks

Use the explicit local PCSX2/BIOS/disc capture arguments with `--capture-point sounds-module-after` and exactly one of `--capture-spu2-transfer`, `--capture-spu2-key-readback on`, `--capture-spu2-key-readback off`, `--capture-spu2-initialized`, or `--capture-cri-low-read`. Each has a separate ignored isolated interpreter profile. Hooks guard original instructions/scratch fill, publish actual operands/results and terminate; they do not resume normal execution after the terminal publication. Transfer destination verification uses a terminal diagnostic read DMA after preserving the original poll, not native game DMA.

Capture `cri-adxi-module-after` separately for the resident return, then run:

```powershell
python tools/capture_ee/verify_spu2_transfer.py --disc rom/BT2-USA.iso
python tools/iop/verify_rpc_startup.py --profile cri-adxi --native work/iop/cri-adxi-stop.json --reference work/pcsx2-capture/cri-adxi-module-after/capture.json --module rom/IRX/CRI_ADXI.IRX --output work/iop/cri-adxi-validation.json
```

The first comparison uses ignored `work/iop/libsd-initialized-native.json` at the post-init/pre-master-volume boundary plus the final native module report. It verifies provenance/hook/publication, eight transferred halfwords, 394 stable configuration values, immediate key readbacks and the three read-only low ABI words. Four dynamic ENDX registers are excluded; playback/general DMA is not verified. The CRI comparison includes all normalized module data/BSS without exclusions and both workers.

After `expand_startup.py` selects a newly reached EE entry, run `python tools/ps2recomp/prepare_runtime.py` before reconfiguring CMake. This preserves raw generated output and prepares the selected runtime source with checked deterministic routing and hash receipts.


## Boot IOP heap handler and transfer

Generate FILEIO from the supplied, disc-matched IOPRP image before configuring optional native targets:

```powershell
python tools/iop/generate_boot_heap.py --disc rom/BT2-USA.iso
```

The boot heap handler is translated at 0x140000 and invoked only for the checked first allocation. Its boot entry/worker is not executed or counted as resident. Generated files and receipts stay under ignored work/iop/.

Use the existing explicit local PCSX2/BIOS/disc command with capture points `fileio-bind-after` (heap service despite its historical name), `heap-alloc-after`, and `heap-copy-after`. Each traps its unique caller after completion. Heap client/service buffer stability uses the correct client 0x3AC840 rather than LOADFILE's client. Validate and prepare the private checkpoint bundles:

```powershell
python tools/capture_ee/verify_heap_bind.py --capture work/pcsx2-capture/fileio-bind-after --disc rom/BT2-USA.iso --output work/bt2-generation/heap-bind.bin
python tools/capture_ee/verify_heap_copy.py --capture work/pcsx2-capture/heap-copy-after --disc rom/BT2-USA.iso --output work/bt2-generation/heap-allocation.bin
```

The copy verifier also verifies the allocation and bind, producing an 88-byte allocation/copy bundle. Allocation alone uses `verify_heap_allocation.py --capture work/pcsx2-capture/heap-alloc-after` and the same disc/output arguments, producing 68 bytes. Append heap-bind.bin and heap-allocation.bin after the existing SCMD bundle in the optional entry-probe invocation. With heap execution, the probe writes an ignored IOP RAM sidecar at `<STOP_REPORT>.iop.bin`; use `--native work/bt2-generation/entry-stop.json` on the copy verifier to compare native phases/client/destination.

The pipeline now accepts `--heap-bind-capture`, `--heap-allocation-capture`, `--heap-copy-capture` and `--heap-disc`, in addition to the existing explicit startup/LOADFILE captures. Supply the three directories above and the user's local disc. It checks prerequisite evidence/environment and passes prepared bundles automatically. No general heap allocator, file operations or physical DMA scheduling is implied.

### First embedded module and cleanup

Use the existing capture command with `--capture-point module-buffer-after`, `heap-free-after` and `dmac-startup-after`. These unique return checkpoints require the user's explicit local disc and produce ignored capture directories. Then run:

```powershell
python tools/iop/generate_embedded.py --disc rom/BT2-USA.iso
python tools/capture_ee/verify_module_buffer.py
```

The generator validates the retail ELF hash/file-backed source segment, full transferred IRX, relocated entry, independent loader return/metadata, local disc hash and disc-matched CDVDMAN version. Its `.cpp`, `.h` and receipts remain in ignored `work/iop/`. Optional native targets require this source alongside the thirteen module sources and boot FILEIO source. Regenerate it before building those targets. No additional entry-probe argument is required: the existing heap evidence enables the bounded owned buffer load and free.

After the full native probe, add `--native work/bt2-generation/entry-stop.json` to the verifier. Native IDs are authored and differ from retail IDs; the embedded module returns 1 and is not retained as a resident. Freed code/heap bytes may remain as diagnostic residue, but native ownership checks reject accesses. The verifier compares code/returns and bounded DMAC setup; it does not establish full boot, VIF execution or presentation.

### VIF configuration and repeated graph setup

Use the existing capture command with the explicit local disc and `--capture-point vif-startup-after`, `graph-csr-after`, or `graph-startup-after`. The first publishes VIF1 configuration, GIF status and VU control at the unique return. The CSR hook selects the second helper frame and executes the displaced instruction on the earlier invocation; it records the actual original CSR LD. The graph return hook independently records return/SP. These hooks use an authored file-backed segment in an unused ELF program-header record. Hashes and raw RAM stay ignored.

```powershell
python tools/capture_ee/verify_vif_startup.py --native work/bt2-generation/entry-stop.json
```

The verifier checks source/environment hashes, hook/stub bytes, marker/SP, source packet bytes and native configuration/counts. Once the second graph phase completes, it also checks both graph captures. It does not use the diagnostic 0x12001010 LD as an IMR readback or infer video timing. No additional probe argument is required. Regenerate prepared local source with `prepare_runtime.py` after selecting entries: VU reset assignments now route to the checked control hook while raw generated output remains unchanged and ignored.

### GIF configuration and EE heap continuation

The existing local disc-backed capture command accepts `texture-idle`, `texture-path-idle`, `texture-poll`, `texture-complete`, `texture-after`, `main-priority-after`, `ee-arena-after` and `ee-pools-after`. The completion hook allows busy loads to continue by executing the displaced ANDI before resuming; publications and selected source windows are checked for stability. Pool pointers are checked independently at the unique return.

```powershell
python tools/capture_ee/verify_texture_startup.py --output work/bt2-generation/texture-startup.bin
python tools/capture_ee/verify_texture_startup.py --native work/bt2-generation/entry-stop.json
```

Append the ignored 224-byte texture bundle after heap-allocation.bin in the optional entry-probe command. The normal pipeline has `--texture-startup-captures` (requires existing heap-copy evidence) to verify and append it. The native probe reports configuration, logical request completion, main-thread priority, EE lock activity and pool pointers. Captured readbacks describe this request, not a native video clock or generic DMA timing model.

Map preparation additionally admits one-word zero gaps after BEQ zero,zero and its non-control delay slot, with no checked direct edge into a gap and no unknown indirect/coprocessor branch. File-backed extent, size, range accounting and overlap quarantine remain required. The map records padding provenance and remains provisional. Regeneration preserved all previously selected raw-function hashes; selection provenance was refreshed only after that equality check.

Use `--capture-point heap-second-after` and `python tools/capture_ee/verify_second_heap.py` to inspect the second IOP request. The verifier checks return/size/client/source identity and the conflicting FILEIO placement. It intentionally supplies no allocation success bundle. See IOP_HEAP_CAPACITY_PLAN.md before changing ownership.


## Checked second heap layout

`config/iop_layout.json` records authored translation placements for the five images moved out of the captured second heap. The native generator uses those defaults when `--base` is omitted; explicit overrides remain available for experiments. Regenerate moved images, then run `python tools/iop/verify_layout.py` before building. The checker verifies image/source hashes, all fourteen translated images, the reserved heap interval and worker/call/argument stack region for overlap and RAM bounds. The embedded nonresident image is released before heap reuse and is not a persistent placement.

`python tools/capture_ee/verify_second_heap.py --native work/bt2-generation/entry-stop.json` compares the real translated handler result and lifecycle. The earlier embedded-module checker compares residual freed code only before heap reuse: after reuse, its bytes are undefined and are not historical completion evidence. Native lifecycle tests still independently verify its earlier execution/release. Arbitrary allocator requests remain rejected.


### Bounded sound delivery verification

`python tools/capture_ee/verify_sound_rpc.py --native work/bt2-generation/entry-stop.json` validates ignored independent bind/command/thread/console captures and the native SDRDRV result/ownership. It requires the corrected EE thread-start boundary; later retail checkpoints are forward evidence. See [SDRDRV scope](SDRDRV_RPC_PLAN.md) and [EE thread start](EE_THREAD_START_PLAN.md). Captures and their game-derived bytes stay local and ignored.


### EE sound thread-start checkpoint

`python tools/capture_ee/verify_ee_thread.py --output work/bt2-generation/ee-thread-start.bin` verifies local thread query/Status/return/context and sound/device forward captures, then writes a checked 56-byte bundle. Append this as the optional 22nd probe argument after the texture bundle. Use `--native work/bt2-generation/entry-stop.json` to compare owned ready context, SOUNDS server/client/result/input and request metadata. The optional probe now reports RPC registration inventory. Bundles/captures/generated code remain ignored; ready state does not imply running threads.

### DBCMAN request evidence

`python tools/capture_ee/verify_dbc_rpc.py --native work/bt2-generation/entry-stop.json` validates the reached four-bind/first-command group and pending second-command reverse-DMA boundary. The verifier checks local capture hashes, executable and environment identities, real caller publications, relocated code, owned client/server addresses and request metadata. The second request must remain pending at unresolved sifman:7; no retail success is replayed.

New isolated capture points: `rpc-dbc-second-bind-after`, `rpc-dbc-command-before`, `rpc-dbc-command-after`, `rpc-dbc-binds-after`, `rpc-dbc-sema-after`, `rpc-dbc-init-after`, `rpc-dbc-setup-before`, `rpc-dbc-setup-after`. These retain the existing local disc/BIOS capture workflow; client/result windows are stable twice, but live IOP snapshots do not establish exact first DMA payload/completion.

### Initial DBCMAN reverse transfer

`capture_ee.py --capture-point rpc-dbc-setup-before --capture-dbc-dma` captures an explicitly redirected immutable diagnostic source, actual SIF submission/poll and EE readback using the isolated local interpreter. Requires local PCSX2, BIOS and disc arguments. All output is ignored. Related captures: rpc-dbc-next-before/after, rpc-dbc-port1-before (preserves the first slot call), rpc-dbc-slots-after (second parent return). Run `python tools/capture_ee/verify_dbc_dma.py --native work/bt2-generation/entry-stop.json` and `verify_dbc_rpc.py` with the same argument for owned transport and the completed four-request group. These tools do not equate live device bytes or timing.

### MCSERV startup evidence

`capture_ee.py` accepts `rpc-mc-bind-after`, `rpc-mc-command-before` `rpc-mc-command-after` and `rpc-mc-query-before` using the existing local BIOS/disc arguments. `python tools/capture_ee/verify_mc_rpc.py --native work/bt2-generation/entry-stop.json` checks the independent initialization input/result and owned native client/code. Captures and derived files stay ignored; card operations and timing are not inferred.

### First card slice and callback diagnostics

`capture_ee.py --capture-point rpc-mc-query-before --capture-mc-sio2-slice` uses the isolated local interpreter and stops after the original sceSetSliceDMA, before START. Supply the existing local PCSX2/BIOS/disc arguments. `python tools/capture_ee/verify_mc_sio2.py --native work/bt2-generation/entry-stop.json` checks environment/provenance, hook, API arguments/return, registers, owned source bytes and exact pending native START. `rpc-mc-callback-before` captures the separate retail callback frontier; its unrequested stack data is not a native payload template. All outputs remain ignored. Run the final native checkpoint after CTest, which can overwrite the shared diagnostic report.

### Timed absent-card query evidence

`capture_ee.py` supports isolated `--capture-iop-delay`, `--capture-iop-card-fifo`, `--capture-iop-card-alarm` and diagnostic `--capture-iop-ee-bind` modes at `rpc-mc-query-before`. These execute/observe the actual reached APIs or hardware stores. `--capture-mc-sio2-flow` records five delays and FIFO probes alongside the DMA sequence. `--absent-cards` creates a separately named profile with both cards disabled; use it at `rpc-mc-callback-before` to validate the primary result and reply shape. No user profile is modified.

Run `python tools/capture_ee/verify_mc_timed.py` and `verify_mc_sio2_flow.py` against the restored full native report. The timed verifier checks capture identity/profile, instrumentation, owned pending bind, immutable secondary payload/readback and the actual -11 primary result. It does not equate undefined stack fields or physical scheduling. Raw captures, generated sources and prepared maps remain ignored.

### First disc-search frontier

Use `capture_ee.py --capture-point rpc-disc-init-after`, `rpc-disc-bind-sema`, or `rpc-disc-bind-after`, with the existing explicit local PCSX2/BIOS/disc arguments and `--absent-cards`. The semaphore hook filters client 0x3A8780 and preserves the displaced branch/store for earlier clients. All three checkpoints validate original instruction words; descriptor/client/state windows are checked for stability. Run `python tools/capture_ee/verify_disc_startup.py` against the final full native report after CTest. It validates capture identity/profile/hooks, actual semaphore/bind evidence, prior card completion and the unsupported native search-service submission. It does not grant lookup/read success. Captures and selected generated bindings remain ignored.

### Disc metadata search

Capture `rpc-disc-command-before` and `rpc-disc-command-after` with the existing local PCSX2/BIOS/disc arguments and `--absent-cards`. `--capture-point rpc-disc-command-before --capture-disc-dma --absent-cards` stops immediately after the original first metadata submission, without redirecting its source; it does not observe a completion poll or handler return. The separate after-return capture provides that result evidence.

Run `python tools/capture_ee/verify_disc_startup.py` to check identity/profile, hooks, bind, first request/result and submission bytes and produce ignored `work/bt2-generation/disc-bind.bin`. Run `python tools/iop/generate_disc_service.py --disc rom/BT2-USA.iso` before rebuilding optional IOP targets. This generates only ignored translated CDVDFSV files after checking local-disc and retail code/GP ownership. Append `disc-bind.bin` and the explicit local ISO path after `ee-thread-start.bin` in the full entry-probe arguments. Run the full checkpoint again after CTest and then the verifier. No proprietary payload is committed; the ordinary host still requires no game data.

## First disc command evidence

`python tools/capture_ee/verify_disc_command.py --disc rom/BT2-USA.iso` checks isolated NCMD bind/semaphore/ready/request/readback captures, environment identities, observer words and RAM hashes. It creates ignored `work/bt2-generation/ncmd-bind.bin` and checks native readiness when present. This does not certify native sector completion.

New checkpoints: `rpc-ncmd-bind-sema`, `rpc-ncmd-bind-after`, `rpc-ncmd-call-sema`, `rpc-ncmd-before`, `rpc-ncmd-after`. Use `--absent-cards` with the existing local PCSX2/BIOS/disc arguments. `--capture-disc-type` observes original register reads; `--capture-disc-api` observes a selected original API return in an isolated interpreter profile. Modes cannot be combined. Readiness API observers `sc-read2` / `sc-media` use `rpc-ncmd-call-sema`; read-setup observers `sc-key`, `sc-dual`, `sc-lsn`, `sc-timeout` use `rpc-ncmd-before`. A capture timeout is not completion evidence.


### First owned sector completion

`capture_ee.py --capture-point rpc-ncmd-before --capture-disc-api KIND --absent-cards` supports original read/import observers: `read-rv`, `read-callback`, `read-callback-after`, `sync`, `sync-after`, `error`, `read-alarm`, `read-alarm-cancel`, `read-disable`, `read-transfer`, `read-transfer-poll`, `read-tail`, `read-completion`, `read-final`, `search-afs`, `search-afs2`, `search-op`, `search-ed`, and `search-logo`. Supply the existing explicit local PCSX2/BIOS/disc paths. Observers invoke original imports; callback-after executes displaced original words through a checked trampoline. Callback-entry observation publishes no completion result. Captures and their hashes remain ignored.

Run `python tools/capture_ee/verify_disc_read.py --disc rom/BT2-USA.iso --native work/bt2-generation/entry-stop.json` after the full entry run. It checks independent read framing, callback, timeout/sync/transport observations, directory tokenization, all five file lookups, owned source bytes, progress and generated EE cleanup. The full probe writes ignored `.ee.bin` and `.iop.bin` evidence sidecars after native sector execution. Pass `--sector-bytes work/iop/native-disc-sectors.bin` to also check the isolated native read output.

`bt2-iop-service-tests --disc-read ISO OUTPUT` validates the bounded original IOP search/read/readiness path and the five-file metadata batch using the explicitly supplied local ISO. Missing backend data, wrong epoch and repeated completion are rejected without new callback effects. `disc-sector-bounds` uses authored synthetic bytes and requires no game files. Native logical DMA timing is not equated with physical PCSX2 poll counts; full driver residency and arbitrary streaming remain unsupported.


Provisional map preparation now recognizes one bounded word-table switch idiom: explicit unsigned index bound, default branch, scaled index, file-backed table load and `jr` with a NOP delay slot. Every table entry must be aligned and land in an existing candidate body span; gaps remain isolated unreachable NOPs. Changed bounds, control words, truncated tables, out-of-body targets and targets in padding remain quarantined. This permits generation of the actually reached formatter path without relaxing arbitrary computed jumps or copying an implementation from a reference project.


## CRI AutoDMA and constructor evidence

`capture_cri_thread.py` supports original `program`, `transfer`, `irq` and `volume` observers in addition to earlier start/wait boundaries. `capture_ee.py --capture-point rpc-cri-nth-before --cri-call-index N --absent-cards` observes a selected original CRI request (1..64); supply existing explicit local PCSX2/BIOS/disc arguments. Observers publish original behavior; they do not inject successful replies.

`python tools/capture_ee/verify_cri_autodma.py --native work/bt2-generation/entry-stop.json` checks original capture provenance, transfer programming, pending owned DMA and the sixteen constructor pairs. For current relocated disc workspace, add `--workspace 0x1D4B00` to `verify_disc_read.py`. Private inputs, capture RAM and generated translations stay ignored.


## Second-channel captures and repeatable startup measurement

Use `capture_ee.py --capture-point rpc-cri-nth-before --cri-call-index N --absent-cards` for original requests 36..42. `--capture-point rpc-cri-nth-control-after --cri-call-index 40` publishes the final original control-wrapper return after its fifth 0x408 request; its counter counts control-wrapper returns, excluding the two command-2 calls. Earlier returns execute the original displaced branch/delay slot. Supply the existing explicit local PCSX2/BIOS/disc paths.

`python tools/capture_ee/verify_cri_second_channel.py --native work/bt2-generation/entry-stop.json` verifies the bounded group, captured allocations, exact requests/results, normalized constructor objects and EE cleanup. It excludes identified live worker state; it does not certify playback.

`python tools/ps2recomp/run_startup_probe.py --disc rom/BT2-USA.iso` replaces the need for an ignored helper script to run the current startup checkpoint. Rebuild bt2-entry-probe first after source changes. Existing local bundles and generated files are required; it prints automatically measured distinct EE/kernel entry counts separately from invocation events. See BUILDING.md for the reproducible command and limitations.


Main-thread evidence checkpoints: `rpc-main-status-after`, `rpc-main-priority-before`, `rpc-main-priority-after`, `rpc-worker4-created/started` and `rpc-worker5-created/started`. The latter tool labels denote the next native sequence; original observed IDs are 6/7. `verify_ee_main_status.py --native work/bt2-generation/entry-stop.json` checks original provenance, consumed native status fields, original worker contexts and the retained next-worker boundary. All four scoped worker lifecycles now validate; actual child instruction execution is unimplemented. Add `--service-ram work/iop/second-channel-native.bin` to the second-channel validator when checking its five isolated pair objects against a full EE checkpoint with 37 CRI calls.


## Four-helper lifecycle and first RPC wait

`rpc-helper-created/started` and `rpc-helper2-created/started` observe original IDs 4/5. Historical `rpc-worker4-created/started` and `rpc-worker5-created/started` observe IDs 6/7. `rpc-helper-ready-status` / `rpc-helper-suspend-after` observe the first helper's ready query and SuspendThread return; `rpc-helper7-ready-status` / `rpc-helper7-suspend-after` select the last helper and preserve the earlier shared-function invocation. Selector 0x37 is SuspendThread, not WakeupThread.

`rpc-ee-server-registered` / `rpc-ee-server-wait` publish the original service registration / first SleepThread call. They save original Status, s0 and GP before terminal DI/SYNC to prevent preemption during readout. The observer terminates at the capture and does not deliver any bind or callback. `verify_ee_main_status.py` compares all original provenance, helper descriptors/reset contexts, both ready-query/suspend results, native owned thread states and the retained main-priority boundary. It does not certify a runnable EE scheduler.

### Tail-call splitting and additional map proofs

`analyze_game.py --discover-blocker` separates a direct `J` tail-call target from the observed function when Ghidra absorbed it as intra-function flow. The target must be a call target, open a stack frame within its first two instructions, or directly follow a completed `JR RA`/delay-slot pair (a leaf with no fall-through entry). Both functions remain provisional and are still filtered by `prepare_map.py`. The map prover additionally accepts the BEQL switch layout with an annulled delay slot, a moved index (`DADDU v1,rs,zero`), and padding NOPs after a proven switch jump or `JR RA`. Branches into any gap are still rejected.

After discovery, regenerate (`prepare_map.py`, `generate_game.py`). Confirm that previously selected raw-source hashes are unchanged before refreshing the extra-entry receipt. Then select reached entries with `expand_startup.py`, run `prepare_runtime.py`, reconfigure and rebuild `bt2-entry-probe`.

### Interrupt/load captures and comparison

New capture points: `rpc-vblank-first` (first VBLANK_START handler entry), `rpc-load-poll-idle` (main's first idle call in its request poll) and `rpc-load-poll-done` (main leaves the poll). Run them with the existing `capture_ee.py` arguments (`--pcsx2`, `--bios`, `--disc`, `--absent-cards`). `python tools/capture_ee/verify_vblank_wake.py --native work/bt2-generation/entry-stop.json` compares the first delivery phase, the owned helper wakes, the disc request sequence and the loaded archive table. `tools/iop/generate_disc_service.py` now also translates the CDVDFSV S-command handler after checking its original registration.

New capture points: `rpc-display-init-after` (main returns from display setup at `0x100658`; its IOP RAM holds the reloaded embedded module) and `rpc-timer-seed-after` (main returns from its system-time read at `0x100660`; the marker's v0 holds the result). Run them with the existing `capture_ee.py` arguments. `python tools/capture_ee/verify_display_reset.py --native work/bt2-generation/entry-stop.json` compares the repeated clear's field sample, the embedded reload placement/result/free, the 512×448 double-buffer environment and GS library state. `tools/iop/generate_embedded.py` now also requires `rpc-display-init-after` and emits a second translation of the embedded module at the original reload placement (`0x196530`). Run the normal startup probe after CTest: the `generated-entry-probe` test rewrites `entry-stop.json` with its reduced report.

`python tools/capture_ee/verify_timer2.py --native work/bt2-generation/entry-stop.json` compares the EE timer library state and alarm list with `rpc-timer-seed-after`, checks that every logical Timer 2 wrap was delivered and acknowledged, and reports the native and original system-time seeds without equating them.

New capture point: `rpc-sound-bank-after` (main returns from its first sound-bank load at its loop head `0x100680`, first entry). `python tools/capture_ee/verify_sound_bank.py --native work/bt2-generation/entry-stop.json` compares the IOP sound block, both bank blocks at their captured heap placements, the SOUNDS request record and the loader slot table.

New capture point: `rpc-dbzp-loaded` (main's first frame step after closing its first loaded file, `0x10036C`): `DBZP.BIN` in EE RAM and FILEIO read-buffer residue in IOP RAM.

`python tools/capture_ee/verify_dbzp_load.py --native work/bt2-generation/entry-stop.json` checks the native FILEIO lifecycle (bind, six calls, five worker completions). It then compares the loaded `DBZP.BIN` (0x7D48F bytes at `0x3B3100`) with `rpc-dbzp-loaded`.

`prepare_map.py`'s bounded switch proof accepts a second compiler layout: SLTIU bound / BEQ / LUI in the delay slot / SLL / ADDIU / ADDU / LW / JR through any register. Its registers are decoded and checked, and the LW displacement is part of the table address.

Runtime-loaded overlays (`tools/ps2recomp/overlay_programs.py`).
- `python tools/ps2recomp/overlay_programs.py --extract <ISO>` extracts `DBZP.BIN`, checks its pinned identity and wraps it as an ignored local ELF.
- `analyze_game.py`, `prepare_map.py` and `generate_game.py` accept `--program dbzp` and keep a separate Ghidra project, exports, map and output.
- `inspect_blocker.py` and `expand_startup.py` route targets inside the overlay range to the overlay map and receipt (`work/bt2-generation/overlays.json`, `overlays.cmake`, `bt2_overlays.h`).

New capture point: `rpc-overlay-dispenv` (`0x311860`, the overlay frame's first display-environment flip). `verify_overlay_dispenv.py` compares the display block, the overlay range and the recorded GS display registers.

`prepare_map.py` treats coprocessor arithmetic (COP1/COP2 with rs ≥ 16) as data flow in padding proofs. BC0/BC1/BC2 condition branches are checked against padding like other conditional branches; COP0 CO-group operations stay refused. `prepare_runtime.py` routes the generated BC2T/BC2F condition to `PS2Runtime::cop2Condition`.

The entry probe takes a diagnostic phase stop: `BT2_PHASE_STOP=<hex address>` stops at the first entry to, or return to, that address, as a branch boundary with a normal report. Use it to compare earlier-phase capture verifiers against a later checkpoint. Copy the report before the next run; it does not change guest behavior. Reports also list the live guest return stack at a stop (`stop_returns`).

**Rendering the frame.** `tools/capture_ee/render_gs_frame.py --native work/bt2-generation/entry-stop.json --output work/frames/frame-latest.png` renders the displayed framebuffer from the probe's owned GS memory dump (`entry-stop.json.gs.bin`: value and known-bit mask per word). It uses the report's DISPFB/DISPLAY registers; pixels the model never wrote show as a magenta checker. The PNG comes from game data, so it stays in ignored `work/`.

**Frame budget.** The entry probe stops after 120 display flips by default, as a checked boundary after a completed flip. `BT2_FRAME_LIMIT=<n>` changes the budget and `0` disables it. Each Timer 1 delay wait is capped separately (100,000 polls) to detect a wait that never ends.

**Viewer mode.** `run_startup_probe.py --viewer [--frames N] [--scale integer|stretch]` presents every flip in a Win32 window (`BT2_VIEWER`, `BT2_VIEWER_SCALE`) and keeps the last frame shown after a stop.

**IOP write watchpoint.** `BT2_IOP_WATCH=<hex>` stops the native IOP on the first nonzero store to that address and reports the writing PC and thread.

**Map and generation.**
- `prepare_map.py` proves BEQ-guarded switch tables whose delay slot keeps the index register intact.
- For overlay programs, the padding proof accepts J targets in the main executable's .text.
- `prepare_runtime.py` routes inline DirectJump dispatches (J to another program's function) as tail jumps.
