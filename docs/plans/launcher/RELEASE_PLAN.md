# Release package

Asked by the user on 2026-11-04: a folder with what the release needs, zipped under `dist/`, built as if for distribution, with the player providing the game's data and HD textures; then: make the zip fit to distribute.

**Decision (the user, 2026-11-04): BT3-Recomp's model** ("look how bt3 did it and adapt"; it replaces the same day's first choice, the player compiling the game's code). The BT3 port (`work/references/bt3r`, `README.md`, `docs/DEPLOY.md`):

- The repository holds no game code; the generated sources are made from the developer's own disc and never committed.
- **The release ships the compiled program, the game's code compiled in** (the `DBZP.BIN` overlay too: "recompiled into the runner and never shipped" as a file).
- **The release ships no game data**: on the first run its install wizard reads the player's own USA ISO, checks the files by SHA-256 and extracts what the game reads into `data/`.
- **No console content**: its kernel and IOP side is native (no BIOS, no captures).

For BT2's port that means: the compiled runtime ships (as `bt2-entry-probe.exe` does today); the game's data stays on the player's disc image, which the runtime reads directly (nothing to extract); everything taken from the console or prepared from the game outside the program goes.

## Built (2026-11-04)

**The package:** `python tools/build/package.py` makes `dist/BT2-Recomp-<version>/` and its zip:

| In the package | From |
| --- | --- |
| `bt2-game.exe`, `bt2-launcher.exe`, `bt2-entry-probe.exe` | `build/probe/Release` |
| `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | Visual Studio's redistributable C runtime (what the programs import besides Windows' own; Vulkan comes with the graphics driver) |
| `assets/` | the port's beam struggle art (the launcher's logo is left out: the game's title logo) |
| `data/`, `data/textures/` | empty, with a note each: the player's disc image, the player's HD texture pack |
| `README.txt`, `ONLINE-SETUP.txt` | `docs/guides/release/` |
| `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`, `SOURCE.txt` | GPL-3.0; paraLLEl-GS (LGPL-3.0), Granite, volk, the Vulkan headers, xxHash, the C runtime; the source's address and commit |

The tool refuses disc images, the game's executable, debug files and missing inputs by name (`tools/build/test_package.py`).

**The game's starting memory from the player's disc** (`runtime/ee/guest_image.*`): the runtime reads `SLUS_214.41` from the disc image, checks it is the executable this port was built from (its size and hash: another disc, region or revision is refused by name), and places its loadable segments in main RAM as the console's loader does. Byte-identical to the image the tools prepared (`bt2-guest-image-tests`, with the developer's disc); the gate passes with it (battle hash `11860f808d115d87`). `guest-memory.bin` is no longer a startup input: the runtime's first argument is the word `disc`.

## The console's state made native (2026-11-04)

What the captures were used for, found by tracing (step 1) and replaced (steps 2 to 4):

| Capture | What the game used of it | Now |
| --- | --- | --- |
| `kernel-snapshot.bin` (512 KB, the console's kernel memory) | Only two searches at its start: the game installs its own `FindAddress` (syscall 0x83, its code at 0x338388) and looks for its handlers 0x338350 and 0x338388 in the kernel's syscall table; found at 0x80014E68 and 0x80014F0C (slots 0x5A and 0x83 of the table at 0x80014D00). Recorded with `BT2_KERNEL_READS` over a boot, a CPU fight, a story episode and a two-player fight: the same 21,444 words (the two scans), nothing else. | The kernel reads as zero except the syscall table, which shows the handlers registered with `SetSyscall`, and the game's own patches (`ps2_runtime_memory.cpp`). The same reads find the same two slots with the same values. |
| The kernel payloads (`bt2-kernel-runtime*`) | Code the game itself writes into the kernel (from its own memory at 0x36B328 and 0x36BAF8). | Unchanged: the game's code, compiled in like the rest. |
| `sif-dma.bin` (2 MB of IOP memory) | Three words checked: the IOP's command buffers 0x19600 and 0x19680, the EE receive address 0x93640. | `supplySifDma()`: those values. |
| `sif-bind.bin` (2 MB and a record) | Words checked against constants the code holds. | `supplySifBind()`: the flags. |
| `cd-init.bin`, `scmd-bind.bin`, the bind and allocation records, the register words (18 files) | Each compared with constants in the code, or one value kept (the console's settings word 0x21C12010). | `EeRuntime::bootState()`: the values and flags, set before the game's first instruction. |

`guest-memory.bin` was replaced the same day (above). The runtime's command line is now `STOP_REPORT DISC_IMAGE`; `work/bt2-generation` keeps only developer files and is not shipped; the game program finds its folder (`findRoot`) as a release's own folder when no repository is above it.

**Verified (2026-11-04):** the gate with fresh runs (it now deletes the last run's reports first: a failed run had been judged from stale ones): the six probe runs, 31/31 comparisons with the original, the battle run's hash `11860f808d115d87` unchanged; a story episode and a two-player fight complete; an online session between two probes; the package started through `bt2-game.exe` from a folder outside the repository with only the package and a disc image (the runtime found next to the game program, its settings and log in the folder, 30 s of play without an error).

**The package** has 25 files: the three programs, the three C runtime DLLs, the beam struggle art the project drew, the data notes, the player's guides, the license, the third-party notices, the source note. Left out: the launcher's logo (the game's title logo); without it the launcher shows its title.

## Decisions for the user

- **Decided (2026-11-04):** BT3-Recomp's model: the compiled program ships with the game's code in it; no game data and no console content ship.
- **Open:** when to publish; and the launcher's logo in the repository (`assets/launcher/logo.png`, the game's title logo; it is committed but not shipped).
