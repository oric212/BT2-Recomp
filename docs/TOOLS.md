# Local executable tools

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

Project root: `D:\Cs\Cpp\BT2-Recomp`. Ghidra 12.1.4 is installed at `D:\Cs\Cpp\tools\ghidra_12.1.4_PUBLIC`; launch the GUI with `D:\Cs\Cpp\tools\run-ghidra.bat`. It uses the existing Adoptium JDK 25. Tool versions and archive hashes are recorded in `config/ghidra.lock.json`.

The R5900 extension v2.1.37 was built from pinned source against this Ghidra version. Gradle 9.8.0 is installed beside Ghidra. These tools remain outside the repository.

```powershell
python tools/ghidra/analyze_game.py
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
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin"
```

The tool copies PCSX2 and the selected BIOS into ignored `work/pcsx2-capture/`, with a separate settings profile. It instruments only a copy of the validated local ELF: 24 bytes at `0x00338388` write marker `0xB702` at `0x00387E80` and spin before the handler's first read. Read-only Windows process access captures 32 MiB of physical EE RAM from the exported `EEmem` pointer in the process it launched. It checks the marker, trap bytes, isolated data folder and stability of the kernel prefix across two reads, then terminates only that process. Hashes, instrumentation and timing are recorded in `capture.json`. Original game/BIOS files and the user's PCSX2 profile are preserved. This is an instrumented standalone ELF diagnostic; the IOP remains active during capture.

After building the pipeline, run:

```powershell
python tools/ps2recomp/probe_kernel.py work/pcsx2-capture/ee-ram.bin --capture-note "PCSX2 2.9.81; instrumented ELF handler entry, stable kernel prefix"
```

This extracts a read-only kernel window and invokes the generated scan handler in isolation. Hashes and notes are saved under ignored `work/bt2-generation/`. The result does not establish a full boot, exact BIOS exception behavior or writable kernel-patch compatibility. No capture is downloaded or bundled.

### Retail disc revision confirmation

```powershell
python tools/verify_game/verify_disc.py "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso"
```

This streams the local ISO against the official Redump size/MD5/SHA-1 recorded in `config/retail_revision.json`, reads bounded ISO9660 root records, and compares the embedded executable's size/SHA-256 with the local ELF. Reports stay under ignored `work/disc-validation/`. Only reference checksum metadata is stored in Git.

### Snapshot-driven entry pipeline

```powershell
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin
```

The snapshot requires its `.json` sidecar from `probe_kernel.py`; size, SHA-256 and capture note are checked before the run. The entry probe verifies both successful scans, their converged table base, restored stack and the next dispatch boundary. CTest retains the no-snapshot boundary. Supplying a snapshot is explicit; there is no automatic kernel-memory loading.

### Local Timer 3 boundary capture and read replay

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point timer3-mode
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
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point timer2-mode
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json
```

The isolated 40-byte authored trap at `0x00338C58` reads `0x10001010`, publishes payload/marker `0xB704`, then spins. Observed mode: `0x80`. Capture remains under ignored `work/pcsx2-capture/timer2-mode/`. Pipeline validates exact expected trap instructions, source hash, RAM hash/size, marker and payload for both timers. `--timer2-capture` requires Timer 3 capture and a kernel snapshot. The native probe accepts the validated startup word once; wrong RAM pointers, duplicate reads and writes fail. Omitting it retains the Timer 2 read stop. With it, generated code completes one observed write of `0x382` to `0xB0001010` and reaches EnableIntc, as described below. This is captured startup replay, not a running timer implementation.

The Timer 2 continuation now accepts one observed `0x382` mode store to `0xB0001010` after capture consumption, records the register bit update and rejects other values/repetitions. It advances to EnableIntc (`0x14`), cause 11, PC `0x0032E9A8`. `generated-timer-write` executes the actual generated helper with EIE clear/set, checks ErrorEPC return, Status restoration, LL-bit clearing, preservation of status flags and unchanged main RAM. Counter synchronization, gating, rate/event processing and interrupt delivery remain unimplemented.

### INTC mask checkpoint and EnableIntc continuation

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point intc-mask
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json --intc-capture work/pcsx2-capture/intc-mask/capture.json
```

