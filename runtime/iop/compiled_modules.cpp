#include "module_service.h"
#include "disc_native.h"
#include "disc_metadata.h"
#include "disc_sectors.h"
void bt2_iop_disc(IopRuntime&,IopContext&);
#include "sio2man_native.h"
#include "mcman_native.h"
#include "mcserv_native.h"
#include "dbcman_native.h"
#include "sio2d_native.h"
#include "ds2ud_native.h"
#include "libsd_native.h"
#include "sdrdrv_native.h"
#include "modhsyn_native.h"
#include "modsesq2_native.h"
#include "cdvdstm_native.h"
#include "sounds_native.h"
#include "cri_adxi_native.h"
#include "fileio_boot_native.h"
#include "embedded_native.h"
void bt2_iop_embedded(IopRuntime&,IopContext&);
void bt2_iop_fileio_boot(IopRuntime&,IopContext&);
void bt2_iop_cri_adxi(IopRuntime&,IopContext&);
void bt2_iop_sounds(IopRuntime&,IopContext&);
void bt2_iop_cdvdstm(IopRuntime&,IopContext&);
void bt2_iop_modsesq2(IopRuntime&,IopContext&);
void bt2_iop_modhsyn(IopRuntime&,IopContext&);
void bt2_iop_sdrdrv(IopRuntime&,IopContext&);
void bt2_iop_libsd(IopRuntime&,IopContext&);
void bt2_iop_ds2ud(IopRuntime&,IopContext&);
void bt2_iop_sio2d(IopRuntime&,IopContext&);
void bt2_iop_dbcman(IopRuntime&,IopContext&);
void bt2_iop_mcserv(IopRuntime&,IopContext&);
void bt2_iop_mcman(IopRuntime&,IopContext&);
void bt2_iop_module(IopRuntime&,IopContext&);
std::vector<NativeIopModule> bt2_compiled_iop_modules() {
    return {{"cdrom0:\\IRX\\SIO2MAN.IRX;1",iop_module_base,iop_module_entry,iop_module_gp,iop_expected_image,
        iop_export_bounds,bt2_iop_module,[](const IopRuntime& rt) {
            const auto lib=rt.libraries.find("sio2man");
            if(lib==rt.libraries.end() || lib->second.version!=0x204 || rt.threads.size()!=1 || rt.events.size()!=1 || rt.semaphores.size()!=1
                || !rt.sio2_control_known || rt.sio2_control!=0x3BC || !rt.cpu_interrupts_enabled)
                throw std::runtime_error("Native SIO2MAN resident effects differ");
            const auto& worker=rt.threads.begin()->second;const auto& event=rt.events.begin()->second;const auto& sema=rt.semaphores.begin()->second;
            if(!worker.waiting || worker.wait_bits!=0x4155 || worker.wait_mode!=1 || worker.wait_event!=rt.events.begin()->first
                || worker.entry!=iop_module_base+0x39C || worker.gp!=iop_module_gp || worker.priority!=16 || worker.stacksize!=0x800
                || event.attr!=2 || event.bits || sema.count!=1 || sema.maximum!=64)
                throw std::runtime_error("Native SIO2MAN worker/kernel state differs");
            const auto irq=rt.interrupts.find(17);
            if(irq==rt.interrupts.end() || !irq->second.enabled || irq->second.mode!=1 || irq->second.handler!=iop_module_base+0x584)
                throw std::runtime_error("Native SIO2MAN IRQ state differs");
            for(unsigned channel:{11u,12u}) {
                const auto dma=rt.dma_channels.find(channel);
                if(dma==rt.dma_channels.end() || !dma->second.enabled_known || !dma->second.enabled || !dma->second.priority_known || dma->second.priority!=3)
                    throw std::runtime_error("Native SIO2MAN DMA setup differs");
            }
        }}, {"cdrom0:\\IRX\\MCMAN.IRX;1",iop_mcman_base,iop_mcman_entry,iop_mcman_gp,iop_mcman_image,
        iop_mcman_export_bounds,bt2_iop_mcman,[](const IopRuntime& rt) {
            const auto lib=rt.libraries.find("mcman");
            if(lib==rt.libraries.end() || lib->second.version!=0x207 || lib->second.table!=iop_mcman_base
                || rt.semaphores.size()!=3 || rt.threads.size()!=1 || rt.events.size()!=1 || !rt.threads.begin()->second.waiting)
                throw std::runtime_error("Native MCMAN resident objects differ");
            unsigned drivers=0;
            for(const auto& d:rt.drivers)if(d.descriptor) {
                ++drivers;
                if(d.name!="mc" || d.descriptor!=iop_mcman_base+0x1200C || d.operations!=iop_mcman_base+0x11FA0 || d.gp!=iop_mcman_gp)
                    throw std::runtime_error("Native MCMAN driver ownership differs");
            }
            if(drivers!=1 || !rt.entries.contains(iop_mcman_base+0xFC10) || rt.security_handlers.at(4)!=iop_mcman_base+0x3ACC
                || rt.security_handlers.at(5)!=iop_mcman_base+0x3B04 || rt.kelf_path_handler!=iop_mcman_base+0xFCA4)
                throw std::runtime_error("Native MCMAN callbacks differ");
            for(unsigned offset:{0x13F10u,0x25228u}) {
                std::uint32_t id;std::memcpy(&id,rt.ram.data()+iop_mcman_base+offset,4);
                const auto sem=rt.semaphores.find(id);
                if(sem==rt.semaphores.end() || sem->second.attr!=1 || sem->second.count!=1 || sem->second.maximum!=1)
                    throw std::runtime_error("Native MCMAN priority semaphore differs");
            }
        }}, {"cdrom0:\\IRX\\MCSERV.IRX;1",iop_mcserv_base,iop_mcserv_entry,iop_mcserv_gp,iop_mcserv_image,
        iop_mcserv_export_bounds,bt2_iop_mcserv,[](const IopRuntime& rt) {
            const auto lib=rt.libraries.find("mcserv");
            if(lib==rt.libraries.end() || lib->second.version!=0x201 || !rt.sif_transport_attached || !rt.rpc_initialized
                || rt.threads.size()!=2 || rt.rpc_queues.size()!=1 || rt.rpc_servers.size()!=1)
                throw std::runtime_error("Native MCSERV resident objects differ");
            const auto queue=rt.rpc_queues.find(iop_mcserv_base+0x36E0);const auto server=rt.rpc_servers.find(0x80000400);
            if(queue==rt.rpc_queues.end() || server==rt.rpc_servers.end())throw std::runtime_error("Native MCSERV queue/server identity differs");
            const auto& worker=rt.threads.at(queue->second.thread);const auto& rpc=server->second;
            if(!worker.waiting || worker.ready || worker.wait_rpc!=queue->first || worker.entry!=iop_mcserv_base+0x280
                || worker.gp!=iop_mcserv_gp || worker.stacksize!=0x1000 || worker.priority!=104
                || rpc.descriptor!=iop_mcserv_base+0x36F8 || rpc.handler!=iop_mcserv_base+0x324 || rpc.buffer!=iop_mcserv_base+0x32B0
                || rpc.queue!=queue->first || rpc.gp!=iop_mcserv_gp)
                throw std::runtime_error("Native MCSERV worker/server ownership differs");
        }}, {"cdrom0:\\IRX\\DBCMAN.IRX;1",iop_dbcman_base,iop_dbcman_entry,iop_dbcman_gp,iop_dbcman_image,
        iop_dbcman_export_bounds,bt2_iop_dbcman,[](const IopRuntime& rt) {
            const auto lib=rt.libraries.find("dbcman");
            if(lib==rt.libraries.end() || lib->second.version!=0x102 || rt.threads.size()!=6 || rt.rpc_queues.size()!=5 || rt.rpc_servers.size()!=5 || rt.semaphores.size()!=4)
                throw std::runtime_error("Native DBCMAN resident objects differ");
            const std::array<unsigned,4> ids={0x80001300,0x8000131C,0x8000131E,0x8000131F};
            const std::array<unsigned,4> entries={0x1D94,0x1E2C,0x1EC4,0x1EC4};
            for(unsigned n=0;n<4;++n) {
                const auto q=rt.rpc_queues.find(iop_dbcman_base+0x2D80+n*24);const auto rpc=rt.rpc_servers.find(ids[n]);
                if(q==rt.rpc_queues.end() || rpc==rt.rpc_servers.end() || rpc->second.queue!=q->first || rpc->second.gp!=iop_dbcman_gp)
                    throw std::runtime_error("Native DBCMAN RPC ownership differs");
                const auto& t=rt.threads.at(q->second.thread);
                if(!t.waiting || t.ready || t.wait_rpc!=q->first || t.entry!=iop_dbcman_base+entries[n] || t.gp!=iop_dbcman_gp || t.priority!=20 || t.stacksize!=(n<2?0x800u:0x400u))
                    throw std::runtime_error("Native DBCMAN worker state differs");
            }
            std::uint32_t handle;std::memcpy(&handle,rt.ram.data()+iop_dbcman_base+0x2770,4);
            const auto& sem=rt.semaphores.at(handle);
            if(sem.attr || sem.count!=1 || sem.maximum!=64)throw std::runtime_error("Native DBCMAN semaphore differs");
        }}, {"cdrom0:\\IRX\\SIO2D.IRX;1",iop_sio2d_base,iop_sio2d_entry,iop_sio2d_gp,iop_sio2d_image,
        iop_sio2d_export_bounds,bt2_iop_sio2d,[](const IopRuntime& rt) {
            const auto lib=rt.libraries.find("sio2d");
            if(lib==rt.libraries.end() || lib->second.version!=0x203 || rt.threads.size()!=7 || rt.semaphores.size()!=6 || rt.events.size()!=2 || !rt.vblank_event)
                throw std::runtime_error("Native SIO2D resident objects differ");
            std::uint32_t id;std::memcpy(&id,rt.ram.data()+iop_sio2d_base+0x1BE8,4);
            const auto& t=rt.threads.at(id);const auto& event=rt.events.at(rt.vblank_event);
            if(!t.waiting || t.ready || t.entry!=iop_sio2d_base+0xAE4 || t.gp!=iop_sio2d_gp || t.priority!=46 || t.stacksize!=0x600
                || t.wait_event!=rt.vblank_event || t.wait_bits!=4 || t.wait_mode!=1 || event.attr!=2 || event.bits)
                throw std::runtime_error("Native SIO2D Vblank waiter differs");
            for(unsigned offset:{0x1BECu,0x1BF0u}) {
                std::uint32_t handle;std::memcpy(&handle,rt.ram.data()+iop_sio2d_base+offset,4);const auto& sem=rt.semaphores.at(handle);
                if(sem.attr || sem.count!=1 || sem.maximum!=64)throw std::runtime_error("Native SIO2D semaphore differs");
            }
        }}, {"cdrom0:\\IRX\\DS2U_D.IRX;1",iop_ds2ud_base,iop_ds2ud_entry,iop_ds2ud_gp,iop_ds2ud_image,
        iop_ds2ud_export_bounds,bt2_iop_ds2ud,[](const IopRuntime& rt) {
            if(rt.threads.size()!=9 || rt.events.size()!=4 || rt.semaphores.size()!=6 || rt.in_interrupt || rt.sio2_irq_status)
                throw std::runtime_error("Native DS2U_D resident objects differ");
            for(unsigned field:{0x22C4u,0x2480u}) {
                std::uint32_t id;std::memcpy(&id,rt.ram.data()+iop_ds2ud_base+field,4);const auto& t=rt.threads.at(id);
                if(t.entry!=iop_ds2ud_base+0x1DC || t.gp!=iop_ds2ud_gp || t.priority!=46 || t.stacksize!=0x800)
                    throw std::runtime_error("Native DS2U_D worker parameters differ");
                if(field==0x22C4 && (!t.waiting || t.wait_event!=rt.vblank_event || t.wait_bits!=1 || t.wait_mode!=1))
                    throw std::runtime_error("Native DS2U_D start-phase wait differs");
                if(field==0x2480 && (t.waiting || t.ready))throw std::runtime_error("Disconnected-port DS2U_D worker is not dormant");
            }
        }}, {"cdrom0:\\IRX\\LIBSD.IRX;1",iop_libsd_base,iop_libsd_entry,iop_libsd_gp,iop_libsd_image,
        iop_libsd_export_bounds,bt2_iop_libsd,[](const IopRuntime& rt) {
            const auto& library=rt.libraries.at("libsd");
            if(library.version!=0x105 || library.table!=iop_libsd_base || library.gp!=iop_libsd_gp || rt.threads.size()!=9 || rt.events.size()!=4 || rt.semaphores.size()!=6)
                throw std::runtime_error("Native LIBSD resident objects differ");
            const std::map<std::uint32_t,std::uint32_t> expected={{0x1F801404,0xBF900000},{0x1F80140C,0xBF900800},
                {0x1F8010F0,0x077FFF77},{0x1F801570,0x07BBFF7F},{0x1F801014,0x200B31E1},{0x1F801414,0x200B31E1}};
            if(rt.sound_bus!=expected || !rt.cpu_interrupts_enabled)throw std::runtime_error("Native LIBSD bus state differs");
        }}, {"cdrom0:\\IRX\\SDRDRV.IRX;1",iop_sdrdrv_base,iop_sdrdrv_entry,iop_sdrdrv_gp,iop_sdrdrv_image,
        iop_sdrdrv_export_bounds,bt2_iop_sdrdrv,[](const IopRuntime& rt) {
            if(rt.threads.size()!=10 || rt.events.size()!=4 || rt.semaphores.size()!=6 || rt.rpc_servers.size()!=6)
                throw std::runtime_error("Native SDRDRV resident objects differ");
            std::uint32_t id;std::memcpy(&id,rt.ram.data()+iop_sdrdrv_base+0x14C8,4);const auto& t=rt.threads.at(id);
            if(t.entry!=iop_sdrdrv_base+0x4E0 || t.gp!=iop_sdrdrv_gp || t.priority!=24 || t.stacksize!=0x800
                || !t.waiting || t.wait_rpc!=iop_sdrdrv_base+0x15C0 || t.wait_event)
                throw std::runtime_error("Native SDRDRV RPC worker differs");
        }}, {"cdrom0:\\IRX\\MODHSYN.IRX;1",iop_modhsyn_base,iop_modhsyn_entry,iop_modhsyn_gp,iop_modhsyn_image,
        iop_modhsyn_export_bounds,bt2_iop_modhsyn,[](const IopRuntime& rt) {
            const auto& lib=rt.libraries.at("modhsyn");
            if(lib.version!=0x104 || lib.table!=iop_modhsyn_base+0xBC90 || lib.gp!=iop_modhsyn_gp
                || rt.threads.size()!=10 || rt.events.size()!=4 || rt.semaphores.size()!=6)
                throw std::runtime_error("Native MODHSYN resident registration differs");
        }}, {"cdrom0:\\IRX\\MODSESQ2.IRX;1",iop_modsesq2_base,iop_modsesq2_entry,iop_modsesq2_gp,iop_modsesq2_image,
        iop_modsesq2_export_bounds,bt2_iop_modsesq2,[](const IopRuntime& rt) {
            const auto& lib=rt.libraries.at("modsesq2");
            if(lib.version!=0x101 || lib.table!=iop_modsesq2_base+0x39D0 || lib.gp!=iop_modsesq2_gp
                || rt.threads.size()!=10 || rt.events.size()!=4 || rt.semaphores.size()!=6)
                throw std::runtime_error("Native MODSESQ2 resident registration differs");
        }}, {"cdrom0:\\IRX\\CDVDSTM.IRX;1",iop_cdvdstm_base,iop_cdvdstm_entry,iop_cdvdstm_gp,iop_cdvdstm_image,
        iop_cdvdstm_export_bounds,bt2_iop_cdvdstm,[](const IopRuntime& rt) {
            const auto& lib=rt.libraries.at("cdvdstm");
            if(lib.version!=0x101 || lib.table!=iop_cdvdstm_base+0x4010 || lib.gp!=iop_cdvdstm_gp
                || rt.threads.size()!=10 || rt.events.size()!=5 || rt.semaphores.size()!=7)
                throw std::runtime_error("Native CDVDSTM resident objects differ");
            std::uint32_t sem,event;std::memcpy(&sem,rt.ram.data()+iop_cdvdstm_base+0x4784,4);
            std::memcpy(&event,rt.ram.data()+iop_cdvdstm_base+0x4788,4);
            const auto& semaphore=rt.semaphores.at(sem);const auto& flag=rt.events.at(event);
            if(semaphore.attr!=1 || semaphore.option || semaphore.count!=1 || semaphore.maximum!=1
                || event!=rt.cdvd_interrupt_event || flag.attr!=2 || flag.option!=2 || flag.bits!=0x29)
                throw std::runtime_error("Native CDVDSTM synchronization differs");
            const auto driver=std::find_if(rt.drivers.begin(),rt.drivers.end(),[](const auto& d){return d.name=="cdrom_stm";});
            if(driver==rt.drivers.end() || driver->gp!=iop_cdvdstm_gp || driver->descriptor!=iop_cdvdstm_base+0x4720 || driver->operations!=iop_cdvdstm_base+0x46B4)throw std::runtime_error("Native CDVDSTM driver differs");
        }}, {"cdrom0:\\IRX\\SOUNDS.IRX;1",iop_sounds_base,iop_sounds_entry,iop_sounds_gp,iop_sounds_image,
        iop_sounds_export_bounds,bt2_iop_sounds,[](const IopRuntime& rt) {
            if(rt.threads.size()!=11 || rt.events.size()!=5 || rt.semaphores.size()!=7 || rt.rpc_servers.size()!=7)
                throw std::runtime_error("Native SOUNDS resident objects differ");
            const auto& server=rt.rpc_servers.at(0x02000004);const auto& queue=rt.rpc_queues.at(server.queue);
            const auto& worker=rt.threads.at(queue.thread);
            if(worker.entry!=iop_sounds_base+0x284 || worker.gp!=iop_sounds_gp || worker.priority!=29 || worker.stacksize!=0x1000
                || !worker.waiting || worker.wait_rpc!=server.queue || server.queue!=worker.stack+0xF78 || worker.wait_event
                || server.descriptor!=worker.stack+0xF90 || server.handler!=iop_sounds_base+0xB0 || server.buffer!=iop_sounds_base+0x116E0)
                throw std::runtime_error("Native SOUNDS RPC worker differs");
        }}, {"cdrom0:\\IRX\\CRI_ADXI.IRX;1",iop_cri_adxi_base,iop_cri_adxi_entry,iop_cri_adxi_gp,iop_cri_adxi_image,
        [](IopRuntime& rt) {
            iop_cri_adxi_export_bounds(rt);
            // Guarded local CRI startup capture: three read-only words at 0/4/8 are zero.
            // This is a BT2 startup ABI profile, not general BIOS/kernel RAM ownership.
            for(unsigned address:{0u,4u,8u})rt.abi_read_only_words.emplace(address,0);
        },bt2_iop_cri_adxi,[](const IopRuntime& rt) {
            if(rt.threads.size()!=13 || rt.events.size()!=7 || rt.semaphores.size()!=7 || rt.rpc_servers.size()!=8
                || rt.native_export_results.at("libsd:4")!=0 || !rt.spu2_io_transfer_completed || rt.spu2_fifo_halfwords!=8 || rt.spu2_io_status_reads!=1)
                throw std::runtime_error("Native CRI_ADXI resident effects differ");
            std::uint32_t parent,child;std::memcpy(&parent,rt.ram.data()+iop_cri_adxi_base+0xDE18,4);std::memcpy(&child,rt.ram.data()+iop_cri_adxi_base+0xDE1C,4);
            const auto& waiter=rt.threads.at(parent);const auto& worker=rt.threads.at(child);const auto& queue=rt.rpc_queues.at(iop_cri_adxi_base+0xED80);
            if(waiter.entry!=iop_cri_adxi_base+0x88 || waiter.gp!=iop_cri_adxi_gp || waiter.priority!=39 || waiter.stacksize!=0x800
                || !waiter.waiting || waiter.ready || waiter.wait_event!=rt.vblank_event || waiter.wait_bits!=4 || waiter.wait_mode!=1 || waiter.wait_result || waiter.wait_rpc
                || worker.entry!=iop_cri_adxi_base+0x7D4 || worker.gp!=iop_cri_adxi_gp || worker.priority!=48 || worker.stacksize!=0x800
                || !worker.waiting || worker.ready || worker.wait_event || worker.wait_rpc!=iop_cri_adxi_base+0xED80 || queue.thread!=child || queue.servers.size()!=1)
                throw std::runtime_error("Native CRI_ADXI workers differ");
            const auto& server=rt.rpc_servers.at(0x90000200);
            if(server.queue!=iop_cri_adxi_base+0xED80 || server.descriptor!=iop_cri_adxi_base+0xED30
                || server.handler!=iop_cri_adxi_base+0x720 || server.buffer!=iop_cri_adxi_base+0xE7C0 || server.gp!=iop_cri_adxi_gp)
                throw std::runtime_error("Native CRI_ADXI RPC server differs");
            for(unsigned irq:{9u,36u,40u})if(!rt.enabled_interrupts.contains(irq) || !rt.interrupts.at(irq).enabled || rt.interrupts.at(irq).gp!=iop_libsd_gp)
                throw std::runtime_error("Native LIBSD IRQ ownership differs");
        }}};
}


