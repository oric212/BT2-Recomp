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
| `assets/` | the launcher's logo and the port's beam struggle art |
| `data/`, `data/textures/` | empty, with a note each: the player's disc image, the player's HD texture pack |
| `work/bt2-generation/` | the runtime's startup inputs (`run_startup_probe.INPUTS`), where the game program looks for them |
| `README.txt`, `ONLINE-SETUP.txt` | `docs/guides/release/` |
| `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`, `SOURCE.txt` | GPL-3.0; paraLLEl-GS (LGPL-3.0), Granite, volk, the Vulkan headers, xxHash, the C runtime; the source's address and commit |

The tool refuses disc images, the game's executable, debug files and missing inputs by name (`tools/build/test_package.py`).

**The game's starting memory from the player's disc** (`runtime/ee/guest_image.*`): the runtime reads `SLUS_214.41` from the disc image, checks it is the executable this port was built from (its size and hash: another disc, region or revision is refused by name), and places its loadable segments in main RAM as the console's loader does. Byte-identical to the image the tools prepared (`bt2-guest-image-tests`, with the developer's disc); the gate passes with it (battle hash `11860f808d115d87`). `guest-memory.bin` is no longer a startup input: the runtime's first argument is the word `disc`.

## What still keeps the package private

| What | Size | Use |
| --- | --- | --- |
| `kernel-snapshot.bin` | 512 KB | the console's kernel memory (PCSX2 with the BIOS): every guest read of 0x80000000 to 0x80080000 is answered from it (`ps2_runtime_memory.cpp`) |
| Code copied from the console's kernel (`game/generated/bt2-kernel-runtime*`, `BT2_BUILD_KERNEL_PAYLOAD`, `BT2_BUILD_STARTUP_KERNEL_PAYLOAD`) | small | compiled in; patched over the snapshot (`enableKernelPatch`, `enableStartupKernelPayload`) |
| `sif-dma.bin`, `sif-bind.bin` | 2 MB each | IOP memory captured at startup checkpoints (`supplySifDma`, `supplySifBind`) |
| 18 small inputs (`texture-startup.bin` no longer read) | 4 to 1,596 bytes | register values and the IOP services' bind records at startup checkpoints (`probe/probe_inputs.cpp`) |

## Steps

1. **Inventory by use.** Which kernel addresses the game reads (a count per word over the gate's runs, the story sweep and a played fight), which bytes of the IOP captures are read, and what each small input holds. Checked: a run with every unread byte zeroed reaches the gate's battle hash.
2. **The kernel native.** The words the game reads are produced by the runtime (as its syscalls already are) and the copied kernel code is hand-written; compared word for word with the snapshot; then the snapshot and the payload options go.
3. **The IOP captures native.** What the game reads from them comes from the native services; compared byte for byte; then they go.
4. **The small inputs as named constants** where they are facts (register values, record layouts), each with its source in a comment; then `work/bt2-generation` leaves the package and the game program stops looking for it.
5. **The release.** The package with nothing taken from the console or prepared from the game outside the program; a first start in a clean folder with only a disc image; `LEGAL.md`'s release rule checked against the package's file list.

## Decisions for the user

- **Decided (2026-11-04):** BT3-Recomp's model: the compiled program ships with the game's code in it; no game data and no console content ship.
