# Game content and dependency licenses

No proprietary game content belongs in this repository: disc images, retail executables, models, textures, music, voices, movies, or other extracted assets. Users provide their own game files locally.

Store local game content under ignored `game_data/` or `rom/`. Store generated code derived from the retail executable under ignored `game/generated/` or `generated/`; committing it requires an explicit future decision. Store analysis output under ignored `research/` or `work/`.

Respect dependency licenses. Before integrating a dependency, record its source, pinned version, license, and attribution or distribution requirements. No third-party dependency is bundled in the foundation.

Review staged files before every commit; ignore rules help prevent accidental inclusion.