std::uint32_t NativeIopService::allocateBootHeap(unsigned epoch,unsigned size) {
    const bool second=boot_heap_allocations==1 && boot_heap_frees==1 && buffer_loads==1 && size==0x81800;
    const bool third=boot_heap_allocations==2 && boot_heap_frees==1 && size==0x8D0 && disc_search_calls==6 && disc_read_calls==1;
    const bool fourth=boot_heap_allocations==3 && boot_heap_frees==1 && size==0x41040 && cri_control_calls==2;
    const unsigned pointer=fourth?0x150600u:third?0x14FD00u:0xCE500u;
    if(epoch!=epoch_ || failed_ || (!second && !third && !fourth && (boot_heap_allocations || size!=0x360)) || residents.size()!=13)
        throw std::runtime_error("Unsupported boot IOP heap allocation");
    // Reject conflicting ownership before touching service input or executing code.
    for(auto [lo,hi]:runtime.ranges)if(lo<pointer+((size+255)&~255u) && pointer<hi)
        throw std::runtime_error("Captured IOP heap overlaps native ownership");
    try {
        // Only the verified allocation handler is entered; FILEIO boot entry/workers are not replayed.
        if(!second && !third && !fourth) {runtime.map(iop_fileio_boot_base,iop_fileio_boot_image);iop_fileio_boot_export_bounds(runtime);
        definitions_.push_back({"boot:FILEIO.heap",iop_fileio_boot_base,iop_fileio_boot_base+0x27D4,iop_fileio_boot_gp,
            iop_fileio_boot_image,iop_fileio_boot_export_bounds,bt2_iop_fileio_boot,[](const IopRuntime&){throw std::runtime_error("Boot FILEIO residency not implemented");}}); }
        runtime.write(iop_fileio_boot_base+0x4050,size,4); // Owned translated heap service input buffer.
        runtime.providers[{"sysmem",4}]=[this,size,second,third,fourth,pointer](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0 || c.get(5)!=size || c.get(6)!=0 || boot_heap_allocations!=(fourth?3u:third?2u:second?1u:0u))
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
    if(epoch!=epoch_ || failed_ || pointer!=0xCE500 || boot_heap_allocations!=1 || buffer_loads || residents.size()!=13)
        throw std::runtime_error("Unsupported native memory module load");
    runtime.check(pointer,864);
    if(!std::equal(iop_embedded_file.begin(),iop_embedded_file.end(),runtime.ram.begin()+pointer))
        throw std::runtime_error("Owned embedded IRX bytes differ");
    // The SDK's command -9 returns the CDVDMAN version. This captured startup
    // profile is 0x225 from the disc-matched IOPRP module header; support is
    // restricted to this module's exact import PC.
    const auto previous=runtime.providers.at({"cdvdman",50});
    unsigned queries=0;
    try {
        runtime.map(0xCE930,iop_embedded_image);
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0xFFFFFFF7)return previous(rt,c);
            if(c.pc!=0xCE984 || c.get(5)!=0x1FFEF0 || queries++)throw std::runtime_error("Embedded CDVD version query differs");
            return 0x225u;
        };
        entry={};entry.pc=0xCE930;entry.set(28,0xD69B0);entry.set(29,0x1FFF00);entry.set(31,IopRuntime::sentinel);
        bt2_iop_embedded(runtime,entry);
        runtime.providers[{"cdvdman",50}]=previous;
        if(entry.pc!=IopRuntime::sentinel || entry.get(2)!=1 || queries!=1)
            throw std::runtime_error("Embedded nonresident completion differs");
        // modload result & 3 == 1 releases the loaded image. Keep the source heap
        // separately owned; freed code bytes are diagnostic residue only.
        std::erase(runtime.ranges,std::pair<std::uint32_t,std::uint32_t>{0xCE930,0xCE9C0});
        ++buffer_loads;return {next_id_++,entry.get(2)};
    }catch(...) {runtime.providers[{"cdvdman",50}]=previous;failed_=true;throw;}
}

