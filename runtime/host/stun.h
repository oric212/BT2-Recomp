// STUN binding requests (RFC 5389), for online battles without an open port (docs/plans/online/ONLINE_BATTLE_PLAN.md,
// "Without an open port"): a public STUN server answers a request with the address and port it saw the request come
// from, which is this PC's socket as the internet sees it, the address the other player sends to. The session's own
// socket asks, so the answer is the mapping its packets use. Pure functions; the transport sends and receives.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace stun {
constexpr std::uint32_t cookie=0x2112A442;
using Transaction=std::array<std::uint8_t,12>;
// A binding request: type 1, no attributes, the magic cookie, the transaction id.
inline std::vector<std::uint8_t> request(const Transaction& id) {
    std::vector<std::uint8_t> out{0x00,0x01,0x00,0x00,0x21,0x12,0xA4,0x42};
    out.insert(out.end(),id.begin(),id.end());
    return out;
}
// Whether a datagram is a STUN message (the two top bits zero and the cookie), not a game packet.
inline bool isStun(std::span<const std::uint8_t> packet) {
    return packet.size()>=20 && (packet[0]&0xC0)==0 && packet[4]==0x21 && packet[5]==0x12 && packet[6]==0xA4 && packet[7]==0x42;
}
struct Mapped {std::uint32_t ip=0;std::uint16_t port=0;}; // Host order.
// The address a binding success answer to `id` reports: XOR-MAPPED-ADDRESS, or MAPPED-ADDRESS from older servers;
// IPv4 only. Nothing for anything else (another transaction, an error answer, a damaged message).
inline std::optional<Mapped> mapped(std::span<const std::uint8_t> packet,const Transaction& id) {
    if(!isStun(packet) || packet[0]!=0x01 || packet[1]!=0x01)return std::nullopt;
    for(unsigned n=0;n<12;++n)if(packet[8+n]!=id[n])return std::nullopt;
    const std::size_t length=std::size_t(packet[2])<<8|packet[3];
    if(20+length>packet.size())return std::nullopt;
    std::optional<Mapped> plain;
    for(std::size_t at=20;at+4<=20+length;) {
        const unsigned type=unsigned(packet[at])<<8|packet[at+1];
        const std::size_t size=std::size_t(packet[at+2])<<8|packet[at+3];
        if(at+4+size>20+length)return std::nullopt;
        const auto* v=packet.data()+at+4;
        if((type==0x0020 || type==0x0001) && size>=8 && v[1]==0x01) { // IPv4.
            Mapped m{std::uint32_t(v[4])<<24|std::uint32_t(v[5])<<16|std::uint32_t(v[6])<<8|v[7],std::uint16_t(v[2]<<8|v[3])};
            if(type==0x0020) {m.port^=std::uint16_t(cookie>>16);m.ip^=cookie;return m;}
            plain=m;
        }
        at+=4+((size+3)&~std::size_t(3));
    }
    return plain;
}
}
