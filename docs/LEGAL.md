# Game content and dependency licenses

No proprietary game content belongs in this repository: disc images, retail executables, models, textures, music, voices, movies, or other extracted assets. Users provide their own game files locally.

Store local game content under ignored `game_data/` or `rom/`. Store generated code derived from the retail executable under ignored `game/generated/` or `generated/`; committing it requires an explicit future decision. Store analysis output under ignored `research/` or `work/`.

Respect dependency licenses. Before integrating a dependency, record its source, pinned version, license, and attribution or distribution requirements. No third-party dependency is bundled in the foundation.

## Evaluated tool dependencies

Dependency sources and binaries stay in ignored `work/` and `build/`; this repository records revisions in `config/ps2recomp.lock.json` and commits orchestration scripts.

| Dependency | Version | Inspected license file |
| --- | --- | --- |
| [PS2Recomp](https://github.com/ran-j/PS2Recomp/tree/c5a9d02573410a2085a4b4b831b0b68ba3515440) | `c5a9d02573410a2085a4b4b831b0b68ba3515440` | `LICENSE`: GPLv3 |
| [ELFIO](https://github.com/serge1/ELFIO/tree/8ae6cec5d60495822ecd57d736f66149da9b1830) | Release_3.12 | `LICENSE.txt`: MIT |
| [toml11](https://github.com/ToruNiina/toml11/tree/be08ba2be2a964edcdb3d3e3ea8d100abc26f286) | v4.4.0 | `LICENSE`: MIT |
| [fmt](https://github.com/fmtlib/fmt/tree/407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) | 12.1.0 | `LICENSE`: MIT with optional compiled-object exception |
| [libdwarf](https://github.com/davea42/libdwarf-code/tree/2e088b983562860f497b4fd789a3e61524eb857c) | v2.2.0 | `COPYING` and `src/lib/libdwarf/LIBDWARFCOPYRIGHT`: LGPL 2.1 for the library, plus file-specific BSD/public-domain exceptions |
| [Rabbitizer](https://github.com/Decompollaborate/rabbitizer/tree/d5804fa488474830a47fbf9f3693048ec1e45589) | 1.14.3 | `LICENSE`: MIT |
| [nlohmann/json](https://github.com/nlohmann/json/tree/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03) | v3.11.3 | `LICENSE.MIT`: MIT |

Retain copyright/license notices and satisfy applicable source and copyleft requirements when distributing dependencies. No upstream runtime is linked into `bt2-port`. Future runtime linking and generated-output licensing require assessment before distribution; generator use does not resolve ownership of retail-derived code. libdwarf's separate GPLv2 dwarfdump tool is disabled.

Review staged files before every commit; ignore rules help prevent accidental inclusion.

The generated-code probe adapter is authored project code and links no upstream runtime. The local translated source and the resulting executable contain retail-derived code and remain ignored. Do not publish or commit either as part of the project foundation.

## Installed analysis tools

Ghidra 12.1.4 includes its Apache 2.0 license and third-party notices in the external installation. The pinned Emotion Engine extension uses Apache 2.0 (`work/ghidra-emotionengine/LICENSE`). Retain the installed tools' licenses and notices if redistributing them. No Ghidra installation, extension binary, project database, or proprietary-derived analysis report is committed.

PS2SDK source files consulted for startup ABI conventions identify Academic Free License 2.0 in their headers. They remain local research references under ignored `work/abi-reference/`; no SDK implementation is bundled or linked. Any future reuse must assess the applicable license and retain required notices.
