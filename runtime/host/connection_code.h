// An online battle's connection code (docs/plans/online/ONLINE_BATTLE_PLAN.md): the host's IPv4 address in eight
// characters the joiner types or pastes instead of an address and port, such as "K7QM-2XPA". Crockford's base32 (no I,
// L, O or U; I, L and O read as 1, 1 and 0): 32 bits of address and an 8-bit check over the address and port, so a
// mistyped code is refused (all but one in 256) rather than joining somewhere else. A port other than the usual one
// adds four characters ("K7QM-2XPA-01Z8"). No server takes part: the code is the address.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace connection_code {
constexpr std::string_view digits="0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::uint16_t usual_port=7411; // The launcher's default port; a code for it carries no port.

inline unsigned check(std::uint32_t ip,std::uint16_t port) {
    std::uint64_t hash=0xCBF29CE484222325ull;
    for(unsigned n=0;n<4;++n)hash=(hash^((ip>>(8*n))&0xFF))*0x100000001B3ull;
    for(unsigned n=0;n<2;++n)hash=(hash^((port>>(8*n))&0xFF))*0x100000001B3ull;
    return unsigned(hash&0xFF);
}
// `address`: dotted IPv4. Nothing for other text.
inline std::optional<std::string> make(std::string_view address,std::uint16_t port) {
    std::uint32_t ip=0;unsigned parts=0,part=0,length=0;
    for(std::size_t i=0;i<=address.size();++i) {
        if(i==address.size() || address[i]=='.') {
            if(!length || part>255)return std::nullopt;
            ip=ip<<8|part;++parts;part=length=0;
        } else if(address[i]>='0' && address[i]<='9' && length<3) {part=part*10+unsigned(address[i]-'0');++length;}
        else return std::nullopt;
    }
    if(parts!=4)return std::nullopt;
    const std::uint64_t value=std::uint64_t(ip)<<8|check(ip,port);
    std::string code;
    for(int n=7;n>=0;--n)code+=digits[(value>>(5*n))&31];
    code=code.substr(0,4)+"-"+code.substr(4);
    if(port!=usual_port) {
        code+='-';
        for(int n=3;n>=0;--n)code+=digits[(port>>(5*n))&31];
    }
    return code;
}
// Whether a router's internet address can be reached from the internet: not a private range (10/8, 172.16/12,
// 192.168/16) and not one a provider shares among its customers (100.64/10, carrier NAT).
inline bool reachable(std::string_view address) {
    const auto code=make(address,usual_port);
    if(!code)return false;
    unsigned a=0,b=0;std::size_t i=0;
    while(i<address.size() && address[i]!='.')a=a*10+unsigned(address[i++]-'0');
    for(++i;i<address.size() && address[i]!='.';)b=b*10+unsigned(address[i++]-'0');
    return !(a==10 || (a==172 && b>=16 && b<32) || (a==192 && b==168) || (a==100 && b>=64 && b<128) || a==127 || a==0);
}
struct Address {std::string address;std::uint16_t port=0;};
// A code in any case, with or without its dashes and spaces; nothing when the text is not a valid code.
inline std::optional<Address> read(std::string_view text) {
    std::uint64_t value=0,port=0;unsigned count=0;
    for(const char raw:text) {
        if(raw=='-' || raw==' ')continue;
        char c=raw>='a' && raw<='z'?char(raw-'a'+'A'):raw;
        if(c=='O')c='0';
        if(c=='I' || c=='L')c='1';
        const auto at=digits.find(c);
        if(at==std::string_view::npos || ++count>12)return std::nullopt;
        if(count<=8)value=value<<5|at;else port=port<<5|at;
    }
    if(count==8)port=usual_port;
    else if(count!=12 || port>0xFFFF || !port)return std::nullopt;
    const std::uint32_t ip=std::uint32_t(value>>8);
    if(check(ip,std::uint16_t(port))!=(value&0xFF))return std::nullopt;
    return Address{std::to_string(ip>>24)+"."+std::to_string(ip>>16&255)+"."+std::to_string(ip>>8&255)+"."+std::to_string(ip&255),std::uint16_t(port)};
}
}
