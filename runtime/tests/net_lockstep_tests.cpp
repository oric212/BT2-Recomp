// Online lockstep (runtime/host/net_lockstep.h): two sessions over an in-memory link that drops, delays and cuts
// packets, and over UDP on this PC; both must play the same input on every field, and every way a session ends must
// say why.
#include "host/connection_code.h"
#include "host/net_lockstep.h"
#include "host/stun.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <source_location>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#endif

namespace {
void check(bool ok,std::source_location at=std::source_location::current()) {
    if(!ok) {std::cerr<<"net lockstep: check failed at line "<<at.line()<<'\n';std::exit(1);}
}

using Clock=std::chrono::steady_clock;

// Both directions of a link: each packet is dropped one time in `drop_every` (0: never) and arrives `latency` later.
struct Link {
    std::mutex mutex;std::condition_variable arrived;
    std::deque<std::pair<Clock::time_point,std::vector<std::uint8_t>>> queue[2];
    unsigned drop_every=0,sent=0;std::chrono::milliseconds latency{0};bool cut=false;
};
class End final : public LockstepTransport {
public:
    End(Link& link,unsigned side):link_(link),side_(side) {}
    void send(std::span<const std::uint8_t> packet) override {
        std::lock_guard lock(link_.mutex);
        if(link_.cut || (link_.drop_every && ++link_.sent%link_.drop_every==0))return;
        link_.queue[1-side_].emplace_back(Clock::now()+link_.latency,std::vector<std::uint8_t>(packet.begin(),packet.end()));
        link_.arrived.notify_all();
    }
    std::optional<std::vector<std::uint8_t>> receive(std::chrono::milliseconds timeout) override {
        std::unique_lock lock(link_.mutex);
        const auto deadline=Clock::now()+timeout;
        auto& queue=link_.queue[side_];
        while(true) {
            if(!queue.empty() && queue.front().first<=Clock::now()) {auto packet=std::move(queue.front().second);queue.pop_front();return packet;}
            const auto now=Clock::now();
            if(now>=deadline)return std::nullopt;
            link_.arrived.wait_until(lock,queue.empty()?deadline:std::min(deadline,queue.front().first));
        }
    }
private:
    Link& link_;unsigned side_;
};

// What a player presses on a field: a different pattern for each side.
PadInput pressed(unsigned side,unsigned field) {
    PadInput input;input.pressed=std::uint16_t((field*7+side*3)%11==0?1u<<(field%16):0u);
    input.sticks[2]=std::uint8_t(side?255-field%256:field%256);
    return input;
}

struct Side {
    std::vector<std::array<PadInput,2>> played;std::string error;unsigned checks=0,delay=0,waiting_calls=0;double rtt_ms=0;
};
// One side's session: `fields` fields, the drift hash `hash(field)`; `before_field(field)` may end it early (false).
void play(LockstepTransport& transport,unsigned side,LockstepOptions options,unsigned fields,Side& out,std::function<std::uint64_t(unsigned)> hash,
          std::function<bool(unsigned)> before_field={}) {
    options.waiting=[&out](double){++out.waiting_calls;};
    try {
        Lockstep session(transport,options);
        if(!session.connect(std::chrono::seconds(5)))throw std::runtime_error("no connection");
        out.delay=session.delay();
        for(unsigned field=0;field<fields;++field) {
            if(before_field && !before_field(field))break;
            out.played.push_back(session.field(pressed(side,field),hash(field)));
            out.checks=session.checksCompared();out.rtt_ms=session.roundTripMs();
        }
    } catch(const std::exception& e) {out.error=e.what();}
}

LockstepOptions options(bool host,unsigned delay=2) {
    LockstepOptions o;o.host=host;o.delay=delay;o.program=42;o.rule_names="Rule A";o.silence=std::chrono::milliseconds(400);return o;
}
std::uint64_t same(unsigned field) {return field*31+5;}

bool contains(const std::string& text,std::string_view part) {return text.find(part)!=std::string::npos;}

// Both sides played the same input on the first `fields` fields: port 1 the host's, port 2 the joiner's, `delay`
// late. The side that finishes first leaves, which the other may hear on its last fields.
void checkAlike(const Side& host,const Side& joiner,unsigned fields,unsigned delay) {
    const auto ended=[](const Side& side){return side.error.empty() || contains(side.error,"left the session");};
    if(!ended(host) || !ended(joiner))std::cerr<<"host: "<<host.error<<"\njoiner: "<<joiner.error<<'\n';
    check(ended(host) && ended(joiner));
    check(host.played.size()>=fields && joiner.played.size()>=fields && host.delay==delay && joiner.delay==delay);
    for(unsigned field=0;field<fields;++field) {
        check(host.played[field]==joiner.played[field]);
        const PadInput neutral{};
        check(host.played[field][0]==(field<delay?neutral:pressed(0,field-delay)));
        check(host.played[field][1]==(field<delay?neutral:pressed(1,field-delay)));
    }
    check(host.checks>=fields/Lockstep::check_interval-1 && joiner.checks>=fields/Lockstep::check_interval-1);
}

void session(Link& link,Side& host,Side& joiner,unsigned fields,unsigned delay,std::function<std::uint64_t(unsigned)> joiner_hash=same,
             std::function<bool(unsigned)> joiner_before={},std::uint64_t joiner_program=42,std::uint32_t joiner_rules=0) {
    auto joiner_options=options(false);joiner_options.program=joiner_program;joiner_options.rules=joiner_rules;
    End a_end(link,0),b_end(link,1);
    std::thread a([&]{play(a_end,0,options(true,delay),fields,host,same);});
    std::thread b([&]{play(b_end,1,joiner_options,fields,joiner,joiner_hash,joiner_before);});
    a.join();b.join();
}
constexpr unsigned margin=2*Lockstep::max_delay+2; // Fields played past those compared.
}

