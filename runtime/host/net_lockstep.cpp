#include "host/net_lockstep.h"
#include "host/connection_code.h"
#include "host/stun.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <deque>
#include <iostream>
#include <random>
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
// 1 hello (joiner, every 100 ms): program u64, rules u32, stamp u32.
// 2 welcome (host, to each hello): program u64, rules u32, delay u8 (0: the joiner chooses it), the hello's stamp u32.
// 5 start (joiner, every 50 ms until answered): delay u8, round trip ms u16. 6 go (host, to each start): delay u8.
// 3 inputs: fields of the other player's input held u32, first field u32, count u8, count inputs of 6 bytes
//   (buttons u16, sticks RX RY LX LY), the sender's latest drift check: field u32 (none: all ones), hash u64; then
//   the sender's stamp u32, the other side's latest stamp u32 and how many ms ago it came u16 (none: all ones).
// 4 bye: why u8 (0 left, 1 the games went apart, 2 a different program, 3 different gameplay settings).
// Lost packets heal by themselves: every inputs packet repeats what the other side has not confirmed.

namespace {
using Clock=std::chrono::steady_clock;
constexpr std::array<std::uint8_t,4> magic{'B','T','2','L'};
enum Kind : std::uint8_t {Hello=1,Welcome=2,Inputs=3,Bye=4,Start=5,Go=6};
enum Why : std::uint8_t {Left=0,Apart=1,Different=2,Rules=3};
constexpr std::uint32_t no_check=0xFFFFFFFFu;
constexpr std::uint16_t no_stamp=0xFFFF;
constexpr unsigned max_inputs=32; // Per packet: more than the two sides can be apart (twice the delay).
constexpr auto resend=std::chrono::milliseconds(10),greet=std::chrono::milliseconds(100),restart=std::chrono::milliseconds(50);
constexpr double field_ms=1001.0/60.0;

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

Lockstep::Lockstep(LockstepTransport& transport,const LockstepOptions& options):transport_(transport),options_(options),made_(Clock::now()) {
    if(options.host && options.delay>max_delay)throw std::runtime_error("Online session: the input delay is 1 to "+std::to_string(max_delay)+" fields, or 0 to choose it");
    greeted_=made_-greet;
}

Lockstep::~Lockstep() {leave();}

void Lockstep::leave() {
    if(!connected_ || ended_)return;
    ended_=true;
    Writer bye(Bye);bye.u8(Left);
    for(int n=0;n<3;++n)transport_.send(bye.bytes);
}

std::uint32_t Lockstep::now() const {
    return std::uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-made_).count());
}

unsigned Lockstep::delayFor(double round_trip_ms,double spread_ms) {
    const double one_way=round_trip_ms/2+spread_ms/2+2;
    return std::clamp(unsigned(std::ceil(one_way/field_ms))+1,1u,max_delay);
}

