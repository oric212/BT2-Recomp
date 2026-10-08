#include "module_service.h"
#include "fileio_boot_native.h"
#include "embedded_native.h"
void bt2_iop_embedded(IopRuntime&,IopContext&);
void bt2_iop_embedded_second(IopRuntime&,IopContext&);
void bt2_iop_fileio_boot(IopRuntime&,IopContext&);

// Boot heap: IOP heap allocations/frees for EE uploads and loading embedded modules from uploaded buffers.

std::uint32_t NativeIopService::allocateBootHeap(unsigned epoch,unsigned size) {
    const bool second=boot_heap_allocations==1 && boot_heap_frees==1 && buffer_loads==1 && size==0x81800;
    const bool third=boot_heap_allocations==2 && boot_heap_frees==1 && size==0x8D0 && disc_search_calls==6 && disc_read_calls==1;
    const bool fourth=boot_heap_allocations==3 && boot_heap_frees==1 && size==0x41040 && cri_control_calls==2;
    const bool fifth=boot_heap_allocations==4 && boot_heap_frees==1 && size==0x8D0 && cri_control_calls==34;
    const bool sixth=boot_heap_allocations==5 && boot_heap_frees==1 && size==0x4040 && cri_create_calls==2;
    // Main reloads the embedded buffer after the first archive load; captured placement 0x196100.
    const bool seventh=boot_heap_allocations==6 && boot_heap_frees==1 && buffer_loads==1 && size==0x360 && cri_control_calls>=38;
    // Main's first sound-bank load: two blocks, first-fit into the freed reload block (captured
    // rpc-sound-bank-after: 0x196100 and 0x196400).
    const bool bank_first=boot_heap_allocations==7 && boot_heap_frees==2 && buffer_loads==2 && size==0x2C0;
    const bool bank_second=boot_heap_allocations==8 && boot_heap_frees==2 && buffer_loads==2 && size==0x180;
    const bool later=seventh || bank_first || bank_second;
    const unsigned pointer=bank_second?0x196400u:bank_first?0x196100u:seventh?0x196100u:sixth?0x192000u:fifth?0x191700u:fourth?0x150600u:third?0x14FD00u:0xCE500u;
    if(epoch!=epoch_ || failed_ || (!second && !third && !fourth && !fifth && !sixth && !later && (boot_heap_allocations || size!=0x360)) || residents.size()!=13)
        throw std::runtime_error("Unsupported boot IOP heap allocation");
    // Reject conflicting ownership before touching service input or executing code.
    for(auto [lo,hi]:runtime.ranges)if(lo<pointer+((size+255)&~255u) && pointer<hi)
        throw std::runtime_error("Captured IOP heap overlaps native ownership");
    try {
        // Only the verified allocation handler is entered; FILEIO boot entry/workers are not replayed.
        if(!second && !third && !fourth && !fifth && !sixth && !later) {runtime.map(iop_fileio_boot_base,iop_fileio_boot_image);iop_fileio_boot_export_bounds(runtime);
        definitions_.push_back({"boot:FILEIO.heap",iop_fileio_boot_base,iop_fileio_boot_base+0x27D4,iop_fileio_boot_gp,
            iop_fileio_boot_image,iop_fileio_boot_export_bounds,bt2_iop_fileio_boot,[](const IopRuntime&){throw std::runtime_error("Boot FILEIO residency not implemented");}}); }
        runtime.write(iop_fileio_boot_base+0x4050,size,4); // Owned translated heap service input buffer.
        runtime.providers[{"sysmem",4}]=[this,size,second,third,fourth,fifth,sixth,seventh,bank_first,bank_second,pointer](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0 || c.get(5)!=size || c.get(6)!=0 || boot_heap_allocations!=(bank_second?8u:bank_first?7u:seventh?6u:sixth?5u:fifth?4u:fourth?3u:third?2u:second?1u:0u))
                throw std::runtime_error("Unsupported native AllocSysMemory request");
            // Captured allocations reuse the released block or append the CRI block; 256-byte rounding.
            rt.map(pointer,std::vector<std::uint8_t>((size+255)&~255u));
            ++boot_heap_allocations;return pointer;
        };
        IopContext c;c.pc=iop_fileio_boot_base+0x27D4;c.set(4,1);c.set(5,iop_fileio_boot_base+0x4050);c.set(6,4);
        c.set(28,iop_fileio_boot_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);
        runtime.execute(runtime,c);runtime.providers.erase({"sysmem",4});
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_fileio_boot_base+0x3FE0 || !boot_heap_allocations
            || runtime.read(c.get(2),4)!=pointer)throw std::runtime_error("Native FILEIO heap handler effects differ");
        return runtime.read(c.get(2),4);
    } catch(...) {runtime.providers.erase({"sysmem",4});failed_=true;throw;}
}

