#include "host/net_lockstep.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif
#endif

// Packets: "BT2L", a kind byte, then little-endian fields.
// 1 hello (joiner): program u64, rules u32. 2 welcome (host): program u64, rules u32, delay u8.
// 3 inputs: fields of the other player's input held u32, first field u32, count u8, count inputs of 6 bytes
//   (buttons u16, sticks RX RY LX LY), the sender's latest drift check: field u32 (none: all ones), hash u64.
// 4 bye: why u8 (0 left, 1 the games went apart, 2 a different program, 3 different gameplay settings).
// Lost packets heal by themselves: every inputs packet repeats what the other side has not confirmed.

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::array<std::uint8_t,4> magic{'B','T','2','L'};
enum Kind : std::uint8_t {Hello=1,Welcome=2,Inputs=3,Bye=4};
enum Why : std::uint8_t {Left=0,Apart=1,Different=2,Rules=3};
constexpr std::uint32_t no_check=0xFFFFFFFFu;
constexpr unsigned max_inputs=32; // Per packet: more than the two sides can be apart (twice the delay).
constexpr auto resend=std::chrono::milliseconds(10),greet=std::chrono::milliseconds(200);

struct Writer {
    std::vector<std::uint8_t> bytes;
    explicit Writer(Kind kind) {bytes.assign(magic.begin(),magic.end());bytes.push_back(kind);}
    void u8(std::uint8_t v) {bytes.push_back(v);}
    void u16(std::uint16_t v) {u8(std::uint8_t(v));u8(std::uint8_t(v>>8));}
    void u32(std::uint32_t v) {u16(std::uint16_t(v));u16(std::uint16_t(v>>16));}
    void u64(std::uint64_t v) {u32(std::uint32_t(v));u32(std::uint32_t(v>>32));}
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t at=5;bool ok=true;
    std::uint64_t take(unsigned size) {
        if(at+size>bytes.size()) {ok=false;return 0;}
        std::uint64_t v=0;for(unsigned n=0;n<size;++n)v|=std::uint64_t(bytes[at+n])<<(8*n);
        at+=size;return v;
    }
    std::uint8_t u8() {return std::uint8_t(take(1));}
    std::uint16_t u16() {return std::uint16_t(take(2));}
    std::uint32_t u32() {return std::uint32_t(take(4));}
    std::uint64_t u64() {return take(8);}
};
std::optional<Kind> kindOf(std::span<const std::uint8_t> packet) {
    if(packet.size()<5 || !std::equal(magic.begin(),magic.end(),packet.begin()))return std::nullopt;
    return Kind(packet[4]);
}
std::string why(Why reason,const std::string& rule_names) {
    switch(reason) {
    case Apart: return "the two games went apart (found by the other player)";
    case Different: return "the other player runs a different build of the game";
    case Rules: return "the other player's gameplay settings differ ("+rule_names+" must be alike)";
    default: return "the other player left the session";
    }
}
}

Lockstep::Lockstep(LockstepTransport& transport,const LockstepOptions& options):transport_(transport),options_(options) {
    if(options.host && options.delay>max_delay)throw std::runtime_error("Online session: the input delay is 0 to "+std::to_string(max_delay)+" fields");
}

Lockstep::~Lockstep() {
    if(!connected_ || ended_)return;
    Writer bye(Bye);bye.u8(Left);
    for(int n=0;n<3;++n)transport_.send(bye.bytes);
}

bool Lockstep::connect(std::chrono::milliseconds timeout) {
    const auto start=Clock::now();
    Writer hello(Hello);hello.u64(options_.program);hello.u32(options_.rules);
    // A greeting that does not match ends both sides' attempts, each saying why.
    const auto refuse=[&](Why reason) {
        Writer bye(Bye);bye.u8(reason);transport_.send(bye.bytes);
        ended_=true;throw std::runtime_error("Online session: "+why(reason,options_.rule_names));
    };
    if(connected_)return true;
    while(!connected_) {
        if(Clock::now()-start>timeout)return false;
        if(!options_.host && Clock::now()-greeted_>=greet) {transport_.send(hello.bytes);greeted_=Clock::now();}
        const auto packet=transport_.receive(std::chrono::milliseconds(20));
        if(!packet)continue;
        const auto kind=kindOf(*packet);
        if(!kind)continue;
        Reader in{*packet};
        if(*kind==Bye) {
            const auto reason=Why(in.u8());
            if(in.ok && (reason==Different || reason==Rules)) {ended_=true;throw std::runtime_error("Online session: "+why(reason,options_.rule_names));}
            continue;
        }
        if(options_.host && *kind==Hello) {
            const auto program=in.u64();const auto rules=in.u32();
            if(!in.ok)continue;
            if(program!=options_.program)refuse(Different);
            if(rules!=options_.rules)refuse(Rules);
            delay_=options_.delay;connected_=true;
        } else if(!options_.host && *kind==Welcome) {
            const auto program=in.u64();const auto rules=in.u32();const unsigned delay=in.u8();
            if(!in.ok)continue;
            if(program!=options_.program)refuse(Different);
            if(rules!=options_.rules)refuse(Rules);
            if(delay>max_delay)throw std::runtime_error("Online session: the host's input delay is outside 0 to "+std::to_string(max_delay));
            delay_=delay;connected_=true;
        }
    }
    if(options_.host)sendWelcome();
    // The first `delay` fields are neutral on both machines; each side knows the other's.
    local_.assign(delay_,PadInput{});remote_.assign(delay_,PadInput{});peer_has_=delay_;
    heard_=Clock::now();
    return true;
}