std::uint32_t NativeIopService::freeBootHeap(unsigned epoch,unsigned pointer) {
    if(epoch!=epoch_ || failed_ || pointer!=0xCE500 || boot_heap_allocations!=1 || buffer_loads!=1 || boot_heap_frees)
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


std::vector<std::uint8_t> NativeIopService::callSoundRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t service) {
    const bool sounds=service==0x02000004;auto& requests=sounds?sounds_rpc_requests:sound_rpc_requests;
    const auto client=sounds?0x6351C0u:0x3A5C80u;const auto result=sounds?0x635180u:0x3A5C40u;
    if((service!=0x80000701 && !sounds) || epoch!=epoch_ || failed_ || requests || command!=(sounds?16u:0xE620u)
        || input.size()!=(sounds?256u:64u) || result_size!=(sounds?64u:16u) || (sounds && sound_rpc_requests!=1) || residents.size()!=13 || boot_heap_allocations!=2)
        throw std::runtime_error("Unsupported registered RPC native request");
    auto response=executeRegisteredRpc(service,command,input,result_size,client,result);++requests;return response;
}

std::vector<std::uint8_t> NativeIopService::callDbcRpc(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::span<std::uint8_t> ee_memory) {
    if(epoch!=epoch_ || failed_ || dbc_rpc_requests>=4 || command!=(dbc_rpc_requests>=2?0x80001301u:dbc_rpc_requests?0x80001304u:0x80001363u) || input.size()!=144 || result_size!=144
        || sounds_rpc_requests!=1 || residents.size()!=13 || boot_heap_allocations!=2)throw std::runtime_error("Unsupported DBCMAN native request");
    std::array<std::uint32_t,36> expected{};if(dbc_rpc_requests==1){expected[0]=0x310;expected[1]=0x3A27C0;}
    if(dbc_rpc_requests>=2){expected[0]=3;expected[1]=1;expected[2]=dbc_rpc_requests-2;expected[10]=0x3B0B00+(dbc_rpc_requests-2)*0x180;expected[11]=expected[10]+0x80;}
    if(std::memcmp(input.data(),expected.data(),sizeof(expected)))throw std::runtime_error("DBCMAN input outside observed initialization profile");
    const bool transfer=dbc_rpc_requests==1 && !ee_memory.empty();
    if(transfer) {
        if(ee_memory.size()!=32u*1024*1024 || dbc_dma_submissions || dbc_dma_polls || runtime.providers.contains({"sifman",7}) || runtime.providers.contains({"sifman",8}))
            throw std::runtime_error("DBCMAN DMA endpoint outside initial owned scope");
        runtime.providers[{"sifman",7}]=[this,ee_memory](IopRuntime& rt,IopContext& c) {
            const auto descriptor=c.get(4);
            if(c.pc!=0x621DC || c.get(5)!=1 || c.get(28)!=0x6A720 || descriptor!=0x1E1EE0 || dbc_dma_submissions)
                throw std::runtime_error("Unsupported DBCMAN reverse DMA submission");
            rt.check(descriptor,16);
            const std::array<std::uint32_t,4> expected={0x62778,0x3A27C0,128,0};
            for(unsigned n=0;n<4;++n)if(rt.read(descriptor+n*4,4)!=expected[n])throw std::runtime_error("DBCMAN DMA descriptor differs");
            rt.check(expected[0],expected[2]);
            if(expected[1]>ee_memory.size()-expected[2])throw std::runtime_error("DBCMAN DMA EE destination outside RAM");
            // Owned logical transport; bytes come from actual native IOP state.
            std::copy_n(rt.ram.begin()+expected[0],expected[2],dbc_dma_payload.begin());
            std::copy(dbc_dma_payload.begin(),dbc_dma_payload.end(),ee_memory.begin()+expected[1]);
            ++dbc_dma_submissions;return 0x710001u;
        };
        runtime.providers[{"sifman",8}]=[this](IopRuntime&,IopContext& c) {
            if(c.pc!=0x621E4 || c.get(4)!=0x710001 || dbc_dma_submissions!=1 || dbc_dma_polls)
                throw std::runtime_error("Unsupported DBCMAN reverse DMA completion poll");
            ++dbc_dma_polls;return UINT32_MAX;
        };
    }
    try {
        auto response=executeRegisteredRpc(0x80001300,command,input,result_size,0x3A03C0,0x3A04C0);
        if(transfer && (dbc_dma_submissions!=1 || dbc_dma_polls!=1))throw std::runtime_error("DBCMAN DMA lifecycle incomplete");
        if(transfer){runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});}
        ++dbc_rpc_requests;return response;
    }catch(...) {if(transfer){runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});}throw;}

}

