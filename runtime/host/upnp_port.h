// The host of an online battle (docs/plans/online/ONLINE_BATTLE_PLAN.md): its UDP port opened on the home router by
// UPnP (Windows' NAT traversal API), so the other player can reach it without forwarding the port by hand, and the
// addresses to tell the other player. The router is asked on a thread of its own (finding it can take seconds); the
// port is closed again when this ends. Routers without UPnP, or with it switched off, leave it to the player.
#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <condition_variable>

class UpnpPort {
public:
    enum class State {Asking,Opened,Failed};
    explicit UpnpPort(std::uint16_t port);
    ~UpnpPort(); // Closes the port on the router again.
    UpnpPort(const UpnpPort&)=delete;
    UpnpPort& operator=(const UpnpPort&)=delete;
    State state() const;
    std::string externalAddress() const; // The router's address on the internet, once opened.
    std::string failure() const;         // Why it could not be opened.
    // This PC's address on the home network (the one its traffic to the internet leaves from); empty without one.
    static std::string localAddress();
private:
    void run();
    std::uint16_t port_;
    mutable std::mutex lock_;std::condition_variable stop_signal_;bool stop_=false;
    State state_=State::Asking;std::string external_,failure_;
    std::thread thread_; // Last: it starts in the constructor.
};
