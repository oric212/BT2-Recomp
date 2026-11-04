# Release package

Asked by the user on 2026-11-04: a folder with what the release needs, zipped under `dist/`, built as if for distribution, with the player providing the game's data and HD textures; not published yet.

## Built (2026-11-04)

`python tools/build/package.py` makes `dist/BT2-Recomp-<version>/` and its zip:

| In the package | From |
| --- | --- |
| `bt2-game.exe`, `bt2-launcher.exe`, `bt2-entry-probe.exe` | `build/probe/Release` |
| `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | Visual Studio's redistributable C runtime (what the programs import besides Windows' own; Vulkan comes with the graphics driver) |
| `assets/` | the launcher's logo and the port's beam struggle art |
| `data/`, `data/textures/` | empty, with a note each: the player's disc image, the player's HD texture pack |
| `work/bt2-generation/` | the runtime's 22 startup inputs (`run_startup_probe.INPUTS`), where the game program looks for them |
| `README.txt`, `ONLINE-SETUP.txt` | `docs/guides/release/` |
| `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`, `SOURCE.txt` | GPL-3.0; paraLLEl-GS (LGPL-3.0), Granite, volk, the Vulkan headers, xxHash, the C runtime; the source's address and commit |

The tool refuses disc images, the game's executable, debug files and missing inputs by name (`tools/build/test_package.py`). Settings, saves and logs are made on the player's first start (`user/`, `saves/`).

## What keeps it from being published

1. **The runtime program contains code compiled from the game's executable** (`docs/project/LEGAL.md`: the translated source and its executable stay unpublished). The usual ways out, each the user's decision:
   - The player generates and compiles the code from their own disc: the generator, the runtime's source and a compiler on the player's PC (Visual Studio's build tools, about 6 minutes for the generated units, more for a first build).
   - Publishing the compiled runtime regardless: a legal risk the project has not taken.
2. **The startup inputs are game- and console-derived.** `guest-memory.bin` (32 MB) is the game's executable laid out in memory (`tools/ps2recomp/prepare_runtime.py`, `build_memory`): the launcher can make it from the player's disc at the first start. `kernel-snapshot.bin` (512 KB) and the IOP captures `sif-bind.bin` and `sif-dma.bin` (2 MB each) come from PCSX2 running the console's BIOS: to drop them, the state the runtime reads from them is set up natively (as the IOP services already are), each piece checked against the captures before they go. The other 18 inputs are register values and small records (4 to 1,596 bytes) that become constants where they are facts.

## Next

1. List what the runtime reads from each captured input (the kernel's tables, the IOP buffers' fields) and replace it natively, compared byte for byte with the captures; then `guest-memory.bin` from the player's disc in the launcher.
2. The user's decision on the runtime program (above).
