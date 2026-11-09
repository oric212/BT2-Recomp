// An online battle's connection code (docs/plans/online/ONLINE_BATTLE_PLAN.md): the host's IPv4 address in eight
// characters the joiner types or pastes instead of an address and port, such as "K7QM-2XPA". Crockford's base32 (no I,
// L, O or U; I, L and O read as 1, 1 and 0): 32 bits of address and an 8-bit check over the address and port, so a
// mistyped code is refused (all but one in 256) rather than joining somewhere else. A port other than the usual one
// adds four characters ("K7QM-2XPA-01Z8"). No server takes part: the code is the address.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
inline std::optional<std::array<std::uint8_t,16>> ipv6(std::string_view text);
inline bool reachable(std::string_view address) {
    if(address.find(':')!=std::string_view::npos) { // IPv6: a global unicast address (2000::/3).
        const auto bytes=ipv6(address);
        return bytes && ((*bytes)[0]&0xE0)==0x20;
    }
    const auto code=make(address,usual_port);
    if(!code)return false;
    unsigned a=0,b=0;std::size_t i=0;
    while(i<address.size() && address[i]!='.')a=a*10+unsigned(address[i++]-'0');
    for(++i;i<address.size() && address[i]!='.';)b=b*10+unsigned(address[i++]-'0');
    return !(a==10 || (a==172 && b>=16 && b<32) || (a==192 && b==168) || (a==100 && b>=64 && b<128) || a==127 || a==0);
}
// `changes_port`: an IPv4 address behind a router that gives each destination another port (a "hard" NAT); its port
// then says little, and the other side reaches it by probing ports (host/net_lockstep.cpp).
struct Address {std::string address;std::uint16_t port=0;bool changes_port=false;};
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

// IPv6 text (with "::" for a run of zero groups) as its 16 bytes; nothing for other text.
inline std::optional<std::array<std::uint8_t,16>> ipv6(std::string_view text) {
    std::vector<int> head,tail;bool gap=false;
    std::vector<int>* groups=&head;
    std::size_t i=0;
    if(text.starts_with("::")) {gap=true;groups=&tail;i=2;}
    while(i<text.size()) {
        unsigned value=0,digits=0;
        while(i<text.size() && text[i]!=':') {
            const char c=text[i];
            const int d=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:-1;
            if(d<0 || ++digits>4)return std::nullopt;
            value=value<<4|unsigned(d);++i;
        }
        if(!digits)return std::nullopt;
        groups->push_back(int(value));
        if(i<text.size()) {
            ++i;
            if(i<text.size() && text[i]==':') {if(gap)return std::nullopt;gap=true;groups=&tail;++i;}
            else if(i==text.size())return std::nullopt;
        }
    }
    const std::size_t count=head.size()+tail.size();
    if(gap?count>7:count!=8)return std::nullopt;
    std::array<std::uint8_t,16> out{};
    for(std::size_t n=0;n<head.size();++n) {out[2*n]=std::uint8_t(head[n]>>8);out[2*n+1]=std::uint8_t(head[n]);}
    for(std::size_t n=0;n<tail.size();++n) {const std::size_t at=16-2*(tail.size()-n);out[at]=std::uint8_t(tail[n]>>8);out[at+1]=std::uint8_t(tail[n]);}
    return out;
}
inline std::string ipv6Text(const std::array<std::uint8_t,16>& bytes) {
    std::string out;const char* hex="0123456789abcdef";
    for(unsigned n=0;n<8;++n) {
        const unsigned group=unsigned(bytes[2*n])<<8|bytes[2*n+1];
        if(n)out+=':';
        bool started=false;
        for(int shift=12;shift>=0;shift-=4) {const unsigned d=(group>>shift)&15;if(d || started || !shift) {out+=hex[d];started=true;}}
    }
    return out;
}