std::vector<std::uint8_t> NativeIopService::executeRegisteredRpc(std::uint32_t service,std::uint32_t command,std::span<const std::uint8_t> input,unsigned result_size,std::uint32_t client,std::uint32_t result) {
    const auto found=runtime.rpc_servers.find(service);
    if(found==runtime.rpc_servers.end())throw std::runtime_error("No owned registered RPC server");
    const auto server=found->second;const auto queue=runtime.rpc_queues.find(server.queue);
    if(queue==runtime.rpc_queues.end() || runtime.current_thread || !runtime.cpu_interrupts_enabled)
        throw std::runtime_error("registered RPC request outside idle owned context");
    auto& worker=runtime.threads.at(queue->second.thread);
    if(!worker.waiting || worker.ready || worker.wait_rpc!=server.queue || worker.gp!=server.gp)
        throw std::runtime_error("registered RPC worker is not parked at its queue");
    runtime.check(server.buffer,static_cast<unsigned>(input.size()));
    runtime.check(worker.context.get(29)-64,64);
    runtime.check(server.descriptor,68);runtime.check(server.queue,24);
    if(runtime.read(server.queue+12,4) || runtime.read(server.queue+4,4))throw std::runtime_error("registered RPC queue already has a request");
    try {
        std::copy(input.begin(),input.end(),runtime.ram.begin()+server.buffer);
        // Reached SDK request fields; link/end retain ownership after dequeue.
        for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,8>{{{12,static_cast<unsigned>(input.size())},{28,client},{32,0x203A97C0},{36,command},{40,result},{44,result_size},{48,1},{52,5}}})
            runtime.write(server.descriptor+offset,value,4);
        runtime.write(server.queue+16,server.descriptor,4);runtime.write(server.queue+12,0,4);runtime.write(server.queue+4,1,4);
        // The public RPC loop calls the registered handler on its worker stack.
        // Retain the parked loop CPU; this bounded synchronous request borrows it.
        IopContext c=worker.context;c.pc=server.handler;c.branch_pending=false;c.delay_instruction=false;c.load_register=0;
        c.set(4,command);c.set(5,server.buffer);c.set(6,static_cast<unsigned>(input.size()));c.set(28,server.gp);c.set(31,IopRuntime::sentinel);
        runtime.current_thread=queue->second.thread;runtime.execute(runtime,c);runtime.current_thread=0;
        if(c.pc!=IopRuntime::sentinel || c.get(29)!=worker.context.get(29))throw std::runtime_error("registered RPC handler did not return to its worker frame");
        if(!c.get(2))throw std::runtime_error("registered RPC returned null response");
        runtime.check(IopRuntime::physical(c.get(2)),result_size);
        runtime.write(server.queue+4,0,4);
        return {runtime.ram.begin()+IopRuntime::physical(c.get(2)),runtime.ram.begin()+IopRuntime::physical(c.get(2))+result_size};
    } catch(...) {runtime.current_thread=0;failed_=true;throw;}
}

