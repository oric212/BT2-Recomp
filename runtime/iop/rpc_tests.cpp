#include "sif_rpc_provider.h"
#include <iostream>
void check(bool v){if(!v)throw std::runtime_error("Native RPC ownership test differs");}
template<class F> void rejected(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}check(failed);}
int main(){try {
    IopRuntime rt;rt.map(0x50000,std::vector<std::uint8_t>(0x400));install_iop_sif_rpc_provider(rt);
    IopContext c;c.set(2,42);c.set(28,0x59290);c.set(29,0x50300);
    check(rt.import("sifman",29,c)==0);rejected([&]{rt.import("sifcmd",14,c);});check(!rt.rpc_initialized);
    rt.sif_transport_attached=true;c.set(4,1);rejected([&]{rt.import("sifcmd",14,c);});c.set(4,0);
    check(rt.import("sifcmd",14,c)==42 && rt.rpc_initialized);check(rt.import("sifcmd",14,c)==42);
    rt.threads.emplace(0x20001,IopRuntime::Thread{0x2000000,0,0x50080,0,0x800,20,0x59290,{},true});
    c.set(4,0x50100);c.set(5,0x20001);auto before=rt.ram;
    rejected([&]{rt.import("sifcmd",19,c);});check(rt.ram==before && rt.rpc_queues.empty());
    rt.current_thread=0x20001;check(rt.import("thbase",20,c)==rt.current_thread);check(rt.import("sifcmd",19,c)==42);
    before=rt.ram;rejected([&]{rt.import("sifcmd",19,c);});check(rt.ram==before);
    c.set(4,0x50200);c.set(5,0x80000400);c.set(6,0x50080);c.set(7,0x500C0);
    rt.write(0x50318,0x50100,4);rt.write(0x5020C,0xDEADBEEF,4);
    rt.write(0x50310,0x50084,4);before=rt.ram;rejected([&]{rt.import("sifcmd",17,c);});check(rt.ram==before && rt.rpc_servers.empty());
    rt.write(0x50310,0,4);check(rt.import("sifcmd",17,c)==42 && rt.read(0x5020C,4)==0xDEADBEEF && rt.entries.empty());
    check(rt.rpc_servers.at(0x80000400).gp==0x59290 && rt.read(0x50108,4)==0x50200);
    before=rt.ram;rejected([&]{rt.import("sifcmd",17,c);});check(rt.ram==before);
    c.set(4,0x50100);rt.write(0x5010C,0x50200,4);rejected([&]{rt.import("sifcmd",22,c);});check(!rt.threads.at(rt.current_thread).waiting);
    rt.write(0x5010C,0,4);bool yielded=false;try{rt.import("sifcmd",22,c);}catch(const IopYield&){yielded=true;}
    check(yielded && rt.threads.at(rt.current_thread).waiting && rt.threads.at(rt.current_thread).wait_rpc==0x50100 && !rt.threads.at(rt.current_thread).ready);
    std::cout<<"Native RPC initialization, ownership, registration and no-request wait verified.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
