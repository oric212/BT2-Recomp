// Online lockstep (runtime/host/net_lockstep.h): two sessions over an in-memory link that drops, delays and cuts
// packets; both must play the same input on every field, and every way a session ends must say why.
#include "host/net_lockstep.h"
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <source_location>
#include <thread>

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
    std::vector<std::array<PadInput,2>> played;std::string error;unsigned checks=0,delay=0;
};
// One side's session: `fields` fields, the drift hash `hash(field)`; `before_field(field)` may end it early (false).
void play(Link& link,unsigned side,LockstepOptions options,unsigned fields,Side& out,std::function<std::uint64_t(unsigned)> hash,
          std::function<bool(unsigned)> before_field={}) {
    End end(link,side);
    try {
        Lockstep session(end,options);
        if(!session.connect(std::chrono::seconds(5)))throw std::runtime_error("no connection");
        out.delay=session.delay();
        for(unsigned field=0;field<fields;++field) {
            if(before_field && !before_field(field))break;
            out.played.push_back(session.field(pressed(side,field),hash(field)));
            out.checks=session.checksCompared();
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
    std::thread a([&]{play(link,0,options(true,delay),fields,host,same);});
    std::thread b([&]{play(link,1,joiner_options,fields,joiner,joiner_hash,joiner_before);});
    a.join();b.join();
}
constexpr unsigned margin=2*Lockstep::max_delay+2; // Fields played past those compared.
}

int main() {
    {   // A clean link.
        Link link;Side host,joiner;
        session(link,host,joiner,600+margin,2);
        checkAlike(host,joiner,600,2);
    }
    {   // No delay at all, and the largest.
        for(const unsigned delay:{0u,Lockstep::max_delay}) {
            Link link;Side host,joiner;
            session(link,host,joiner,200+margin,delay);
            checkAlike(host,joiner,200,delay);
        }
    }
    {   // Every third packet lost, each 3 ms late: the repeats heal it.
        Link link;link.drop_every=3;link.latency=std::chrono::milliseconds(3);Side host,joiner;
        session(link,host,joiner,400+margin,2);
        checkAlike(host,joiner,400,2);
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
    {   // The link goes silent after field 50: both stop after the silence.
        Link link;Side host,joiner;
        session(link,host,joiner,600,2,same,[&](unsigned field){
            if(field==50) {std::lock_guard lock(link.mutex);link.cut=true;}
            return true;
        });
        check(contains(host.error,"no word") && contains(joiner.error,"no word"));
    }
    {   // Nobody there: connecting gives up after its time, and can be called again.
        Link link;End end(link,0);
        Lockstep session(end,options(true));
        check(!session.connect(std::chrono::milliseconds(50)) && !session.connect(std::chrono::milliseconds(50)));
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