NativeIopService::Resident NativeIopService::loadBootBuffer(unsigned epoch,unsigned pointer) {
    // Second load: main's reload at the captured 0x196100 buffer, image at 0x196530 (same +0x430 layout).
    const bool reload=buffer_loads==1 && boot_heap_frees==1 && boot_heap_allocations==7 && pointer==0x196100;
    if(epoch!=epoch_ || failed_ || (!reload && (pointer!=0xCE500 || boot_heap_allocations!=1 || buffer_loads)) || residents.size()!=13)
        throw std::runtime_error("Unsupported native memory module load");
    const std::uint32_t image=reload?0x196530u:0xCE930u;
    runtime.check(pointer,864);
    if(!std::equal(iop_embedded_file.begin(),iop_embedded_file.end(),runtime.ram.begin()+pointer))
        throw std::runtime_error("Owned embedded IRX bytes differ");
    // The SDK's command -9 returns the CDVDMAN version. This captured startup
    // profile is 0x225 from the disc-matched IOPRP module header; support is
    // restricted to this module's exact import PC.
    const auto previous=runtime.providers.at({"cdvdman",50});
    unsigned queries=0;
    try {
        runtime.map(image,reload?iop_embedded_second_image:iop_embedded_image);
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0xFFFFFFF7)return previous(rt,c);
            if(c.pc!=image+0x54 || c.get(5)!=0x1FFEF0 || queries++)throw std::runtime_error("Embedded CDVD version query differs");
            return 0x225u;
        };
        entry={};entry.pc=image;entry.set(28,image+0x8080);entry.set(29,0x1FFF00);entry.set(31,IopRuntime::sentinel);
        if(reload)bt2_iop_embedded_second(runtime,entry);else bt2_iop_embedded(runtime,entry);
        runtime.providers[{"cdvdman",50}]=previous;
        if(entry.pc!=IopRuntime::sentinel || entry.get(2)!=1 || queries!=1)
            throw std::runtime_error("Embedded nonresident completion differs");
        // modload result & 3 == 1 releases the loaded image. Keep the source heap
        // separately owned; freed code bytes are diagnostic residue only.
        std::erase(runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{image,image+0x90});
        ++buffer_loads;return {next_id_++,entry.get(2)};
    }catch(...) {runtime.providers[{"cdvdman",50}]=previous;failed_=true;throw;}
}

std::uint32_t NativeIopService::freeBootHeap(unsigned epoch,unsigned pointer) {
    const bool reload=pointer==0x196100 && boot_heap_allocations==7 && buffer_loads==2 && boot_heap_frees==1;
    if(epoch!=epoch_ || failed_ || (!reload && (pointer!=0xCE500 || boot_heap_allocations!=1 || buffer_loads!=1 || boot_heap_frees)))
        throw std::runtime_error("Unsupported native boot heap free");
    runtime.check(pointer,0x400);
    unsigned frees=0;
    try {
        runtime.write(iop_fileio_boot_base+0x4050,pointer,4);
        runtime.providers[{"sysmem",5}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=pointer || frees++)throw std::runtime_error("Native FreeSysMemory ownership differs");
            std::erase(rt.ranges,std::pair<std::uint32_t,std::uint32_t>{pointer,pointer+0x400});return 0u;
        };
        IopContext c;c.pc=iop_fileio_boot_base+0x27D4;c.set(4,2);c.set(5,iop_fileio_boot_base+0x4050);c.set(6,4);
        c.set(28,iop_fileio_boot_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        runtime.providers.erase({"sysmem",5});
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_fileio_boot_base+0x3FE0 || runtime.read(c.get(2),4) || frees!=1)
            throw std::runtime_error("Native FILEIO free handler effects differ");
        ++boot_heap_frees;return 0;
    }catch(...) {runtime.providers.erase({"sysmem",5});failed_=true;throw;}
}
