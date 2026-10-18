# Story mode without input

Part of step 6 of the [whole-game coverage plan](WHOLE_GAME_COVERAGE_PLAN.md): run every Dragon Adventure part and its battles without input, so the code only story mode reaches (its map, its scene scripts, its pre-battle screen, its battle rules) runs and its stops are found. Written 2026-10-19. Names stay neutral; addresses are BT2's.

## What is known (BT2's code and data, checked where noted)

- **Choosing an episode:** the main menu's first entry sets mode 6 or 7; mode 7 is the episode screen (`0x41F880`); mode 8 takes the episode from progress `+48`, plays its opening (`0x4211D8`) and sets progress flag `0x200`, after which main runs `0x2A8A38` instead of the versus battle routine.
- **Episodes and parts:** an episode table in the executable (`0x344F30`, 16 bytes per episode: an available flag, the number of parts, the map file 15879 Earth or 15880 Namek) lists **25 episodes with 207 parts**. Episode 23 is marked unavailable (flag 0).
- **Scenes:** each part has three consecutive scene files; the first is **33 + 3 × (parts before it)** (`0x2AC580`; progress `+52` is the part). A scene file is a script: `GSCF`, with sections `GSHD`, `GSCD` (code: `GSAC` blocks numbered `0xFFFFFFFF` for the start and 1000, 1001, 2001 to 2025, 3001 to 3003, each ending in `EOFC`) and `GSTD`. An instruction word holds the opcode in its upper 16 bits and the argument count in bits 8 to 15; each argument word holds `(value << 8) | type` (type `0x0A`: an integer). All 621 scene files (33 to 653) decode.
- **The story loop:** `0x2A8A38` runs `0x2AC520`: the map (`0x2AC308`: per scene, load its file, hand it to the script machine `0x29F518`, set it up `0x2ACF90`, then each frame update the map `0x2ACB20` and check for an event `0x2AC1F8`), then, unless the episode is over (`0x2AC9A8`), the pre-battle screen (`0x2AC450` → `0x255EA0`, which waits for a confirmation `0x243BA8` and then calls the same battle setters as `BT2_START_BATTLE`), the battle routine `0x103000` and an after-battle step (`0x2AC4A0`), in a loop.
- **Events** (`0x2AC1F8`, state at `0x3B26B0`: phase, kind, number): kinds 2 to 4 make the number the next scene (`0x2ACE58`), set by opcodes 1004, 1007, 1012 and 1013; kind 1 ends the map without a scene change (opcode 1011, never used by a scene file).
- **The script machine:** about 100 functions at `0x29E000`-`0x2A1000`; the map's setup (`0x2AB878`) registers the command table (`0x344C98`, about 90 opcodes in groups of a thousand) and also loads file 13259. Opcodes the scene files use: below 1000 (the machine's own instructions), 1002 (stores its argument in the story state), 1003, 1005, 1009 (adds its argument to a set of up to five: likely the map's open events or markers), 1010, 1012, 1013, and the groups 4001-4008, 8001-8010, 9001, 10004-10009, 11001, 12001-12004, 13001-13007, 15001.
- **Map markers (found 2026-10-19):** the map's frame loop (`0x2ABF30`) runs until the scene's script ends or the map mode's exit test (`0x2AD898`, from a callback table) says so. The exit test asks whether the player has reached a marker (`0x20C870`, a byte of the map object) and which (`0x20C8E8`), runs that marker's block of the scene script (block 2000 + marker, `0x2AD1F0`), whose command 1002 names the next scene, then fades out and ends the map. Command arguments index the script's values (section `GSDT`), which hold the map file, the lead character's models and the markers' scenes. **157 of the 207 parts have a marker that leads to their own story scene** (first scene + 1); the other 50 (for example all of episode 4) start their story some other way.
- **Checked:** `BT2_START_EPISODE=<episode>[,<part>]` starts episodes 0 to 24 (each loads its 3D map, nothing interpreted; episode 23's first scene is empty, matching its unavailable flag). With `,marker:<m>` a tool-only replacement of the two marker queries reports the player at marker *m* once: episode 0 part 0 with marker 14 leaves the map for scene 34, the episode's opening cutscene (Piccolo, "Five peaceful years have passed..."), which runs without input until its first text box waits for a button. On the way it sent SOUNDS request `0x80`, now implemented from the module's code (handler `0xF2C`: load a bank into an existing slot, keeping the bank count). `tools/ps2recomp/story_scenes.py` lists per part the markers, the marker to the story scene, the scenes reached and the commands used ([VALIDATION.md](../../progress/VALIDATION.md)).

## What is missing

- **Text boxes** wait for a button: a tool-only advance is needed (find the message's wait; command 8010 is not it, it places an object).
- **The pre-battle screen** (`0x255EA0`) waits for a confirmation (`0x243BA8`).
- **The 50 parts without a story marker** start their story another way (not looked at yet).

## Next steps

1. Find where a text box waits for its button and add a tool-only advance (for every scene, not per scene).
2. Accept the pre-battle screen's confirmation the same way; then a part's battle runs: check its picture.
3. The 50 other parts; then the overnight run of every part's battle (207 parts, roughly 1.5 to 2 minutes each, about 5 to 7 hours), after the user confirms it.

## Limits

- Tool runs only, as the other starters; nothing changes the player's programs.
- No scene script or story data is copied into the repository; lists stay in ignored `work/`.
- If the battle data cannot be found from code and data alone, ask the user before driving the map with scripted input, even for evidence.
