#pragma once
#include "native_runtime.h"

// Scoped first-library registration. Existing clients/version replacement are
// deliberately unsupported; no captured kernel addresses are imported.
inline void install_iop_library_provider(IopRuntime& rt) {
    rt.providers[{"loadcore",6}]=[](IopRuntime& memory,IopContext& c) {
        const auto table=c.get(4);
        const auto bound=memory.export_bounds.find(table);
        if(bound==memory.export_bounds.end() || memory.read(table,4)!=0x41C00000)
            throw std::runtime_error("Unvalidated IOP export table");
        if(memory.read(table+4,4)!=0 || memory.read(table+10,2)!=0)
            throw std::runtime_error("Unsupported IOP export linkage/mode");
        std::string name;
        for(unsigned n=0;n<8;++n) {
            const auto ch=memory.read(table+12+n,1);
            if(ch) {if(ch<32 || ch>126)throw std::runtime_error("Invalid library name");name+=static_cast<char>(ch);}
        }
        if(name.empty() || memory.libraries.contains(name))
            throw std::runtime_error("IOP duplicate/version registration unsupported");
        IopRuntime::Library library{table,memory.read(table+8,2),{}};
        for(unsigned n=0;n<bound->second;++n) {
            const auto target=memory.read(table+20+n*4,4);
            if(target) {if(target%4)throw std::runtime_error("Unaligned IOP export");memory.check(IopRuntime::physical(target),4);}
            library.slots.push_back(target);
        }
        memory.libraries.emplace(name,std::move(library));
        memory.write(table,memory.library_head,4);
        memory.write(table+4,0,4);
        memory.library_head=table;
        return 0u;
    };
}
