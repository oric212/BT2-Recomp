// Online battles in lockstep (docs/plans/online/ONLINE_BATTLE_PLAN.md): both machines run the whole game and only
// the controllers' input crosses the network. Each field the local input is sent for the field `delay` later, and
// the field waits for the other player's input of that field; port 1 plays the host's input, port 2 the joiner's.
// Every 60 fields both sides compare a hash of what the game asked the native services so far (the same on both
// machines while the fight goes alike, whatever each player's settings and patches); a difference, silence or the
// other player leaving stops the session with an error that says so.
#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "host/connection_code.h"
#include "services/devices/pad_input.h"

// Datagrams to and from the other player.
class LockstepTransport {
public:
    virtual ~LockstepTransport()=default;
    virtual void send(std::span<const std::uint8_t> packet)=0;
    // The next packet from the other player, or nothing once `timeout` has passed.
    virtual std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout)=0;
    // This side as the internet sees it: its IPv4 address and port from STUN, and its IPv6 address with the socket's
    // port (IPv6 has no address translation); empty without STUN. `findingAddresses`: STUN is still being asked.
    virtual std::vector<connection_code::Address> publicAddresses() const {return {};}
    virtual bool findingAddresses() const {return false;}
    // Host: sends to the joiner's addresses until the joiner is heard, so the host's router and firewall let the joiner's
    // packets in (hole punching; the joiner's reply code gives the addresses). An IPv4 port is punched with the ports
    // around it, for routers that give each destination a new port.
    virtual void punch(const std::vector<connection_code::Address>&) {}
};

// UDP over Winsock. The host listens on `port` and answers whoever greets it first; the joiner sends to the host.
// `stun`: the socket asks public STUN servers for its address as the internet sees it (publicAddress), for the codes
// the players exchange when no port is open (docs/plans/online/ONLINE_BATTLE_PLAN.md, "Without an open port").
// IPv4 and IPv6 on one socket; the joiner greets every address of the host's code until one answers.
// `hard` (tests): this side behaves as behind a router that changes ports, without asking STUN.
std::unique_ptr<LockstepTransport> hostUdp(std::uint16_t port,bool stun=false,bool hard=false);
std::unique_ptr<LockstepTransport> joinUdp(const std::vector<connection_code::Address>& host,bool stun=false,bool hard=false);
inline std::unique_ptr<LockstepTransport> joinUdp(const std::string& address,std::uint16_t port,bool stun=false) {return joinUdp({{address,port}},stun);}

// A worse network in front of a transport, for measuring on one PC (BT2_NET_SIM): each packet received arrives
// `latency` later, give or take up to `jitter` (in order, as most links keep it), and one in `loss_every` is lost
// (0: none). Applied on both machines it makes a round trip of twice the latency.
struct NetworkSimulation {std::chrono::milliseconds latency{0},jitter{0};unsigned loss_every=0;};
// "latency=<ms>,jitter=<ms>,loss=<percent>"; any part may be left out. Throws on other text.
NetworkSimulation parseNetworkSimulation(std::string_view text);
std::unique_ptr<LockstepTransport> simulateNetwork(std::unique_ptr<LockstepTransport> inner,const NetworkSimulation& simulation);

struct LockstepOptions {
    bool host=true;
    unsigned delay=0;          // The host's input delay in fields, 1 to 8; 0: chosen from the round trip measured on connecting.
    std::uint64_t program=0;   // The running program's hash: two different builds refuse each other.
    std::uint32_t rules=0;     // Settings that change the fight, as bits: two players must have them alike.
    std::string rule_names;    // Those settings, named in the refusal.
    std::chrono::milliseconds silence{5000}; // No word from the other player for this long ends the session.
    // While a field waits for the other player: called every 100 ms with how long it has waited (to keep a window
    // answering and say why the game stands still). It may throw to end the session.
    std::function<void(double seconds)> waiting;
};

