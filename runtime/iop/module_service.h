#pragma once
#include "library_provider.h"
#include "kernel_provider.h"
#include "ioman_provider.h"
#include "sif_rpc_provider.h"
#include "vblank_provider.h"
#include "cdvd_provider.h"
#include <string_view>

struct NativeIopModule {
    std::string path;
    std::uint32_t base,entry,gp;
    std::span<const std::uint8_t> image;
    std::function<void(IopRuntime&)> exports;
    std::function<void(IopRuntime&,IopContext&)> execute;
    std::function<void(const IopRuntime&)> validate;
};

class NativeIopService {
public:
    struct Resident {unsigned id;std::uint32_t result;};
    explicit NativeIopService(unsigned epoch,std::vector<NativeIopModule> definitions):epoch_(epoch),definitions_(std::move(definitions)) {
        if(!epoch_)throw std::runtime_error("Native IOP needs a fresh epoch");
        runtime.map(0x1FF000,std::vector<std::uint8_t>(0x1000));
        runtime.map(0x1FE000,std::vector<std::uint8_t>(0x1000));
        install_iop_library_provider(runtime);install_iop_kernel_providers(runtime);install_iop_ioman_provider(runtime);
        install_iop_sif_rpc_provider(runtime);install_iop_vblank_provider(runtime);install_iop_cdvd_provider(runtime);
        runtime.execute=[this](IopRuntime& rt,IopContext& c){
            while(c.pc!=IopRuntime::sentinel) {
                rt.check(IopRuntime::physical(c.pc),4);
                const auto before=c.pc;bool dispatched=false;
                for(const auto& module:definitions_)if(IopRuntime::physical(c.pc)>=module.base && IopRuntime::physical(c.pc)<module.base+module.image.size()) {
                    module.execute(rt,c);dispatched=true;break;
                }
                if(!dispatched || c.pc==before)throw std::runtime_error("Unmapped or stalled native IOP module dispatch");
            }
        };
    }
    NativeIopService(const NativeIopService&)=delete;
    NativeIopService& operator=(const NativeIopService&)=delete;
    Resident load(unsigned epoch,std::string_view path,std::span<const std::uint8_t> arguments={}) {
        if(epoch!=epoch_ || failed_ || !arguments.empty() || residents.contains(std::string(path)))
            throw std::runtime_error("Invalid native IOP epoch, arguments, state or duplicate load");
        const NativeIopModule* found=nullptr;
        for(const auto& module:definitions_)if(module.path==path){found=&module;break;}
        if(!found)throw std::runtime_error("No compiled native IOP module for requested path: "+std::string(path));
        if(path.size()>255 || path.find('\0')!=std::string_view::npos)throw std::runtime_error("Invalid native IOP module path");
        try {
            runtime.map(found->base,found->image);found->exports(runtime);
            for(unsigned n=0;n<path.size();++n)runtime.write(0x1FE100+n,static_cast<unsigned char>(path[n]),1);
            runtime.write(0x1FE100+static_cast<std::uint32_t>(path.size()),0,1);
            runtime.write(0x1FE000,0x1FE100,4);runtime.write(0x1FE004,0,4);
            entry={};entry.pc=found->entry;entry.set(28,found->gp);entry.set(29,0x1FFF00);
            entry.set(31,IopRuntime::sentinel);entry.set(4,1);entry.set(5,0x1FE000);
            runtime.execute(runtime,entry);
            if(entry.pc!=IopRuntime::sentinel || (entry.get(2)!=0 && entry.get(2)!=2))throw std::runtime_error("IOP module did not return resident completion: "+std::to_string(entry.get(2)));
            found->validate(runtime);
            const Resident resident{next_id_++,entry.get(2)};residents.emplace(std::string(path),resident);return resident;
        } catch(...) {failed_=true;throw;}
    }
    void attachEeTransport(unsigned epoch) {
        if(epoch!=epoch_ || failed_ || runtime.sif_transport_attached)throw std::runtime_error("Invalid native SIF transport attachment");
        runtime.sif_transport_attached=true;
    }
    std::uint32_t allocateBootHeap(unsigned epoch,unsigned size);
    Resident loadBootBuffer(unsigned epoch,unsigned pointer);
    std::uint32_t freeBootHeap(unsigned epoch,unsigned pointer);
    unsigned buffer_loads=0;
    unsigned boot_heap_frees=0;
    unsigned boot_heap_allocations=0;
    bool supports(std::string_view path) const {for(const auto& m:definitions_)if(m.path==path)return true;return false;}
    unsigned epoch() const {return epoch_;}
    IopRuntime runtime;
    IopContext entry;
    std::map<std::string,Resident> residents;
private:
    unsigned epoch_,next_id_=1;
    bool failed_=false;
    std::vector<NativeIopModule> definitions_;
};
std::vector<NativeIopModule> bt2_compiled_iop_modules();
