BT2 Recomp v0.1
===============

A native Windows port of Dragon Ball Z: Budokai Tenkaichi 2 (PS2, USA, SLUS-21441).
No emulator: the game's own code, recompiled for the PC.
You need your own copy of the game: nothing of the game comes with this download.

What you need
-------------
- Windows 10 or 11, 64-bit
- A graphics card with up-to-date drivers (Vulkan)
- Your own disc image of the game, USA release (SLUS-21441), as an .iso file.
  Other regions and revisions do not work: the game says so when it starts.

Install
-------
1. Unzip this folder anywhere you like (not inside Program Files).
2. Put your game's .iso file into the "data" folder.

Play
----
1. Start bt2-launcher.exe.
2. Look through Settings (graphics, controls, sound), then choose "Play".
   Next time you can start bt2-game.exe: it goes straight into the game
   with your settings.

- Controls: keyboard or gamepad (Xbox controllers and DualSense).
  See and change the buttons in the launcher under Settings, Controls; a second
  controller plays 2P on the same PC.
- In the game, Escape (or the PS button) opens the in-game menu.
- Tab fast-forwards.
- Saves: memory cards are kept in the "saves" folder, one folder per card.
  Keep this folder when you update to a newer version.

Optional extras (launcher, Settings)
------------------------------------
- HD textures: put a PCSX2 texture pack for this game into "data\textures",
  in a folder of its own (data\textures\<pack name>\replacements\...),
  then switch on "HD textures" under Graphics.
- Sharper picture (super-sampling), widescreen, 60 fps battles, Quick start
  (straight to the main menu with your save).
  None of them changes how the game plays.
- Online battles with a friend on another PC: see ONLINE-SETUP.txt.

Known issues
------------
This is a first version. Versus battles, the menus and saving are well tested;
the rest has had less play.

- Story mode has been partly tested: report anything that goes wrong.
- 60 fps battles: the destroyed Namek stage can flicker.
- HD textures: fighters under water are shown without HD textures;
  a texture can briefly look wrong in the first fight.
- Online: the "press O to recover" prompt is sometimes not shown.
  Online battles are 1P vs 2P on the Dueling screen; both players need the
  same version.

If the game stops
-----------------
user\last-run.log has the messages: please include it when you report a problem.
"This disc image is not Dragon Ball Z: Budokai Tenkaichi 2 (USA, SLUS-21441)"
means another region or revision, or a damaged image.

Files in this folder
--------------------
- bt2-launcher.exe, bt2-game.exe   the programs you start
- bt2-entry-probe.exe              the game itself (started by the two above)
- data, saves, assets              your disc and textures, your cards, the launcher's pictures
- user                             your settings and the last run's messages (made on first start)
- LICENSE.txt, THIRD-PARTY-NOTICES.txt, SOURCE.txt