class Lockstep {
public:
    static constexpr unsigned max_delay=8,check_interval=60,rtt_samples=6;
    Lockstep(LockstepTransport& transport,const LockstepOptions& options);
    ~Lockstep(); // Tells the other player the session ended (as leave).
    // Host: waits for a joiner's greeting; joiner: greets every 100 ms, measures the round trip over the host's answers,
    // and starts the session with the host's delay or one chosen from it. False when `timeout` passed first (call again
    // to go on waiting); throws on a different program or gameplay settings.
    bool connect(std::chrono::milliseconds timeout);
    // One field: `local` is played `delay` fields later; `check` is this field's drift hash (compared every 60
    // fields). Returns the input of ports 1 and 2 for this field, waiting for the other player's if needed.
    std::array<PadInput,2> field(const PadInput& local,std::uint64_t check);
    // Tells the other player this side left; the session is over.
    void leave();
    // While this side stands still (its in-game menu is open): the other player keeps hearing from it (its game waits
    // and says so instead of ending the session after its silence), and this side takes the other's packets, so it
    // learns when they leave. Call it every 100 ms or so. Throws as `field` does when the session ends.
    void keepAlive();
    // The input delay for a round trip and its spread: the other player's input of a field must arrive before the
    // field, with a field to spare for the two games' pacing.
    static unsigned delayFor(double round_trip_ms,double spread_ms);
    unsigned delay() const {return delay_;}
    bool host() const {return options_.host;}
    unsigned fieldsPlayed() const {return field_;}
    unsigned checksCompared() const {return checks_compared_;}
    std::uint64_t waits() const {return waits_;}           // Fields that had to wait for the other player.
    double waitedSeconds() const {return waited_;}
    double lastWaitSeconds() const {return last_wait_;}    // The latest field's wait (0 when it did not wait).
    double roundTripMs() const {return rtt_ms_;}           // Smoothed; measured on connecting, then from the inputs packets.
private:
    void sendWelcome(std::uint32_t echo);
    void sendInputs();
    void start(unsigned delay);
    void handle(std::span<const std::uint8_t> packet);
    void compareCheck(unsigned field,std::uint64_t mine,std::uint64_t theirs);
    [[noreturn]] void stop(const std::string& reason,std::uint8_t why);
    std::uint32_t now() const; // Milliseconds since this session was made, for the round trip stamps.
    LockstepTransport& transport_;
    LockstepOptions options_;
    unsigned delay_=0,field_=0,peer_has_=0,checks_compared_=0;
    bool connected_=false,ended_=false;
    std::vector<PadInput> local_,remote_;      // By field; the first `delay` fields are neutral on both sides.
    std::map<unsigned,std::uint64_t> checks_,peer_checks_; // Drift hashes not yet compared, by field.
    std::optional<std::pair<unsigned,std::uint64_t>> last_check_;
    std::chrono::steady_clock::time_point made_,heard_,greeted_;
    // Joiner while connecting: round trips measured over the host's answers; the delay the session starts with.
    std::vector<double> samples_;std::optional<unsigned> starting_;
    // Round trip stamps: the other side's latest stamp and when it arrived.
    std::uint32_t peer_stamp_=0;std::chrono::steady_clock::time_point peer_stamp_at_;bool has_peer_stamp_=false;
    double rtt_ms_=0;
    std::uint64_t waits_=0;double waited_=0,last_wait_=0;
};

// 64-bit FNV-1a, the drift hash and the program's hash.
inline std::uint64_t lockstepHash(std::uint64_t hash,std::span<const std::uint8_t> bytes) {
    for(const auto byte:bytes)hash=(hash^byte)*0x100000001B3ull;
    return hash;
}
constexpr std::uint64_t lockstep_hash_start=0xCBF29CE484222325ull;

// "host:<port>", "join:<address>:<port>" or "join:<code>" (host/connection_code.h) (BT2_NET); throws on other text.
// The joiner's `targets`: every address of the host's code (address and port: the first, for messages).
struct LockstepAddress {bool host=true;std::string address;std::uint16_t port=0;std::vector<connection_code::Address> targets;};
LockstepAddress parseLockstepAddress(std::string_view text);
