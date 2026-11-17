BT2 Recomp
==========

BT2 Recomp lets you play Dragon Ball Z: Budokai Tenkaichi 2 as a normal
Windows program. It is not an emulator: the game's own code has been turned
into a PC program.

The game itself is not included. You need your own copy.


What you need
-------------
- Windows 10 or 11, 64-bit.
- A graphics card with up-to-date drivers.
- A disc image (.iso file) of your own copy of the game. It must be the USA
  version, SLUS-21441. Other versions do not work; the game tells you if
  yours is not the right one.


Setting up
----------
1. Unzip this folder wherever you like, for example in Documents or in a
   Games folder. Don't put it in "Program Files".
2. Put your game's .iso file into the "data" folder.

That's all.


Playing
-------
1. Open bt2-launcher.exe.
2. If you like, look through Settings first. Then choose Play.

After the first time, you can open bt2-game.exe instead. It skips the
launcher and starts the game straight away with your settings.

Keys to know:
- Escape opens the menu during the game. On a PlayStation controller, the
  PS button does the same.
- Hold Tab to fast-forward.

Controllers that work: Xbox controllers, DualSense, DualShock 4, and most
USB or Bluetooth gamepads. 8BitDo pads work in D-input or Switch mode.

To change the buttons, open the launcher and go to Settings > Controls.

To play against a friend on the same PC, plug in a second controller.

To change the volume, go to Settings > Sound. Music, voices and effects each
have their own slider.


Your saves
----------
Your saves are kept in the "saves" folder.

When you update to a new version, copy the "saves" folder into the new
version's folder, and your saves come with you.

To bring over saves from PCSX2 or a PS2, read SAVES.txt.


Extras
------
You find all of these in the launcher's Settings. None of them changes how
the game plays.

- HD textures: put a PCSX2 texture pack for this game in its own folder
  inside "data\textures". Then switch on HD textures in Settings >
  Graphics.
- A sharper picture (super-sampling), widescreen, and 60 fps battles.
- Quick start: skips the logos, the opening film and the title screen, and
  goes straight to the main menu with your save loaded.
- Online battles against a friend on another PC: read ONLINE-SETUP.txt.


Known problems
--------------
This is an early version. Versus battles, the menus and saving have been
tested the most. The rest has been played less.

- Story mode has only been partly tested. Please tell us if something goes
  wrong in it.
- With 60 fps battles on, the destroyed Namek stage can flicker.
- With HD textures on, fighters under water are not shown in HD. A texture
  can also look wrong for a moment in the first fight.
- In online battles, the "press O to recover" message sometimes does not
  appear.


If something goes wrong
-----------------------
The file "user\last-run.log" describes what happened. Please attach it when
you report a problem.

If you see "This disc image is not Dragon Ball Z: Budokai Tenkaichi 2 (USA,
SLUS-21441)", your .iso is a different version of the game, or it is
damaged.


What is in this folder
----------------------
- bt2-launcher.exe: opens the launcher (settings, online battles, play).
- bt2-game.exe: starts the game straight away.
- bt2-entry-probe.exe: the game itself. The two programs above start it;
  you don't need to open it yourself.
- data: your game's .iso file and HD texture packs go here.
- saves: your memory cards.
- user: your settings and the log of the last run.
- assets: pictures the programs use.
- README.txt, SAVES.txt, ONLINE-SETUP.txt: these guides.
- LICENSE.txt, THIRD-PARTY-NOTICES.txt, SOURCE.txt: license details.
