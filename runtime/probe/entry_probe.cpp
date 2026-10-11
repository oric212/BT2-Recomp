#include "gs/cpu/cpu_display.h"
#ifdef BT2_GPU_GS
#include "gs/gpu/gpu_gs_mirror.h"
#endif
#ifdef _WIN32
#include "host/viewer_window.h"
#endif
#include "probe_state.h"
#include "ee/ps2_runtime_macros.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

// Entry probe: runs the generated BT2 entry on a prepared guest image with the supplied evidence,
// classifies the checked boundary where it stops, validates the reached checkpoint and writes the
// private report. Stages live in probe_inputs.cpp, startup_bindings.cpp, checkpoint_checks.cpp and
// probe_reports.cpp.

int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 25) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT [KERNEL_SNAPSHOT [TIMER3_CAPTURE_WORD [TIMER2_CAPTURE_WORD [INTC_MASK_WORD [TIMER2_READBACK_WORD [OSD_CONFIG_WORD [OSD_READBACK_WORD [DMA_STATUS_WORD [DMA5_CONTROL_WORD [SIF_COMMAND_WORD [SIF_DMA_BUNDLE [SIF_RPC_STATE_WORD [SIF_BIND_BUNDLE]]]]]]]]]]]]]");
        const ProbeModes modes=ProbeModes::fromArgc(argc);
        const bool cd_mode=modes.cd,bind_mode=modes.bind,snapshot_mode=modes.snapshot,timer_mode=modes.timer,timer2_mode=modes.timer2,
            intc_mode=modes.intc,readback_mode=modes.readback,osd_mode=modes.osd,osd_readback_mode=modes.osd_readback,
            dma_status_mode=modes.dma_status,sif_rpc_mode=modes.sif_rpc,sif_dma_mode=modes.sif_dma,sif_mode=modes.sif,dma5_mode=modes.dma5;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(PS2Runtime::ram_size))
            throw std::runtime_error("Invalid prepared guest image size");
        std::vector<std::uint8_t> image(PS2Runtime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size())))
            throw std::runtime_error("Could not read guest image");
        PS2Runtime runtime;
        runtime.ee_sound_dispatch_probe = std::getenv("BT2_EE_SOUND_DISPATCH_PROBE") != nullptr;
        runtime.ee_queued_sound_rpc = !runtime.ee_sound_dispatch_probe;
        // Frame budget: stop after this many display flips (default 1000, none in viewer mode;
        // BT2_FRAME_LIMIT overrides, 0 disables).
        const bool viewer_mode = std::getenv("BT2_VIEWER") != nullptr;
        runtime.frame_limit = viewer_mode ? 0 : 1000;
        if (const char* limit = std::getenv("BT2_FRAME_LIMIT")) runtime.frame_limit = static_cast<unsigned>(std::stoul(limit));
#ifdef _WIN32
        // Viewer mode: present the displayed framebuffer at each flip; keep the last frame visible after
        // the run stops until the window is closed. Closing the window during a run ends the process.
        const char* scale = std::getenv("BT2_VIEWER_SCALE");
        static ViewerWindow viewer(scale && std::string_view(scale) == "stretch" ? ViewerWindow::Scaling::Stretch : ViewerWindow::Scaling::Integer);
        struct ViewerHold {bool active;~ViewerHold(){if(active && viewer.opened())viewer.waitUntilClosed();}} viewer_hold{viewer_mode};
        if (viewer_mode) runtime.on_display_flip = [&runtime] {
            const auto dispfb = runtime.gs_privileged[9] ? runtime.gs_privileged[9] : runtime.gs_privileged[7];
            const auto display = runtime.gs_privileged[9] ? runtime.gs_privileged[10] : runtime.gs_privileged[8];
            viewer.present(decodeDisplay(runtime.gif_dma.gs, dispfb, display), runtime.gs_display_flips);
            if (viewer.closed()) {std::cerr << "Viewer closed; ending the run." << std::endl; std::exit(0);}
        };
