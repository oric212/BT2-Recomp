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
    if(epoch!=epoch_ || failed_ || boot_heap_allocations || size!=0x360 || residents.size()!=13)
        throw std::runtime_error("Unsupported boot IOP heap allocation");
    try {
        // Only the verified allocation handler is entered; FILEIO boot entry/workers are not replayed.
        runtime.map(iop_fileio_boot_base,iop_fileio_boot_image);iop_fileio_boot_export_bounds(runtime);
        definitions_.push_back({"boot:FILEIO.heap",iop_fileio_boot_base,iop_fileio_boot_base+0x27D4,iop_fileio_boot_gp,
            iop_fileio_boot_image,iop_fileio_boot_export_bounds,bt2_iop_fileio_boot,[](const IopRuntime&){throw std::runtime_error("Boot FILEIO residency not implemented");}});
        runtime.write(iop_fileio_boot_base+0x4050,size,4); // Owned translated heap service input buffer.
        runtime.providers[{"sysmem",4}]=[this,size](IopRuntime& rt,IopContext& c) {
            if(c.get(4)!=0 || c.get(5)!=size || c.get(6)!=0 || boot_heap_allocations)
                throw std::runtime_error("Unsupported native AllocSysMemory request");
            // Captured first block; 256-byte rounding. Other allocations/modes remain checked failures.
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
