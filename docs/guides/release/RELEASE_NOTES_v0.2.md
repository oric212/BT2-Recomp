# BT2 Recomp v0.2

*Dragon Ball Z: Budokai Tenkaichi 2* (PS2, USA, SLUS-21441) as a native Windows program. The game's own code is recompiled for the PC; it is not an emulator.

**You need your own copy of the game.** Nothing of the game is in this download: put your USA disc image (`.iso`) into the `data` folder.

## New in v0.2

- **Voices fixed:** spoken lines (the announcer and the characters' dialogue) were far too quiet compared with the music and effects. They are now mixed the way the original game's sound driver mixes them, each at the volume and position the game asks for.
- **A separate volume for voices:** Settings > Sound now has Master, Music, Voices and Effects.
- **More controllers:** the DualShock 4 (by USB, Bluetooth or Sony's wireless adapter) and other USB or Bluetooth gamepads, such as 8BitDo pads in D-input or Switch mode, now work alongside Xbox controllers and the DualSense. Two players on one PC can each use any of them, and every button can be changed.
- **Films** are decoded with exactly the same arithmetic as PCSX2.
- **Checked against the original:** the port was compared step by step with the original game running in PCSX2. The fight's logic and graphics, the first menus and the game's requests to its services match. It has also been played on several PCs, and online between two homes.

## Updating from v0.1

Unzip v0.2 into a new folder and copy your `saves` folder (and `data` with your disc image and texture packs) from the old one. Your settings from v0.1 work as they are; Voices starts at 100%. For online battles both players need v0.2.

## Install and play

1. Download `BT2-Recomp-v0.2.zip` and unzip it anywhere (not inside Program Files).
2. Put your game's `.iso` into the `data` folder.
3. Start `bt2-launcher.exe`, look through Settings, choose **Play**.

Windows 10 or 11 (64-bit) and a graphics card with Vulkan drivers. `README.txt` in the download has the details; `ONLINE-SETUP.txt` explains online battles.

## Known issues

- Story mode has been partly tested.
- 60 fps battles: the destroyed Namek stage can flicker.
- HD textures: fighters under water are not in HD; a texture can briefly look wrong in the first fight.
- Online: the "press O to recover" prompt is sometimes not shown.

If the game stops, `user\last-run.log` has the messages; please include it in a report.