std::vector<std::uint8_t> NativeIopService::callMcRpc(unsigned epoch,std::span<const std::uint8_t> input) {
    if(epoch!=epoch_ || failed_ || mc_rpc_requests || dbc_rpc_requests!=4 || input.size()!=48
        || std::any_of(input.begin(),input.end(),[](auto b){return b!=0;}))throw std::runtime_error("MCSERV outside observed initial request");
    auto response=executeRegisteredRpc(0x80000400,0xFE,input,12,0x3A6F80,0x3A8540);
    ++mc_rpc_requests;return response;
}

std::vector<std::uint8_t> NativeIopService::callMcQuery(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee_memory) {
    std::array<std::uint32_t,12> expected{};expected[7]=0x3A7480;
    if(epoch!=epoch_ || failed_ || mc_rpc_requests!=1 || mc_query_requests || input.size()!=48
        || std::memcmp(input.data(),expected.data(),48))throw std::runtime_error("MCSERV query outside captured profile");
    if(ee_memory.size()!=32u*1024*1024 || runtime.pending_ee_bind.thread || runtime.providers.contains({"sifcmd",15}) || runtime.providers.contains({"sifman",7}) || runtime.providers.contains({"sifman",8}))
        throw std::runtime_error("Card timed dispatch requires owned EE RPC endpoint");
    runtime.providers[{"sifcmd",15}]=[this,ee_memory](IopRuntime& rt,IopContext& c)->std::uint32_t {
        if(c.pc!=0xA1244 || c.get(4)!=0xA1CA0 || c.get(5)!=0x80000704 || c.get(6)
            || rt.pending_ee_bind.thread || !rt.timed_wait_owner || !rt.threads.contains(rt.current_thread)
            || rt.threads.at(rt.current_thread).entry!=0xA0FF0 || rt.threads.at(rt.current_thread).priority!=24)
            throw std::runtime_error("IOP EE bind outside reached SDRDRV helper");
        std::uint32_t registered;std::memcpy(&registered,ee_memory.data()+0x3A6D18,4);
        if(registered)throw std::runtime_error("Pending EE bind scope requires the observed unregistered server");
        constexpr unsigned packet=0x1FF400;
        rt.check(0xA1CA0,40);rt.check(packet,64);
        if(rt.read(0xA1CA0,4) || rt.read(0xA1CC4,4))throw std::runtime_error("Duplicate native EE sound bind");
        // Owned logical outgoing request. No response/server pointer or API return is supplied.
        const std::array<std::uint32_t,16> request={64,0,0x80000009,0,6,packet,2,0xA1CA0,0x80000704};
        for(unsigned n=0;n<request.size();++n)rt.write(packet+n*4,request[n],4);
        rt.write(0xA1CA0,packet,4);rt.write(0xA1CA4,2,4);rt.write(0xA1CA8,0,4);
        rt.pending_ee_bind={rt.current_thread,0xA1CA0,0x80000704,packet,2};
        auto& t=rt.threads.at(rt.current_thread);t.ready=false;t.waiting=true;t.wait_ee_bind=true;
        throw IopYield{};
    };
    runtime.providers[{"sifman",7}]=[this,ee_memory](IopRuntime& rt,IopContext& c) {
        if(c.pc!=0x51064 || c.get(4)!=0x1E1778 || c.get(5)!=1 || mc_dma_submissions
            || rt.timed_wakes!=5 || rt.card_alarm_fires!=4 || rt.sio2_card_fifo_packets!=5)
            throw std::runtime_error("MCSERV secondary DMA outside owned query");
        const std::array<std::uint32_t,4> descriptor={0x1E16B8,0x3A7480,192,0};
        rt.check(c.get(4),16);rt.check(descriptor[0],descriptor[2]);
        for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+4*n,4)!=descriptor[n])
            throw std::runtime_error("MCSERV secondary DMA descriptor differs");
        std::copy_n(rt.ram.begin()+descriptor[0],192,mc_dma_payload.begin());
        std::copy(mc_dma_payload.begin(),mc_dma_payload.end(),ee_memory.begin()+descriptor[1]);
        ++mc_dma_submissions;return 0x730001u;
    };
    runtime.providers[{"sifman",8}]=[this](IopRuntime&,IopContext& c) {
        if(c.pc!=0x5106C || c.get(4)!=0x730001 || mc_dma_submissions!=1 || mc_dma_polls)
            throw std::runtime_error("MCSERV secondary DMA poll differs");
        ++mc_dma_polls;return UINT32_MAX;
    };
    const auto cleanup=[&]{runtime.providers.erase({"sifcmd",15});runtime.providers.erase({"sifman",7});runtime.providers.erase({"sifman",8});};
    std::vector<std::uint8_t> response;
    try {response=executeRegisteredRpc(0x80000400,1,input,4,0x3A6F80,0x3A8540);cleanup();}
    catch(...) {cleanup();throw;}
    if(mc_dma_submissions!=1 || mc_dma_polls!=1)throw std::runtime_error("MCSERV secondary DMA incomplete");

    ++mc_query_requests;return response;
}

