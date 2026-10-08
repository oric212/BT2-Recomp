#include "providers.h"

namespace {
std::string iop_owned_string(IopRuntime& rt,std::uint32_t p) {
    std::string result;
    for(unsigned n=0;n<256;++n) {const auto ch=rt.read(p+n,1);if(!ch)return result;result+=static_cast<char>(ch);}
    throw std::runtime_error("Unterminated IOP driver string");
}
std::uint32_t iop_driver_callback(IopRuntime& rt,const IopContext& caller,const IopRuntime::Driver& driver,unsigned slot) {
    const auto target=rt.read(driver.operations+slot*4,4);
    if(!target || target%4 || !rt.execute || rt.driver_callback_active)throw std::runtime_error("Invalid or nested IOP driver callback");
    rt.check(IopRuntime::physical(target),4);
    IopContext c=caller;c.pc=target;c.branch_pending=false;c.delay_instruction=false;c.load_register=0;
    c.set(4,driver.descriptor);c.set(28,driver.gp);c.set(31,IopRuntime::sentinel);
    rt.entry_targets.insert(target);rt.driver_callback_active=true;
    try {rt.execute(rt,c);rt.driver_callback_active=false;}
    catch(...) {rt.driver_callback_active=false;throw;}
    if(c.pc!=IopRuntime::sentinel)throw std::runtime_error("IOP driver callback did not return");
    return c.get(2);
}
}

void install_iop_ioman_provider(IopRuntime& rt) {
    rt.providers[{"ioman",21}]=[](IopRuntime& memory,IopContext& c) {
        if(memory.driver_callback_active)throw std::runtime_error("Nested IOP driver mutation");
        const auto name=iop_owned_string(memory,c.get(4));
        for(auto& driver:memory.drivers)if(driver.descriptor && driver.name==name) {
            iop_driver_callback(memory,c,driver,1);driver={};return 0u;
        }
        return UINT32_MAX; // Public DelDrv returns -1 for an absent device.
    };
    rt.providers[{"ioman",20}]=[](IopRuntime& memory,IopContext& c) {
        if(memory.driver_callback_active)throw std::runtime_error("Nested IOP driver mutation");
        const auto free=std::find_if(memory.drivers.begin(),memory.drivers.end(),[](const auto& d){return !d.descriptor;});
        if(free==memory.drivers.end())return UINT32_MAX;
        const auto p=c.get(4);memory.check(IopRuntime::physical(p),20);
        IopRuntime::Driver driver{p,memory.read(p+16,4),c.get(28),iop_owned_string(memory,memory.read(p,4))};
        if(driver.name.empty())throw std::runtime_error("Empty IOP driver name");
        iop_owned_string(memory,memory.read(p+12,4));memory.check(IopRuntime::physical(driver.operations),8);
        *free=driver;
        try {if(iop_signed(iop_driver_callback(memory,c,driver,0))<0) {*free={};return UINT32_MAX;}}
        catch(...) {*free={};throw;}
        return 0u;
    };
}