// A code for several addresses (an IPv4 one and an IPv6 one; docs/plans/online/ONLINE_BATTLE_PLAN.md, "Without an open
// port"): each as a kind byte (4, 5 for IPv4 behind a router that changes ports, or 6), its bytes and its port (two bytes, high first), then a check byte over all of it;
// written in the same base32 in groups of four. One IPv4 address alone keeps the short code above. Never eight or twelve
// characters long, so the two kinds cannot be confused.
inline std::optional<std::string> make(const std::vector<Address>& addresses) {
    if(addresses.empty())return std::nullopt;
    if(addresses.size()==1 && addresses[0].address.find(':')==std::string::npos && !addresses[0].changes_port)return make(addresses[0].address,addresses[0].port);
    std::vector<std::uint8_t> bytes;
    for(const auto& a:addresses) {
        if(a.address.find(':')!=std::string::npos) {
            const auto v6=ipv6(a.address);
            if(!v6)return std::nullopt;
            bytes.push_back(6);bytes.insert(bytes.end(),v6->begin(),v6->end());
        } else {
            if(!make(a.address,usual_port))return std::nullopt; // Not IPv4 text.
            bytes.push_back(a.changes_port?5:4);
            std::uint32_t ip=0;unsigned part=0;
            for(const char c:a.address+".") {if(c=='.') {ip=ip<<8|part;part=0;} else part=part*10+unsigned(c-'0');}
            for(int shift=24;shift>=0;shift-=8)bytes.push_back(std::uint8_t(ip>>shift));
        }
        bytes.push_back(std::uint8_t(a.port>>8));bytes.push_back(std::uint8_t(a.port));
    }
    std::uint64_t hash=0xCBF29CE484222325ull;
    for(const auto b:bytes)hash=(hash^b)*0x100000001B3ull;
    bytes.push_back(std::uint8_t(hash));
    std::string code;unsigned buffer=0,bits=0;
    for(const auto b:bytes) {
        buffer=(buffer<<8|b)&0xFFFF;bits+=8;
        while(bits>=5) {code+=digits[(buffer>>(bits-5))&31];bits-=5;}
    }
    if(bits)code+=digits[(buffer<<(5-bits))&31];
    std::string grouped;
    for(std::size_t n=0;n<code.size();++n) {if(n && n%4==0)grouped+='-';grouped+=code[n];}
    return grouped;
}
// Every address in a code of either kind; empty when the text is not a valid code.
inline std::vector<Address> readAll(std::string_view text) {
    if(const auto one=read(text))return {*one};
    std::vector<std::uint8_t> bytes;unsigned buffer=0,bits=0,count=0;
    for(const char raw:text) {
        if(raw=='-' || raw==' ')continue;
        char c=raw>='a' && raw<='z'?char(raw-'a'+'A'):raw;
        if(c=='O')c='0';
        if(c=='I' || c=='L')c='1';
        const auto at=digits.find(c);
        if(at==std::string_view::npos)return {};
        ++count;buffer=(buffer<<5|unsigned(at))&0xFFFF;bits+=5;
        if(bits>=8) {bytes.push_back(std::uint8_t(buffer>>(bits-8)));bits-=8;}
    }
    if(count==8 || count==12 || bytes.size()<2 || bits>=5 || (buffer&((1u<<bits)-1)))return {}; // Only padding may be left.
    std::uint64_t hash=0xCBF29CE484222325ull;
    for(std::size_t n=0;n+1<bytes.size();++n)hash=(hash^bytes[n])*0x100000001B3ull;
    if(std::uint8_t(hash)!=bytes.back())return {};
    std::vector<Address> out;
    for(std::size_t at=0;at+1<bytes.size();) {
        const unsigned kind=bytes[at++];const std::size_t size=kind==4 || kind==5?4:kind==6?16:0;
        if(!size || at+size+2>bytes.size()-1)return {};
        Address a;
        a.changes_port=kind==5;
        if(kind!=6)a.address=std::to_string(bytes[at])+"."+std::to_string(bytes[at+1])+"."+std::to_string(bytes[at+2])+"."+std::to_string(bytes[at+3]);
        else {std::array<std::uint8_t,16> v6{};for(unsigned n=0;n<16;++n)v6[n]=bytes[at+n];a.address=ipv6Text(v6);}
        at+=size;
        a.port=std::uint16_t(bytes[at]<<8|bytes[at+1]);at+=2;
        if(!a.port)return {};
        out.push_back(a);
    }
    return out;
}
}
