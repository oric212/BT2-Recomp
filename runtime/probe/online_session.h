// BT2_NET=host:<port> | join:<address>:<port>: an online battle in lockstep (host/net_lockstep.h,
// docs/plans/online/ONLINE_BATTLE_PLAN.md). BT2_NET_DELAY=<fields>: the host's input delay (default 2).
#pragma once

class EeRuntime;
class HostInput;
class ViewerWindow;

// First thing in main: a session's fixed setup, as switches the rest of the probe reads. Both machines start fresh
// into the Dueling screen (BT2_START_SCREEN=30, the menus' sides kept) with every fighter unlocked, no card, the
// local player's view alone (host 1, joiner 2), fast forward and fast loading off. Refuses switches a session
// cannot keep (another start, a script or live input for port 2).
void prepareOnlineSession();
// Once the local controller is set up (on_pad_input, or BT2_PAD_SCRIPT in tool runs): connects to the other player
// and plays both ports from the session. With a player's window it shows the wait there, with Cancel (back to the
// launcher); tool runs wait 5 minutes as the host, 1 as the joiner.
void startOnlineSession(EeRuntime& runtime,ViewerWindow* window,HostInput* input);
