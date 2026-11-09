Online battles
==============

Two players on two PCs fight 1P vs 2P. One hosts, the other joins with the
host's code. Both need the same version of BT2 Recomp and their own copy of
the game in the "data" folder.

Host
----
1. bt2-launcher.exe: "Online battle", "Host a battle".
2. Your code (such as SC07-3ZKY) is copied by itself. Paste it (Ctrl+V)
   into a chat to the other player.
3. Wait: the battle starts when they join.

The first time, Windows may ask whether bt2-entry-probe.exe may use the
network: allow it.

Join
----
1. Copy the host's code from the chat.
2. bt2-launcher.exe: "Online battle", "Join with a code".
3. The code is already filled in: press Enter.

In the battle
-------------
Both games open on the Dueling screen with every fighter unlocked: choose
"1P vs 2P". The host is 1P, the one who joins is 2P; a line at the top says
which you are. Escape leaves the battle.

If it does not connect
----------------------
- The host's note says "Home network only": the router did not open the
  port by itself. Forward the UDP port it names to the PC it names on your
  router's page, then host again. If it says your provider shares your
  address, let the other player host instead.
- "different build" or "gameplay settings differ": both need the same
  version and the same "Beam struggles by buttons" setting (Enhancements).
  Everything else (HD, widescreen, 60 fps) may differ.
- "Connection slow" or frequent waits: the host raises "Input delay" under
  Online battle, Options.

If a battle stops with "the two games went apart", please report it with
both players' user\last-run.log.
