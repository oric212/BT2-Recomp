// BT2_NET=host:<port> | join:<address>:<port>: an online battle in lockstep (host/net_lockstep.h,
// docs/plans/online/ONLINE_BATTLE_PLAN.md). BT2_NET_DELAY=<fields>: the host's input delay (default 2).
#pragma once

class EeRuntime;
class FramePacer;
class HostInput;
class PlayerSession;
class ViewerWindow;

// First thing in main: a session's fixed setup, as switches the rest of the probe reads. Both machines start fresh
// into the Dueling screen (BT2_START_SCREEN=30, the menus' sides kept) with every fighter unlocked, no card, the
// local player's view alone (host 1, joiner 2), fast forward and fast loading off. Refuses switches a session
// cannot keep (another start, a script or live input for port 2).
void prepareOnlineSession();
// Once the local controller is set up (on_pad_input, or BT2_PAD_SCRIPT in tool runs): connects to the other player
// and plays both ports from the session. With a player's window it shows the wait there (the address to give the other
// player, the router's port), with Cancel back to the launcher; tool runs wait 5 minutes as the host, 1 as the joiner.
// In a player's game the window shows the connection and any wait for the other player; with a pacer the fields keep
// their pace after a wait. BT2_NET_UPNP=1: the host opens its port on the router by UPnP (host/upnp_port.h).
// BT2_NET_SIM=latency=<ms>,jitter=<ms>,loss=<percent>: a worse network on this side (tests on one PC).
void startOnlineSession(EeRuntime& runtime,ViewerWindow* window,HostInput* input,PlayerSession* player,FramePacer* pacer);
