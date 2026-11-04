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
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "services/devices/pad_input.h"

// Datagrams to and from the other player.
class LockstepTransport {
public:
    virtual ~LockstepTransport()=default;
    virtual void send(std::span<const std::uint8_t> packet)=0;
    // The next packet from the other player, or nothing once `timeout` has passed.
    virtual std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout)=0;
};

// UDP over Winsock. The host listens on `port` and answers whoever greets it first; the joiner sends to the host.
std::unique_ptr<LockstepTransport> hostUdp(std::uint16_t port);
std::unique_ptr<LockstepTransport> joinUdp(const std::string& address,std::uint16_t port);

struct LockstepOptions {
    bool host=true;
    unsigned delay=2;          // Fields between reading an input and playing it (the host's; the joiner takes it).
    std::uint64_t program=0;   // The running program's hash: two different builds refuse each other.
    std::uint32_t rules=0;     // Settings that change the fight, as bits: two players must have them alike.
    std::string rule_names;    // Those settings, named in the refusal.
    std::chrono::milliseconds silence{5000}; // No word from the other player for this long ends the session.
};

class Lockstep {
public:
    static constexpr unsigned max_delay=8,check_interval=60;
    Lockstep(LockstepTransport& transport,const LockstepOptions& options);
    ~Lockstep(); // Tells the other player the session ended.
    // Host: waits for a joiner's greeting; joiner: greets every 200 ms until the host answers. False when `timeout`
    // passed first (call again to go on waiting); throws on a different program or gameplay settings.
    bool connect(std::chrono::milliseconds timeout);
    // One field: `local` is played `delay` fields later; `check` is this field's drift hash (compared every 60
    // fields). Returns the input of ports 1 and 2 for this field, waiting for the other player's if needed.
    std::array<PadInput,2> field(const PadInput& local,std::uint64_t check);
    unsigned delay() const {return delay_;}
    unsigned fieldsPlayed() const {return field_;}
    unsigned checksCompared() const {return checks_compared_;}
    std::uint64_t waits() const {return waits_;}           // Fields that had to wait for the other player.
    double waitedSeconds() const {return waited_;}
private:
    void sendWelcome();
    void sendInputs();
    void handle(std::span<const std::uint8_t> packet);
    void compareCheck(unsigned field,std::uint64_t mine,std::uint64_t theirs);
    [[noreturn]] void stop(const std::string& reason,std::uint8_t why);
    LockstepTransport& transport_;
    LockstepOptions options_;
    unsigned delay_=0,field_=0,peer_has_=0,checks_compared_=0;
    bool connected_=false,ended_=false;
    std::vector<PadInput> local_,remote_;      // By field; the first `delay` fields are neutral on both sides.
    std::map<unsigned,std::uint64_t> checks_,peer_checks_; // Drift hashes not yet compared, by field.
    std::optional<std::pair<unsigned,std::uint64_t>> last_check_;
    std::chrono::steady_clock::time_point heard_,greeted_;
    std::uint64_t waits_=0;double waited_=0;
};

// 64-bit FNV-1a, the drift hash and the program's hash.
inline std::uint64_t lockstepHash(std::uint64_t hash,std::span<const std::uint8_t> bytes) {
    for(const auto byte:bytes)hash=(hash^byte)*0x100000001B3ull;
    return hash;
}
constexpr std::uint64_t lockstep_hash_start=0xCBF29CE484222325ull;

// "host:<port>" or "join:<address>:<port>" (BT2_NET); throws on other text.
struct LockstepAddress {bool host=true;std::string address;std::uint16_t port=0;};
LockstepAddress parseLockstepAddress(std::string_view text);
