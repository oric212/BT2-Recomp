# Validation record

What has actually been checked, how, and when. Only checks that were run are listed; anything else is not validated. Details are in the [monthly history](2026-10.md); the current status is in [PROG.md](../PROG.md).

**Kinds of check:** *Original*: compared with the original game (PCSX2 captures or an independent decoder). *Exact*: byte-exact comparison between two port runs (state hash of every guest memory per field, VU1 output hash). *Ran*: the run reached its frame budget with no stop and nothing interpreted; the picture was not judged by the user. *User*: the user looked or listened and confirmed.

## Checked

| Area | What was checked | Kind | Date |
| --- | --- | --- | --- |
| Gate (every commit) | Debug and Release builds, 21 unit tests, the Python suites, the probe runs, 32 comparisons and renders, map closure, and the scripted 1P-vs-COM fight against its baseline (5,100 frames, VU1 hash `5a4960667d4a3dff`) | Exact, Original | last passed 2026-10-18 |
| Startup | 46 comparison scripts against PCSX2 captures: 42 pass; `cri_rpc`, `second_heap`, `cri_preemption` and `spu2_initialization` fail | Original | 2026-10-13 |
| Native services | The same stop and identical EE memory as the emulated IOP, except two regions of stale IOP bytes | Exact | 2026-10-11 |
| Movies | The logo and opening films' first pictures match an independent decode; the boot films' sound level, channel order and timing match a PCSX2 recording | Original | 2026-10-13 |
| Interior entries | The scripted fight with and without them: same state on all 6,534 fields | Exact | 2026-10-18 |
| Interpreter | The scripted fight with every function interpreted against compiled: same state on all fields, same VU1 hash | Exact | 2026-10-18 |
| Runtime rules for the battle starter | The scripted fight before and after the change: same state on all 6,534 fields | Exact | 2026-10-18 |
| Idle demo fight | Plays (after the `VCLIP` fix) and returns to the title; run to flip 13,000 | Ran | 2026-10-18 |
| Battles without input | Characters 0 to 10 (each against the next) and 100 to 129 (against 0), variant 0, Rocky Area: each ran 4,500 flips, nothing interpreted. Characters from 130 are not valid | Ran | 2026-10-18 |
| Stages | Goku against character 1 on stages 0 to 15 (16 distinct stages): each ran 4,500 flips, nothing interpreted; stage numbers from 16 give Wasteland again | Ran | 2026-10-19 |
| Screens without input | The ten main-menu screens started by mode (`BT2_START_SCREEN`): Dragon Adventure intro, Ultimate Battle Z, Dragon Tournament, Dueling, Ultimate Training, Evolution Z, Item Shop, Data Center, Options, Dragon Library; each ran 1,200 flips, nothing interpreted | Ran | 2026-10-19 |
| Story episode starts | Episodes 0 to 24 started by `BT2_START_EPISODE`: 24 load their 3D map with their own lead character and ran 1,500 flips, nothing interpreted; episode 23 draws no 3D because its first scene's script is empty (the game's episode table marks it unavailable). From 25 the game reads past its tables (28 and up stop presenting frames). Story battles not reached | Ran | 2026-10-19 |
| Story scene scripts | All 621 scene files (33 to 653) decode with `story_scenes.py`; 25 episodes with 207 parts per the game's episode table; 193 parts have a map marker to their story scene and 7 go there without one (the other 7 are the unavailable episode 23) | Ran | 2026-10-19 |
| Story scene from a marker | Episode 0 part 0, marker 14 (`BT2_START_EPISODE=0,0,marker:14`): the map hands over to scene 34, the opening cutscene, which plays without input up to its first text box; SOUNDS request `0x80` (implemented from the module's code, unit-tested) answered; 3,000 flips, nothing interpreted | Ran | 2026-10-19 |
| Story parts without input | Episode 0 part 0: map, marker, cutscene with its text boxes, pre-battle screen, battle, "CLEAR!!", save questions answered without saving, next part's map (4,543 flips). Sample of 8 parts (episodes 0, 1, 6, 9, 13, 17, 21, 24, first part with a marker): every one reached and finished its battle, 1 cleared and 7 lost (the player's side has no input), nothing interpreted | Ran | 2026-10-19 |
| Story sweep | All 200 parts that can be started (`story_sweep.py`, the CPU on both sides, the 3 chases reported caught): every one reached and finished its battle, 73 cleared and 127 lost, nothing interpreted; the cleared ones answered the save questions without saving | Ran | 2026-10-19 |
| Story towns and shop | All 73 scenes optional markers lead to (`story_sweep.py --optional`): each ran to its end without input, nothing interpreted: 32 towns with their conversations and items (all seven Dragon Balls among them; the item shop: greeting, member's card, buy list, back out), 41 fights (14 cleared, 27 lost) | Ran | 2026-10-19 |
| Menu screens explored | `menu_explorer.py`: eight main-menu screens (all but Dueling and Dragon Adventure), up to 200 button paths each (1,476 runs), each screen reaching 958 to 1,154 functions, nothing interpreted. Found: Dragon Tournament's status query of memory card port 1 (now answered as an empty port; 200 more runs without a stop) and an Evolution Z hang that comes from the starters' missing New Game unlocks (the game's own search over an all-locked character list; after a real New Game the list has unlocked characters) | Ran | 2026-10-19 |
| New Game and Item Fusion | The starters run the title's New Game (the starting Z-items and unlocks): Evolution Z then opens as after a real New Game, and the menu explorer runs all eight screens without a stop (Evolution Z 1,064 functions). With the user's save copied in from its card folder: Item Fusion of two Support items (Master's Essence) and of a character item (item 450 owned afterwards, on a copy of the save without it), with the game's dialogue and animation; a wrong pair is refused | Ran | 2026-10-19 |
| Runtime rules for the screen starter | The scripted fight before and after the general SetVSyncFlag and INTC_STAT rules: same state on all 6,534 fields | Exact | 2026-10-19 |
| Frames | Demo fights, title dialogues, main and Dueling menus, character and map selection, loading screen, battle | User | 2026-10-13 |
| Sound | A WAV of menus and a fight | User | 2026-10-13 |
| Saves | The user's 100% save loads from a card folder | User | 2026-10-13 |

## Not validated yet

- Characters 11 to 99, variants 1 to 8, other character and stage pairings (left for the final validation); the screens beyond their first page (they wait for input); the story parts after a lost battle (the next part is started on its own instead), the chase itself (reported caught), how a town's fight block starts its fight (reached, not decoded), buying in the shop (0 Zeni); Dragon Balls inside a fight and stage destruction, special sagas unlocked by a fight's condition (the user's notes).
- 3D frames, menu frames, memory card replies and GS readback pixels against the original.
- Writing a save; special moves and ultimates in play; live sound in play.