std::array<PadInput,2> Lockstep::field(const PadInput& local,std::uint64_t check) {
    if(!connected_)throw std::logic_error("Lockstep::field before connect");
    while(const auto packet=transport_.receive(std::chrono::milliseconds(0)))handle(*packet);
    if(field_%check_interval==0) {
        last_check_={field_,check};
        if(const auto theirs=peer_checks_.find(field_);theirs!=peer_checks_.end()) {compareCheck(field_,check,theirs->second);peer_checks_.erase(theirs);}
        else checks_[field_]=check;
    }
    local_.push_back(local);
    sendInputs();
    if(remote_.size()<=field_) {
        ++waits_;
        const auto start=Clock::now();auto sent=start;
        while(remote_.size()<=field_) {
            if(const auto packet=transport_.receive(resend))handle(*packet);
            const auto now=Clock::now();
            if(now-sent>=resend) {sendInputs();sent=now;}
            if(now-heard_>options_.silence)
                stop("no word from the other player for "+std::to_string(options_.silence.count()/1000)+" s",Left);
        }
        waited_+=std::chrono::duration<double>(Clock::now()-start).count();
    }
    const PadInput mine=local_[field_],theirs=remote_[field_];
    ++field_;
    return options_.host?std::array<PadInput,2>{mine,theirs}:std::array<PadInput,2>{theirs,mine};
}

void Lockstep::sendWelcome() {
    Writer welcome(Welcome);welcome.u64(options_.program);welcome.u32(options_.rules);welcome.u8(std::uint8_t(delay_));
    transport_.send(welcome.bytes);
}

void Lockstep::sendInputs() {
    Writer out(Inputs);
    const unsigned first=std::min<unsigned>(peer_has_,unsigned(local_.size()));
    const unsigned count=std::min<unsigned>(unsigned(local_.size())-first,max_inputs);
    out.u32(unsigned(remote_.size()));out.u32(first);out.u8(std::uint8_t(count));
    for(unsigned n=0;n<count;++n) {
        const auto& input=local_[first+n];out.u16(input.pressed);
        for(const auto stick:input.sticks)out.u8(stick);
    }
    out.u32(last_check_?last_check_->first:no_check);out.u64(last_check_?last_check_->second:0);
    transport_.send(out.bytes);
}

void Lockstep::handle(std::span<const std::uint8_t> packet) {
    const auto kind=kindOf(packet);
    if(!kind)return; // Not ours.
    Reader in{packet};
    switch(*kind) {
    case Hello: // The joiner missed the welcome.
        if(options_.host && in.u64()==options_.program && in.u32()==options_.rules && in.ok)sendWelcome();
        break;
    case Welcome: break;
    case Inputs: {
        const unsigned has=in.u32(),first=in.u32(),count=in.u8();
        std::vector<PadInput> inputs(count);
        for(auto& input:inputs) {input.pressed=in.u16();for(auto& stick:input.sticks)stick=in.u8();}
        const unsigned check_field=in.u32();const std::uint64_t check=in.u64();
        if(!in.ok)return;
        peer_has_=std::max(peer_has_,std::min<unsigned>(has,unsigned(local_.size())));
        for(unsigned n=0;n<count;++n)if(first+n==remote_.size())remote_.push_back(inputs[n]);
        if(check_field!=no_check) {
            if(const auto mine=checks_.find(check_field);mine!=checks_.end()) {compareCheck(check_field,mine->second,check);checks_.erase(mine);}
            else if(check_field>=field_)peer_checks_[check_field]=check; // Not reached here yet.
        }
        break;
    }
    case Bye: {
        const auto reason=Why(in.u8());
        ended_=true;
        throw std::runtime_error("Online session: "+why(reason,options_.rule_names)+" at field "+std::to_string(field_));
    }
    default: return;
    }
    heard_=Clock::now();
}

void Lockstep::compareCheck(unsigned field,std::uint64_t mine,std::uint64_t theirs) {
    if(mine!=theirs)stop("the two games went apart by field "+std::to_string(field)+" (their requests to the game's services differ)",Apart);
    ++checks_compared_;
}