#endif
#ifdef BT2_GPU_GS
        // BT2_GPU_GS=mirror: render on paraLLEl-GS as well and compare VRAM at each flip.
        // BT2_GPU_GS=on: paraLLEl-GS draws alone; the CPU model only validates packets.
        static GpuGsMirror gpu_mirror;
        const char* gpu_mode = std::getenv("BT2_GPU_GS");
        if (gpu_mode && std::string_view(gpu_mode) != "mirror" && std::string_view(gpu_mode) != "on")
            throw std::runtime_error("BT2_GPU_GS must be mirror or on");
        if (gpu_mode) {
            const bool gpu_only = std::string_view(gpu_mode) == "on";
            gpu_mirror.attach(runtime.gif_dma.gs);
            runtime.gif_dma.gs.rasterize = !gpu_only;
            // With the viewer, the window shows the GPU scanout instead of the CPU framebuffer.
            // BT2_GPU_GS_SNAPSHOT=<flip>:<path> writes that flip's GPU scanout as a binary PPM.
            const char* snapshot = std::getenv("BT2_GPU_GS_SNAPSHOT");
            const unsigned snapshot_flip = snapshot ? static_cast<unsigned>(std::stoul(snapshot)) : 0;
            const std::string snapshot_path = snapshot && std::strchr(snapshot, ':') ? std::strchr(snapshot, ':') + 1 : "";
            runtime.on_display_flip = [&runtime, viewer_mode, gpu_only, snapshot_flip, snapshot_path] {
                const auto flip = runtime.gs_display_flips;
                if (gpu_only) gpu_mirror.flip(); else gpu_mirror.compare(runtime.gif_dma.gs, flip);
                if (!viewer_mode && flip != snapshot_flip) return;
                const auto frame = gpu_mirror.scanout(runtime.gs_privileged, runtime.gs_crt_request[1], flip);
                if (flip == snapshot_flip && !snapshot_path.empty()) writePpm(snapshot_path, frame);
                if (!viewer_mode || !frame.width) return;
                viewer.present(frame, flip);
                if (viewer.closed()) {std::cerr << "Viewer closed; ending the run." << std::endl; std::exit(0);}
            };
        }
#else
        if (std::getenv("BT2_GPU_GS")) throw std::runtime_error("BT2_GPU_GS needs a probe built with -DBT2_GPU_GS=ON");
