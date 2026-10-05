#include "module_service.h"
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
    if(epoch!=epoch_ || failed_ || (!second && (boot_heap_allocations || size!=0x360)) || residents.size()!=13)
        throw std::runtime_error("Unsupported boot IOP heap allocation");
    // Reject conflicting ownership before touching service input or executing code.
    for(auto [lo,hi]:runtime.ranges)if(lo<0xCE500+((size+255)&~255u) && 0xCE500<hi)
        throw std::runtime_error("Captured IOP heap overlaps native ownership");
    try {
        // Only the verified allocation handler is entered; FILEIO boot entry/workers are not replayed.
        if(!second) {runtime.map(iop_fileio_boot_base,iop_fileio_boot_image);iop_fileio_boot_export_bounds(runtime);
        definitions_.push_back({"boot:FILEIO.heap",iop_fileio_boot_base,iop_fileio_boot_base+0x27D4,iop_fileio_boot_gp,
            iop_fileio_boot_image,iop_fileio_boot_export_bounds,bt2_iop_fileio_boot,[](const IopRuntime&){throw std::runtime_error("Boot FILEIO residency not implemented");}}); }
        runtime.write(iop_fileio_boot_base+0x4050,size,4); // Owned translated heap service input buffer.
        runtime.providers[{"sysmem",4}]=[this,size,second](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0 || c.get(5)!=size || c.get(6)!=0 || boot_heap_allocations!=(second?1u:0u))
                throw std::runtime_error("Unsupported native AllocSysMemory request");
            // Two independently captured allocations reuse the released address; 256-byte rounding.
            rt.map(0xCE500,std::vector<std::uint8_t>((size+255)&~255u));
            ++boot_heap_allocations;return 0xCE500u;
        };
        IopContext c;c.pc=iop_fileio_boot_base+0x27D4;c.set(4,1);c.set(5,iop_fileio_boot_base+0x4050);c.set(6,4);
        c.set(28,iop_fileio_boot_gp);c.set(29,0x1FFF00);c.set(31,IopRuntime::sentinel);
        runtime.execute(runtime,c);runtime.providers.erase({"sysmem",4});
        if(c.pc!=IopRuntime::sentinel || c.get(2)!=iop_fileio_boot_base+0x3FE0 || !boot_heap_allocations
            || runtime.read(c.get(2),4)!=0xCE500)throw std::runtime_error("Native FILEIO heap handler effects differ");
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