std::uint32_t NativeIopService::callDiscSearch(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee,const std::string& path) {
    static const std::array<std::string,6> names={"\\DATA\\ZS2US.DIR;1","\\DATA\\ZS2US_1.AFS;1","\\DATA\\ZS2US_2.AFS;1","\\DATA\\ZS2USOP.SFD;1","\\DATA\\ZS2USED.SFD;1","\\DATA\\LOGO_O.SFD;1"};

    if(epoch!=epoch_ || failed_ || disc_search_calls>=names.size() || (disc_search_calls && disc_read_calls!=1) || input.size()!=300 || ee.size()!=32*1024*1024
        || std::memcmp(input.data()+36,names[disc_search_calls].c_str(),names[disc_search_calls].size()+1)
        || input[292]!=0 || input[293]!=0x86 || input[294]!=0x3A || input[295] || input[296] || input[297] || input[298] || input[299])
        throw std::runtime_error("Unsupported owned disc search request");
    DiscMetadata disc(path);const auto metadata=disc.lookup(names[disc_search_calls]);
    if(!metadata)throw std::runtime_error("Validated local directory index missing");
    unsigned lba=0,size=0;std::memcpy(&lba,metadata->data(),4);std::memcpy(&size,metadata->data()+4,4);
    const std::array<std::pair<unsigned,unsigned>,6> extents={{{0xE2A,64},{0x1FC26,0x419B9800},{0xA2F9A,0x60014000},{0x14245,0x5CF0800},{0xE2B,0x9A0D000},{0x8B1,0x2BC800}}};
    if(std::pair{lba,size}!=extents[disc_search_calls])throw std::runtime_error("Disc revision/search metadata differs");
    const auto saved=runtime.providers;const auto prior_dma=disc_dma_submissions;
    bool locked=false;unsigned locks=0,lookups=0;
    try {
        if(!disc_search_calls) {runtime.map(iop_disc_base,iop_disc_image);
        definitions_.push_back({"boot:CDVDFSV.search",iop_disc_base,iop_disc_base+0x754,iop_disc_gp,iop_disc_image,
            [](IopRuntime&){},bt2_iop_disc,[](const IopRuntime&){throw std::runtime_error("CDVDFSV boot residency not implemented");}});}
        for(unsigned n=0;n<input.size();++n)runtime.write(iop_disc_base+0x6498+n,input[n],1);
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            const auto value=rt.read(c.get(5),4);
            if(c.pc!=iop_disc_base+0x4B68 || c.get(4)!=0xFFFFFFF6 || (value!=0xFF && value!=0))throw std::runtime_error("Unexpected search mutex operation");
            // Bounded serialized request: acquire the owned free search bit, then release it.
            if(value==0xFF){if(locked || locks++)throw std::runtime_error("Search mutex already owned");locked=true;}
            else {if(!locked || lookups!=1 || disc_dma_submissions!=prior_dma+1)throw std::runtime_error("Search release before completion");locked=false;}
            return value;
        };
        runtime.providers[{"cdvdman",84}]=[&](IopRuntime& rt,IopContext& c) {
            if(!locked || lookups++ || c.pc!=iop_disc_base+0x4BB0 || c.get(4)!=iop_disc_base+0x6498 || c.get(5)!=iop_disc_base+0x64BC || c.get(6))throw std::runtime_error("Unsupported layer/search destination");
            for(unsigned n=0;n<32;++n)rt.write(c.get(4)+n,(*metadata)[n],1);
            return 1u;
        };
        runtime.providers[{"sifman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.pc!=iop_disc_base+0x4A3C || c.get(4)!=iop_disc_base+0x53B0 || c.get(5)!=1 || disc_dma_submissions!=prior_dma || !locked || lookups!=1)throw std::runtime_error("Unexpected metadata DMA submit");
            const std::array<unsigned,4> expected={iop_disc_base+0x6498,0x3A8600,36,0};
            for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+n*4,4)!=expected[n])throw std::runtime_error("Metadata DMA descriptor differs");
            for(unsigned n=0;n<36;++n)ee[0x3A8600+n]=static_cast<std::uint8_t>(rt.read(expected[0]+n,1));
            ++disc_dma_submissions;return 0x750001u;
        };
        IopContext c;c.pc=iop_disc_base+0x754;c.set(4,0);c.set(5,iop_disc_base+0x6498);c.set(6,300);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_disc_base+0x5808 || runtime.read(c.get(2),4)!=1 || locked || locks!=1 || disc_dma_submissions!=prior_dma+1 || disc_dma_polls)throw std::runtime_error("Original search handler completion differs");
        runtime.providers=saved;++disc_search_calls;return 1;
    }catch(...){runtime.providers=saved;failed_=true;throw;}
}

std::uint32_t NativeIopService::callDiscReady(unsigned epoch) {
    if(epoch!=epoch_ || failed_ || disc_search_calls!=1 || disc_ready_calls || runtime.disc_type_known
        || runtime.read(iop_disc_base+0x6088,4))throw std::runtime_error("Unsupported owned disc readiness request");
    // One serialized readiness request owns acquire -> state queries -> release.
    // SC(-3)/SC(-12) ignore param; residual stack bytes are not input state.
    const auto saved=runtime.providers;unsigned stage=0;
    try {
        runtime.disc_type_known=true;
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.pc!=iop_disc_base+0x4B68)throw std::runtime_error("Disc readiness import differs");
            const auto value=c.get(4)==0xFFFFFFF6?rt.read(c.get(5),4):0u;
            if(stage==0 && c.get(4)==0xFFFFFFF6 && value==14){++stage;return 14u;}
            if(stage==1 && c.get(4)==0xFFFFFFFD){++stage;return 0u;}
            if(stage==2 && c.get(4)==0xFFFFFFF4){++stage;return 1u;}
            if(stage==3 && c.get(4)==0xFFFFFFF6 && value==0){++stage;return 0u;}
            throw std::runtime_error("Unsupported disc readiness state transition");
        };
        IopContext c;c.pc=iop_disc_base+0x3ABC;c.set(4,14);c.set(5,iop_disc_base+0x6088);c.set(6,0);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_disc_base+0x5830 || runtime.read(c.get(2),4)!=2
            || stage!=4 || runtime.disc_type_reads!=2 || runtime.disc_ready_status_reads!=1)throw std::runtime_error("Original readiness handler completion differs");
        runtime.providers=saved;runtime.disc_type_known=false;++disc_ready_calls;return 2;
    }catch(const std::exception& e){const auto detail=std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc)+" / a0="+std::to_string(runtime.last_import_arguments[0])+" / a1="+std::to_string(runtime.last_import_arguments[1]);runtime.providers=saved;runtime.disc_type_known=false;failed_=true;throw std::runtime_error(detail);}
}

