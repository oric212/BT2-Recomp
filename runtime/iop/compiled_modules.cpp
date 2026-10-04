#include "module_service.h"
#include "sio2man_native.h"
#include "mcman_native.h"
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
        iop_mcman_export_bounds,bt2_iop_mcman,[](const IopRuntime&) {throw std::runtime_error("Native MCMAN effects require validation");}}};
}
