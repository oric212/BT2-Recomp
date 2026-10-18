# Story mode without input

Part of step 6 of the [whole-game coverage plan](WHOLE_GAME_COVERAGE_PLAN.md): run every Dragon Adventure episode and its battles without input, so the code only story mode reaches (its map, its event script, its pre-battle screen, its battle rules) is run and its stops are found. Written 2026-10-19. Names stay neutral; addresses are BT2's.

## What is known (BT2's code, checked in runs where noted)

- **Choosing an episode:** the main menu's first entry sets mode 6 or 7. Mode 6 plays the intro; mode 7 is the episode screen (`0x41F880`); mode 8 takes the episode number from progress `+48` (25 entries in its music table), plays its opening (`0x4211D8`), sets progress flag `0x200` and returns to mode 7, which returns from the overlay's entry. Main then runs `0x2A8A38` instead of the versus battle routine.
- **The story routine:** `0x2A8A38` clears the story state at `0x3B2600` (1,588 bytes), takes the episode and part from progress `+48` and `+52` (`0x2AC948`), loads the episode (`0x2AC580`) and runs `0x2AC520`: the map (`0x2AC308`), then, unless the episode is over (`0x2AC9A8`), the pre-battle screen (`0x2AC450` → `0x255EA0`), the battle routine `0x103000` and an after-battle step (`0x2AC4A0`), in a loop.
- **Events:** each map frame, `0x2AC1F8` reads the event state at `0x3B26B0` (phase, kind, number) and reports when an event fires; `0x2ACE58` records the event number. The kinds (1 to 7) are set by small handlers (`0x2A7080`, `0x2A7D18`, `0x2A7D58`, `0x2A7DB8`, `0x2A8030`, `0x2A8070`, …) that take their argument from a script (`0x29FEB0`): the episode's events are commands of a script.
- **The script machine:** the map's setup (`0x2AB878`) registers a command table (`0x344C98`: about 90 entries of opcode and handler; opcodes in groups of a thousand: 1, 1001-1016, 4001-4009, 8001-8012, 9001-9003, 10001-10010, 11001, 12001-12011, 13001-13007, 14001-14004, 15001-15002, 16001-16002, 17001-17003, 18001) with a stack-based interpreter of about 100 functions (`0x29E000`-`0x2A1000`; `0x29F080` registers, `0x29FEB0` pops an argument), and loads the story script from file 13259 (and file 1895 or 1896, chosen by a flag). The event handlers are opcodes 1004 to 1013.
- **The pre-battle screen** (`0x255EA0`) waits for a confirmation (`0x243BA8`), then sets the battle up with the same setters `BT2_START_BATTLE` uses, from the story's battle data (`gp-17440`).
- **Checked:** `BT2_START_EPISODE=<episode>[,<part>]` (`runtime/probe/game_starter.cpp`) sets those fields and the flag and returns from the entry. Episodes are 0 to 24: each loads its 3D map with its own lead character and waits for input, nothing interpreted, except **episode 23, which draws no 3D** (open: the first thing to look at in the next pass). From 25 the game reads past its tables. Details in [VALIDATION.md](../../progress/VALIDATION.md).

## What is missing

The battles of an episode start from its script when the player reaches a point on the map, and the pre-battle screen waits for a button. Without input a run stays on the map.

## Approach

1. **Decode the script machine:** its instruction format and dispatch loop (the command table is known), the script's position and the per-episode entry (`0x2AC580`), then list each episode's battle commands with their numbers by reading file 13259 offline (a tool under `tools/`, output in ignored `work/`).
2. **Starter for a story battle:** `BT2_START_EPISODE=<episode>,<part>,<battle>` sets the episode up as now, then a tool-only replacement of the map's event check (`0x2AC1F8`) fires the chosen battle event once, and a tool-only replacement of the pre-battle screen's confirmation accepts it; everything else is the game's own code. Checked by its picture (the right fighters on the right stage) against the episode's battle list.
3. **Sweep:** every episode and battle, one run each (Release, headless, a frame budget, `BT2_STALL_FIELDS`), as `battle_sweep.py` does for versus battles; stops are fixed in general (step 5).

## Limits

- Tool runs only, as the other starters; nothing changes the player's programs.
- No story script is copied into the repository; the battle lists stay in ignored `work/`.
- If the script's battle data cannot be found from the code, the fallback is the user's 100% save (every episode open) with the episode screen started by mode, which still needs the map driven: stop and ask before scripting input.