The isolated authored trap at `0x0032E9A4`, immediately before EnableIntc, reads `0x1000F010` and publishes marker `0xB705` plus mask `0x1002`. Output stays ignored under `work/pcsx2-capture/intc-mask/`. Exact trap, source/RAM hashes, size, marker and payload are validated through the shared word-capture validator. `--intc-capture` requires Timer 2 capture; this checkpoint applies to the reached syscall, not initial BIOS state. EnableIntc supports only cause 11, returns changed/already-enabled `1`/`0` and preserves other mask bits. It records `0x1802` and does not process pending interrupts. Omitting it retains the EnableIntc stop. With it, startup reaches the second Timer 2 read at `0x00338DD8`; repeated reads remain unsupported pending validation.

### Validated Timer 2 readback

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point timer2-readback
python tools/ps2recomp/run_pipeline.py --reuse-generated --kernel-snapshot work/bt2-generation/kernel-snapshot.bin --timer3-capture work/pcsx2-capture/timer3-mode/capture.json --timer2-capture work/pcsx2-capture/timer2-mode/capture.json --intc-capture work/pcsx2-capture/intc-mask/capture.json --timer2-readback-capture work/pcsx2-capture/timer2-readback/capture.json
```

The isolated trap at second read PC `0x00338DD8` reads `0x10001010` and publishes marker `0xB706` plus observed mode `0x382`. Exact trap/source/RAM/payload provenance stays checked; outputs are ignored. Readback requires all earlier explicit inputs. The runtime compares its recorded post-write mode to the validated expected word, then returns its recorded state once. Missing writes, mismatch, wrong RAM pointers and further reads fail. Omitting this validation preserves the second-read stop. With it, generated `FUN_00338DB8` returns and startup creates semaphore ID 2 before stopping at CreateThread (`0x20`, PC `0x0032EA68`). This does not validate counter timing, future status flags or interrupt delivery.

The full checkpoint pipeline now creates dormant thread ID 2 and reaches ChangeThreadPriority (`0x29`, main ID 1 / priority 1, PC `0x0032EAF8`). Creation validates the 36-byte SDK descriptor and consumed fields, records bounded stack/GP/priority/heap metadata and writes only the four low context words established by pinned Play! ThreadReset. IDs 1/256 are reserved for the authored main/idle model; IDs are not validated against retail BIOS allocation. No scheduler or native worker is launched. Bootstrap tests verify invalid descriptors, preserved RAM/upper registers, ID allocation/capacity, exact context writes and current-thread query. The later generated helper returns -1 through its IE-clear guard; other helper paths have not run.


The full checkpoint pipeline now passes the observed main ID 1 / priority 1 ChangeThreadPriority request and stops at GetOsdConfigParam (`0x4B`, PC `0x0032ED18`). Bootstrap tests cover previous-priority return, repeated requests, rejected arguments and state preservation. Only the main thread is runnable; no scheduler is implemented. OSD configuration remains unsupported pending evidence.


### First OSD configuration checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point osd-config
```

Add `--osd-capture work/pcsx2-capture/osd-config/capture.json` to the full pipeline command above. It requires Timer 2 readback and all earlier explicit inputs. The trap reads the first returned OSD stack word at `0x003385F4`; output stays ignored. Verified provenance supplies one native read; startup then stops at SetOsdConfigParam (`0x4A`). Omitting it retains the earlier OSD read stop. No host preferences or broader BIOS OSD behavior are implemented.


### OSD same-value setter/readback checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point osd-readback
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
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point dma-status
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point dma5-control
```

Add `--dma-status-capture work/pcsx2-capture/dma-status/capture.json` and `--dma5-control-capture work/pcsx2-capture/dma5-control/capture.json` to the full pipeline with all earlier OSD-readback/checkpoint inputs. The first requires OSD readback; the second requires DMA status. Full 32-bit payload/provenance validation supplies only the observed one-time reads. The bounded adapter records channel 5 handler/enable requests; it has no DMA transfers or interrupt delivery. Without the optional SIF capture, continuation stops at SifGetReg selector 0x80000000. Omitting channel input stops at CHCR; omitting both stops at D_STAT. Detailed earlier results are historical; see [current status](PROG.md).

## First SIF query checkpoint

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point sif-command
```