void Lockstep::stop(const std::string& reason,std::uint8_t reason_code) {
    ended_=true;
    Writer bye(Bye);bye.u8(reason_code);
    for(int n=0;n<3;++n)transport_.send(bye.bytes);
    throw std::runtime_error("Online session: "+reason);
}

LockstepAddress parseLockstepAddress(std::string_view text) {
    const auto port=[&](std::string_view digits) {
        unsigned value=0;
        const auto [end,error]=std::from_chars(digits.data(),digits.data()+digits.size(),value);
        if(error!=std::errc{} || end!=digits.data()+digits.size() || !value || value>65535)throw std::runtime_error("BT2_NET: the port is 1 to 65535: "+std::string(text));
        return std::uint16_t(value);
    };
    LockstepAddress address;
    if(text.starts_with("host:")) {address.port=port(text.substr(5));return address;}
    if(text.starts_with("join:")) {
        const auto rest=text.substr(5);const auto colon=rest.rfind(':');
        if(colon==std::string_view::npos || !colon)throw std::runtime_error("BT2_NET: join:<address>:<port>: "+std::string(text));
        address.host=false;address.address=std::string(rest.substr(0,colon));address.port=port(rest.substr(colon+1));
        return address;
    }
    throw std::runtime_error("BT2_NET is host:<port> or join:<address>:<port>: "+std::string(text));
}

#ifdef _WIN32
namespace {
class UdpTransport final : public LockstepTransport {
public:
    UdpTransport(bool host,const std::string& address,std::uint16_t port) {
        WSADATA data;
        if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Online session: Winsock did not start");
        started_=true;
        socket_=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(socket_==INVALID_SOCKET)throw error("no UDP socket");
        // A datagram to a port nobody listens on would make the next receive fail; it is only a lost packet.
        BOOL report=FALSE;DWORD returned=0;
        WSAIoctl(socket_,SIO_UDP_CONNRESET,&report,sizeof report,nullptr,0,&returned,nullptr,nullptr);
        sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);local.sin_port=htons(host?port:0);
        if(::bind(socket_,reinterpret_cast<sockaddr*>(&local),sizeof local))throw error("port "+std::to_string(port)+" cannot be opened");
        if(!host) {
            addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;
            addrinfo* found=nullptr;
            if(getaddrinfo(address.c_str(),std::to_string(port).c_str(),&hints,&found) || !found)throw error("the address "+address+" is not found");
            std::memcpy(&peer_,found->ai_addr,sizeof peer_);freeaddrinfo(found);
            has_peer_=true;
        }
    }
    ~UdpTransport() override {
        if(socket_!=INVALID_SOCKET)closesocket(socket_);
        if(started_)WSACleanup();
    }
    void send(std::span<const std::uint8_t> packet) override {
        if(!has_peer_)return; // The host answers once someone greeted it.
        ::sendto(socket_,reinterpret_cast<const char*>(packet.data()),int(packet.size()),0,reinterpret_cast<const sockaddr*>(&peer_),sizeof peer_);
    }
    std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout) override {
        const auto deadline=Clock::now()+timeout;
        while(true) {
            const auto left=std::chrono::duration_cast<std::chrono::microseconds>(std::max(deadline-Clock::now(),Clock::duration::zero()));
            fd_set set;FD_ZERO(&set);FD_SET(socket_,&set);
            timeval wait{long(left.count()/1000000),long(left.count()%1000000)};
            if(::select(0,&set,nullptr,nullptr,&wait)<=0)return std::nullopt;
            std::vector<std::uint8_t> packet(1500);
            sockaddr_in from{};int from_size=sizeof from;
            const int size=::recvfrom(socket_,reinterpret_cast<char*>(packet.data()),int(packet.size()),0,reinterpret_cast<sockaddr*>(&from),&from_size);
            if(size<0)continue;
            if(!has_peer_) { // The host's other player: the first to send one of these packets.
                if(size<int(magic.size()) || !std::equal(magic.begin(),magic.end(),packet.begin()))continue;
                peer_=from;has_peer_=true;
            }
            else if(from.sin_addr.s_addr!=peer_.sin_addr.s_addr || from.sin_port!=peer_.sin_port)continue;
            packet.resize(std::size_t(size));
            return packet;
        }
    }
private:
    std::runtime_error error(const std::string& what) {return std::runtime_error("Online session: "+what+" (Winsock error "+std::to_string(WSAGetLastError())+")");}
    bool started_=false,has_peer_=false;
    SOCKET socket_=INVALID_SOCKET;
    sockaddr_in peer_{};
};
}

std::unique_ptr<LockstepTransport> hostUdp(std::uint16_t port) {return std::make_unique<UdpTransport>(true,std::string(),port);}
std::unique_ptr<LockstepTransport> joinUdp(const std::string& address,std::uint16_t port) {return std::make_unique<UdpTransport>(false,address,port);}
#endif