bool Lockstep::connect(std::chrono::milliseconds timeout) {
    if(connected_)return true;
    const auto begin=Clock::now();
    // A greeting that does not match ends both sides' attempts, each saying why.
    const auto refuse=[&](Why reason) {
        Writer bye(Bye);bye.u8(reason);transport_.send(bye.bytes);
        ended_=true;throw std::runtime_error("Online session: "+why(reason,options_.rule_names));
    };
    while(!connected_) {
        if(Clock::now()-begin>timeout)return false;
        if(!options_.host && Clock::now()-greeted_>=(starting_?restart:greet)) {
            if(starting_) {Writer out(Start);out.u8(std::uint8_t(*starting_));out.u16(std::uint16_t(std::min(rtt_ms_,65000.0)));transport_.send(out.bytes);}
            else {Writer hello(Hello);hello.u64(options_.program);hello.u32(options_.rules);hello.u32(now());transport_.send(hello.bytes);}
            greeted_=Clock::now();
        }
        const auto packet=transport_.receive(std::chrono::milliseconds(10));
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
            const auto program=in.u64();const auto rules=in.u32();const auto stamp=in.u32();
            if(!in.ok)continue;
            if(program!=options_.program)refuse(Different);
            if(rules!=options_.rules)refuse(Rules);
            sendWelcome(stamp);
        } else if(options_.host && *kind==Start) {
            const unsigned delay=in.u8();const unsigned rtt=in.u16();
            if(!in.ok || !delay || delay>max_delay)continue;
            rtt_ms_=rtt;start(delay);
            Writer go(Go);go.u8(std::uint8_t(delay));transport_.send(go.bytes);
        } else if(!options_.host && *kind==Welcome) {
            const auto program=in.u64();const auto rules=in.u32();const unsigned delay=in.u8();const auto echo=in.u32();
            if(!in.ok)continue;
            if(program!=options_.program)refuse(Different);
            if(rules!=options_.rules)refuse(Rules);
            if(delay>max_delay)throw std::runtime_error("Online session: the host's input delay is outside 1 to "+std::to_string(max_delay));
            if(starting_)continue;
            samples_.push_back(double(now()-echo));
            if(samples_.size()<rtt_samples)continue;
            // The fastest answer is the link's own round trip; the middle one's lead over it is its spread. (A host that
            // answers between drawing its waiting screen adds tens of milliseconds to some answers: measured on one PC,
            // the slowest answers chose 3 fields where 2 were enough.)
            auto sorted=samples_;std::sort(sorted.begin(),sorted.end());
            rtt_ms_=sorted.front();
            starting_=delay?delay:delayFor(rtt_ms_,sorted[sorted.size()/2]-sorted.front());
            greeted_=Clock::now()-restart; // Start at once.
        } else if(!options_.host && starting_ && (*kind==Go || *kind==Inputs)) {
            start(*starting_); // The host started (an inputs packet: its answer was lost).
            if(*kind==Inputs)handle(*packet);
        }
    }
    return true;
}

void Lockstep::start(unsigned delay) {
    // The first `delay` fields are neutral on both machines; each side knows the other's.
    delay_=delay;connected_=true;
    local_.assign(delay_,PadInput{});remote_.assign(delay_,PadInput{});peer_has_=delay_;
    heard_=Clock::now();
}

std::array<PadInput,2> Lockstep::field(const PadInput& local,std::uint64_t check) {
    if(!connected_ || ended_)throw std::logic_error("Lockstep::field outside a session");
    while(const auto packet=transport_.receive(std::chrono::milliseconds(0)))handle(*packet);
    if(field_%check_interval==0) {
        last_check_={field_,check};
        if(const auto theirs=peer_checks_.find(field_);theirs!=peer_checks_.end()) {compareCheck(field_,check,theirs->second);peer_checks_.erase(theirs);}
        else checks_[field_]=check;
    }
    local_.push_back(local);
    sendInputs();
    last_wait_=0;
    if(remote_.size()<=field_) {
        ++waits_;
        const auto begin=Clock::now();auto sent=begin,told=begin;
        while(remote_.size()<=field_) {
            if(const auto packet=transport_.receive(resend))handle(*packet);
            const auto now=Clock::now();
            if(now-sent>=resend) {sendInputs();sent=now;}
            if(now-heard_>options_.silence)
                stop("no word from the other player for "+std::to_string(options_.silence.count()/1000)+" s",Left);
            if(options_.waiting && now-told>=std::chrono::milliseconds(100)) {told=now;options_.waiting(std::chrono::duration<double>(now-begin).count());}
        }
        last_wait_=std::chrono::duration<double>(Clock::now()-begin).count();
        waited_+=last_wait_;
    }
    const PadInput mine=local_[field_],theirs=remote_[field_];
    ++field_;
    return options_.host?std::array<PadInput,2>{mine,theirs}:std::array<PadInput,2>{theirs,mine};
}

void Lockstep::sendWelcome(std::uint32_t echo) {
    Writer welcome(Welcome);welcome.u64(options_.program);welcome.u32(options_.rules);welcome.u8(std::uint8_t(options_.delay));welcome.u32(echo);
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
    out.u32(now());
    if(has_peer_stamp_) {
        out.u32(peer_stamp_);
        out.u16(std::uint16_t(std::min<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-peer_stamp_at_).count(),no_stamp-1)));
    } else {out.u32(0);out.u16(no_stamp);}
    transport_.send(out.bytes);
}

