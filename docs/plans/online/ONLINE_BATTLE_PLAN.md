# Online battle

Asked by the user on 2026-11-03: a way to make the two-player battle an online battle. This plan records what was found and proposes a way; the choices marked **Decision** are the user's.

## What the references show

- **BT3 on the PS2 has no online play.** Its main menu keeps a hidden "Dragon Net Battle" row whose handler only plays a sound; its `DragonNet_*.pak` files are 64-byte stubs, and the executable has no network code (`work/references/bt3r/docs/NETMENU.md`). The online play BT3 is known for came with the Wii release, not with the PS2 game BT2 shares its engine with.
- **The BT3 port (bt3r) built its own netplay** (`work/references/bt3r/ps2xRuntime/src/lib/ps2_netplay.cpp`, read for the approach only):
  - **Lockstep over UDP.** Only pad input crosses the network (6 bytes a frame); both machines run the whole game. Each packet repeats the last 8 frames' input, so a lost datagram heals with the next one. An input read on frame F is applied on frame F + delay (1 frame on loopback, 2 on a LAN).
  - **Frame numbers relative to the connection,** because two machines connecting mid-session are at different frame counts.
  - **The host decides the match setup** and stamps it into every packet.
  - **Checksums** of the game state every 60 frames to detect a desync.
  - **Rollback** (a missing remote input predicted, the game rewound and re-run when it was wrong), which needed save states of guest memory, devices, kernel records and every guest fiber's stack; and a **state sync at connect** that sends the host's state over TCP.
  - **Their menu entry was retired** (`docs/NEW-NETMENU.md`): routing the hidden row into the game's own Duel state while a custom page owned the screen and the pad could not both hold; the transport stayed.

## What BT2's port already gives (measured 2026-11-03)

- **Determinism.** Two runs of the CPU fight with the window, sound on the audio device, fast loading, pacing and 60 fps agree on all 3,385 fields of the state hash (EE RAM, scratchpad, VU memory). Timers are logical (counted in fields), sound streams advance per field (`services/cri_audio.h`), no real-time clock is read; the host clock only stamps memory card files.
- **Graphics settings do not change the fight.** Between 16x with HD and 1x without, the state differs from flip 173 only in the two 340 KB buffers the game reads back from the GS; at field 3,000 everything else is equal. A desync check must leave out memory written by GS readbacks.
- **Game patches change memory** (the widescreen patch: from field 1), **but not the fight.**
- **The pad seam.** The pad service asks for each port's input once per field (`EeRuntime::on_pad_input`, `on_pad2_input`). Lockstep supplies both ports' input for each field on both machines.
- **Guest threads run on Windows fibers** (`ee/retained_ee_stack.cpp`), the model bt3r's rollback snapshots needed; BT2's port has no save states.
- **Not there yet:** a second controller in a player's session (port 2 is answered as empty outside tool runs), so local two-player is a first step too.

## The user's requirement (2026-11-03): each player keeps their own patches and save

Nothing is transferred: each machine keeps its own game patches, enhancements and memory card. Lockstep only needs the **fight** to be computed alike, not all of memory, so this was measured with the fight's mixed audio, which comes only from the game's own sound commands (every hit, voice and move at its moment) and is byte-identical exactly when the fight goes the same way:

- **Widescreen patch on against off** (the CPU fight, `BT2_START_BATTLE=0,1,0`, 3,000 flips, 106 s of audio): byte-identical. The memory that differs is drawing data: the projections (577.3 against 433, the 4:3 to 16:9 factor), the display lists, the GS readback buffers.
- **The user's own save copied in against a new game's data:** byte-identical.
- **Graphics settings** (16x with HD against 1x), **fast loading, 60 fps, sound on the device:** only the readback buffers differ (above).
- **The control:** the opponent's CPU level 3 instead of 2 changes the audio from 26.8 s on, so the audio does follow the fight.

**So it is possible, with one condition: both machines enter the match the same way, from a fresh start,** not through each player's own menus and save: the game takes random seeds when the battle loads (from Timer 2, counted in fields since the boot), and different menus, saves or loading would give different seeds and a different heap.

**The user's choice (2026-11-03): the game's own screens, everything unlocked.** Picking fighters outside the game would mean one place setting up both sides; instead both machines start fresh straight into the Dueling screen (mode 30, as the tool `BT2_START_SCREEN=30` does: no card check, logos or films) and run in lockstep from there, so the players choose "1P vs 2P", their fighters on the game's own select screen (both cursors, as on the console), the stage and the settings, each with their own controller. The select screen is built from the save's unlocks, which must be alike on both machines: every online match has every fighter unlocked, set in memory for the session, whatever either save holds; nothing of either save crosses the network, and neither card is written in a session (the roster's records: below).

- **Desync check without the whole state:** a hash of the game's sound commands (or of the fight's own state once its addresses are known), since memory hashes differ by design between players' settings.
- **Patches that change gameplay** (none today; the widescreen patch is drawing only) would have to match; each patch is marked as drawing or gameplay, and a session refuses mismatched gameplay patches.
- **Not covered yet:** fighters customized with Z-items in versus (their equipment would cross as part of the choice, a few bytes, if the direct start takes it).

