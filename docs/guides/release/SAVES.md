Your saves
==========

This guide explains where your saves are kept, how to back them up, how to
move them to a new version or another PC, and how to bring in a save from
PCSX2 or a real PS2.


Where your saves are
--------------------
Your saves are kept in the "saves" folder, next to bt2-game.exe.

Each folder inside "saves" is one memory card, for example "Card 1". Inside
a card there is one folder for each game's save. This game's save folder is
called BASLUS-21441DBZT2.

    saves\
      Card 1\
        BASLUS-21441DBZT2\     <- your Budokai Tenkaichi 2 save

The game saves to the card in slot 1. You choose that card in the launcher,
in Settings > Game files > Memory card. That list also has "New empty card",
if you want to start a new game without losing your old save.


Back up your saves
------------------
Close the game. Then copy the whole "saves" folder somewhere safe.

To restore the backup later, close the game and copy the folder back.


Move to a new version or another PC
-----------------------------------
1. Close the game.
2. Copy the "saves" folder from the old BT2 Recomp folder into the new one.
3. Open the new version's launcher. In Settings > Game files > Memory card,
   check that the card you want is chosen.

You can bring your settings along too: copy the "user" folder as well.


Bring in a save from PCSX2 or a PS2
-----------------------------------
The launcher can't do this by itself yet. It does list PCSX2 card files
that are in the "saves" folder, but marks them "unsupported".

For now, a small helper program copies the save across. It needs Python.

Setting up the helper (once):

1. Install Python 3.10 or newer from https://www.python.org. During the
   installation, tick "Add python.exe to PATH".
2. On the BT2 Recomp release page on GitHub, download "Source code (zip)"
   and unzip it. The helpers are in its "tools\saves" folder.
3. Open that "tools\saves" folder in File Explorer. Click the address bar,
   type cmd and press Enter. A command window opens in that folder.

Then follow the section below that matches where your save is.


From PCSX2
----------
PCSX2 keeps its memory cards in "Documents\PCSX2\memcards". The card in
slot 1 is usually called Mcd001.ps2.

1. See which saves are on the card. Type this into the command window
   (with your own user name instead of YOU):

       python card_image.py "C:\Users\YOU\Documents\PCSX2\memcards\Mcd001.ps2"

   Your Budokai Tenkaichi 2 save is listed as BASLUS-21441DBZT2.

2. Copy the save onto a card in BT2 Recomp's "saves" folder. This example
   makes a new card called "From PCSX2":

       python card_image.py "C:\Users\YOU\Documents\PCSX2\memcards\Mcd001.ps2" BASLUS-21441DBZT2 "C:\Games\BT2-Recomp\saves\From PCSX2"

   Use your own folders instead of the ones in the example. Keep the quote
   marks: they are needed when a folder name has spaces in it.

3. In the launcher, go to Settings > Game files > Memory card and choose
   "From PCSX2".

If your PCSX2 memory card is a folder instead of a .ps2 file, turn it into
a file first. PCSX2 can do that in its Memory Cards settings.


From a save file (.cbs)
-----------------------
Saves copied off a real PS2, or downloaded from a save website, often come
as a single file. CodeBreaker save files (.cbs) can be brought in directly:

    python import_save.py "C:\Downloads\save.cbs" "C:\Games\BT2-Recomp\saves\Imported"

Then choose the "Imported" card in the launcher.

Other kinds of save files (.psu, .max, .xps, .sps) can't be read yet. A
memory card tool such as mymc can first put them onto a PCSX2 card file
(.ps2). Then follow "From PCSX2" above.


Good to know
------------
- The helpers never overwrite anything. If the card already has a
  BASLUS-21441DBZT2 folder, they stop and tell you. Choose another card
  name, or move the old save somewhere else first.
- Close the game before you change anything in the "saves" folder.
- Saves made by BT2 Recomp are the same as the original game's saves.
  Moving them back to PCSX2 or a PS2 isn't supported yet.