void Lockstep::handle(std::span<const std::uint8_t> packet) {
    const auto kind=kindOf(packet);
    if(!kind)return; // Not ours.
    Reader in{packet};
    switch(*kind) {
    case Hello: { // The joiner missed the answers.
        const auto program=in.u64();const auto rules=in.u32();const auto stamp=in.u32();
        if(options_.host && in.ok && program==options_.program && rules==options_.rules)sendWelcome(stamp);
        break;
    }
    case Start: // The joiner missed the go.
        if(options_.host) {Writer go(Go);go.u8(std::uint8_t(delay_));transport_.send(go.bytes);}
        break;
    case Welcome: case Go: break;
    case Inputs: {
        const unsigned has=in.u32(),first=in.u32(),count=in.u8();
        std::vector<PadInput> inputs(count);
        for(auto& input:inputs) {input.pressed=in.u16();for(auto& stick:input.sticks)stick=in.u8();}
        const unsigned check_field=in.u32();const std::uint64_t check=in.u64();
        const auto stamp=in.u32();const auto echo=in.u32();const auto held=in.u16();
        if(!in.ok)return;
        peer_has_=std::max(peer_has_,std::min<unsigned>(has,unsigned(local_.size())));
        for(unsigned n=0;n<count;++n)if(first+n==remote_.size())remote_.push_back(inputs[n]);
        if(check_field!=no_check) {
            if(const auto mine=checks_.find(check_field);mine!=checks_.end()) {compareCheck(check_field,mine->second,check);checks_.erase(mine);}
            else if(check_field>=field_)peer_checks_[check_field]=check; // Not reached here yet.
        }
        if(!has_peer_stamp_ || std::int32_t(stamp-peer_stamp_)>0) {peer_stamp_=stamp;peer_stamp_at_=Clock::now();has_peer_stamp_=true;}
        if(held!=no_stamp) {
            const double sample=double(std::int32_t(now()-echo-held));
            if(sample>=0 && sample<10000)rtt_ms_=rtt_ms_>0?rtt_ms_*0.9+sample*0.1:sample;
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
        address.host=false;
        if(colon==std::string_view::npos)
            if(auto targets=connection_code::readAll(rest);!targets.empty()) {
                address.address=targets[0].address;address.port=targets[0].port;address.targets=std::move(targets);
                return address;
            }
        if(colon==std::string_view::npos || !colon)throw std::runtime_error("BT2_NET: join:<address>:<port>: "+std::string(text));
        address.address=std::string(rest.substr(0,colon));address.port=port(rest.substr(colon+1));
        address.targets={{address.address,address.port}};
        return address;
    }
    throw std::runtime_error("BT2_NET is host:<port>, join:<address>:<port> or join:<code>: "+std::string(text));
}

NetworkSimulation parseNetworkSimulation(std::string_view text) {
    NetworkSimulation simulation;
    while(!text.empty()) {
        const auto comma=text.find(',');const auto part=text.substr(0,comma);
        text=comma==std::string_view::npos?std::string_view{}:text.substr(comma+1);
        const auto equals=part.find('=');
        unsigned value=0;
        const auto digits=equals==std::string_view::npos?std::string_view{}:part.substr(equals+1);
        const auto [end,error]=std::from_chars(digits.data(),digits.data()+digits.size(),value);
        if(digits.empty() || error!=std::errc{} || end!=digits.data()+digits.size())throw std::runtime_error("BT2_NET_SIM: latency=<ms>,jitter=<ms>,loss=<percent>: "+std::string(part));
        const auto name=part.substr(0,equals);
        if(name=="latency")simulation.latency=std::chrono::milliseconds(value);
        else if(name=="jitter")simulation.jitter=std::chrono::milliseconds(value);
        else if(name=="loss") {
            if(value>50)throw std::runtime_error("BT2_NET_SIM: loss is 0 to 50 percent");
            simulation.loss_every=value?100/value:0;
        }
        else throw std::runtime_error("BT2_NET_SIM: latency=<ms>,jitter=<ms>,loss=<percent>: "+std::string(part));
    }
    return simulation;
}

namespace {
class SimulatedNetwork final : public LockstepTransport {
public:
    SimulatedNetwork(std::unique_ptr<LockstepTransport> inner,const NetworkSimulation& simulation):inner_(std::move(inner)),simulation_(simulation) {}
    void send(std::span<const std::uint8_t> packet) override {inner_->send(packet);}
    std::vector<connection_code::Address> publicAddresses() const override {return inner_->publicAddresses();}
    bool findingAddresses() const override {return inner_->findingAddresses();}
    void punch(const std::vector<connection_code::Address>& to) override {inner_->punch(to);}
    std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout) override {
        const auto deadline=Clock::now()+timeout;
        while(true) {
            while(auto packet=inner_->receive(std::chrono::milliseconds(0)))hold(std::move(*packet));
            const auto now=Clock::now();
            if(!held_.empty() && held_.front().first<=now) {auto packet=std::move(held_.front().second);held_.pop_front();return packet;}
            if(now>=deadline)return std::nullopt;
            const auto until=held_.empty()?deadline:std::min(deadline,held_.front().first);
            if(auto packet=inner_->receive(std::chrono::duration_cast<std::chrono::milliseconds>(until-now)))hold(std::move(*packet));
        }
    }
private:
    std::unique_ptr<LockstepTransport> inner_;NetworkSimulation simulation_;
    std::deque<std::pair<Clock::time_point,std::vector<std::uint8_t>>> held_;
    std::mt19937 random_{7};unsigned received_=0;
    // A packet that came: lost, or held until its time (never before the one ahead of it).
    void hold(std::vector<std::uint8_t> packet) {
        if(simulation_.loss_every && ++received_%simulation_.loss_every==0)return;
        auto due=Clock::now()+simulation_.latency;
        if(simulation_.jitter.count())due+=std::chrono::milliseconds(std::uniform_int_distribution<long long>(-simulation_.jitter.count(),simulation_.jitter.count())(random_));
        if(!held_.empty())due=std::max(due,held_.back().first);
        held_.emplace_back(due,std::move(packet));
    }
};
}

std::unique_ptr<LockstepTransport> simulateNetwork(std::unique_ptr<LockstepTransport> inner,const NetworkSimulation& simulation) {
    return std::make_unique<SimulatedNetwork>(std::move(inner),simulation);
}

#ifdef _WIN32
namespace {
class UdpTransport final : public LockstepTransport {
public:
    // One IPv6 socket that also carries IPv4 (as ::ffff:a.b.c.d), so both kinds of address work without a second port.
    UdpTransport(bool host,const std::vector<connection_code::Address>& targets,std::uint16_t port,bool stun):host_(host),made_(Clock::now()) {
        WSADATA data;
        if(WSAStartup(MAKEWORD(2,2),&data))throw std::runtime_error("Online session: Winsock did not start");
        started_=true;
        socket_=::socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if(socket_==INVALID_SOCKET)throw error("no UDP socket");
        DWORD v6_only=0;setsockopt(socket_,IPPROTO_IPV6,IPV6_V6ONLY,reinterpret_cast<const char*>(&v6_only),sizeof v6_only);
        // A datagram to a port nobody listens on would make the next receive fail; it is only a lost packet.
        BOOL report=FALSE;DWORD returned=0;
        WSAIoctl(socket_,SIO_UDP_CONNRESET,&report,sizeof report,nullptr,0,&returned,nullptr,nullptr);
        sockaddr_in6 local{};local.sin6_family=AF_INET6;local.sin6_addr=in6addr_any;local.sin6_port=htons(host?port:0);
        if(::bind(socket_,reinterpret_cast<sockaddr*>(&local),sizeof local))throw error("port "+std::to_string(port)+" cannot be opened (another program may use it)");
        int size=sizeof local;getsockname(socket_,reinterpret_cast<sockaddr*>(&local),&size);
        port_=ntohs(local.sin6_port);
        for(const auto& t:targets) {
            sockaddr_in6 at{};
            if(resolve(t.address,t.port,at))targets_.push_back(at);
        }
        if(!host && targets_.empty())throw error("the host's address is not found");
        if(stun) {
            // Two public STUN servers: the port each saw tells whether the router keeps one port for every destination.
            for(const auto& [name,server_port]:{std::pair<const char*,std::uint16_t>{"stun.l.google.com",19302},{"stun.cloudflare.com",3478}}) {
                sockaddr_in6 server{};
                if(resolve(name,server_port,server,AF_INET))stun_servers_.push_back(server);
            }
            stun_ports_.assign(stun_servers_.size(),0);
            std::random_device random;
            for(auto& byte:transaction_)byte=std::uint8_t(random());
            ipv6_=globalIpv6();
        }
    }
    ~UdpTransport() override {
        if(socket_!=INVALID_SOCKET)closesocket(socket_);
        if(started_)WSACleanup();
    }
    void send(std::span<const std::uint8_t> packet) override {
        if(has_peer_) {sendTo(peer_,packet);return;}
        // The joiner greets every address of the host's code; while STUN is asked (at most 2 s) it waits, so its
        // router's port for the host comes after the STUN servers' and the prediction in its reply code holds.
        if(host_ || (stunAsking() && Clock::now()-made_<std::chrono::seconds(2)))return;
        for(const auto& target:targets_)sendTo(target,packet);
    }
    std::vector<connection_code::Address> publicAddresses() const override {
        std::vector<connection_code::Address> out;
        if(v4_)out.push_back(*v4_);
        if(!ipv6_.empty())out.push_back({ipv6_,port_});
        return out;
    }
    bool findingAddresses() const override {return stunAsking();}
    void punch(const std::vector<connection_code::Address>& to) override {
        punch_.clear();
        for(const auto& a:to) {
            const bool v6=a.address.find(':')!=std::string::npos;
            // IPv4: the ports around the given one too (a router that gives each destination a new port usually counts
            // up from the last); IPv6: exactly that port.
            for(int d=v6?0:-8;d<=(v6?0:56);++d) {
                const int p=int(a.port)+d;
                if(p<1 || p>65535)continue;
                sockaddr_in6 at{};
                if(resolve(a.address,std::uint16_t(p),at))punch_.push_back(at);
            }
        }
        last_punch_={};
        std::cerr<<"Online session: punching "<<punch_.size()<<" addresses toward the reply code's"<<std::endl;
    }
    std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout) override {
        const auto deadline=Clock::now()+timeout;
        while(true) {
            tend();
            // Waits in short slices, so STUN requests and punch packets keep going while a field or the connection waits.
            const auto left=std::chrono::duration_cast<std::chrono::microseconds>(std::max(std::min(deadline-Clock::now(),
                Clock::duration(std::chrono::milliseconds(50))),Clock::duration::zero()));
            fd_set set;FD_ZERO(&set);FD_SET(socket_,&set);
            timeval wait{long(left.count()/1000000),long(left.count()%1000000)};
            if(::select(0,&set,nullptr,nullptr,&wait)<=0) {
                if(Clock::now()>=deadline)return std::nullopt;
                continue;
            }
            std::vector<std::uint8_t> packet(1500);
            sockaddr_in6 from{};int from_size=sizeof from;
            const int size=::recvfrom(socket_,reinterpret_cast<char*>(packet.data()),int(packet.size()),0,reinterpret_cast<sockaddr*>(&from),&from_size);
            if(size<0)continue;
            packet.resize(std::size_t(size));
            if(stun::isStun(packet)) {stunAnswer(packet,from);continue;}
            if(packet.size()>=4 && std::equal(punch_magic.begin(),punch_magic.end(),packet.begin()))continue; // Only opens the way.
            const bool ours=packet.size()>=5 && std::equal(magic.begin(),magic.end(),packet.begin());
            bool same=has_peer_ && sameEndpoint(from,peer_);
            // The joiner: the first of the host's addresses to answer is the one used from then on.
            if(!same && !host_ && !has_peer_ && ours)
                for(const auto& target:targets_)
                    if(sameEndpoint(from,target)) {peer_=from;has_peer_=true;same=true;std::cerr<<"Online session: the host answered at "<<text(from)<<std::endl;break;}
            // The host's other player: the first to greet, or a new greeting once the last one has been silent for a
            // second (a joiner that started again comes from another port).
            if(!same) {
                if(!host_ || !ours || packet[4]!=Hello || (has_peer_ && Clock::now()-heard_<std::chrono::seconds(1)))continue;
                peer_=from;has_peer_=true;punch_.clear(); // Heard: the way is open.
                std::cerr<<"Online session: the other player greeted from "<<text(from)<<std::endl;
            }
            heard_=Clock::now();
            return packet;
        }
    }
private:
    static constexpr std::array<std::uint8_t,4> punch_magic{'B','T','2','P'};
    std::runtime_error error(const std::string& what) {return std::runtime_error("Online session: "+what+" (Winsock error "+std::to_string(WSAGetLastError())+")");}
    // An address of either kind as the dual socket uses it (IPv4 as ::ffff:a.b.c.d).
    static bool resolve(const std::string& address,std::uint16_t port,sockaddr_in6& out,int family=AF_UNSPEC) {
        addrinfo hints{};hints.ai_family=family;hints.ai_socktype=SOCK_DGRAM;
        addrinfo* found=nullptr;
        if(getaddrinfo(address.c_str(),std::to_string(port).c_str(),&hints,&found) || !found)return false;
        out={};out.sin6_family=AF_INET6;out.sin6_port=htons(port);
        if(found->ai_family==AF_INET6)std::memcpy(&out,found->ai_addr,sizeof out);
        else {
            const auto* v4=reinterpret_cast<const sockaddr_in*>(found->ai_addr);
            out.sin6_addr.u.Byte[10]=0xFF;out.sin6_addr.u.Byte[11]=0xFF;std::memcpy(&out.sin6_addr.u.Byte[12],&v4->sin_addr,4);
        }
        freeaddrinfo(found);
        return true;
    }
    static bool sameEndpoint(const sockaddr_in6& a,const sockaddr_in6& b) {
        return a.sin6_port==b.sin6_port && !std::memcmp(&a.sin6_addr,&b.sin6_addr,sizeof a.sin6_addr);
    }
    static std::string text(const sockaddr_in6& a) {
        char out[INET6_ADDRSTRLEN]{};
        if(IN6_IS_ADDR_V4MAPPED(&a.sin6_addr))inet_ntop(AF_INET,&a.sin6_addr.u.Byte[12],out,sizeof out);
        else inet_ntop(AF_INET6,&a.sin6_addr,out,sizeof out);
        return std::string(out)+":"+std::to_string(ntohs(a.sin6_port));
    }
    // This PC's global IPv6 address (2000::/3), the one it sends from; empty without one. A UDP socket "connected" to an
    // internet address sends nothing; it only picks the address a packet would leave from.
    static std::string globalIpv6() {
        const SOCKET probe=::socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if(probe==INVALID_SOCKET)return {};
        std::string out;
        sockaddr_in6 to{};to.sin6_family=AF_INET6;to.sin6_port=htons(53);inet_pton(AF_INET6,"2001:4860:4860::8888",&to.sin6_addr);
        sockaddr_in6 local{};int size=sizeof local;
        if(!::connect(probe,reinterpret_cast<sockaddr*>(&to),sizeof to) && !getsockname(probe,reinterpret_cast<sockaddr*>(&local),&size)
           && (local.sin6_addr.u.Byte[0]&0xE0)==0x20) {
            char found[INET6_ADDRSTRLEN]{};
            if(inet_ntop(AF_INET6,&local.sin6_addr,found,sizeof found))out=found;
        }
        closesocket(probe);
        return out;
    }
    void sendTo(const sockaddr_in6& to,std::span<const std::uint8_t> packet) {
        ::sendto(socket_,reinterpret_cast<const char*>(packet.data()),int(packet.size()),0,reinterpret_cast<const sockaddr*>(&to),sizeof to);
    }
    bool stunAsking() const {return !stun_servers_.empty() && !stun_settled_;}
    // A STUN server's answer: the port it saw. Once both answered (or 2 s after the first), the IPv4 address is settled:
    // the same port from both, or a router that gives each destination a new port, whose next one is predicted.
    void stunAnswer(std::span<const std::uint8_t> packet,const sockaddr_in6& from) {
        const auto m=stun::mapped(packet,transaction_);
        if(!m || stun_settled_)return;
        for(std::size_t n=0;n<stun_servers_.size();++n)
            if(sameEndpoint(from,stun_servers_[n]))stun_ports_[n]=m->port;
        in_addr ip{};ip.s_addr=htonl(m->ip);
        char found[INET_ADDRSTRLEN]{};
        if(inet_ntop(AF_INET,&ip,found,sizeof found))stun_ip_=found;
        if(!first_answer_)first_answer_=Clock::now();
        if(std::all_of(stun_ports_.begin(),stun_ports_.end(),[](unsigned p){return p!=0;}))settle();
    }
    void settle() {
        stun_settled_=true;
        std::vector<unsigned> seen;for(const unsigned p:stun_ports_)if(p)seen.push_back(p);
        if(seen.empty() || stun_ip_.empty()) {std::cerr<<"Online session: STUN: no answer"<<(ipv6_.empty()?"; no IPv6":"; IPv6 only")<<std::endl;return;}
        unsigned port=seen.back();
        const bool changes=seen.size()==2 && seen[0]!=seen[1];
        if(changes)port=unsigned(std::clamp(int(seen[1])+(int(seen[1])-int(seen[0])),1,65535)); // The next port it gives.
        v4_=connection_code::Address{stun_ip_,std::uint16_t(port)};
        std::cerr<<"Online session: STUN: ports seen";
        for(const unsigned p:stun_ports_)std::cerr<<' '<<p;
        std::cerr<<" (local "<<port_<<"): "<<(changes?"the router gives each destination a new port; predicted ":"the router keeps the port; ")<<port
                 <<(ipv6_.empty()?"; no IPv6":"; IPv6 too")<<std::endl;
    }
    // STUN every half second until both servers answered (2 s after the first answer, or 10 s without one); punch packets
    // every 100 ms until the joiner is heard.
    void tend() {
        const auto now=Clock::now();
        if(stunAsking()) {
            if((first_answer_ && now-*first_answer_>=std::chrono::seconds(2)) || now-made_>=std::chrono::seconds(10))settle();
            else if(now-last_stun_>=std::chrono::milliseconds(500)) {
                const auto request=stun::request(transaction_);
                for(std::size_t n=0;n<stun_servers_.size();++n)if(!stun_ports_[n])sendTo(stun_servers_[n],request);
                last_stun_=now;
            }
        }
        if(!punch_.empty() && now-last_punch_>=std::chrono::milliseconds(100)) {for(const auto& at:punch_)sendTo(at,punch_magic);last_punch_=now;}
    }
    bool host_,started_=false,has_peer_=false;
    Clock::time_point made_;
    SOCKET socket_=INVALID_SOCKET;std::uint16_t port_=0;
    sockaddr_in6 peer_{};Clock::time_point heard_{};
    std::vector<sockaddr_in6> targets_; // The joiner's: the host's addresses.
    std::vector<sockaddr_in6> stun_servers_;std::vector<unsigned> stun_ports_;stun::Transaction transaction_{};
    Clock::time_point last_stun_{};std::optional<Clock::time_point> first_answer_;bool stun_settled_=false;std::string stun_ip_;
    std::optional<connection_code::Address> v4_;std::string ipv6_;
    std::vector<sockaddr_in6> punch_;Clock::time_point last_punch_{};
};
}

std::unique_ptr<LockstepTransport> hostUdp(std::uint16_t port,bool stun) {return std::make_unique<UdpTransport>(true,std::vector<connection_code::Address>{},port,stun);}
std::unique_ptr<LockstepTransport> joinUdp(const std::vector<connection_code::Address>& host,bool stun) {return std::make_unique<UdpTransport>(false,host,0,stun);}
#endif
