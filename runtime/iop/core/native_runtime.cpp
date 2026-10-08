#include "native_runtime.h"

// IopRuntime core: owned memory ranges, interrupt dispatch and import resolution
// (registered providers, otherwise exports of loaded native modules).

void IopRuntime::runInterrupt(IopContext& handler) {
    in_interrupt=true;cpu_interrupts_enabled=false;
    try {execute(*this,handler);in_interrupt=false;cpu_interrupts_enabled=true;}
    catch(...) {in_interrupt=false;cpu_interrupts_enabled=true;throw;}
}

IopContext IopRuntime::interruptContext(IopContext base,std::uint32_t pc,std::uint32_t argument,std::uint32_t gp) {
    base.pc=pc;base.branch_pending=false;base.delay_instruction=false;base.load_register=0;
    base.set(4,argument);base.set(28,gp);base.set(31,sentinel);return base;
}

void IopRuntime::map(std::uint32_t base,std::span<const std::uint8_t> data) {
    if(!data.size() || base>=ram_size || data.size()>ram_size-base)throw std::runtime_error("IOP mapping outside owned RAM");
    for(auto [lo,hi]:ranges)if(base<hi && lo<base+data.size())throw std::runtime_error("Overlapping IOP ownership");
    std::copy(data.begin(),data.end(),ram.begin()+base);ranges.emplace_back(base,base+static_cast<std::uint32_t>(data.size()));
}

void IopRuntime::check(std::uint32_t address,unsigned size) {
    // Cached/uncached RAM aliases are normalized only within physical IOP RAM.
    for(auto [lo,hi]:ranges)if(address>=lo && address<=hi && size<=hi-address)return;
    blocked_address=address;throw std::runtime_error("Unimplemented IOP memory/hardware access");
}

std::uint32_t IopRuntime::import(const char* library,unsigned ordinal,IopContext& c) {
    const auto key=std::make_pair(std::string(library),ordinal);
    imports.push_back(key.first+":"+std::to_string(ordinal));
    for(unsigned n=0;n<4;++n)last_import_arguments[n]=c.get(4+n);
    last_import_ra=c.get(31);last_import_sp=c.get(29);
    const auto found=providers.find(key);
    if(found==providers.end()) {
        const auto library_entry=libraries.find(library);
        if(library_entry==libraries.end() || ordinal>=library_entry->second.slots.size() || !library_entry->second.slots[ordinal] || !execute)
            throw std::runtime_error("Unresolved IOP import: "+imports.back());
        if(c.load_register || c.branch_pending)throw std::runtime_error("Pending CPU state at native IOP export call unsupported");
        const auto target=library_entry->second.slots[ordinal];check(physical(target),4);
        IopContext callee=c;callee.pc=target;callee.delay_instruction=false;callee.set(28,library_entry->second.gp);callee.set(31,sentinel);
        entry_targets.insert(target);
        try {execute(*this,callee);}
        catch(const IopYield&) {throw std::runtime_error("Native IOP export suspension requires retained call frames");}
        if(callee.pc!=sentinel)throw std::runtime_error("Native IOP export did not return");
        const auto ra=c.get(31),gp=c.get(28);c.r=callee.r;c.set(31,ra);c.set(28,gp);c.hi=callee.hi;c.lo=callee.lo;
        c.load_register=callee.load_register;c.load_value=callee.load_value;
        native_export_results[key.first+":"+std::to_string(ordinal)]=callee.get(2);
        return callee.get(2);
    }
    return found->second(*this,c);
}
