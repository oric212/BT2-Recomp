BT2 Recomp
==========

Dragon Ball Z: Budokai Tenkaichi 2 running natively on Windows.
This is not an emulator: the game's own code has been turned into a PC program.

The game itself is not included. You need your own copy.


What you need
-------------
- Windows 10 or 11 (64-bit).
- A graphics card with up-to-date drivers.
- A disc image (.iso) of your own copy of the game. It must be the USA version
  (SLUS-21441). Other versions do not work, and the game tells you so.


Setting up
----------
1. Unzip this folder somewhere you like, for example your Documents or
   Games folder. Avoid "Program Files".
2. Put your game's .iso file into the "data" folder.

That's it.


Playing
-------
1. Open bt2-launcher.exe.
2. Have a look through Settings if you like, then choose Play.

After the first time you can open bt2-game.exe instead. It skips the launcher
and goes straight into the game with your settings.

Useful keys:
- Escape (or the PS button on a PlayStation controller) opens the menu in the
  game.
- Tab fast-forwards.

Controllers: Xbox controllers, DualSense, DualShock 4, and most USB or
Bluetooth gamepads (8BitDo pads work in D-input or Switch mode). You can
change the buttons in the launcher under Settings, Controls. Plug in a second
controller to play against a friend on the same PC.

Volume: Settings, Sound has separate sliders for music, voices and effects.


Your saves
----------
Your saves live in the "saves" folder. To keep them when you update, copy
the "saves" folder into the new version's folder.

To bring saves over from PCSX2 or a PS2, read SAVES.txt.


Extras
------
All of these are in the launcher's Settings. None of them changes how the
game plays.

- HD textures: put a PCSX2 texture pack for this game in its own folder
  inside "data\textures", then switch on HD textures under Graphics.
- A sharper picture (super-sampling), widescreen and 60 fps battles.
- Quick start: skips the intro and goes straight to the main menu with your
  save loaded.
- Online battles against a friend on another PC: read ONLINE-SETUP.txt.


Known problems
--------------
This is an early version. Versus battles, the menus and saving have been
tested the most. The rest has been played less.

- Story mode has only been partly tested. Please tell us if something goes
  wrong.
- With 60 fps on, the destroyed Namek stage can flicker.
- With HD textures on, fighters under water are not in HD, and a texture
  can look wrong for a moment in the first fight.
- Online, the "press O to recover" prompt sometimes does not show up.


If something goes wrong
-----------------------
The file user\last-run.log says what happened. Please attach it when you
report a problem.

"This disc image is not Dragon Ball Z: Budokai Tenkaichi 2 (USA, SLUS-21441)"
means your .iso is a different version, or it is damaged.


What is in this folder
----------------------
- bt2-launcher.exe       opens the launcher (settings, online, play)
- bt2-game.exe           starts the game directly
- bt2-entry-probe.exe    the game itself; the two programs above start it
- data                   your game's .iso and texture packs go here
- saves                  your memory cards
- user                   your settings and the last run's log
- assets                 pictures the program uses
- README.txt, SAVES.txt, ONLINE-SETUP.txt   these guides
- LICENSE.txt, THIRD-PARTY-NOTICES.txt, SOURCE.txt   license details
