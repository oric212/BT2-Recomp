Your saves
==========

This guide explains where your saves are kept, how to back them up and move
them, and how to bring in a save from PCSX2 or a real PS2.


Where your saves are
--------------------
Saves are kept in the "saves" folder, next to bt2-game.exe.

Each folder inside "saves" is one memory card, for example "Card 1".
Inside a card there is one folder for each save. The game's save folder is
called BASLUS-21441DBZT2.

    saves\
      Card 1\
        BASLUS-21441DBZT2\     <- your Budokai Tenkaichi 2 save

The game saves to the card that is in slot 1. You choose that card in the
launcher under Settings, Game files, Memory card. That list also has
"New empty card" if you want to start fresh without losing anything.


Back up your saves
------------------
Close the game, then copy the whole "saves" folder somewhere safe.

To restore a backup, close the game and copy the folder back.


Move to a new version or another PC
-----------------------------------
1. Close the game.
2. Copy the "saves" folder from the old folder into the new one.
3. In the new version's launcher, check that Settings, Game files,
   Memory card shows the card you want.

Your settings can come along too: copy the "user" folder as well.


Bring in a save from PCSX2 or a PS2
-----------------------------------
The launcher can't do this yet. It lists PCSX2 card files in the saves
folder, but marks them "unsupported". For now there is a small helper
program that copies the save across. It needs Python.

One-time setup:

1. Install Python 3.10 or newer from https://www.python.org. During setup,
   tick "Add python.exe to PATH".
2. On the BT2 Recomp release page on GitHub, download "Source code (zip)" and
   unzip it. The helpers are in its tools\saves folder.
3. Open that tools\saves folder in File Explorer, click the address bar, type
   cmd and press Enter. A command window opens in that folder.

Then follow the section that matches where your save is.


From PCSX2
----------
PCSX2 keeps its memory cards in Documents\PCSX2\memcards. Slot 1's card is
usually Mcd001.ps2.

1. See which saves are on the card:

       python card_image.py "C:\Users\YOU\Documents\PCSX2\memcards\Mcd001.ps2"

   Your Budokai Tenkaichi 2 save shows up as BASLUS-21441DBZT2.

2. Copy it onto a card in BT2 Recomp's saves folder. This example makes a
   new card called "From PCSX2":

       python card_image.py "C:\Users\YOU\Documents\PCSX2\memcards\Mcd001.ps2" BASLUS-21441DBZT2 "C:\Games\BT2-Recomp\saves\From PCSX2"

   Use your own paths. Keep the quotes: they are needed when a path has
   spaces.

3. In the launcher, go to Settings, Game files, Memory card and choose
   "From PCSX2".

If your PCSX2 card is a folder instead of a .ps2 file, turn it into a file
card first: PCSX2 can convert it in its Memory Cards settings.


From a save file (.cbs)
-----------------------
Saves copied off a real PS2, or downloaded from a save website, often come
as a single file. CodeBreaker files (.cbs) can be brought in directly:

    python import_save.py "C:\Downloads\save.cbs" "C:\Games\BT2-Recomp\saves\Imported"

Then choose the "Imported" card in the launcher.

Other save file types (.psu, .max, .xps, .sps) can't be read yet. A memory
card tool such as mymc can put them onto a PCSX2 card file (.ps2) first.
Then follow "From PCSX2" above.


Good to know
------------
- The helpers never overwrite anything. If the card already has a
  BASLUS-21441DBZT2 folder, they stop and say so. Choose another card name,
  or move the old save out first.
- Close the game before you change anything in the saves folder.
- Saves made by BT2 Recomp are the same as the original game's saves.
  Moving them back to PCSX2 or a PS2 isn't supported yet.