Add `--sif-command-capture work/pcsx2-capture/sif-command/capture.json` to the full pipeline with all earlier captures, including channel 5. The trap at `0x00331A34` publishes returned `v0` and selector `a0`; strict source/RAM hashes, trap, marker and selector validation precede replay. Observed selector `0x80000000` returns `0x00019600` once. Prepared `FUN_00331F38` replaces nine omitted CACHE 0x18 sites with aligned, RAM-bounded line checks; raw generated source remains unchanged. Coherent host RAM has no guest cache model. The executed path checks one line and reaches SifSetDma `0x77`; no DMA/IOP transfer is performed.

## First SIF command receive and RPC state checkpoints

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point sif-dma-before
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point sif-dma-after
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point sif-rpc-state
```

Append `--sif-dma-before-capture work/pcsx2-capture/sif-dma-before/capture.json --sif-dma-after-capture work/pcsx2-capture/sif-dma-after/capture.json` to the full pipeline including `--sif-command-capture`. Both are required together. The tool also captures the first 2 MiB of IOP RAM through PCSX2's IOPmem export; source/RAM hashes, traps, descriptors, packets, table fields and observed receive effects/return are verified. Packet/table windows are checked twice; unrelated kernel and IOP memory continue running. The ignored `sif-dma.bin` bundle contains the before IOP state and observed return, enabling only a synchronous logical CHANGE_SADDR receive model.

Append `--sif-rpc-state-capture work/pcsx2-capture/sif-rpc-state/capture.json` to continue through the reached RPCINIT query. It requires the paired receive captures and matching PCSX2/BIOS provenance. Observed selector `0x80000002` returns 1 once. Omit this input to retain that query stop; omit the paired inputs to retain the first SifSetDma stop.

Checked coherent main-RAM byte/word aliases `[0x20000000,0x22000000)` support the reached packet allocator; other access widths and address regions remain unsupported. Full continuation stops at the second submission: RPC bind command `0x80000009`, server ID `0x80000592`, packet source `0x203A97C0`, size 64. No general transfer scheduler, IRQ delivery or RPC service response is implemented. All EE/IOP/capture/generated output stays ignored.

## First RPC bind response and isolated callback

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point sif-bind-after
cmake --build build/probe --config Debug --target bt2-rpc-bind-probe
python tools/capture_ee/verify_rpc_bind.py work/pcsx2-capture/sif-bind-after/capture.json --run-probe
```

Use the existing generated-probe CMake configuration. The verifier checks the first CD initialization bind's request, registered IOP service/queue, reply, completed client and packet release. It writes the checked image and provenance to ignored `work/bt2-generation/`. The probe reconstructs only the explicit pending callback fixture and runs three original generated functions with strict returns. Negative interrupt-only SignalSema is scoped to this probe callback. This command validates the callback independently. The full pipeline below now connects bounded logical delivery with caller-context preservation; hardware interrupt timing, CD initialization execution and disc access remain incomplete.

## Startup RPC bind delivery and first service-call checkpoint

Capture the call semaphore boundary independently, after the first bind returns:

```powershell
python tools/capture_ee/capture_ee.py --pcsx2 "D:\Emulation\PCXS2\pcsx2-qt.exe" --bios "D:\Emulation\PCXS2\bios\scph39001.bin" --capture-point rpc-call-sema
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
python tools/iop/inspect_image.py --disc "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso"
```

The tool compares the existing local `rom/IRX/IOPRP300.IMG` with its exact ISO9660 path and writes bounded ROMDIR/ELF-header inventory to ignored `work/iop/image.json`. It does not extract, load or execute modules.

Use `capture_ee.py --disc "rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso"` with existing PCSX2/BIOS arguments for each capture point: `reset-submit-after`, `reset-cmd-clear`, `reset-flags-after`, `reset-rpc-clear`, `reset-subaddr-clear`. Each launches an independently instrumented local ELF with that local disc mounted. Publications record the submission return and subsequent register clears; they are EE evidence, not post-reset IOP snapshots.

Append `--reset-submission-captures work/pcsx2-capture` to the full pipeline after all reset-preparation inputs. It verifies image identity, capture hashes/traps/publications, packet/path, matching disc/PCSX2/BIOS and retains provenance in the ignored pipeline receipt. Omitting this new input preserves the pre-submission boundary. Earlier standalone checkpoints and these disc-backed observations remain separate experiments. The bounded runtime acknowledges the exact request, invalidates old service state and stops at the first boot-readiness poll; it does not execute IOP code or fabricate readiness.
