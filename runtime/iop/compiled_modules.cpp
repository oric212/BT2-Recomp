#include "module_service.h"
#include "sio2man_native.h"
#include "mcman_native.h"
#include "mcserv_native.h"
#include "dbcman_native.h"
#include "sio2d_native.h"
#include "ds2ud_native.h"
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
        }}};
}
