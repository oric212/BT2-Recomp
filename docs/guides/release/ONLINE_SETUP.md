Online battles: setup
=====================

Two players on two PCs fight 1P vs 2P over the internet (or a home network).
One player hosts, the other joins with a code.

Both players need
-----------------
- The same version of BT2 Recomp (the same download). Different versions
  refuse each other and say so.
- Their own copy of the game in the "data" folder.
- The same "Beam struggles by buttons" setting (Enhancements). Everything else
  (HD textures, widescreen, 60 fps, super-sampling) may differ: each player
  keeps their own.

The host
--------
1. Start bt2-launcher.exe, choose "Online battle", then "Host a battle".
2. The first time, Windows Firewall may ask whether bt2-entry-probe.exe may use
   the network: allow it (at least on private networks).
3. The screen shows a code such as SC07-3ZKY. Highlight it and press
   Enter (or Cross) to copy it, then send it to the other player (any chat).
4. Wait. The battle starts by itself when the other player joins.

The screen also shows whether your router opened the port:

- "Port 7411: opened": nothing more to do.
- "Port 7411: forward it by hand": your router did not open it by itself (UPnP
  is off or not supported). Open your router's settings page and forward
  UDP port 7411 to the address shown under "Home code" (this PC). Then
  host again.
- "Code: none, the address is shared": your internet provider shares one
  internet address among many customers (carrier-grade NAT). Other players
  cannot reach you, so let the other player host instead.

The player who joins
--------------------
1. Start bt2-launcher.exe, choose "Online battle", then "Join with a code".
2. Paste the code (Ctrl+V) or type it, then press Enter.
3. The battle starts as soon as the host answers. If nothing happens after
   10 seconds, check the code, that the host is waiting, and that the host's
   port is open.

On the same home network, use the host's "Home code" instead.

In the battle
-------------
- Both games start in the Dueling screen with every fighter unlocked. Choose
  "1P vs 2P".
- The host is 1P: the top row on the character select, the fighter on the
  left, and the one who moves through the menus. The other player is 2P:
  the bottom row, the fighter on the right. A line at the top of the screen
  says which you are while you are in the menus.
- In the fight each player sees their own fighter, full screen.
- Escape leaves the battle (the other player is told), and so does closing
  the window. Fast forward and the in-game menu are not available online.

If the connection is slow
-------------------------
A line says "Connection slow", or the game waits with "Waiting for the
other player". The host can raise "Input delay" under Online battle, Options
(Auto chooses it from the connection; each step is 1/60 s more between
pressing a button and the game playing it, and fewer waits).

If a battle ends with a message
-------------------------------
- "the other player left the session": they closed the game or pressed Escape.
- "no word from the other player for 5 s": the connection broke.
- "the two games went apart": the two games no longer match (this should not
  happen; please report it with both players' user\last-run.log).
- "different build" or "gameplay settings differ": see "Both players need".

Options (Online battle, Options)
--------------------------------
- Port: 7411 unless you need another one; a code carries the port with it.
- Input delay: Auto, or 1 to 8 fields.
- Open the port by UPnP: on by default; the port is closed again when the
  battle ends.
