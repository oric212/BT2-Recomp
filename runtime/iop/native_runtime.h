#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

struct IopContext {
    std::array<std::uint32_t,32> r{};
    std::uint32_t pc=0,hi=0,lo=0,branch_target=0,load_value=0;
    unsigned load_register=0;
    bool branch_pending=false,delay_instruction=false;
    std::uint32_t get(unsigned n) const {return n?r.at(n):0;}
    void set(unsigned n,std::uint32_t value) {if(n)r.at(n)=value;}
    std::uint32_t merge(unsigned n) const {return load_register==n && n?load_value:get(n);}
    void advance() {
        const auto next=branch_pending?branch_target:pc+4;
        delay_instruction=branch_pending;branch_pending=false;
        if(load_register)set(load_register,load_value);
        load_register=0;pc=next;r[0]=0;
    }
    void load(unsigned n,std::uint32_t value) {load_register=n;load_value=value;}
    void branch(std::uint32_t target) {
        if(delay_instruction)throw std::runtime_error("IOP branch in delay slot unsupported");
        branch_pending=true;branch_target=target;
    }
};

class IopRuntime {
public:
    static constexpr std::uint32_t ram_size=2*1024*1024,sentinel=0xFFFFFFFC;
    using Provider=std::function<std::uint32_t(IopRuntime&,IopContext&)>;
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(ram_size);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> ranges;
    std::map<std::pair<std::string,unsigned>,Provider> providers;
    struct Library {std::uint32_t table; unsigned version; std::vector<std::uint32_t> slots;};
    std::map<std::string,Library> libraries;
    std::map<std::uint32_t,unsigned> export_bounds;
    std::uint32_t library_head=0;
    std::set<std::uint32_t> entries;
    std::vector<std::string> imports;
    std::uint32_t executing_pc=0,blocked_address=0;
    unsigned steps=0,access_width=0;
    std::uint32_t access_value=0;
    std::string access_kind;
    void map(std::uint32_t base,std::span<const std::uint8_t> data) {
        if(!data.size() || base>=ram_size || data.size()>ram_size-base)throw std::runtime_error("IOP mapping outside owned RAM");
        for(auto [lo,hi]:ranges)if(base<hi && lo<base+data.size())throw std::runtime_error("Overlapping IOP ownership");
        std::copy(data.begin(),data.end(),ram.begin()+base);ranges.emplace_back(base,base+static_cast<std::uint32_t>(data.size()));
    }
    void check(std::uint32_t address,unsigned size) {
        // Cached/uncached RAM aliases are normalized only within physical IOP RAM.
        for(auto [lo,hi]:ranges)if(address>=lo && address<=hi && size<=hi-address)return;
        blocked_address=address;throw std::runtime_error("Unimplemented IOP memory/hardware access");
    }
    static std::uint32_t physical(std::uint32_t address) {return address&0x1FFFFFFF;}
    std::uint32_t read(std::uint32_t address,unsigned size) {
        if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
        access_width=size;access_kind="read";access_value=0;
        const auto p=physical(address);check(p,size);
        if((size==2 && p%2) || (size==4 && p%4))throw std::runtime_error("Unaligned IOP load");
        std::uint32_t value=0;std::memcpy(&value,ram.data()+p,size);return value;
    }
    void write(std::uint32_t address,std::uint32_t value,unsigned size) {
        if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
        access_width=size;access_kind="write";access_value=value;
        const auto p=physical(address);check(p,size);
        if((size==2 && p%2) || (size==4 && p%4))throw std::runtime_error("Unaligned IOP store");
        std::memcpy(ram.data()+p,&value,size);
    }
    void tick(IopContext& c) {executing_pc=c.pc;if(++steps>100000)throw std::runtime_error("IOP diagnostic budget exceeded");}
    std::uint32_t import(const char* library,unsigned ordinal,IopContext& c) {
        const auto key=std::make_pair(std::string(library),ordinal);
        imports.push_back(key.first+":"+std::to_string(ordinal));
        const auto found=providers.find(key);
        if(found==providers.end())throw std::runtime_error("Unresolved IOP import: "+imports.back());
        return found->second(*this,c);
    }
};
inline std::int32_t iop_signed(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}

inline std::uint32_t iop_add(std::uint32_t a,std::uint32_t b) {
    const auto v=static_cast<std::int64_t>(iop_signed(a))+iop_signed(b);
    if(v<INT32_MIN || v>INT32_MAX)throw std::runtime_error("IOP signed overflow exception unsupported");
    return static_cast<std::uint32_t>(v);
}
inline std::uint32_t iop_sub(std::uint32_t a,std::uint32_t b) {
    const auto v=static_cast<std::int64_t>(iop_signed(a))-iop_signed(b);
    if(v<INT32_MIN || v>INT32_MAX)throw std::runtime_error("IOP signed overflow exception unsupported");
    return static_cast<std::uint32_t>(v);
}
inline std::pair<std::uint32_t,std::uint32_t> iop_div(std::uint32_t a,std::uint32_t b,bool sign) {
    if(!b)return {sign && iop_signed(a)<0?1u:UINT32_MAX,a};
    if(sign) {
        if(a==0x80000000 && b==UINT32_MAX)return {a,0};
        return {static_cast<std::uint32_t>(iop_signed(a)/iop_signed(b)),static_cast<std::uint32_t>(iop_signed(a)%iop_signed(b))};
    }
    return {a/b,a%b};
}
