# Beam struggle by buttons

The user's request (2026-11-02): in a beam struggle, mash Square or Cross instead of rotating the sticks, with the
on-screen prompt showing Square/Cross; a setting, on by default. The original controls stay available.

**Built 2026-11-03:** the setting "Beam struggles by buttons" (Enhancements, on by default, from the next start; the
user's choices the same day: one button, Square, or X with the Xbox button art, the same button;
`struggle_buttons` in the settings file, `BT2_STRUGGLE_BUTTONS=1` and `BT2_TEXTURE_OVERRIDES` for tool runs). Not yet
seen by the user in play.

## How the game measures a struggle (found 2026-11-03)

- **Input to the fighter.** Each frame 0x207630 fills a fighter's input: for a fighter a controller plays, the
  buttons from that port's record of the pad library (`0x3B0B00 + 0x180 × port`, buttons at `+0x12C`) into fighter
  `+0x20` (the previous frame's into `+0x44`), and the four sticks (through 0x28C520 and 0x28C598, dead zone 50) as
  floats at fighter `+0x08` to `+0x14`; the previous frame's values move to `+0x24` to `+0x43`. For a CPU fighter
  the AI (0x1214F8) gives both. Whether the CPU plays a side is the byte the side setter writes (system structure,
  pointer at 0x3876A0, `+0xE0 + 0x1D0 × side + 0x1C4`).
- **The push.** The struggle's state (0x22FFC8) calls 0x217E58 once a frame for each fighter: for each stick whose
  current and previous positions are both longer than 0.5, the angle between them (dot product, arccosine), counted
  only below 90 degrees; the result is the larger of the two sticks' angles in radians. The state stores it at
  fighter `+0xA38` and scales it by its modifiers. Spinning either stick pushes.
- **The prompt** is an animation of six 64 x 64 frames of a turning stick inside a starburst (texture names in
  `tools/graphics/textures/make_struggle_button_art.py`), found by naming every texture drawn before and during a
  scripted struggle.

## What the setting does

- `ee/struggle_buttons.*` wraps 0x217E58 (installed only when its code hashes as expected): the generated function
  runs as always; for a fighter a controller plays, its result is raised to what new presses of Square give.
  A press is worth 2.4 radians of stick turn (about 0.38 of a turn: steady mashing at 7 a second matches turning a
  stick about 2.7 times a second), paid out at most 1.5 radians a frame (the original's largest result). CPU fighters
  and the sticks are unchanged. Tests: `struggle_buttons_tests.cpp`.
- The prompt: the port's own art (one face button, Square, or X with the Xbox button art, pressed and released in
  turn), drawn by `tools/graphics/textures/make_struggle_button_art.py` into `assets/struggle-buttons/`, copied next
  to the programs by the build. The game program passes the folder as `BT2_TEXTURE_OVERRIDES`: with an HD pack its
  files replace the pack's of the same names; without one they are the only replacements (texture naming then runs
  for them). The six files are loaded at the start, so the art is there at the prompt's first draw.
- Measured on a scripted struggle (both fighters Goku, both fire at once, player 1 pressing Cross every 8 fields; Cross counted then, Square only since):
  with the setting player 1 pushes 93.6 radians over 78 of 159 frames and wins; without it the same input pushes
  nothing and the other fighter wins.

## Test tools made for it (tool runs only; refused in a player's session)

- Pad scripts and `BT2_PAD_RECORD` hold stick positions (`lx=HH` and so on).
- A second controller: `BT2_PAD2_SCRIPT` (a pad script for port 2) or `BT2_PAD2_LIVE=gamepad|keyboard` (the same
  gamepad input as port 1, or the gamepad in port 1 and the keyboard in port 2); recorded beside `BT2_PAD_RECORD` as
  `<file>.pad2.txt`. Port 2 then answers as a connected DualShock 2 and the socket table's second word is set at
  detection (what brings the pad library's port 2 record to its ready state).
- `BT2_START_BATTLE_PLAYER=1|2` (side 0, or both sides, played by controllers), `BT2_START_BATTLE_CPU_LEVEL` (side 1's
  CPU level), `BT2_START_SCREEN_PLAYERS=1` (a screen's battles keep the sides the menus chose).
- The call trace prints `v0` and takes `a0=<hex>` (every call with that first argument).

## Two-player supers (2026-11-03)

The user could not fire supers in a two-player fight. Measured: a super needs fighter flag word 0 bit 1, which
0x20DBC0 sets only when the opponent is within lock-on range: the distance (0x210A20; 176 with the fighters apart)
against the fighter's range (0x20DB08: command object `+0xFAC` plus 0x210D50; 72.4 in the measured fight). The HUD
shows the same state ("Lock on!", the radar). The user expects supers from afar: whether the original fires them
out of lock-on range is still to be checked (the user's knowledge, or PCSX2). With the fighters close, the Kamehameha fires in a two-player fight as in
a one-player one; holding L2 charges ki, and holding it on spends a blast stock on the aura. Not a port fault as far
as measured; not compared with PCSX2.

## Left

- The user's play with the setting: the feel of the press value (2.4 radians), the art.
- A struggle against the CPU in play (the scripted one is two players).