int main() {
    {   // The delay chosen for a round trip: a home network 2 fields, 60 ms 3, a long way at most 8.
        check(Lockstep::delayFor(1,0)==2 && Lockstep::delayFor(20,0)==2 && Lockstep::delayFor(30,0)==3 && Lockstep::delayFor(60,0)==3 && Lockstep::delayFor(60,30)==4);
        check(Lockstep::delayFor(100,0)==5 && Lockstep::delayFor(2000,0)==Lockstep::max_delay);
    }
    {   // A clean link.
        Link link;Side host,joiner;
        session(link,host,joiner,600+margin,2);
        checkAlike(host,joiner,600,2);
    }
    {   // The smallest delay and the largest.
        for(const unsigned delay:{1u,Lockstep::max_delay}) {
            Link link;Side host,joiner;
            session(link,host,joiner,200+margin,delay);
            checkAlike(host,joiner,200,delay);
        }
    }
    {   // Every third packet lost, each 3 ms late: the repeats heal it, also while connecting.
        Link link;link.drop_every=3;link.latency=std::chrono::milliseconds(3);Side host,joiner;
        session(link,host,joiner,400+margin,2);
        checkAlike(host,joiner,400,2);
    }
    {   // The delay chosen by the joiner from the round trip it measured (20 ms each way: 40 ms, 3 fields), and the
        // round trip measured during the session from the inputs packets.
        Link link;link.latency=std::chrono::milliseconds(20);Side host,joiner;
        session(link,host,joiner,300+margin,0);
        check(host.delay==joiner.delay && host.delay>=3 && host.delay<=4);
        checkAlike(host,joiner,300,host.delay);
        check(host.rtt_ms>=35 && host.rtt_ms<=80 && joiner.rtt_ms>=35 && joiner.rtt_ms<=80);
    }
    {   // The joiner's game differs from field 120: both stop, each saying why.
        Link link;Side host,joiner;
        session(link,host,joiner,600,2,[](unsigned field){return field>=120?1:same(field);});
        check(contains(host.error,"went apart") && contains(joiner.error,"went apart"));
        check(host.played.size()<=125 && joiner.played.size()<=125);
    }
    {   // A different build is refused by both.
        Link link;Side host,joiner;
        session(link,host,joiner,10,2,same,{},43);
        check(contains(host.error,"different build") && contains(joiner.error,"different build"));
        check(host.played.empty() && joiner.played.empty());
    }
    {   // Different gameplay settings are refused by both, by name.
        Link link;Side host,joiner;
        session(link,host,joiner,10,2,same,{},42,1);
        check(contains(host.error,"Rule A must be alike") && contains(joiner.error,"Rule A must be alike"));
        check(host.played.empty() && joiner.played.empty());
    }
    {   // The joiner leaves after 100 fields: the host stops saying so.
        Link link;Side host,joiner;
        session(link,host,joiner,600,2,same,[](unsigned field){return field<100;});
        check(joiner.error.empty() && joiner.played.size()==100);
        check(contains(host.error,"left the session") && host.played.size()<=103);
    }
    {   // The link goes silent after field 50: both stop after the silence, having said meanwhile that they wait.
        Link link;Side host,joiner;
        session(link,host,joiner,600,2,same,[&](unsigned field){
            if(field==50) {std::lock_guard lock(link.mutex);link.cut=true;}
            return true;
        });
        check(contains(host.error,"no word") && contains(joiner.error,"no word"));
        check(host.waiting_calls>=3 && joiner.waiting_calls>=3);
    }
    {   // Nobody there: connecting gives up after its time, and can be called again.
        Link link;End end(link,0);
        Lockstep session(end,options(true));
        check(!session.connect(std::chrono::milliseconds(50)) && !session.connect(std::chrono::milliseconds(50)));
    }
    {   // UDP on this PC, through the simulated network (15 ms each way, a packet in 20 lost): the same inputs.
        const auto simulation=parseNetworkSimulation("latency=15,jitter=3,loss=5");
        check(simulation.latency.count()==15 && simulation.jitter.count()==3 && simulation.loss_every==20);
        auto host_end=simulateNetwork(hostUdp(47411),simulation);
        auto joiner_end=simulateNetwork(joinUdp("127.0.0.1",47411),simulation);
        Side host,joiner;
        std::thread a([&]{play(*host_end,0,options(true,0),300+margin,host,same);});
        std::thread b([&]{play(*joiner_end,1,options(false),300+margin,joiner,same);});
        a.join();b.join();
        checkAlike(host,joiner,300,host.delay);
        check(host.delay>=3 && host.rtt_ms>=25);
        for(const char* bad:{"latency=x","speed=3","loss=60","latency"}) {
            bool refused=false;
            try {parseNetworkSimulation(bad);} catch(const std::runtime_error&) {refused=true;}
            check(refused);
        }
    }
    {   // STUN (host/stun.h): a binding request, and the address its answer reports (XOR-MAPPED-ADDRESS, or the older
        // MAPPED-ADDRESS); an answer to another transaction, an error answer or a damaged one gives nothing.
        stun::Transaction id{};for(unsigned n=0;n<12;++n)id[n]=std::uint8_t(n*7+1);
        const auto request=stun::request(id);
        check(request.size()==20 && request[1]==1 && stun::isStun(request) && !stun::isStun(std::vector<std::uint8_t>{'B','T','2','L',1}));
        const auto answer=[&](std::uint16_t type,bool xored,std::uint32_t ip,std::uint16_t port) {
            std::vector<std::uint8_t> out{0x01,0x01,0x00,0x0C,0x21,0x12,0xA4,0x42};
            out.insert(out.end(),id.begin(),id.end());
            const std::uint16_t p=xored?std::uint16_t(port^0x2112):port;const std::uint32_t a=xored?ip^stun::cookie:ip;
            for(const std::uint8_t b:{std::uint8_t(type>>8),std::uint8_t(type),std::uint8_t(0),std::uint8_t(8),std::uint8_t(0),std::uint8_t(1),
                                       std::uint8_t(p>>8),std::uint8_t(p),std::uint8_t(a>>24),std::uint8_t(a>>16),std::uint8_t(a>>8),std::uint8_t(a)})out.push_back(b);
            return out;
        };
        const std::uint32_t ip=203u<<24|113u<<8|254u;
        const auto xored=stun::mapped(answer(0x0020,true,ip,61002),id),plain=stun::mapped(answer(0x0001,false,ip,7411),id);
        check(xored && xored->ip==ip && xored->port==61002 && plain && plain->ip==ip && plain->port==7411);
        auto other=answer(0x0020,true,ip,1);other[8]^=1;check(!stun::mapped(other,id));
        auto failed=answer(0x0020,true,ip,1);failed[1]=0x11;check(!stun::mapped(failed,id));
        auto cut=answer(0x0020,true,ip,1);cut.resize(26);check(!stun::mapped(cut,id));
    }
#ifdef _WIN32
    {   // Hole punching over UDP on this PC: the host's punch packets go to the address given, and a joiner's transport
        // drops them (and STUN messages) instead of handing them to the lockstep; the host's own packets still come.
        WSADATA data;check(!WSAStartup(MAKEWORD(2,2),&data));
        const SOCKET raw=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        sockaddr_in at{};at.sin_family=AF_INET;at.sin_port=htons(47415);inet_pton(AF_INET,"127.0.0.1",&at.sin_addr);
        check(raw!=INVALID_SOCKET && !::bind(raw,reinterpret_cast<sockaddr*>(&at),sizeof at));
        DWORD wait_ms=500;setsockopt(raw,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&wait_ms),sizeof wait_ms);
        auto host_end=hostUdp(47416);
        host_end->punch({"127.0.0.1",47415});
        check(!host_end->receive(std::chrono::milliseconds(250))); // Nobody greets; the punches go out meanwhile.
        char got[64];sockaddr_in from{};int from_size=sizeof from;
        const int size=::recvfrom(raw,got,sizeof got,0,reinterpret_cast<sockaddr*>(&from),&from_size);
        check(size==4 && std::string(got,4)=="BT2P" && ntohs(from.sin_port)==47416);
        // The raw socket as a host: a joiner's transport sends to it, then gets a punch, a STUN answer and a game packet back.
        auto joiner_end=joinUdp("127.0.0.1",47415);
        host_end.reset(); // No more punches; the ones still queued at the raw socket are skipped.
        const std::uint8_t hello[]{'B','T','2','L',1};joiner_end->send(hello);
        int greeting=0;
        for(int n=0;n<20 && greeting!=5;++n)greeting=::recvfrom(raw,got,sizeof got,0,reinterpret_cast<sockaddr*>(&from),&from_size);
        check(greeting==5);
        stun::Transaction none{};const auto stray=stun::request(none);
        for(const auto& packet:{std::vector<std::uint8_t>{'B','T','2','P'},stray,std::vector<std::uint8_t>{'B','T','2','L',3}})
            ::sendto(raw,reinterpret_cast<const char*>(packet.data()),int(packet.size()),0,reinterpret_cast<const sockaddr*>(&from),from_size);
        const auto first=joiner_end->receive(std::chrono::milliseconds(500));
        check(first && first->size()==5 && (*first)[4]==3 && !joiner_end->publicAddress());
        closesocket(raw);WSACleanup();
    }
    // BT2_TEST_STUN=1 (needs the internet, so not in the gate): the transport learns this PC's address from the public
    // STUN servers within a few seconds.
    if(const char* internet=std::getenv("BT2_TEST_STUN");internet && std::string(internet)=="1") {
        auto end=hostUdp(47417,true);
        for(int n=0;n<40 && !end->publicAddress();++n)end->receive(std::chrono::milliseconds(100));
        check(end->publicAddress().has_value() && connection_code::reachable(end->publicAddress()->address));
        std::cout<<"STUN: this PC is reachable at port "<<end->publicAddress()->port<<" of its internet address\n";
    }