**The roster (found 2026-11-03 with the user's 100% save, a CodeBreaker file unpacked as mymc reads it):** the save data's 152 records of four bytes at +0x25A8 decide which fighters can be chosen: a new game has (3,1) for those it starts with and (0,0) for the rest, the completed save (1,1) for all. A new game's save with only those records taken from the completed one shows the completed save's character select on all 19 rows of seven (pictures compared; the new game's differ on every row). Built: `BT2_UNLOCK_ROSTER=1` sets them after the fresh start's New Game (`probe/game_starter.cpp`); no card is loaded in a session, so neither player's save takes part at all.

**The game's own screens with different settings (2026-11-03):** two runs into the Dueling screen with the roster unlocked, both ports scripted alike into 1P vs 2P, the select screens and a fight (80 s): one with widescreen, 16x and HD, one without: byte-identical audio.

**Each player full screen (the user: no split screen online; built 2026-11-03,** `ee/online_view.*`, `GsCommandSink::hidden_view`, `BT2_ONLINE_VIEW=1|2`): the game's view setup (0x25BB38; BT3's is the same function, `work/references/bt3r/.../game_overrides.cpp`, `[netview]`) splits when the view's +0x258 is set, mode 1 the left half and any other the right. In a session both halves are set up as the full screen on both machines alike, each marked by its player in its scissor's top row; the renderer empties the other player's scissor in a copy of the GS data, so that view draws nothing. Measured: the fight's audio byte-identical as player 1, as player 2 with widescreen, and split screen; the pictures show each player's own view over the whole screen. At first both players' radars stayed where their halves had them (the HUD carries no mark): the fights' radars (0x1172C8, found by a store watch on the radar's GS packet in the display list) are left out on both machines (the user: hide them); the audio stays byte-identical.

## Proposed way

1. **Local two-player.** Port 2 connected in the player's game, with the second controller (or keyboard half) mapped in the launcher and the in-game menu. The game's own "1P vs 2P" then works as on the console. Checked by playing. **Built 2026-11-03:** the setting "Player 2 controller" (Controls, off by default, from the next start) connects port 2 in the player's game: with two gamepads or more the second plays port 2, with one gamepad the keyboard does (`assignPlayers`, `host/input_mapping.h`; `HostInput::gamepads`, `PlayerSession::pad2Input`); the in-game menu and fast forward stay player 1's. Tests: the assignment and the setting's text form. Not yet played by the user.
2. **Lockstep online, from a fresh start into the Dueling screen.** Host and joiner connect in the launcher; both machines start fresh straight into the Dueling screen with every fighter unlocked, each with its own patches and enhancements, and run in lockstep from field 0 there. Port 1 is the host's controller, port 2 the joiner's. Per field each side sends its input (with the last 8 repeated) and waits for the other's input of that field minus the delay; the pacer's wait becomes that wait. Input delay chosen from the measured round trip (16.7 ms a field). Every 60 fields both sides exchange a hash of the fight (the sound commands so far); a mismatch stops the session with a clear message (no faked continuation). Fast forward and fast loading are off in a session. Host and join from the launcher (host: a port; join: address and port), direct UDP.
   Checks: two probes on one PC over loopback with scripted pads and different patches and settings, their fight hashes equal; dropped and delayed packets (a test transport); a forced difference caught by the hash; then the user's two machines.
3. **Getting through home routers.** Direct UDP needs the host's port forwarded. UPnP port mapping on the host, or a relay server, is a later step.
4. **Rollback.** Save states of the whole game (guest memory, fibers' stacks, every native service's state), taken every field and restored to re-run with the right input. It hides the input delay over long distances; it is large work, after lockstep has proven the game stays in step.

## Next: the lockstep transport (designed 2026-11-03, not built; the pass ended at the user's request)

- **Where:** `runtime/host/net_lockstep.*` (Winsock UDP), driven by the probe: `BT2_NET=host:<port>` or `join:<address>:<port>`, `BT2_NET_DELAY=<fields>` (2 to start; 16.7 ms each). A session implies the fresh start into Dueling (`BT2_START_SCREEN=30`), `BT2_UNLOCK_ROSTER=1`, `BT2_ONLINE_VIEW` = the local player (host 1, joiner 2), no card, fast forward and fast loading off.
- **Connecting:** the host waits on its port; the joiner sends a hello every 200 ms until answered. The hello carries a hash of the running program, so two different builds refuse each other by name. Direct address and port (the user's choice): the host forwards the port on their router.
- **Each field:** the pad service asks port 2 then port 1 once per VBlank (`EeRuntime::on_pad2_input`, `on_pad_input`, `services/pad_service.cpp`). The first call of a field does the exchange: the local controller's input (the player's `PlayerSession::padInput`, or a pad script in tests) is recorded for field + delay and sent with the last 8 inputs repeated; then the peer's input for this field is awaited (resending while waiting; a silence of a few seconds stops the session with a clear message). Port 1 is the host's input, port 2 the joiner's; fields before the delay have neutral input. The field number is `vblank_deliveries`, the same on both machines.
- **Drift check:** every 60 fields both send a hash of every native service request so far (`EeRuntime::callNativeService` in `services/sif_rpc.cpp`: the requests' bytes); settings and patches do not change them (measured: the fight's audio is identical across them). A mismatch stops the session by name; the whole-memory state hash cannot be used (it differs by settings by design).
- **Checks:** two probes on this PC over loopback with scripted inputs and different settings: both reach the same fight with byte-identical audio; packets dropped and delayed by a test transport; a forced difference caught by the hash; then the launcher's Host and Join (a text entry for the address, which the menu does not have yet) and the user's two machines.

## Decisions for the user

- **Decided by the user (2026-11-03):** no save or patches transferred; the game's own Dueling and select screens, entered fresh on both machines; every fighter unlocked in online matches.
- **Decided by the user (2026-11-03): direct address and port first** (the host forwards a port); UPnP or a relay later if wanted.
- **Decision: rollback** after lockstep, or lockstep only.
