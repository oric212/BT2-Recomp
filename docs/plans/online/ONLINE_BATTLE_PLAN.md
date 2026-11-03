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
- **Game patches change memory** (the widescreen patch: from field 1), so both machines need the same patches.
- **The pad seam.** The pad service asks for each port's input once per field (`EeRuntime::on_pad_input`, `on_pad2_input`). Lockstep supplies both ports' input for each field on both machines.
- **Guest threads run on Windows fibers** (`ee/retained_ee_stack.cpp`), the model bt3r's rollback snapshots needed; BT2's port has no save states.
- **Not there yet:** a second controller in a player's session (port 2 is answered as empty outside tool runs), so local two-player is a first step too.

## Proposed way

1. **Local two-player.** Port 2 connected in the player's game, with the second controller (or keyboard half) mapped in the launcher and the in-game menu. The game's own "1P vs 2P" then works as on the console. Checked by playing.
2. **Lockstep online, from the game's own menus.** Both machines start a fresh session: the joiner receives the host's memory card (unlocks and options, used for the session only, never written to the joiner's cards) and the host's game patches, then both boot in lockstep from field 0. Port 1 is the host's controller, port 2 the joiner's; the players pick "1P vs 2P" and their fighters on the game's own screens. Per field each side sends its input (with the last 8 repeated) and waits for the other's input of that field minus the delay; the pacer's wait becomes that wait. Input delay chosen from the measured round trip (16.7 ms a field). Every 60 fields both sides exchange a hash of the game state without the readback buffers; a mismatch stops the session with a clear message (no faked continuation). Fast forward and fast loading are off in a session. Host and join from the launcher (host: a port; join: address and port), direct UDP.
   Checks: two probes on one PC over loopback with scripted pads, their state hashes equal every field; dropped and delayed packets (a test transport); a forced difference caught by the hash; then the user's two machines.
3. **Getting through home routers.** Direct UDP needs the host's port forwarded. UPnP port mapping on the host, or a relay server, is a later step.
4. **Rollback.** Save states of the whole game (guest memory, fibers' stacks, every native service's state), taken every field and restored to re-run with the right input. It hides the input delay over long distances; it is large work, after lockstep has proven the game stays in step.

## Decisions for the user

- **Decision: how a match is set up.** (a) The game's own menus from boot with the host's save (proposed: the game's own screens, nothing injected), or (b) a direct start into the battle with fighters and stage chosen in the launcher (`BT2_START_BATTLE`'s way, now tool-only; quicker to build, but it skips the game's screens and needs a rule for which fighters are unlocked).
- **Decision: how players connect.** Direct address and port (proposed first), UPnP, or a relay server someone has to run.
- **Decision: rollback** after lockstep, or lockstep only.