#endif
        if (const char* phase = std::getenv("BT2_PHASE_STOP")) runtime.phase_stop = static_cast<std::uint32_t>(std::stoul(phase, nullptr, 16));
        runtime.loadMemory(image);
        const std::uint32_t osd_word=loadStartupEvidence(runtime,modes,argv);
        const std::vector<std::uint8_t> kernel_before(runtime.kernelBaseline().begin(), runtime.kernelBaseline().end());
        constexpr std::uint32_t zero_start=startup_zero_start,zero_end=startup_zero_end;
        runtime.expectStartupZeroRegion(zero_start, zero_end);
        // Poison only the observed startup zero region so zero stores are observable.
        std::fill(runtime.memory().begin() + zero_start, runtime.memory().begin() + zero_end, std::uint8_t{0xA5});
        R5900Context context{};
        for (auto& reg : context.r) reg = {UINT64_MAX, UINT64_MAX};
        context.f.fill(1.0f);
        context.hi = context.lo = context.hi1 = context.lo1 = UINT64_MAX;
        context.fcr31 = context.sa = UINT32_MAX;
        context.f_acc = 1.0f;
        loadContinuationEvidence(runtime,modes,argv);
        registerStartupFunctions(runtime);
        registerKernelPayloads(runtime,modes);
        if (argc >= 18) loadScmdEvidence(runtime,argv);
        bool stopped = false,execution_budget_exhausted=false;
        std::uint32_t stop_address = 0;
        std::size_t stop_size = 0;
        std::uint32_t stop_selector = 0, stop_handler = 0;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestBranch& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            std::cerr << "Branch target/source/resume: " << std::hex << stop.target << "/" << stop.source_pc << "/" << stop.resume_pc << "\n";
            if (!bind_mode) throw;
            stop_target = stop.target; stop_source = stop.source_pc; stop_resume = stop.resume_pc; stopped = true;
        }
        catch (const GifPacketMismatch& mismatch) {
            // Diagnostics follow the caller's report path; pipeline uses ignored work/.
            for(const auto& item : {std::pair{".gif-expected.bin",&mismatch.expected},
                                    std::pair{".gif-actual.bin",&mismatch.actual}}) {
                std::ofstream dump(std::string(argv[2])+item.first,std::ios::binary);
                if(!dump.write(reinterpret_cast<const char*>(item.second->data()),item.second->size()))
                    throw std::runtime_error("Could not write GIF mismatch diagnostic");
            }
            throw;
        }
        catch (const UnsupportedGuestMemory& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            std::cerr << "Memory address/pc/acks/polls: " << std::hex << stop.address << "/" << context.pc << "/" << runtime.gs_interrupt.acknowledgements << "/" << runtime.gs_interrupt.polls << "\n";
            std::cerr << "Memory sp-offset/ra/main/cri/graph/display: " << std::hex << (runtime.initial_sp-context.r[29].low) << "/" << context.r[31].low << "/" << runtime.main_priority_admissions << "/" << std::dec << runtime.cri_rpc_calls << "/" << runtime.graph_startup_phase << "/" << runtime.display_reset_phase << " stat/polled/mask " << std::hex << runtime.display_intc_stat << "/" << runtime.polled_vblanks << "/" << runtime.intc_mask << "\n";
            if (dma5_mode && !(argc>=18 && runtime.reset_ready_checkpoint_reads)) throw;
            const bool vif_handoff=runtime.native_module_loads==14 && runtime.heap_frees==1 && runtime.dmac_startup_writes==3
                && stop.address==0x10003C10 && stop.size==4 && context.pc==0x30FF70 && context.r[29].low==runtime.initial_sp-32;
            const bool texture_handoff=runtime.graph_startup_phase==4 && runtime.vif_startup.phase==6
                && stop.address==0x1000A000 && stop.size==4 && context.pc==0x28B928
                && context.r[29].low==runtime.initial_sp-128 && runtime.gif_dma.completions==2;
            // Frame phase (after the first display flip, main thread): a refused access to a hardware
            // register (EE registers or GS privileged registers) is the checked boundary of the next
            // subsystem, named by its address. Refused accesses elsewhere (RAM) remain failures.
            const bool hardware_register=(stop.address>=0x10000000 && stop.address<0x10010000) || (stop.address>=0x12000000 && stop.address<0x12002000);
            const bool frame_register_boundary=runtime.gs_display_flips>=1 && runtime.current_thread_id==1 && (stop.size==4 || stop.size==8 || runtime.frame_budget_reached)
                && hardware_register;
            // Frame-phase GS reset (the GS library's CSR reset during the frame loop; next subsystem).
            const bool frame_gs_reset=runtime.gs_display_flips>=1 && runtime.current_thread_id==1 && runtime.display_reset_phase==13
                && stop.address==0x12001000 && stop.size==8 && context.pc==0x30FE44;
            if(runtime.post_sif.supplied && !vif_handoff && !texture_handoff && !frame_register_boundary && !frame_gs_reset) throw; // Explicit subsystem boundaries only.
            if(vif_handoff || texture_handoff || frame_register_boundary || frame_gs_reset) { /* Preserve the checked memory stop; no unsupported request succeeds. */ }
            else if (argc>=18 && runtime.reset_ready_checkpoint_reads) require(
                (stop.address==0x12001000 && stop.size==8 && context.pc==0x30FE44)
                || (stop.address==0x1000F000 && stop.size==4 && context.pc==0x32F10C)
                || (runtime.gs_interrupt.acknowledgements==2 && stop.address==0x12001000 && stop.size==8 && context.pc==0x3104F4)
                || (runtime.gs_field_reads==1 && stop.address==0x1000A000 && stop.size==4 && context.pc==0x3100E4)
                || (runtime.gif_dma.starts==1 && stop.address==0x10009000 && stop.size==4 && context.pc==0x310560)
                || (runtime.gif_dma.starts==2 && stop.address==0x1000E010 && stop.size==4 && context.pc==0x3319CC)
                || (runtime.gif_dma.ready() && stop.address==0x1000A000 && stop.size==4 && context.pc==0x3101A8),
                "Unexpected GS continuation memory boundary");
            else if (dma_status_mode) require(stop.address == 0x1000C000 && stop.size == 4
                    && context.pc == 0x003319EC && runtime.dma_status_reads == 1,
                    "Unexpected DMA channel control boundary");
            else if (osd_readback_mode) require(stop.address == 0x1000E010 && stop.size == 4
                    && context.pc == 0x003319CC && GPR_U32(&context, 4) == 0
                    && runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1,
                    "Unexpected DMA status boundary");
            else if (readback_mode) throw;
            else if (intc_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338DD8
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1
                    && runtime.timer2_mode_reads == 1 && runtime.timer2_mode_writes == 1,
                    "Unexpected post-EnableIntc timer boundary");
            else if (timer2_mode) throw;
            else if (timer_mode) require(stop.address == 0x10001010 && stop.size == 4 && context.pc == 0x00338C58,
                                    "Unexpected Timer 2 MMIO boundary");
            else if (snapshot_mode) require(stop.address == 0x10001810 && stop.size == 4 && context.pc == 0x00338A1C,
                                       "Unexpected snapshot MMIO boundary");
            else require(stop.address == 0x80000000 && stop.size == 4 && context.pc == 0x00338388,
                         "Unexpected entry memory boundary");
            stop_address = stop.address;
            stop_size = stop.size;
            stopped = true;
        }
        catch (const UnsupportedSyscall& stop) {
            if(runtime.ee_sound_probe_attempted) context=runtime.ee_sound_probe_frontier;
            if (bind_mode) std::cerr << "Syscall selector/handler/pc: " << std::hex << stop.selector << "/" << stop.handler << "/" << context.pc << "\n";
            else if (sif_rpc_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 464
                    && GPR_U32(&context, 5) == 1 && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 1,
                    "Unexpected first RPC bind submission boundary");
            else if (sif_dma_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000002
                    && runtime.sif_dma_submissions == 1 && runtime.sif_rpc_reads == 0,
                    "Unexpected RPCINIT query boundary");
            else if (sif_mode) require(stop.selector == 0x77 && !stop.handler && stop.resume_pc == 0x0032F028
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 240
                    && GPR_U32(&context, 5) == 1 && runtime.sif_command_reads == 1
                    && runtime.data_cache_lines == 1, "Unexpected first SIF DMA submission boundary");
            else if (dma5_mode) require(stop.selector == 0x7A && !stop.handler && stop.resume_pc == 0x0032F078
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 0x80000000,
                    "Unexpected first SIF register boundary");
            else if (osd_readback_mode) throw;
            else if (osd_mode) require(stop.selector == 0x4A && !stop.handler && stop.resume_pc == 0x0032ED08
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 124
                    && runtime.osd_config_reads == 1 && runtime.priority_changes == 1
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 128) == osd_word
                    && runtime.read32(runtime.memory().data(), runtime.initial_sp - 124) == ((osd_word & 0xFFFF1FFF) | 0x2000),
                    "Unexpected SetOsdConfigParam boundary");
            else if (readback_mode) require(stop.selector == 0x4B && !stop.handler && stop.resume_pc == 0x0032ED18
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == runtime.initial_sp - 128
                    && runtime.main_thread_priority == 1 && runtime.priority_changes == 1
                    && runtime.timer2_mode_reads == 2 && runtime.timer2_mode_writes == 1
                    && runtime.intc_mask == 0x1802 && runtime.intc_enable_calls == 1,
                    "Unexpected GetOsdConfigParam boundary after priority change");
            else require(!intc_mode && timer2_mode && stop.selector == 0x14 && !stop.handler && stop.resume_pc == 0x0032E9A8
                    && context.pc == stop.resume_pc && GPR_U32(&context, 4) == 11
                    && runtime.timer2_mode_writes == 1 && runtime.timer2_mode_after_write == 0x382
                    && context.cop0_errorepc == 0x00338C90 && !context.ll_bit,
                    "Unexpected syscall after generated Timer 2 write/ERET");
            stop_selector = stop.selector;
            stop_handler = stop.handler;
            stopped = true;
        }
        catch (const std::runtime_error& error) {
            if(!bind_mode || !std::string_view(error.what()).starts_with("Bootstrap execution budget exceeded;"))throw;
            std::cerr << error.what() << "\n";execution_budget_exhausted=true;stopped=true;
        }
        const ProbeStop stop{stopped,execution_budget_exhausted,stop_address,stop_size,stop_selector,stop_handler,stop_target,stop_source,stop_resume};
        if (cd_mode && argc < 18) {
            checkCdContinuation(runtime,context,stop);
            writeCdReport(runtime,context,stop,argv);
            std::cout << "CD-init continuation report written.\n";
            return 0;
        }
        if (argc >= 18) {
            checkRuntimeBoundary(runtime,context,stop);
            writeRuntimeBoundaryReport(runtime,context,stop,argv);
            writeEvidenceDumps(runtime,argv);
            return 0;
        }
        if (bind_mode) {
            checkBindContinuation(runtime,context,stop,kernel_before);
            writeBindReport(runtime,context,stop,argv);
            std::cout << "Generated RPC bind and cleanup returned; first CD-init RPC call reached a checked runtime boundary.\n";
            return 0;
        }
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        checkEntryBoundary(runtime,context,modes,stop,kernel_before,image,args);
        writeEntryReport(runtime,context,modes,stop,args,argv);
        printEntrySummary(context,modes,stop);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Entry probe failed: " << error.what() << '\n';
        return 1;
    }
}
