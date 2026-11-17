# Building BT2 Recomp from source

**Just want to play?** You don't need this page. Download the latest release from the [releases page](https://github.com/oric212/BT2-Recomp/releases) and follow its `README.txt`.

This page is for people who want to build the programs themselves or work on the project. Building takes a while the first time, and you need your own copy of the game: the build turns the game's code into C++ on your PC, and none of the game is in this repository.

> **Honest status:** the project is developed on one PC whose generated code and function map have grown over many passes. Nobody has yet tested a complete build from a fresh clone. The steps below are the right order, but expect to need the [developer tools guide](TOOLS.md) if a step stops. Reports are welcome.

## 1. Install the tools

- Windows 10 or 11, 64-bit.
- **Visual Studio 2022** with the "Desktop development with C++" workload.
- **CMake** 3.21 or newer, **Git**, and **Python** 3.10 or newer, all on your `PATH`.
- **Ghidra** with a JDK and the R5900 extension, for the one-time analysis of the game's code. The versions are listed in [`config/ghidra.lock.json`](../../config/ghidra.lock.json).

## 2. Get the code

```powershell
git clone https://github.com/oric212/BT2-Recomp.git
cd BT2-Recomp
```

All commands on this page run from this folder.

## 3. Add your game files

Make a folder called `rom` in the checkout. Git ignores it, so nothing in it is ever uploaded. Put these in it:

- your disc image of the **USA version (SLUS-21441)**, for example `rom/BT2-USA.iso`;
- `SLUS_214.41` and `SYSTEM.CNF`, copied out of the disc image with any tool that opens `.iso` files.

Check that they are the right version:

```powershell
python tools/verify_game/verify_game.py rom/SLUS_214.41 --system-cnf rom/SYSTEM.CNF
python tools/verify_game/verify_disc.py rom/BT2-USA.iso --executable rom/SLUS_214.41
```

Never commit your disc image, the game's files or anything made from them. See the [content rules](../project/LEGAL.md).

## 4. Fetch the open-source parts

These download pinned versions into the ignored `work/dependencies/` folder:

```powershell
python tools/ps2recomp/build_tools.py               # the recompiler (PS2Recomp)
python tools/graphics/textures/fetch_xxhash.py      # texture names for HD packs
python tools/graphics/gpu/fetch_parallel_gs.py      # the GPU renderer (paraLLEl-GS)
```

## 5. Turn the game's code into C++

1. **Analyse the game** with Ghidra (once):

   ```powershell
   python tools/ghidra/analyze_game.py --ghidra-home "C:\path\to\ghidra" --java-home "C:\path\to\jdk"
   ```

2. **Generate and build** the first version:

   ```powershell
   python tools/ps2recomp/run_pipeline.py
   python tools/ps2recomp/generate_interpreter.py
   ```

3. **Map the rest of the game.** The discovery loop finds every function the game uses, regenerates the C++ and rebuilds:

   ```powershell
   python tools/ps2recomp/discovery_loop.py --map-all
   ```

Everything this makes stays in ignored folders (`game/generated/`, `work/`, `build/`).

## 6. Build the programs

Switch on the GPU renderer, which draws the game's picture, then build Release:

```powershell
python tools/build/msvc.py cmake -S . -B build/probe -DBT2_GPU_GS=ON
python tools/build/msvc.py cmake --build build/probe --config Release
```

`tools/build/msvc.py` sets up the Visual Studio compiler for each command, so you don't need a Developer PowerShell.

The programs end up in `build/probe/Release`:

- `bt2-launcher.exe` opens the launcher;
- `bt2-game.exe` starts the game directly;
- `bt2-entry-probe.exe` is the game itself.

Put your `.iso` into the `data` folder next to them and start `bt2-launcher.exe`.

**How long it takes** (10-core PC):

| What changed | Release build |
| --- | --- |
| Everything (first build) | about 6 minutes |
| A runtime source file | about 15 seconds |
| Nothing | about 5 seconds |

Close the game before you rebuild: Windows can't replace a program that is running.

## 7. Check your build

The gate builds Debug and Release and runs every test, including a scripted fight checked against a recorded result:

```powershell
python tools/gate/run_gate.py --disc rom/BT2-USA.iso
```

It ends with `GATE PASSED` when everything is right.

## 8. Make a release package (optional)

```powershell
python tools/build/package.py
```

This makes `dist/BT2-Recomp-<version>/` and a zip of it: the programs, their runtime files and the player guides. It refuses to include anything from the game.

## Where to go next

- [Developer tools](TOOLS.md): every tool and diagnostic in detail.
- [Runtime layout](RUNTIME_LAYOUT.md): where each part of the code lives.
- [Progress](../PROG.md): what works and what is being worked on.
