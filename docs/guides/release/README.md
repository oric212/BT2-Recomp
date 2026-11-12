BT2 Recomp
==========

A native Windows port of Dragon Ball Z: Budokai Tenkaichi 2 (PS2, USA, SLUS-21441).
You need your own copy of the game: nothing of the game comes with this download.

Requirements
------------
- Windows 10 or 11, 64-bit
- A graphics card with up-to-date drivers (Vulkan)
- Your own disc image of the game (USA release, SLUS-21441), as an .iso file.
  Other regions and revisions do not work: the game says so when it starts.

Getting started
---------------
1. Unzip this folder anywhere you like (not inside Program Files).
2. Put your game's .iso file into the "data" folder.
3. Start bt2-launcher.exe. Check the settings, then choose "Play".
   From then on, bt2-game.exe starts the game directly with your settings.

The launcher's Settings hold the graphics, the controls (keyboard and gamepads,
including a second controller for 1P vs 2P on one PC), the sound and the game's
files (disc, memory card).
In the game, Escape (or the PS button) opens the in-game menu.

HD textures (optional)
----------------------
Put a PCSX2 texture pack for this game into "data\textures", in a folder of
its own: data\textures\<pack name>\replacements\...
Then switch on "HD textures" under Settings, Graphics in the launcher.

Saves
-----
Memory cards live in the "saves" folder, one folder per card. Keep it when
you update to a newer version.

Online battles
--------------
Play 1P vs 2P with a friend on another PC: see ONLINE-SETUP.txt.

Files in this folder
--------------------
- bt2-launcher.exe, bt2-game.exe   the programs you start
- bt2-entry-probe.exe              the game itself (started by the two above)
- data, saves, assets              your disc and textures, your cards, the launcher's pictures
- user                             your settings and the last run's messages (made on first start)
- LICENSE.txt, THIRD-PARTY-NOTICES.txt, SOURCE.txt

If the game stops, user\last-run.log has the messages. "This disc image is not
Dragon Ball Z: Budokai Tenkaichi 2 (USA, SLUS-21441)" means another region or
revision, or a damaged image.