#endif
    {   // Connection codes: eight characters for the usual port, twelve for another; read back in any case, with or
        // without dashes; I, L and O read as 1, 1 and 0; a code with any one character changed is refused.
        const auto code=*connection_code::make("203.0.113.254",connection_code::usual_port);
        check(code.size()==9 && code[4]=='-');
        const auto back=connection_code::read(code);
        check(back && back->address=="203.0.113.254" && back->port==connection_code::usual_port);
        std::string lower;for(const char c:code)if(c!='-')lower+=char(c>='A' && c<='Z'?c-'A'+'a':c);
        check(connection_code::read(lower) && connection_code::read(" "+code+" "));
        const auto other=*connection_code::make("192.168.1.20",40000);
        check(other.size()==14 && connection_code::read(other)->port==40000 && connection_code::read(other)->address=="192.168.1.20");
        for(const char* address:{"0.0.0.0","255.255.255.255","10.0.0.1","8.8.4.4"})
            check(connection_code::read(*connection_code::make(address,1))->address==address);
        unsigned refused=0,changes=0;
        for(std::size_t at=0;at<code.size();++at) {
            if(code[at]=='-')continue;
            for(const char c:connection_code::digits) {
                if(c==code[at])continue;
                auto changed=code;changed[at]=c;++changes;
                refused+=!connection_code::read(changed);
            }
        }
        check(refused*256>=changes*250); // All but about one in 256.
        std::string with_o=code;
        for(auto& c:with_o)if(c=='0')c='O';else if(c=='1')c='L';
        check(connection_code::read(with_o) && connection_code::read(with_o)->address=="203.0.113.254");
        for(const char* bad:{"","ABCD","ABCD-EFGH-J","ABCD-EFGU","1.2.3.4","my-host.example.net"})check(!connection_code::read(bad));
        const auto joined=parseLockstepAddress("join:"+code);
        check(!joined.host && joined.address=="203.0.113.254" && joined.port==connection_code::usual_port);
        check(connection_code::reachable("203.0.113.254") && connection_code::reachable("8.8.8.8"));
        for(const char* shared:{"100.64.0.1","100.127.255.1","10.1.2.3","172.20.0.1","192.168.1.1","127.0.0.1","bad"})check(!connection_code::reachable(shared));
        check(connection_code::reachable("100.128.0.1") && connection_code::reachable("172.32.0.1"));
        for(const char* bad:{"1.2.3","1.2.3.256","a.b.c.d","1.2.3.4.5",""})check(!connection_code::make(bad,7411));
    }
    {   // Addresses.
        const auto host=parseLockstepAddress("host:7400");
        check(host.host && host.port==7400);
        const auto join=parseLockstepAddress("join:192.168.1.20:7400");
        check(!join.host && join.address=="192.168.1.20" && join.port==7400);
        for(const char* bad:{"host:0","host:70000","join:7400","join::7400","peer:1.2.3.4:5","host:12a"}) {
            bool refused=false;
            try {parseLockstepAddress(bad);} catch(const std::runtime_error&) {refused=true;}
            check(refused);
        }
    }
    std::cout<<"Net lockstep checks passed\n";
}