std::uint32_t NativeIopService::callDiscRead(unsigned epoch,std::span<const std::uint8_t> input,std::span<std::uint8_t> ee,const std::string& path) {
    const std::array<unsigned,6> expected={0xE2A,2,0x39DE80,0x100,0x3694C0,0x369580};
    if(epoch!=epoch_ || failed_ || disc_ready_calls!=1 || disc_read_calls || input.size()!=sizeof(expected)
        || std::memcmp(input.data(),expected.data(),sizeof(expected)) || ee.size()!=32*1024*1024)
        throw std::runtime_error("Unsupported owned first disc read");
    const auto payload=read_iso_sectors(path,expected[0],expected[1]);
    const auto saved=runtime.providers;const auto event=runtime.next_event++;
    const auto wait=saved.at({"thevent",10});unsigned stage=0;bool accepted=false,callback=false;
    constexpr unsigned workspace=0x1D4B00,buffer=0x1D5B00,raw=buffer+0x1248,token=0x770001;
    try {
        runtime.map(workspace,std::vector<std::uint8_t>(256));runtime.map(buffer,std::vector<std::uint8_t>(42128));
        runtime.events.emplace(event,IopRuntime::Event{0,0,0});
        // Authored inactive shared workspace and owned FSV buffer. No live retail state is copied.
        runtime.write(iop_disc_base+0x5568,workspace,4);runtime.write(iop_disc_base+0x53A0,buffer,4);
        runtime.write(iop_disc_base+0x5398,buffer,4);runtime.write(iop_disc_base+0x539C,iop_disc_base+0x5390,4);
        runtime.write(iop_disc_base+0x5550,event,4);
        for(unsigned n=0;n<input.size();++n)runtime.write(iop_disc_base+0x6088+n,input[n],1);
        runtime.disc_type_reads=0;runtime.disc_type_known=true;
        runtime.providers[{"cdvdman",50}]=[&](IopRuntime& rt,IopContext& c) {
            const auto command=c.get(4);
            if(c.pc!=iop_disc_base+0x4B68)throw std::runtime_error("Read SC import differs");
            if(stage==0 && command==0xFFFFFFF6 && rt.read(c.get(5),4)==1){++stage;return 1u;}
            if(stage==1 && command==0xFFFFFFFC){++stage;return 0u;}
            if(stage==2 && command==0xFFFFFFEA){++stage;return 1u;}
            if(stage==3 && command==0xFFFFFFE9 && rt.read(c.get(5),4)==0xE2A){++stage;return 0xE2Au;}
            if(stage==4 && command==0xFFFFFFF1){++stage;return 15000u;}
            if(command==0xFFFFFFF6 && rt.read(c.get(5),4)==0 && callback && disc_read_dma_submissions){++stage;return 0u;}
            throw std::runtime_error("Unsupported read SC operation");
        };
        runtime.providers[{"thbase",35}]=[&](IopRuntime& rt,IopContext& c) {
            if(stage!=5 || disc_read_alarm_sets || c.pc!=iop_disc_base+0x4980 || c.get(4)!=iop_disc_base+0x5548 || c.get(5)!=iop_disc_base+0x8C8 || c.get(6)!=c.get(4)
                || rt.read(c.get(4),4)!=552960000 || rt.read(c.get(4)+4,4))throw std::runtime_error("Unsupported disc timeout registration");
            disc_read_alarm={rt.logical_time_us+15000000,c.get(5),c.get(6),c.get(28)};++disc_read_alarm_sets;return 0u;
        };
        runtime.providers[{"thbase",37}]=[&](IopRuntime&,IopContext& c) {
            if(!callback || !disc_read_alarm.deadline || disc_read_alarm_cancels || c.pc!=iop_disc_base+0x4988 || c.get(4)!=disc_read_alarm.callback || c.get(5)!=disc_read_alarm.argument)
                throw std::runtime_error("Unsupported disc timeout cancellation");
            disc_read_alarm={};++disc_read_alarm_cancels;return 0u;
        };
        runtime.providers[{"cdvdman",63}]=[&](IopRuntime& rt,IopContext& c) {
            if(stage!=5 || accepted || c.pc!=iop_disc_base+0x4B80 || c.get(4)!=0xE2A || c.get(5)!=2 || c.get(6)!=raw || c.get(7)!=iop_disc_base+0x54F0
                || rt.read(c.get(29)+16,4)!=2 || rt.read(c.get(29)+20,4)!=iop_disc_base+0xB00 || rt.read(c.get(7),4)!=0x100)
                throw std::runtime_error("Unsupported RV destination/mode/callback");
            // Captured DVD records contain a 12-byte header, 2048 ISO bytes and four trailing bytes.
            for(unsigned sector=0;sector<2;++sector){const auto at=raw+sector*2064,id=0x20030000+0xE2A+sector;
                for(unsigned n=0;n<4;++n)rt.write(at+n,(id>>(8*(3-n)))&255,1);
                for(unsigned n=0;n<2048;++n)rt.write(at+12+n,payload[sector*2048+n],1);
            }
            accepted=true;return 1u;
        };
        auto deliver_read_completion=[&](IopRuntime& rt) {
                if(!accepted || !rt.cpu_interrupts_enabled || rt.in_interrupt || rt.events.at(event).bits)
                    throw std::runtime_error("Read completion before owned bytes/interrupt eligibility");
                IopContext completion;completion.pc=iop_disc_base+0xB00;completion.set(4,workspace);
                completion.set(28,iop_disc_gp);completion.set(29,0x1FF700);completion.set(31,IopRuntime::sentinel);
                rt.in_interrupt=true;try{rt.execute(rt,completion);rt.in_interrupt=false;}catch(...){rt.in_interrupt=false;throw;}
                if(completion.pc!=IopRuntime::sentinel || completion.get(2)!=0 || rt.events.at(event).bits!=0x20)
                    throw std::runtime_error("Original read callback effects differ");
                callback=true;++disc_read_callbacks;
        };
        runtime.providers[{"thevent",10}]=[&](IopRuntime& rt,IopContext& c) {
            if(c.get(4)==event && c.get(5)==0x20 && !callback)throw std::runtime_error("Disc wait before synchronization completed");
            return wait(rt,c);
        };
        runtime.providers[{"cdvdman",11}]=[&](IopRuntime& rt,IopContext& c) {
            if(!accepted || c.pc!=iop_disc_base+0x4AF0 || (c.get(4)!=32 && !(c.get(4)==5 && callback)))throw std::runtime_error(std::string("Unsupported owned read synchronization / accepted=")+(accepted?"1":"0")+" callback="+(callback?"1":"0"));
            if(!callback)deliver_read_completion(rt);
            return 0u;
        };
        runtime.providers[{"cdvdman",8}]=[&](IopRuntime&,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x4AD8)throw std::runtime_error("Read error query before data completion");
            return 0u;
        };
        runtime.providers[{"intrman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x48A4 || c.get(4)!=35 || rt.enabled_interrupts.contains(35))
                throw std::runtime_error("Unexpected disc DMA3 interrupt ownership");
            if(c.get(5))rt.write(c.get(5),0xFFFFFF99,4);
            return 0xFFFFFF99u; // Actual driver completion already disabled DMA3.
        };
        std::set<unsigned> polled;
        runtime.providers[{"sifman",7}]=[&](IopRuntime& rt,IopContext& c) {
            if(!callback || c.pc!=iop_disc_base+0x4A3C)throw std::runtime_error("Sector DMA before owned completion");
            if(disc_read_dma_submissions==0) {
                if(c.get(4)!=iop_disc_base+0x53E0 || c.get(5)!=2)throw std::runtime_error("Unsupported first sector DMA");
                for(unsigned sector=0;sector<2;++sector) {
                    const std::array<unsigned,4> d={raw+sector*2064+12,0x39DE80+sector*2048,2048,0};
                    for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+sector*16+n*4,4)!=d[n])throw std::runtime_error("Sector DMA descriptor differs");
                    for(unsigned n=0;n<2048;++n)ee[d[1]+n]=static_cast<std::uint8_t>(rt.read(d[0]+n,1));
                }
            }else {
                const bool progress=disc_read_dma_submissions==1 || disc_read_dma_submissions==3;
                if(disc_read_dma_submissions>3 || c.get(5)!=1 || c.get(4)!=iop_disc_base+(progress?0x5508:0x54F8))throw std::runtime_error("Unsupported read completion DMA / batch="+std::to_string(disc_read_dma_submissions)+" src="+std::to_string(rt.read(c.get(4),4))+" dst="+std::to_string(rt.read(c.get(4)+4,4))+" bytes="+std::to_string(rt.read(c.get(4)+8,4))+" value="+std::to_string(rt.read(rt.read(c.get(4),4),4)));
                const std::array<unsigned,4> d={iop_disc_base+(progress?0x5818:0x5580),progress?0x369580u:0x3694C0u,progress?4u:144u,0};
                for(unsigned n=0;n<4;++n)if(rt.read(c.get(4)+n*4,4)!=d[n])throw std::runtime_error("Read completion descriptor differs");
                if(progress && rt.read(d[0],4)!=(disc_read_dma_submissions==1?0u:4096u))throw std::runtime_error("Read progress differs");
                if(!progress && (rt.read(d[0],4) || rt.read(d[0]+4,4) || rt.read(d[0]+8,4)!=0x39DE80 || rt.read(d[0]+12,4)!=0x39EE80))throw std::runtime_error("Owned completion range differs");
                for(unsigned n=0;n<d[2];++n)ee[d[1]+n]=static_cast<std::uint8_t>(rt.read(d[0]+n,1));
            }
            return token+disc_read_dma_submissions++;
        };
        runtime.providers[{"sifman",8}]=[&](IopRuntime&,IopContext& c) {
            if(c.pc==iop_disc_base+0x4A44 && c.get(4)==0)return 0xFFFFFFFFu; // No prior DMA exists in the fresh owned buffer.
            if(c.pc!=iop_disc_base+0x4A44 || c.get(4)<token || c.get(4)>=token+disc_read_dma_submissions)throw std::runtime_error("Unsupported sector DMA poll");
            if(polled.insert(c.get(4)).second)++disc_read_dma_polls;
            return 0xFFFFFFFFu; // These owned copies have completed; hardware poll counts are not equated.
        };
        IopContext c;c.pc=iop_disc_base+0x3ABC;c.set(4,1);c.set(5,iop_disc_base+0x6088);c.set(6,24);
        c.set(28,iop_disc_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);runtime.execute(runtime,c);
        if(c.pc!=IopRuntime::sentinel || stage!=6 || !accepted || !callback || disc_read_dma_submissions!=4 || !disc_read_dma_polls || disc_read_alarm.deadline || disc_read_alarm_cancels!=1)
            throw std::runtime_error("Original sector handler incomplete");
        // The original handler resubmits its updated progress word after the completion buffer.
        if(runtime.read(iop_disc_base+0x5818,4)!=4096)throw std::runtime_error("Final owned progress differs");
        runtime.providers=saved;runtime.disc_type_known=false;++disc_read_calls;return runtime.read(c.get(2),4);
    }catch(const std::exception& e){const auto detail=std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc)+" / a0="+std::to_string(runtime.last_import_arguments[0])+" / a1="+std::to_string(runtime.last_import_arguments[1]);runtime.providers=saved;runtime.disc_type_known=false;failed_=true;throw std::runtime_error(detail);}
}

