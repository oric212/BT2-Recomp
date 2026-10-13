# BT2-Recomp

Native Windows x64 port of *Dragon Ball Z: Budokai Tenkaichi 2* (SLUS-21441): statically recompiled game code on a minimal native layer. The working rules are in [AGENTS.md](AGENTS.md); follow them.

@AGENTS.md

## Quick reference

- **State and next step:** `docs/PROG.md`, `docs/progress/ACTIVE_PASS.md`, `docs/project/IMPLEMENTATION_PLAN.md`.
- **Build:** `python tools/build/msvc.py cmake --build build/probe --config Release --target bt2-entry-probe`
- **Gate (before every commit):** `python tools/gate/run_gate.py --disc <iso>`
- **Run:** `python tools/ps2recomp/run_startup_probe.py --disc <iso> --config Release --frames <n> --pad-script <script>`
- **Reach new code:** `python tools/ps2recomp/discovery_loop.py --disc <iso> --frames <n> --pad-script-file <file>`
- **Tools and diagnostics:** `docs/guides/TOOLS.md`. **Where code goes:** `docs/guides/RUNTIME_LAYOUT.md`.
- **Never commit** game content, captures, recordings or generated code; they live in ignored `rom/`, `work/`, `game/generated/`.
