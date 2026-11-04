# Release package

Asked by the user on 2026-11-04: a folder with what the release needs, zipped under `dist/`, built as if for distribution, with the player providing the game's data and HD textures; then: make the zip fit to distribute.

**Decision (the user, 2026-11-04): the player builds the game's code.** The release ships no code or data of the game or the console: on the first start the launcher reads the player's own disc, generates the game's code and compiles it on the player's PC with a bundled free compiler. The console's captured startup state is replaced by native code.

## Built (2026-11-04): the private package

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

The tool refuses disc images, the game's executable, debug files and missing inputs by name (`tools/build/test_package.py`). It stays private until the steps below are done.

## What is game- or console-derived today (inventory of 2026-11-04)

| What | Size | Where it goes |
| --- | --- | --- |
| The game's code, generated (`game/generated/bt2-runtime`) | 11,077 units, 151 MB of source | compiled into `bt2-entry-probe.exe` |
| The `DBZP.BIN` overlay's code (`game/generated/dbzp-runtime`) | 24 MB of source | compiled in |
| VU1 microprogram translations (`game/generated/vu1`) | 0.5 MB | compiled in |
| The interpreter's handler tables (`game/generated/ee-interpreter`) | 26 MB | compiled in |
| Generated binding headers (`work/bt2-generation/bt2_extra_startup.h`, `bt2_overlays.h`) | 5.7 and 2.1 MB | compiled in (`probe/selected_bindings.cpp`, `overlay_bindings.cpp`) |
| Code copied from the console's kernel (`game/generated/bt2-kernel-runtime*`; `BT2_BUILD_KERNEL_PAYLOAD`, `BT2_BUILD_STARTUP_KERNEL_PAYLOAD` on) | small | compiled in |
| `guest-memory.bin` | 32 MB | read at start: the game's executable laid out in memory (`tools/ps2recomp/prepare_runtime.py`, `build_memory`) |
| `kernel-snapshot.bin`; `sif-bind.bin`, `sif-dma.bin` | 512 KB; 2 MB each | read at start: captures of the console's kernel and IOP memory (PCSX2 with the BIOS) |
| 18 small startup inputs | 4 to 1,596 bytes | read at start: register values and small records |

Facts the hand-written runtime holds (addresses, hashes of the game's code, register values, table layouts) are not the game's content and stay.

## Steps

1. **Inventory by use.** For each item above: what the runtime reads from it, found by tracing (which bytes of the captures are read, which generated symbols the hand-written code names). Output: a table per item of what has to be produced natively or by the player's build. Checked: a run with each capture's unread bytes zeroed gives the gate's battle hash.
2. **The console's state, native.** The kernel's tables and the IOP buffers the game reads are set up by the runtime, as the IOP services already are; the kernel's copied code becomes hand-written (it is small). Each replacement is compared byte for byte with the capture it replaces; then the captures and the kernel payload options go. The 18 small inputs become named constants where they are facts.
3. **A boundary between the runtime and the game's code.** The runtime program (shipped) holds no game-derived code; the game's code (generated, the overlay, VU1, the interpreter's tables, the binding headers) goes into a library built on the player's PC (`bt2-game-code.dll`). The generated units already reach the runtime only through `PS2Runtime` (about 30 non-virtual functions, `ps2_runtime.h`): the runtime program exports them and ships their import library. `ps2_runtime.h` then needs only the compiler's own headers (`<stdint.h>`, `<immintrin.h>` and builtins in place of `<array>`, `<bit>`, `<cstring>`), so the player's compiler needs no Windows SDK or Visual C++ headers. Guest exceptions (`SignalException`) unwind through generated frames: clang-cl emits the Visual C++ ABI's unwind tables, and the library links against the shipped `vcruntime140.dll` through an import library made from a definition file. Checked first with a prototype: a handful of generated units built into a library by clang-cl and lld-link with only the bundled files, loaded by the runtime, and the gate's battle hash unchanged.
4. **Generation on the player's PC.** A generator program (PS2Recomp, GPL-3.0, with this project's preparation steps moved from Python into it) reads the game's executable and `DBZP.BIN` from the player's disc image and writes the sources, with the project's function map and settings (addresses and options: facts) shipped. The same disc must give byte-identical sources to the developer's (checked by hashes of the developer's generated tree, shipped: hashes are not content).
5. **Compiling on the player's PC.** Bundled LLVM (clang-cl and lld-link, Apache-2.0 with the LLVM exception; the parts needed only), all cores but a few, a progress screen in the launcher, the result kept until the disc or the release changes. The online build check then hashes the runtime and the game library together.
6. **The release.** The package without the game's code, the binding headers or the captures; the first start in a clean folder on a clean PC (or a clean Windows user) with only a disc image: the library built, the gate's battle hash reached, an online session between a developer build and a player-built one alike.

## Open questions

- **The first start's time:** the developer build compiles the generated units in about 6 minutes on 10 cores with Visual C++; clang-cl's time on the player's PC is measured with the prototype (step 3).
- **The download's size:** clang-cl and lld-link with their headers are roughly 100 to 150 MB compressed; measured with the prototype.
- **Whether PS2Recomp's generator builds and runs without the developer's tools** (it is C++; its dependencies are in `docs/project/LEGAL.md`).

## Decisions for the user

- **Decided (2026-11-04):** the player builds the game's code; no game or console content in the release.
- **Open:** whether the compiler comes in the download or is fetched by the launcher on the first start (the download's size against a step that needs the internet).