std::vector<std::uint8_t> NativeIopService::callCriCreate(unsigned epoch,std::span<const std::uint8_t> input) {
    if(epoch!=epoch_ || failed_ || cri_create_calls || input.size()!=16 || disc_search_calls!=6 || disc_read_calls!=1)
        throw std::runtime_error("CRI create outside observed lifecycle");
    const std::array<unsigned,4> expected={0,0x365BC0,0x14FD40,0x880};
    for(unsigned n=0;n<4;++n){unsigned word;std::memcpy(&word,input.data()+n*4,4);if(word!=expected[n])throw std::runtime_error("CRI create input differs");}
    std::vector<std::uint8_t> response;
    try {response=executeRegisteredRpc(0x90000200,2,input,4,0x355C70,0x39DD80);}
    catch(const std::exception& e){failed_=true;throw std::runtime_error(std::string(e.what())+" / pc="+std::to_string(runtime.executing_pc));}
    unsigned slot;std::memcpy(&slot,response.data(),4);
    if(slot!=0x1B58C0)throw std::runtime_error("CRI create did not return its owned slot");
    ++cri_create_calls;return response;
}

std::vector<std::uint8_t> NativeIopService::callCriControl(unsigned epoch,std::uint32_t command,std::span<const std::uint8_t> input) {
    const bool batch=cri_control_calls>=2;
    const bool stream=batch && !(cri_control_calls&1);
    const auto index=batch?(cri_control_calls-2)/2:0u;
    const auto expected_command=batch?(stream?0x422u:0x400u):cri_control_calls?0x40Cu:0x403u;
    const auto size=batch?(stream?12u:16u):cri_control_calls?12u:0u;
    if(epoch!=epoch_ || failed_ || cri_create_calls!=1 || cri_control_calls>33 || command!=expected_command || input.size()!=size)
        throw std::runtime_error("CRI control outside observed lifecycle");
    if(command==0x40C) {
        const std::array<unsigned,3> expected={0xBB80,0xFFFF,0xFFFF};
        for(unsigned n=0;n<3;++n) {unsigned word;std::memcpy(&word,input.data()+n*4,4);
            if(word!=expected[n])throw std::runtime_error("CRI 0x40C input differs");}
    }
    if(batch) {
        if(boot_heap_allocations!=4 || !runtime.spu2_auto_dma.pending)throw std::runtime_error("CRI batch lacks owned fourth buffer");
        const std::array<unsigned,4> expected=stream?std::array<unsigned,4>{1,0x150640+index*0x4100,0x100,0}
            :std::array<unsigned,4>{0x364B08+index*0x30,iop_cri_adxi_base+0x14A40+index*0x30,1,0x365900+index*0x14};
        for(unsigned n=0;n<size/4;++n) {unsigned word;std::memcpy(&word,input.data()+n*4,4);
            if(word!=expected[n])throw std::runtime_error("CRI batch input differs");}
        runtime.check(0x150640+index*0x4100,0x4100);
    }
    try {
        auto response=executeRegisteredRpc(0x90000200,command,input,batch?4u:command==0x40C?12u:0u,0x355C70,0x39DD80);
        if(batch) {
            unsigned result=0;std::memcpy(&result,response.data(),4);
            if(result!=iop_cri_adxi_base+(stream?0x14A40+index*0x30:0x15580+index*0x14))
                throw std::runtime_error("CRI batch did not return owned original object");
        }
        ++cri_control_calls;return response;
    }catch(...){failed_=true;throw;}
}
