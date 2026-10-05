#include "ps2_runtime.h"
#ifdef BT2_NATIVE_IOP
#include "module_service.h"
#endif
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#ifdef BT2_KERNEL_PAYLOAD
#include <bt2_kernel_functions.h>
#endif
#ifdef BT2_STARTUP_KERNEL_PAYLOAD
#include <bt2_startup_kernel_functions.h>
#endif
#ifdef BT2_EXTRA_STARTUP_BINDINGS
#include <bt2_extra_startup.h>
#endif
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace {
std::string jsonString(std::string_view value) {
    std::string result="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value) {
        if(c=='"' || c=='\\') {result+='\\';result+=static_cast<char>(c);}
        else if(c<32) {result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
        else result+=static_cast<char>(c);
    }
    return result+'"';
}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4 && argc != 5 && argc != 6 && argc != 7 && argc != 8 && argc != 9 && argc != 10 && argc != 11 && argc != 12 && argc != 13 && argc != 14 && argc != 15 && argc != 16 && argc != 17 && argc != 18 && argc != 19 && argc != 20 && argc != 21 && argc != 22) throw std::runtime_error("Usage: bt2-entry-probe MEMORY_IMAGE STOP_REPORT [KERNEL_SNAPSHOT [TIMER3_CAPTURE_WORD [TIMER2_CAPTURE_WORD [INTC_MASK_WORD [TIMER2_READBACK_WORD [OSD_CONFIG_WORD [OSD_READBACK_WORD [DMA_STATUS_WORD [DMA5_CONTROL_WORD [SIF_COMMAND_WORD [SIF_DMA_BUNDLE [SIF_RPC_STATE_WORD [SIF_BIND_BUNDLE]]]]]]]]]]]]]");
        const bool cd_mode = argc >= 17;
        const bool bind_mode = argc >= 16;
        const bool snapshot_mode = argc >= 4;
        const bool timer_mode = argc >= 5;
        const bool timer2_mode = argc >= 6;
        const bool intc_mode = argc >= 7;
        const bool readback_mode = argc >= 8;
        const bool osd_mode = argc >= 9;
        const bool osd_readback_mode = argc >= 10;
        const bool dma_status_mode = argc >= 11;
        const bool sif_rpc_mode = argc >= 15;
        const bool sif_dma_mode = argc >= 14;
        const bool sif_mode = argc >= 13;
        const bool dma5_mode = argc >= 12;
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() != static_cast<std::streamoff>(PS2Runtime::ram_size))
            throw std::runtime_error("Invalid prepared guest image size");
        std::vector<std::uint8_t> image(PS2Runtime::ram_size);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size())))
            throw std::runtime_error("Could not read guest image");
        PS2Runtime runtime;
        runtime.loadMemory(image);
        if (snapshot_mode) {
            std::ifstream kernel(argv[3], std::ios::binary | std::ios::ate);
            if (!kernel || kernel.tellg() != static_cast<std::streamoff>(PS2Runtime::kernel_snapshot_size))
                throw std::runtime_error("Invalid kernel snapshot size");
            std::vector<std::uint8_t> bytes(PS2Runtime::kernel_snapshot_size);
            kernel.seekg(0);
            if (!kernel.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
                throw std::runtime_error("Could not read kernel snapshot");
            runtime.loadKernelSnapshot(bytes);
        }
        if (timer_mode) {
            std::ifstream timer(argv[4], std::ios::binary | std::ios::ate);
            if (!timer || timer.tellg() != 4) throw std::runtime_error("Invalid Timer 3 capture word");
            std::uint32_t word;
            timer.seekg(0);
            if (!timer.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer capture read failed");
            require(word == 0x83, "Timer capture differs from the validated startup observation");
            runtime.supplyTimer3Mode(word);
            runtime.enableKernelPatch(0x80076000, 0x740);
        }
        if (timer2_mode) {
            std::ifstream timer(argv[5], std::ios::binary | std::ios::ate);
            if (!timer || timer.tellg() != 4) throw std::runtime_error("Invalid Timer 2 capture word");
            std::uint32_t word;
            timer.seekg(0);
            if (!timer.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer 2 capture read failed");
            require(word == 0x80, "Timer 2 capture differs from the validated startup observation");
            runtime.supplyTimer2Mode(word);
        }
        if (intc_mode) {
            std::ifstream mask(argv[6], std::ios::binary | std::ios::ate);
            if (!mask || mask.tellg() != 4) throw std::runtime_error("Invalid INTC checkpoint word");
            std::uint32_t word;
            mask.seekg(0);
            if (!mask.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("INTC checkpoint read failed");
            require(word == 0x1002, "INTC checkpoint differs from validated startup capture");
            runtime.supplyIntcMask(word);
        }
        if (readback_mode) {
            std::ifstream capture(argv[7], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid Timer 2 readback word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("Timer 2 readback word read failed");
            require(word == 0x382, "Readback capture differs from validated startup observation");
            runtime.supplyTimer2Readback(word);
        }
        std::uint32_t osd_word = 0;
        if (osd_mode) {
            std::ifstream capture(argv[8], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid OSD checkpoint word");
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&osd_word), 4)) throw std::runtime_error("OSD checkpoint read failed");
            runtime.supplyOsdConfig(osd_word);
        }
        if (osd_readback_mode) {
            std::ifstream capture(argv[9], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid OSD readback word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("OSD readback read failed");
            runtime.supplyOsdReadback(word);
            runtime.enableStartupKernelPayload(std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
        }
        for (int index = 10; index < std::min(argc, 12); ++index) {
            std::ifstream capture(argv[index], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid DMA checkpoint word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("DMA checkpoint read failed");
            if (index == 10) runtime.supplyDmaStatus(word);
            else runtime.supplyDma5Control(word);
        }
        if (sif_mode) {
            std::ifstream capture(argv[12], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid SIF checkpoint word");
            std::uint32_t word;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("SIF checkpoint read failed");
            runtime.supplySifCommand(word);
        }
        if (sif_dma_mode) {
            std::ifstream capture(argv[13], std::ios::binary | std::ios::ate);
            if (!capture || capture.tellg() != static_cast<std::streamoff>(PS2Runtime::sif_iop_size + 4))
                throw std::runtime_error("Invalid SIF DMA checkpoint bundle");
            std::vector<std::uint8_t> bytes(PS2Runtime::sif_iop_size);
            std::uint32_t result;
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bytes.data()), bytes.size())
                    || !capture.read(reinterpret_cast<char*>(&result), 4))
                throw std::runtime_error("SIF DMA checkpoint read failed");
            runtime.supplySifDma(bytes, result);
        }
        if (sif_rpc_mode) {
            std::ifstream capture(argv[14], std::ios::binary | std::ios::ate);
            std::uint32_t word;
            if (!capture || capture.tellg() != 4) throw std::runtime_error("Invalid SIF RPC checkpoint word");
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(&word), 4)) throw std::runtime_error("SIF RPC checkpoint read failed");
            runtime.supplySifRpcState(word);
        }
        if(argc>=19) {
            std::ifstream capture(argv[18],std::ios::binary|std::ios::ate);
            if(!capture || capture.tellg()!=64)throw std::runtime_error("Invalid IOP heap bind evidence");
            std::array<std::uint8_t,64> evidence;capture.seekg(0);
            if(!capture.read(reinterpret_cast<char*>(evidence.data()),64))throw std::runtime_error("IOP heap evidence read failed");
            runtime.supplyHeapBind(evidence);
        }
        if(argc>=20) {
            std::ifstream capture(argv[19],std::ios::binary|std::ios::ate);
            if(!capture || (capture.tellg()!=68 && capture.tellg()!=88))throw std::runtime_error("Invalid heap allocation evidence");
            std::vector<std::uint8_t> evidence(static_cast<std::size_t>(capture.tellg()));capture.seekg(0);
            if(!capture.read(reinterpret_cast<char*>(evidence.data()),evidence.size()))throw std::runtime_error("Heap allocation evidence read failed");
            runtime.supplyHeapAllocation(evidence);
        }
        if(argc>=21) {
            std::ifstream capture(argv[20],std::ios::binary|std::ios::ate);
            if(!capture || capture.tellg()!=224)throw std::runtime_error("Invalid texture GIF evidence");
            std::array<std::uint8_t,224> evidence;capture.seekg(0);
            if(!capture.read(reinterpret_cast<char*>(evidence.data()),evidence.size()))throw std::runtime_error("Texture evidence read failed");
            runtime.texture_gif.supply(evidence);
        }
        if(argc>=22) {
            std::ifstream capture(argv[21],std::ios::binary|std::ios::ate);
            if(!capture || capture.tellg()!=56)throw std::runtime_error("Invalid EE thread-start evidence");
            std::array<std::uint8_t,56> evidence;capture.seekg(0);
            if(!capture.read(reinterpret_cast<char*>(evidence.data()),evidence.size()))throw std::runtime_error("EE thread-start evidence read failed");
            runtime.supplySoundThreadStart(evidence);
        }
        const std::vector<std::uint8_t> kernel_before(runtime.kernelBaseline().begin(), runtime.kernelBaseline().end());
        constexpr std::uint32_t zero_start = 0x00387E80, zero_end = 0x0043058F;
        runtime.expectStartupZeroRegion(zero_start, zero_end);
        // Poison only the observed startup zero region so zero stores are observable.
        std::fill(runtime.memory().begin() + zero_start, runtime.memory().begin() + zero_end, std::uint8_t{0xA5});
        R5900Context context{};
        for (auto& reg : context.r) reg = {UINT64_MAX, UINT64_MAX};
        context.f.fill(1.0f);
        context.hi = context.lo = context.hi1 = context.lo1 = UINT64_MAX;
        context.fcr31 = context.sa = UINT32_MAX;
        context.f_acc = 1.0f;
        if (bind_mode) {
            std::ifstream capture(argv[15], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg() != PS2Runtime::sif_iop_size + 68
                    && capture.tellg() != PS2Runtime::sif_iop_size + 96))
                throw std::runtime_error("Invalid RPC bind capture bundle");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()), bundle.size()))
                throw std::runtime_error("RPC bind bundle read failed");
            runtime.supplySifBind(bundle);
        }
        if (cd_mode) {
            std::ifstream capture(argv[16], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg() != 88 && capture.tellg() != 128)) throw std::runtime_error("Invalid CD-init bundle size");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));
            capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()), bundle.size()))
                throw std::runtime_error("Could not read CD-init bundle");
            runtime.supplyCdInit(bundle);
        }
        runtime.registerFunction(0x31EDB0, FUN_0031EDB0_0x31edb0);
        runtime.registerFunction(0x31F010, FUN_0031F010_0x31f010);
        runtime.registerFunction(0x332970, FUN_00332970_0x332970);
        runtime.registerFunction(0x32EC70, FUN_0032EC70_0x32ec70);
        runtime.registerFunction(0x3322E8, FUN_003322E8_0x3322e8);
        runtime.registerFunction(0x332258, FUN_00332258_0x332258);
        runtime.registerFunction(0x32EC90, FUN_0032EC90_0x32ec90);
        runtime.registerFunction(0x00100008, FUN_00100008_0x100008);
        runtime.registerFunction(0x00338520, FUN_00338520_0x338520);
        runtime.registerFunction(0x003382D0, FUN_003382D0_0x3382d0);
        runtime.registerFunction(0x0032EC60, FUN_0032EC60_0x32ec60);
        runtime.registerFunction(0x00338410, FUN_00338410_0x338410);
        runtime.registerFunction(0x00338510, FUN_00338510_0x338510);
        runtime.registerFunction(0x003383C8, FUN_003383C8_0x3383c8);
        runtime.registerFunction(0x00338388, FUN_00338388_0x338388);
        runtime.registerFunction(0x00338A00, FUN_00338A00_0x338a00);
        runtime.registerFunction(0x00338998, FUN_00338998_0x338998);
        runtime.registerFunction(0x003389A8, FUN_003389A8_0x3389a8);
        runtime.registerFunction(0x003389B8, FUN_003389B8_0x3389b8);
        runtime.registerFunction(0x0032EEE0, FUN_0032EEE0_0x32eee0);
        runtime.registerFunction(0x003389F0, FUN_003389F0_0x3389f0);
        runtime.registerFunction(0x00338B78, FUN_00338B78_0x338b78);
        runtime.registerFunction(0x003251DC, FUN_003251DC_0x3251dc);
        runtime.registerFunction(0x00339E48, FUN_00339E48_0x339e48);
        runtime.registerFunction(0x00338648, FUN_00338648_0x338648);
        runtime.registerFunction(0x003385E0, FUN_003385E0_0x3385e0);
        runtime.registerFunction(0x00321E50, FUN_00321E50_0x321e50);
        runtime.registerFunction(0x00321D90, FUN_00321D90_0x321d90);
        runtime.registerFunction(0x00322F90, FUN_00322F90_0x322f90);
        runtime.registerFunction(0x0032EC80, FUN_0032EC80_0x32ec80);
        runtime.registerFunction(0x0032ECA0, FUN_0032ECA0_0x32eca0);
        runtime.registerFunction(0x003242D0, FUN_003242D0_0x3242d0);
        runtime.registerFunction(0x00331FE8, FUN_00331FE8_0x331fe8);
        runtime.registerFunction(0x00331888, FUN_00331888_0x331888);
        runtime.registerFunction(0x003382B8, FUN_003382B8_0x3382b8);
        runtime.registerFunction(0x0032E970, FUN_0032E970_0x32e970);
        runtime.registerFunction(0x0032F878, FUN_0032F878_0x32f878);
        runtime.registerFunction(0x0032E9C0, FUN_0032E9C0_0x32e9c0);
        runtime.registerFunction(0x0032F070, FUN_0032F070_0x32f070);
        runtime.registerFunction(0x00331D70, FUN_00331D70_0x331d70);
        runtime.registerFunction(0x00331C38, FUN_00331C38_0x331c38);
        runtime.registerFunction(0x00331B70, FUN_00331B70_0x331b70);
        runtime.registerFunction(0x0031F8B0, FUN_0031F8B0_0x31f8b0);
        runtime.registerFunction(0x0031F638, FUN_0031F638_0x31f638);
        runtime.registerFunction(0x00332B70, FUN_00332B70_0x332b70);
        runtime.registerFunction(0x00332790, FUN_00332790_0x332790);
        runtime.registerFunction(0x003321B0, FUN_003321B0_0x3321b0);
        runtime.registerFunction(0x0032F020, FUN_0032F020_0x32f020);
        runtime.registerFunction(0x00331F38, FUN_00331F38_0x331f38);
        runtime.registerFunction(0x00100388, FUN_00100388_0x100388);
        runtime.registerFunction(0x0033A238, FUN_0033A238_0x33a238);
        runtime.registerFunction(0x0033A820, FUN_0033A820_0x33a820);
        runtime.registerFunction(0x0033A810, FUN_0033A810_0x33a810);
        runtime.registerFunction(0x0033A800, FUN_0033A800_0x33a800);
        runtime.registerFunction(0x0033A7C8, FUN_0033A7C8_0x33a7c8);
        runtime.registerFunction(0x0033A7B8, FUN_0033A7B8_0x33a7b8);
        runtime.registerFunction(0x0032ED00, FUN_0032ED00_0x32ed00);
        runtime.registerFunction(0x0032ED10, FUN_0032ED10_0x32ed10);
        runtime.registerFunction(0x0032EAF0, FUN_0032EAF0_0x32eaf0);
        runtime.registerFunction(0x0032FDA0, FUN_0032FDA0_0x32fda0);
        runtime.registerFunction(0x0032F730, FUN_0032F730_0x32f730);
        runtime.registerFunction(0x0032EB50, FUN_0032EB50_0x32eb50);
        runtime.registerFunction(0x0032FA38, FUN_0032FA38_0x32fa38);
        runtime.registerFunction(0x0032EA60, FUN_0032EA60_0x32ea60);
        runtime.registerFunction(0x00338DB8, FUN_00338DB8_0x338db8);
        runtime.registerFunction(0x0032F7A8, FUN_0032F7A8_0x32f7a8);
        runtime.registerFunction(0x0032E9A0, FUN_0032E9A0_0x32e9a0);
        runtime.registerFunction(0x00338B58, FUN_00338B58_0x338b58);
        runtime.registerFunction(0x00338AD8, FUN_00338AD8_0x338ad8);
        runtime.registerFunction(0x00338268, FUN_00338268_0x338268);
        runtime.registerFunction(0x0032E950, FUN_0032E950_0x32e950);
#ifdef BT2_EXTRA_STARTUP_BINDINGS
        registerExtraStartup(runtime);
#endif
        if (timer_mode) {
#ifdef BT2_KERNEL_PAYLOAD
            runtime.registerKernelFunction(0x80076000, FUN_80076000_0x80076000,
                std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036B328, 0x740));
#else
            throw std::runtime_error("Timer replay requires generated kernel payload integration");
#endif
        }
        if (osd_readback_mode) {
#ifdef BT2_STARTUP_KERNEL_PAYLOAD
            runtime.registerKernelFunction(0x80075000, FUN_80075000_0x80075000,
                std::span<const std::uint8_t>(runtime.memory()).subspan(0x0036BAF8, 0x330));
#else
            throw std::runtime_error("OSD readback requires generated startup kernel payload");
#endif
        }
        if (argc >= 18) {
            std::ifstream capture(argv[17], std::ios::binary | std::ios::ate);
            if (!capture || (capture.tellg()!=88 && capture.tellg()!=184 && capture.tellg()!=188 && capture.tellg()!=192 && capture.tellg()!=204 && capture.tellg()!=224 && capture.tellg()!=228 && capture.tellg()!=236 && capture.tellg()!=244 && capture.tellg()!=252 && capture.tellg()!=316 && capture.tellg()!=324 && capture.tellg()!=616 && capture.tellg()!=988 && capture.tellg()!=1124 && capture.tellg()!=1332 && capture.tellg()!=1464 && capture.tellg()!=1596)) throw std::runtime_error("Invalid SCMD bundle size");
            std::vector<std::uint8_t> bundle(static_cast<std::size_t>(capture.tellg()));capture.seekg(0);
            if (!capture.read(reinterpret_cast<char*>(bundle.data()),bundle.size()) || capture.peek()!=EOF)
                throw std::runtime_error("Invalid SCMD bind bundle size");
            runtime.supplyScmdBind(bundle);
        }
        bool stopped = false;
        std::uint32_t stop_address = 0;
        std::size_t stop_size = 0;
        std::uint32_t stop_selector = 0, stop_handler = 0;
        std::uint32_t stop_target = 0, stop_source = 0, stop_resume = 0;
        try { runtime.callFunction(0x00100008, context); }
        catch (const UnsupportedGuestBranch& stop) {
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
            std::cerr << "Memory address/pc/acks/polls: " << std::hex << stop.address << "/" << context.pc << "/" << runtime.gs_interrupt.acknowledgements << "/" << runtime.gs_interrupt.polls << "\n";
            if (dma5_mode && !(argc>=18 && runtime.reset_ready_checkpoint_reads)) throw;
            const bool vif_handoff=runtime.native_module_loads==14 && runtime.heap_frees==1 && runtime.dmac_startup_writes==3
                && stop.address==0x10003C10 && stop.size==4 && context.pc==0x30FF70 && context.r[29].low==runtime.initial_sp-32;
            const bool texture_handoff=runtime.graph_startup_phase==4 && runtime.vif_startup.phase==6
                && stop.address==0x1000A000 && stop.size==4 && context.pc==0x28B928
                && context.r[29].low==runtime.initial_sp-128 && runtime.gif_dma.completions==2;
            if(runtime.post_sif.supplied && !vif_handoff && !texture_handoff) throw; // Explicit subsystem boundaries only.
            if(vif_handoff || texture_handoff) { /* Preserve the checked memory stop; no unsupported request succeeds. */ }
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
        if (cd_mode && argc < 18) {
            require(stopped && !stop_handler && (stop_target || (stop_selector == 0x40 && context.pc == 0x32EC68)),
                    "CD continuation did not reach a checked boundary");
            require(runtime.cd_init_calls == 1 && runtime.cd_init_context_preservations == 1
                    && runtime.sif_dma_submissions == 3 && runtime.semaphore_deletions == 2
                    && runtime.semaphore_waits == 3 && runtime.semaphore_signals == 3,
                    "CD-init callback or cleanup effects differ");
            if (stop_target) {
                const bool mapped_stop = (stop_target == 0x320488 && stop_source == 0x1003AC && stop_resume == 0x1003B4
                        && runtime.guest_calls == 160 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 32)
                    || (stop_target == 0x31F6A8 && stop_source == 0x3204A8 && stop_resume == 0x3204B0
                        && runtime.guest_calls == 161 && runtime.returns == 158 && context.r[29].low == runtime.initial_sp - 112)
                    || (stop_target == 0x32ECB0 && stop_source == 0x31F6CC && stop_resume == 0x31F6D4
                        && runtime.guest_calls == 163 && runtime.returns == 159 && context.r[29].low == runtime.initial_sp - 176);
                require(mapped_stop && runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7,
                        "Reached mapped-entry boundary differs");
            } else if (runtime.cd_semaphores_created) {
                require(runtime.cd_semaphores_created == 4 && runtime.semaphores.size() == 7
                        && runtime.semaphore_polls == 1 && runtime.guest_calls == 173 && runtime.returns == 167
                        && context.r[29].low == runtime.initial_sp - 288,
                        "CD setup and next bind progress differs");
            } else require(runtime.semaphores.size() == 3 && context.r[29].low == runtime.initial_sp - 304,
                           "CD semaphore checkpoint boundary differs");
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write CD continuation report");
            report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
                << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
                << ",\"resume_pc\":" << stop_resume << ",\"calls\":" << runtime.guest_calls
                << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
                << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
                << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
                << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
                << ",\"lo\":" << context.lo << ",\"cd_init_calls\":" << runtime.cd_init_calls
                << ",\"deletions\":" << runtime.semaphore_deletions
                << ",\"polls\":" << runtime.semaphore_polls << ",\"cd_semaphores\":" << runtime.cd_semaphores_created
                << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
                << ",\"descriptor_words\":[";
            for (unsigned index = 0; index < (stop_target ? 0u : 6u); ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
            report << "]}\n";
            std::cout << "CD-init continuation report written.\n";
            return 0;
        }
        if (argc >= 18) {
            const auto semas=runtime.semaphores.size();
            const bool pending_mc_bind=runtime.dbc_rpc_calls==4 && semas==11 && stop_selector==0x77 && context.pc==0x32F028
                && runtime.semaphores[10].maximum==1 && runtime.semaphores[10].count==0 && (runtime.semaphores[10].option==0x3838D8 || (runtime.mc_rpc_binds==1 && runtime.semaphores[10].option==0x3838E8));
            const bool pending_mc_query=runtime.mc_rpc_calls==1 && semas==10 && stop_selector==0x77 && context.pc==0x32F028
                && runtime.read32(runtime.memory().data(),0x3A97DC)==0x3A6F80
                && runtime.read32(runtime.memory().data(),0x3A97E0)==1;
            const bool mc_local_semas=runtime.dbc_rpc_calls==4 && (semas==10 || pending_mc_bind)
                && runtime.semaphores[8].maximum==1 && runtime.semaphores[8].count==(pending_mc_query?0u:1u-runtime.mc_lock_waits+runtime.mc_lock_signals)
                && runtime.semaphores[8].option==0x3824C8 && runtime.semaphores[9].maximum==1
                && runtime.semaphores[9].count==0 && runtime.semaphores[9].option==0x3824D8;
            const bool dbc_semas=runtime.dbc_rpc_binds==4 && runtime.dbc_rpc_calls>=1 && (semas==9 || mc_local_semas)
                && runtime.semaphores[7].maximum==127 && runtime.semaphores[7].option==0x381C28;
            require(stopped && !stop_handler && runtime.scmd_bind_calls==1 && runtime.cd_init_calls==1
                && (runtime.post_cd.supplied ? (semas==7 || semas==8 || dbc_semas)
                    : semas==(runtime.post_sif.rpc_initialized && stop_selector==0x77?8u:7u)),"SCMD continuation state differs");
            if (!runtime.scmd_mmode_calls) {
                require(!stop_target && stop_selector == 0x40 && context.pc == 0x32EC68
                        && runtime.guest_calls == 190 && runtime.returns == 185
                        && context.r[29].low == runtime.initial_sp - 304 && runtime.semaphore_deletions == 3,
                        "SCMD call-semaphore boundary differs");
            } else {
                require(runtime.semaphore_deletions == 4+runtime.serviceOperations() && runtime.semaphore_waits == 5+runtime.serviceOperations()+runtime.ee_heap_lock_waits+runtime.dbc_lock_waits+runtime.mc_lock_waits && runtime.semaphore_signals == 6+runtime.serviceOperations()+(runtime.post_cd.scmd_binds?1u:0u)+runtime.ee_heap_lock_signals+runtime.dbc_lock_signals+runtime.mc_lock_signals
                        && (runtime.sif_dma_submissions == 5 || (runtime.iop_reset_pending && runtime.sif_dma_submissions == 6+runtime.post_sif.submissions+runtime.serviceOperations()+runtime.heap_transfers)), "SCMD media-mode cleanup differs");
                if (stop_target && !runtime.sif_stop_calls) {
                    const std::array<std::array<std::uint32_t,2>,9> reached{{
                        {0x338158,0x1003B8},{0x332188,0x3381D4},{0x331B08,0x332190},
                        {0x32F810,0x331B10},{0x32E9D0,0x32F840},{0x32E990,0x331B20},
                        {0x337F88,0x33824C},{0x33A3F0,0x337FA4},{0x32EF30,0x337FB8}}};
                    require(std::any_of(reached.begin(),reached.end(),[&](const auto& entry) {
                        return entry[0] == stop_target && entry[1] == stop_source && stop_resume == stop_source + 8;
                    }), "Unexpected SCMD mapped-entry boundary");
                } else if (!runtime.sif_stop_calls) require(stop_selector == 0x6B && context.pc == 0x32EF38
                    && runtime.guest_calls == 214 && runtime.returns == 209
                    && context.r[29].low == runtime.initial_sp - 224 && GPR_U32(&context,4) == 1
                    && runtime.dmac_disable_calls == 1 && runtime.dmac_removals == 1
                    && (runtime.post_sif.supplied || (runtime.dmac_mask == 0x800000 && runtime.dmac_handlers.empty()))
                    && runtime.read32(runtime.memory().data(),0x36AA90) == 0
                    && runtime.read32(runtime.memory().data(),0x36AA8C) == 0,
                    "SIF shutdown and DMA-stop boundary differs");
            }
            if (runtime.sif_stop_calls) {
                require(runtime.sif_stop_calls == 1 && runtime.sif2_chcr == 0 && runtime.sif2_qwc == 0
                        && (runtime.iop_reset_pending || context.r[29].low == runtime.initial_sp - 224),
                        "SIF stop continuation state differs");
                if (!runtime.sif_reset_subaddr_reads) {
                    require(!stop_target && stop_selector == 0x7A && context.pc == 0x32F078
                            && runtime.guest_calls == 215 && runtime.returns == 210,
                            "Post-stop SUBADDR boundary differs");
                } else if (!runtime.reset_smflag_writes) {
                    require(runtime.sif_reset_subaddr_reads == 1 && runtime.returns == 212
                            && ((stop_target == 0x32F060 && stop_source == 0x338070 && stop_resume == 0x338078
                                && runtime.guest_calls == 216)
                            || (!stop_target && stop_selector == 0x79 && context.pc == 0x32F068
                                && runtime.guest_calls == 217)), "First reset SMFLAG boundary differs");
                } else if (!runtime.iop_reset_pending) {
                    require(runtime.reset_smflag_writes == 1 && runtime.sif_reset_subaddr_reads == 1
                            && runtime.sif_smflag == 0x30000 && !stop_target && stop_selector == 0x77
                            && context.pc == 0x32F028 && runtime.guest_calls == 218 && runtime.returns == 213
                            && GPR_U32(&context,4) == runtime.initial_sp - 224 && GPR_U32(&context,5) == 1,
                            "IOP reboot submission boundary differs");
                    const std::array<std::uint32_t,4> descriptor{0x3ACC40,0x19600,104,0x44};
                    for (unsigned i=0;i<4;++i) require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4)==descriptor[i],
                                                    "IOP reboot descriptor differs");
                    const std::array<std::uint32_t,6> header{104,0,0x80000003,0,36,0};
                    for (unsigned i=0;i<6;++i) require(runtime.read32(runtime.memory().data(),0x3ACC40+i*4)==header[i],
                                                    "IOP reboot packet header differs");
                    constexpr char boot_argument[] = "rom0:UDNL cdrom0:\\IRX\\IOPRP300.IMG;1";
                    require(std::equal(std::begin(boot_argument),std::end(boot_argument),runtime.memory().begin()+0x3ACC58),
                            "IOP reboot path differs");
                } else {
                    require(!runtime.prior_iop_services_valid && runtime.iop_epoch == 1
                            && runtime.reset_clear_writes == 4 && runtime.reset_rpcinit == 0
                            && runtime.reset_subaddr == 0 && runtime.sif_smflag == 0
                            && runtime.sif_dma_submissions == 6+runtime.post_sif.submissions+runtime.serviceOperations()+runtime.heap_transfers && runtime.reset_smflag_writes == 1
                            && runtime.sif_reset_subaddr_reads == 1,
                            "Pending IOP reboot lifetime/register state differs");
                    if(runtime.reset_ready_checkpoint_reads) {
                        require(runtime.reset_ready_checkpoint_reads==1 && (runtime.post_sif.supplied || !stop_target
                            || (stop_target==0x310548 && stop_source==0x1002B0 && stop_resume==0x1002B8
                                && runtime.gif_dma.starts==1)),
                                "Unexpected ready continuation target/replay");
                        if(runtime.post_sif.supplied) {
                            const bool chain_target=stop_target==0x32F040 && stop_source==0x3319FC && stop_resume==0x331A04 && runtime.post_sif.phase==2;
                            const bool sreg_target=stop_target==0x331840 && stop_source==0x332140 && stop_resume==0x332148 && runtime.post_sif.phase==13;
                            const bool service_stop=!stop_target && stop_selector==0x77 && context.pc==0x32F028
                                && runtime.post_sif.phase==14 && runtime.post_sif.rpc_initialized
                                && runtime.guest_calls==305 && runtime.returns==298 && context.r[29].low==runtime.initial_sp-464
                                && GPR_U32(&context,4)==runtime.initial_sp-464 && GPR_U32(&context,5)==1;
                            const bool cd_stop=runtime.post_cd.supplied && !runtime.post_cd.scmd_supplied && !stop_target
                                && stop_selector==0x40 && context.pc==0x32EC68 && runtime.guest_calls==359 && runtime.returns==353
                                && context.r[29].low==runtime.initial_sp-288 && runtime.post_cd.binds==2 && runtime.post_cd.calls==1;
                            const std::array<std::array<std::uint32_t,3>,4> loader_targets{{
                                {0x333B48,0x1003F8,0x100400},{0x337C28,0x10040C,0x100414},
                                {0x337A00,0x337C34,0x337C3C},{0x337228,0x337A30,0x337A38}}};
                            const bool loader_target=runtime.post_cd.scmd_supplied && runtime.serviceOperations()==5
                                && std::any_of(loader_targets.begin(),loader_targets.end(),[&](const auto& t){return stop_target==t[0] && stop_source==t[1] && stop_resume==t[2];});
                            const bool loader_stop=runtime.post_cd.scmd_supplied && !stop_target && stop_selector==0x77 && context.pc==0x32F028
                                && runtime.guest_calls==400 && runtime.returns==391 && context.r[29].low==runtime.initial_sp-528
                                && GPR_U32(&context,4)==runtime.initial_sp-528 && GPR_U32(&context,5)==1;
                            const std::array<std::array<std::uint32_t,3>,3> loadfile_targets{{
                                {0x337328,0x337A40,0x337A48},{0x325098,0x33735C,0x337364},{0x326270,0x337A68,0x337A70}}};
                            const bool loadfile_helper=std::any_of(loadfile_targets.begin(),loadfile_targets.end(),[&](const auto& t){return stop_target==t[0] && stop_source==t[1] && stop_resume==t[2];});
                            const bool loadfile_version_sema=!stop_target && stop_selector==0x40 && context.pc==0x32EC68 && runtime.post_loadfile.binds==1 && !runtime.post_loadfile.versions
                                && runtime.guest_calls==411 && runtime.returns==404 && context.r[29].low==runtime.initial_sp-464;
                            const bool loadfile_version_submit=!stop_target && stop_selector==0x77 && context.pc==0x32F028 && runtime.post_loadfile.binds==1 && !runtime.post_loadfile.versions
                                && runtime.guest_calls==415 && runtime.returns==406 && context.r[29].low==runtime.initial_sp-608;
                            const bool loadfile_module_sema=!stop_target && stop_selector==0x40 && context.pc==0x32EC68 && runtime.post_loadfile.versions==1
                                && runtime.guest_calls==429 && runtime.returns==423 && context.r[29].low==runtime.initial_sp-384;
                            const bool loadfile_module_submit=!stop_target && stop_selector==0x77 && context.pc==0x32F028 && runtime.post_loadfile.versions==1
                                && runtime.guest_calls==433 && runtime.returns==425 && context.r[29].low==runtime.initial_sp-528 && GPR_U32(&context,4)==runtime.initial_sp-528 && GPR_U32(&context,5)==2;
                            const bool native_loader_progress=runtime.native_module_loads>0 && runtime.post_loadfile.versions==1;
                            const bool loader_progress=runtime.post_loadfile.supplied && runtime.post_loadfile.binds==1 && runtime.post_cd.modes==1
                                && (native_loader_progress || loadfile_helper || loadfile_version_sema || loadfile_version_submit || loadfile_module_sema || loadfile_module_submit);
                            require(loader_progress || cd_stop || loader_target || loader_stop || chain_target || sreg_target || service_stop,"Fresh SIF continuation boundary differs");
                            require(runtime.gif_dma.starts==2 && runtime.gif_dma.completions==2
                                && runtime.gif_dma.gs.packets==(runtime.texture_gif.starts?3u:2u) && runtime.gif_dma.gs.sprites==2 && runtime.gif_dma.gs.pixels==573440
                                && runtime.gif_dma.qwc==0 && runtime.gif_dma.madr==runtime.initial_sp-48
                                && runtime.gif_dma.sync_reads==5 && runtime.gif_dma.vu_reads==2 && runtime.gif_dma.packet_verified,
                                "Fresh SIF prior GS effects differ");
                            require(runtime.gs_interrupt.acknowledgements==2 && runtime.gs_interrupt.polls==2 && runtime.gs_interrupt.events==1
                                && !runtime.gs_interrupt.callbackPending() && runtime.gs_csr_reset_writes==1 && runtime.gs_csr_reads==1
                                && runtime.gs_imr_writes==1 && runtime.gs_crt_calls==1
                                && (runtime.graph_startup_phase==4?runtime.graph_halfword_checkpoint:runtime.halfword_writes)==(runtime.graph_startup_phase>=3?8u:runtime.graph_startup_phase>=2?7u:4u),
                                "Fresh SIF prior GS synchronization differs");
                            for(unsigned page : {0u,140u}) {
                                const auto pixel=runtime.gif_dma.gs.word(GsCommandSink::address(page,10,639,447,false));
                                require(pixel.value==0 && pixel.known==UINT32_MAX,"Fresh SIF startup framebuffer edge differs");
                            }
                            if(loader_progress || cd_stop || loader_target || loader_stop) {
                                require(runtime.post_sif.phase==14 && runtime.post_sif.rpc_initialized && runtime.post_cd.binds==2 && runtime.post_cd.calls==1
                                    && runtime.read32(runtime.memory().data(),0x3A87A8)==0 && runtime.read32(runtime.memory().data(),0x3A87CC)==0x43388
                                    && runtime.read32(runtime.memory().data(),0x3A87BC)==0x441B8 && runtime.dmac_mask==0xA00000
                                    && runtime.dmac_handlers.size()==1 && runtime.dmac_enable_calls==2,
                                    "Fresh CD ownership/completion differs");
                            }
                            if(loader_progress || loader_target || loader_stop) require(runtime.post_cd.scmd_binds==1 && runtime.post_cd.modes==1
                                && runtime.read32(runtime.memory().data(),0x369F40)==0 && runtime.read32(runtime.memory().data(),0x369F64)==0x434F0
                                && runtime.read32(runtime.memory().data(),0x369F54)==0x439A8 && runtime.read32(runtime.memory().data(),0x3695C0)==1,
                                "Fresh SCMD ownership/completion differs");
                            if(loader_stop) {
                                require(runtime.semaphores.size()==8 && runtime.semaphores[7].count==0 && runtime.semaphores[7].maximum==1
                                    && runtime.semaphores[7].option==0x3838D8,"LOADFILE bind semaphore differs");
                                const std::array<std::uint32_t,4> d={0x203A97C0,0x1E640,64,0x44};
                                for(unsigned n=0;n<4;++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==d[n],"LOADFILE descriptor differs");
                                const std::array<std::uint32_t,9> p={64,0,0x80000009,0,5,0x203A97C0,7,0x3ACC00,0x80000006};
                                for(unsigned n=0;n<9;++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==p[n],"LOADFILE request differs");
                            }
                            if(loadfile_module_submit) {
                                require(runtime.semaphores.size()==8 && !runtime.semaphores[7].count && runtime.semaphores[7].maximum==1 && runtime.semaphores[7].option==0x3838E8,
                                    "LOADFILE module semaphore differs");
                                const std::array<std::uint32_t,8> descriptor={0x3ACA00,0x225B0,512,0,0x203A97C0,0x1E640,64,0x44};
                                for(unsigned n=0;n<8;++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==descriptor[n],"LOADFILE module descriptor differs");
                                const std::array<std::uint32_t,14> request={0x20040,0x225B0,0x8000000A,0,5,0x203A97C0,9,0x3ACC00,0,512,0x3ACA00,8,1,0x22568};
                                for(unsigned n=0;n<14;++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==request[n],"LOADFILE module request differs");
                                constexpr char path[]="cdrom0:\\IRX\\SIO2MAN.IRX;1";
                                require(runtime.read32(runtime.memory().data(),0x3ACA00)==0 && std::memcmp(runtime.memory().data()+0x3ACA08,path,sizeof(path))==0,
                                    "LOADFILE module path/arguments differ");
                            }
                            if(service_stop && !runtime.post_cd.supplied) {
                                require(runtime.post_sif.submissions==2 && runtime.post_sif.sreg_responses==1
                                    && runtime.post_sif.ee_receive_address==0x3A94C0 && runtime.dmac_mask==0xA00000
                                    && runtime.dmac_handlers.size()==1 && runtime.dmac_enable_calls==2
                                    && runtime.read32(runtime.memory().data(),0x3A9740)==1
                                    && runtime.read32(runtime.memory().data(),0x3A9598+8)==0x1E640,
                                    "Fresh SIF ownership/handshake differs");
                                const std::array<std::uint32_t,4> descriptor={0x203A97C0,0x1E640,64,0x44};
                                for(unsigned n=0;n<descriptor.size();++n)require(runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+n*4)==descriptor[n],"Fresh RPC descriptor differs");
                                const std::array<std::uint32_t,9> packet={64,0,0x80000009,0,5,0x203A97C0,2,0x3A87A8,0x80000592};
                                for(unsigned n=0;n<packet.size();++n)require(runtime.read32(runtime.memory().data(),0x3A97C0+n*4)==packet[n],"Fresh RPC bind request differs");
                            }
                        }
                        if(!runtime.gs_csr_reset_writes) require(stop_address==0x12001000 && stop_size==8
                            && context.pc==0x30FE44 && runtime.guest_calls==230 && runtime.returns==226
                            && context.r[29].low==runtime.initial_sp-688 && !runtime.halfword_writes,
                            "First GS reset boundary differs");
                        else if(!runtime.gs_imr_writes) require(runtime.gs_csr_reads==1 && stop_selector==0x71
                            && context.pc==0x32EFA8 && runtime.guest_calls==231 && runtime.returns==226
                            && context.r[29].low==runtime.initial_sp-688 && runtime.halfword_writes==3,
                            "GS IMR boundary differs");
                        else if(!runtime.gs_crt_calls) require(stop_selector==2 && context.pc==0x32E868
                            && runtime.guest_calls==231 && runtime.returns==227
                            && context.r[29].low==runtime.initial_sp-608 && runtime.halfword_writes==4,
                            "GS CRT boundary differs");
                        else if(runtime.gif_dma.starts && !runtime.post_sif.supplied) {
                            require(runtime.gif_dma.gs.packets==runtime.gif_dma.starts
                                && runtime.gif_dma.gs.sprites==runtime.gif_dma.starts
                                && runtime.gif_dma.gs.pixels==std::uint64_t(runtime.gif_dma.starts)*640*448
                                && runtime.gif_dma.qwc==0 && runtime.gif_dma.packet_verified
                                && runtime.gif_dma.idle_reads==runtime.gif_dma.starts
                                && runtime.gif_dma.staging_writes==runtime.gif_dma.starts*2,
                                "GS command consumption/GIF state differs");
                            if(runtime.gif_dma.starts==1) require(runtime.gif_dma.chcr==0x101
                                && runtime.gif_dma.madr==runtime.initial_sp-288 && !runtime.gif_dma.completions
                                && runtime.returns==252
                                && ((stop_target==0x310548 && context.pc==0x310548 && !stop_address
                                        && context.r[29].low==runtime.initial_sp-608 && runtime.guest_calls==255)
                                    || (!stop_target && !runtime.gif_dma.syncSupplied()
                                        && stop_address==0x10009000 && stop_size==4 && context.pc==0x310560
                                        && context.r[29].low==runtime.initial_sp-624 && runtime.guest_calls==256)),
                                "First GS synchronization evidence boundary differs");
                            else require(runtime.gif_dma.starts==2 && runtime.gif_dma.completions==2
                                && runtime.gif_dma.sync_reads==5 && runtime.gif_dma.vu_reads==2
                                && runtime.gif_dma.chcr==1 && runtime.gif_dma.madr==runtime.initial_sp-48
                                && stop_address==0x1000E010 && stop_size==4 && context.pc==0x3319CC
                                && context.r[29].low==runtime.initial_sp-192
                                && runtime.guest_calls==265 && runtime.returns==261 && runtime.dma_status_reads==1,
                                "Post-reset SIF DMA status boundary differs");
                            for(unsigned page : {0u,140u}) {
                                if(page==0 && runtime.gif_dma.starts==1)continue;
                                const auto pixel=runtime.gif_dma.gs.word(GsCommandSink::address(page,10,639,447,false));
                                require(pixel.value==0 && pixel.known==UINT32_MAX,"Startup framebuffer clear edge differs");
                            }
                        }
                        else if(runtime.gs_interrupt.acknowledgements==2 && !runtime.post_sif.supplied) require(runtime.gs_interrupt.polls==2
                            && runtime.gs_interrupt.events==1 && runtime.gs_interrupt.status()==8
                            && !runtime.gs_interrupt.callbackPending() && runtime.gs_csr_reset_writes==1
                            && runtime.gs_csr_reads==1 && runtime.gs_imr_writes==1 && runtime.gs_crt_calls==1
                            && runtime.gs_crt_request==std::array<std::uint32_t,3>{1,2,1} && runtime.halfword_writes==4
                            && ((!runtime.gs_field_reads && stop_address==0x12001000 && stop_size==8
                                    && context.pc==0x3104F4 && runtime.guest_calls==254 && runtime.returns==250
                                    && context.r[29].low==runtime.initial_sp-640)
                                || (runtime.gs_field_reads==1 && stop_address==0x1000A000 && stop_size==4
                                    && (context.pc==0x3100E4 || (context.pc==0x3101A8 && runtime.gif_dma.ready() && GPR_U32(&context,4)==0x101
                                        && runtime.gif_dma.idle_reads==1 && runtime.gif_dma.staging_writes==2
                                        && runtime.gif_dma.qwc==15 && runtime.gif_dma.madr==runtime.initial_sp-528)) && runtime.guest_calls==255 && runtime.returns==251
                                    && context.r[29].low==runtime.initial_sp-624)),
                            "INTC/field/GIF DMA diagnostic boundary differs");
                        else if(!runtime.post_sif.supplied) require(runtime.gs_csr_reset_writes==1 && runtime.gs_csr_reads==1 && runtime.gs_imr_writes==1
                            && runtime.gs_crt_calls==1 && runtime.gs_crt_request==std::array<std::uint32_t,3>{1,2,1}
                            && stop_address==0x1000F000 && stop_size==4 && context.pc==0x32F10C
                            && runtime.guest_calls==252 && runtime.returns==247 && context.r[29].low==runtime.initial_sp-656
                            && runtime.halfword_writes==4 && GPR_U32(&context,4)==4,
                            "GS interrupt acknowledgement boundary differs");
                    } else
                    require((stop_target == 0x338108 && stop_source == 0x1003C8 && stop_resume == 0x1003D0
                                && runtime.guest_calls == 222 && runtime.returns == 220 && context.r[29].low == runtime.initial_sp - 32)
                            || (!stop_target && stop_selector == 0x7A && context.pc == 0x32F078
                                && runtime.guest_calls == 224 && runtime.returns == 220 && context.r[29].low == runtime.initial_sp - 48
                                && GPR_U32(&context,4) == 4), "Post-reboot readiness boundary differs");
                }
            }
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write SCMD report");
            report << "{\"status\":\"blocked at checked runtime boundary\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc
                << ",\"selector\":" << stop_selector << ",\"target\":" << stop_target << ",\"source_pc\":" << stop_source
                << ",\"resume_pc\":" << stop_resume << ",\"calls\":" << runtime.guest_calls << ",\"returns\":" << runtime.returns
                << ",\"words\":" << runtime.word_writes << ",\"doublewords\":" << runtime.doubleword_writes
                << ",\"bytes\":" << runtime.byte_writes << ",\"quads\":" << runtime.quadword_writes
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"deletions\":" << runtime.semaphore_deletions << ",\"dma_disables\":" << runtime.dmac_disable_calls
                << ",\"dma_removals\":" << runtime.dmac_removals << ",\"media_mode_calls\":" << runtime.scmd_mmode_calls
                << ",\"sif_stops\":" << runtime.sif_stop_calls << ",\"reset_subaddr_reads\":" << runtime.sif_reset_subaddr_reads
                << ",\"smflag_writes\":" << runtime.reset_smflag_writes
                << ",\"reset_clear_writes\":" << runtime.reset_clear_writes << ",\"iop_epoch\":" << runtime.iop_epoch
                << ",\"halfwords\":" << runtime.halfword_writes << ",\"gs_reset_writes\":" << runtime.gs_csr_reset_writes
                << ",\"gs_imr_writes\":" << runtime.gs_imr_writes << ",\"gs_crt_calls\":" << runtime.gs_crt_calls
                << ",\"native_module_loads\":" << runtime.native_module_loads
                << ",\"native_module_request\":" << std::quoted(runtime.native_module_request)
                << ",\"native_iop_error\":" << std::quoted(runtime.native_iop_error)
                << ",\"native_iop_pc\":" << runtime.native_iop_pc << ",\"native_iop_steps\":" << runtime.native_iop_steps
                << ",\"native_iop_targets\":" << runtime.native_iop_targets << ",\"native_iop_waiters\":" << runtime.native_iop_waiters
                << ",\"heap_transfers\":" << runtime.heap_transfers << ",\"heap_dma_polls\":" << runtime.heap_dma_polls
                << ",\"console_messages\":" << runtime.ee_console_messages.size()
                << ",\"console_first_message\":" << jsonString(runtime.ee_console_messages.empty()?std::string{}:runtime.ee_console_messages.front())
                << ",\"ee_status\":" << context.cop0_status
                << ",\"ee_thread_count\":" << runtime.threads.size()
                << ",\"sound_thread_starts\":" << runtime.sound_thread_starts
                << ",\"dbc_rpc_binds\":" << runtime.dbc_rpc_binds
                << ",\"mc_rpc_binds\":" << runtime.mc_rpc_binds
                << ",\"mc_rpc_calls\":" << runtime.mc_rpc_calls
                << ",\"dbc_rpc_calls\":" << runtime.dbc_rpc_calls
                << ",\"dbc_lock_waits\":" << runtime.dbc_lock_waits
                << ",\"dbc_lock_signals\":" << runtime.dbc_lock_signals
#ifdef BT2_NATIVE_IOP
                << ",\"dbc_dma_submissions\":" << (runtime.native_iop?runtime.native_iop->dbc_dma_submissions:0)
                << ",\"dbc_dma_polls\":" << (runtime.native_iop?runtime.native_iop->dbc_dma_polls:0)
                << ",\"mc_query_requests\":" << (runtime.native_iop?runtime.native_iop->mc_query_requests:0)
#endif
                << ",\"sounds_rpc_binds\":" << runtime.sounds_rpc_binds
                << ",\"sounds_rpc_calls\":" << runtime.sounds_rpc_calls
                << ",\"sound_rpc_binds\":" << runtime.sound_rpc_binds
                << ",\"sound_rpc_calls\":" << runtime.sound_rpc_calls
                << ",\"sound_client_words\":[";
            for(unsigned i=0;i<10;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C80+i*4);
            report<<"],\"sound_payload_words\":[";
            for(unsigned i=0;i<16;++i)report<<(i?",":"")<<runtime.read32(runtime.memory().data(),0x3A5C40+i*4);
            report<<"],\"sound_thread_words\":[";
#ifdef BT2_NATIVE_IOP
            if(runtime.sound_rpc_calls && runtime.native_iop) {
                auto& rt=runtime.native_iop->runtime;
                const auto handle=rt.read(0xA14CC,4);const auto& t=rt.threads.at(handle);
                report<<handle<<","<<t.entry<<","<<t.gp<<","<<t.stacksize<<","<<t.priority<<","<<t.attr<<","<<t.option<<","<<t.waiting<<","<<t.ready;
            }
#endif
            report<<"],\"ee_sound_context\":[";
            if(runtime.sound_thread_starts) {
                const auto& t=runtime.threads.at(1);const auto& c=t.context;
                report<<t.id<<","<<t.ready<<","<<c.pc<<","<<c.r[28].low<<","<<c.r[29].low<<","<<c.r[31].low<<","<<c.r[4].low<<","<<c.cop0_status;
            }
            report<<"],\"iop_rpc_servers\":[";
#ifdef BT2_NATIVE_IOP
            if(runtime.native_iop) {
                bool first=true;
                for(const auto& [id,server]:runtime.native_iop->runtime.rpc_servers) {
                    report<<(first?"":",")<<"["<<id<<","<<server.descriptor<<","<<server.buffer<<","<<server.handler<<","<<server.queue<<","<<server.gp<<"]";first=false;
                }
            }
#endif
            report<<"],\"dbc_client_words\":[";
            for(const auto client:std::array<std::uint32_t,4>{0x3A03C0,0x3A0410,0x3A0438,0x3A0460}) {
                if(client!=0x3A03C0)report<<",";report<<"[";
                for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),client+n*4);report<<"]";
            }
            report<<"],\"dbc_dma_ee_words\":[";
            for(unsigned n=0;n<32;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A27C0+n*4);
            report<<"],\"dbc_dma_payload_words\":[";
#ifdef BT2_NATIVE_IOP
            if(runtime.native_iop)for(unsigned n=0;n<32;++n) {
                std::uint32_t value=0;for(unsigned b=0;b<4;++b)value|=std::uint32_t(runtime.native_iop->dbc_dma_payload[n*4+b])<<(b*8);
                report<<(n?",":"")<<value;
            }
#endif
            report<<"],\"sio2_dma_channels\":[";
#ifdef BT2_NATIVE_IOP
            if(runtime.native_iop)for(unsigned channel: {11u,12u}) {
                const auto& dma=runtime.native_iop->runtime.dma_channels.at(channel);
                report<<(channel==12?",":"")<<"["<<channel<<","<<dma.madr<<","<<dma.bcr<<","<<dma.chcr<<","<<(dma.slice_known?1:0)<<"]";
            }
#endif
            report<<"],\"mc_client_words\":[";
            for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A6F80+n*4);
            report<<"],\"mc_input_words\":[";
            for(unsigned n=0;n<12;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A7000+n*4);
            report<<"],\"mc_result_words\":[";
            for(unsigned n=0;n<3;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A8540+n*4);
            report<<"],\"dbc_result_words\":[";
            for(unsigned n=0;n<36;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x3A04C0+n*4);
            report<<"],\"sounds_client_words\":[";
            if(runtime.sounds_rpc_binds)for(unsigned n=0;n<10;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x6351C0+n*4);
            report<<"],\"sounds_result_words\":[";
            if(runtime.sounds_rpc_calls)for(unsigned n=0;n<16;++n)report<<(n?",":"")<<runtime.read32(runtime.memory().data(),0x635180+n*4);
            report<<"],\"heap_calls\":" << runtime.heap_calls
                << ",\"heap_frees\":" << runtime.heap_frees
                << ",\"dmac_startup_writes\":" << runtime.dmac_startup_writes
                << ",\"dmac_control\":" << runtime.dmac_control << ",\"dmac_priority\":" << runtime.dmac_priority
                << ",\"ee_pool_pointers\":[" << runtime.read32(runtime.memory().data(),0x38A628) << "," << runtime.read32(runtime.memory().data(),0x38A62C) << "," << runtime.read32(runtime.memory().data(),0x38A630) << "," << runtime.read32(runtime.memory().data(),0x38A634) << "]"
                << ",\"main_thread_priority\":" << runtime.main_thread_priority
                << ",\"heap_end_queries\":" << runtime.heap_end_queries
                << ",\"ee_heap_lock_waits\":" << runtime.ee_heap_lock_waits
                << ",\"ee_heap_lock_signals\":" << runtime.ee_heap_lock_signals
                << ",\"timer1_setup_writes\":" << runtime.timer1_setup_writes
                << ",\"timer1_mode\":" << runtime.timer1_mode
                << ",\"texture_gif_phase\":" << runtime.texture_gif.phase
                << ",\"texture_gif_starts\":" << runtime.texture_gif.starts
                << ",\"texture_gif_completions\":" << runtime.texture_gif.completions
                << ",\"texture_gif_polls\":" << runtime.texture_gif.polls
                << ",\"tex0\":[" << runtime.gif_dma.gs.tex0[0] << "," << runtime.gif_dma.gs.tex0[1] << "]"
                << ",\"pabe\":" << runtime.gif_dma.gs.pabe
                << ",\"vif_phase\":" << runtime.vif_startup.phase << ",\"vif_commands\":" << runtime.vif_startup.commands
                << ",\"graph_startup_phase\":" << runtime.graph_startup_phase
                << ",\"vif_words\":" << runtime.vif_startup.words << ",\"vif_fifo_writes\":" << runtime.vif_startup.fifo_writes
                << ",\"vu_resets\":" << runtime.vif_startup.vu_resets << ",\"gif_resets\":" << runtime.vif_startup.gif_resets
                << ",\"vif_registers\":[" << runtime.vif_startup.error << ',' << runtime.vif_startup.cycle << ',' << runtime.vif_startup.mode
                << ',' << runtime.vif_startup.mask << ',' << runtime.vif_startup.code << ',' << runtime.vif_startup.itops
                << ',' << runtime.vif_startup.base << ',' << runtime.vif_startup.offset << ',' << runtime.vif_startup.tops << ']'
                << ",\"heap_binds\":" << runtime.heap_binds
                << ",\"loadfile_binds\":" << runtime.post_loadfile.binds << ",\"loadfile_versions\":" << runtime.post_loadfile.versions
                << ",\"post_cd_binds\":" << runtime.post_cd.binds << ",\"post_cd_calls\":" << runtime.post_cd.calls << ",\"post_scmd_binds\":" << runtime.post_cd.scmd_binds << ",\"post_scmd_modes\":" << runtime.post_cd.modes
                << ",\"post_sif_phase\":" << runtime.post_sif.phase
                << ",\"post_sif_submissions\":" << runtime.post_sif.submissions
                << ",\"post_sif_responses\":" << runtime.post_sif.sreg_responses
                << ",\"gif_completions\":" << runtime.gif_dma.completions
                << ",\"gif_sync_reads\":" << runtime.gif_dma.sync_reads
                << ",\"gs_packets\":" << runtime.gif_dma.gs.packets
                << ",\"gs_sprites\":" << runtime.gif_dma.gs.sprites
                << ",\"gs_pixels\":" << runtime.gif_dma.gs.pixels
                << ",\"gif_starts\":" << runtime.gif_dma.starts
                << ",\"gif_idle_reads\":" << runtime.gif_dma.idle_reads
                << ",\"gif_staging_writes\":" << runtime.gif_dma.staging_writes
                << ",\"gif_qwc\":" << runtime.gif_dma.qwc
                << ",\"gif_madr\":" << runtime.gif_dma.madr
                << ",\"gif_reserved_difference_words\":" << runtime.gif_dma.reserved_difference_words
                << ",\"gif_packet_verified\":" << (runtime.gif_dma.packet_verified ? "true" : "false")
                << ",\"intc_acks\":" << runtime.gs_interrupt.acknowledgements
                << ",\"intc_polls\":" << runtime.gs_interrupt.polls
                << ",\"intc_events\":" << runtime.gs_interrupt.events
                << ",\"gs_field_reads\":" << runtime.gs_field_reads
                << ",\"gs_csr_reads\":" << runtime.gs_csr_reads
                << ",\"memory_address\":" << stop_address << ",\"memory_size\":" << stop_size
                << ",\"reset_ready_checkpoint_reads\":" << runtime.reset_ready_checkpoint_reads
                << ",\"iop_reset_pending\":" << (runtime.iop_reset_pending ? "true" : "false")
                << ",\"prior_iop_services_valid\":" << (runtime.prior_iop_services_valid ? "true" : "false")
                << ",\"sp\":" << context.r[29].low << ",\"scmd_binds\":" << runtime.scmd_bind_calls << ",\"arguments\":[" << GPR_U32(&context,4) << "," << GPR_U32(&context,5) << "]"
                << ",\"descriptor_words\":[";
            for(unsigned i=0;i<((stop_selector==0x40 || stop_selector==0x77)?8u:0u);++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),GPR_U32(&context,4)+i*4);
            report << "],\"packet_words\":[";
            for(unsigned i=0;i<16;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3A97C0+i*4);
            report << "],\"heap_client_words\":[";
            for(unsigned i=0;i<10;++i)report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3AC840+i*4);
            report << "],\"reset_packet_words\":[";
            for(unsigned i=0;i<26;++i) report << (i?",":"") << runtime.read32(runtime.memory().data(),0x3ACC40+i*4);
            report << "]}\n";
#ifdef BT2_NATIVE_IOP
            if(runtime.heap_calls && runtime.native_iop) {
                std::ofstream dump(std::string(argv[2])+".iop.bin",std::ios::binary);
                if(!dump.write(reinterpret_cast<const char*>(runtime.native_iop->runtime.ram.data()),runtime.native_iop->runtime.ram.size()))throw std::runtime_error("Native IOP checkpoint write failed");
            }
#endif
            return 0;
        }
        if (bind_mode) {
            require(stopped && !stop_target && !stop_handler && runtime.sif_bind_callbacks == 1
                    && runtime.sif_bind_context_preservations == 1 && runtime.sif_dma_submissions == 2
                    && runtime.semaphore_waits == 2 && runtime.semaphore_signals == 2
                    && runtime.semaphore_deletions == 1 && runtime.startup_zero_verified
                    && runtime.syscall_registrations == 18 && runtime.custom_syscall_calls == 17
                    && runtime.kernel_word_writes == 668 && runtime.kernel_dispatch_validations == 12
                    && runtime.timer3_mode_reads == 1 && runtime.timer2_mode_reads == 2
                    && runtime.timer2_mode_writes == 1 && runtime.intc_mask == 0x1802
                    && runtime.dmac_mask == 0xA00000 && runtime.dmac_enable_calls == 1
                    && runtime.dma_status_reads == 1 && runtime.dma5_control_reads == 1
                    && runtime.sif_rpc_reads == 1 && runtime.sif_command_reads == 1,
                    "RPC bind continuation prerequisites/effects differ");
            require(runtime.read32(runtime.memory().data(), 0x3A87CC) == 0x3F570
                    && runtime.read32(runtime.memory().data(), 0x3A87BC) == 0x3FB20
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
                    && std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
                    "Bind response/persistent frame/kernel baseline differs");
            if (stop_selector == 0x77) require(context.pc == 0x32F028 && GPR_U32(&context, 5) == 2
                    && GPR_U32(&context, 4) == runtime.initial_sp - 544
                    && GPR_U32(&context, 29) == runtime.initial_sp - 544
                    && runtime.rpc_call_semaphores == 1 && runtime.semaphores.size() == 4
                    && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
                    && runtime.semaphores[3].option == 0x3838E8
                    && runtime.guest_calls == 146 && runtime.returns == 139
                    && runtime.word_writes == 1133 && runtime.doubleword_writes == 131
                    && runtime.byte_writes == 530 && runtime.quadword_writes == 43632
                    && runtime.uncached_word_accesses == 30 && runtime.uncached_byte_accesses == 4
                    && runtime.data_cache_lines == 6, "Unexpected first RPC call submission");
            else require(stop_selector == 0x40 && context.pc == 0x32EC68
                    && GPR_U32(&context, 4) == runtime.initial_sp - 400
                    && runtime.rpc_call_semaphores == 0 && runtime.semaphores.size() == 3,
                    "Unexpected RPC call semaphore checkpoint boundary");
            if (stop_selector == 0x77) {
                const std::array<std::uint32_t, 8> descriptors{0x3A8800, 0x3FB20, 4, 0,
                    0x203A97C0, 0x19600, 64, 0x44};
                const std::array<std::uint32_t, 16> packet{0x440, 0x3FB20, 0x8000000A, 0,
                    5, 0x203A97C0, 3, 0x3A87A8, 0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0};
                for (unsigned index = 0; index < descriptors.size(); ++index)
                    require(runtime.read32(runtime.memory().data(), runtime.initial_sp - 544 + index * 4) == descriptors[index],
                            "Pending first CD-init RPC descriptors differ");
                for (unsigned index = 0; index < packet.size(); ++index)
                    require(runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4) == packet[index],
                            "Pending first CD-init RPC packet differs");
            }
            require(context.hi == 0 && context.lo == 144 && context.hi1 == 0 && context.lo1 == 0
                    && context.cop0_status == 0x10000 && GPR_U32(&context, 28) == 0x38BDF0,
                    "CPU state changed across RPC continuation");
            std::ofstream report(argv[2]);
            if (!report) throw std::runtime_error("Could not write bind continuation report");
            report << "{\"status\":\"blocked at unsupported syscall\",\"entry_executed\":true,\"game_booted\":false,\"pc\":" << context.pc << ",\"selector\":" << stop_selector
                << ",\"target\":" << stop_address << ",\"calls\":" << runtime.guest_calls
                << ",\"returns\":" << runtime.returns << ",\"words\":" << runtime.word_writes
                << ",\"doublewords\":" << runtime.doubleword_writes << ",\"bytes\":" << runtime.byte_writes
                << ",\"quads\":" << runtime.quadword_writes << ",\"cache_lines\":" << runtime.data_cache_lines
                << ",\"aliases\":" << runtime.uncached_word_accesses << ",\"byte_aliases\":" << runtime.uncached_byte_accesses
                << ",\"waits\":" << runtime.semaphore_waits << ",\"signals\":" << runtime.semaphore_signals
                << ",\"semas\":" << runtime.semaphores.size() << ",\"sp\":" << context.r[29].low
                << ",\"lo\":" << context.lo << ",\"bind_callbacks\":" << runtime.sif_bind_callbacks
                << ",\"bind_context_preservations\":" << runtime.sif_bind_context_preservations
                << ",\"semaphore_deletions\":" << runtime.semaphore_deletions
                << ",\"call_semaphore_unused_attr\":" << runtime.rpc_call_sema_attributes
                << ",\"arguments\":[" << GPR_U32(&context, 4) << "," << GPR_U32(&context, 5) << "]"
                << ",\"descriptor_words\":[";
            for (unsigned index = 0; index < (stop_selector == 0x77 ? 8u : 6u); ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), GPR_U32(&context, 4) + index * 4);
            report << "],\"packet_words\":[";
            for (unsigned index = 0; index < 16; ++index)
                report << (index ? "," : "") << runtime.read32(runtime.memory().data(), 0x3A97C0 + index * 4);
            report << "]}\n";
            std::cout << "Generated RPC bind and cleanup returned; first CD-init RPC call reached a checked runtime boundary.\n";
            return 0;
        }
        require(stopped, "Entry did not stop at the expected checked boundary");
        require(runtime.startup_zero_verified && runtime.startup_quadword_writes == (zero_end - zero_start) / 16
                && runtime.startup_byte_writes == (zero_end - zero_start) % 16,
                "Entry zeroing did not complete before SetupThread");
        require(runtime.memory()[zero_start - 1] == image[zero_start - 1]
                && runtime.memory()[zero_end] == image[zero_end], "Entry zeroing crossed its bounds");
        require(runtime.quadword_writes == (zero_end - zero_start) / 16 + (timer_mode ? 512u : 0u)
                && runtime.byte_writes == (zero_end - zero_start) % 16 + (osd_readback_mode ? 512u : 0u) + (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected startup/fill store counts");
        require(context.hi == 0 && context.lo == (sif_dma_mode ? 144u : 0u) && context.hi1 == 0 && context.lo1 == 0
                && context.sa == 0 && context.fcr31 == 0 && context.f_acc == 0, "CPU startup state incorrect");
        require(std::all_of(context.f.begin(), context.f.end(), [](float value) { return value == 0; }), "FPR clearing failed");
        const std::array<std::uint32_t, 5> args{GPR_U32(&context, 4), GPR_U32(&context, 5),
            GPR_U32(&context, 6), GPR_U32(&context, 7), GPR_U32(&context, 8)};
        require(runtime.thread_arguments == std::array<std::uint32_t, 5>{0x0038BDF0, 0xFFFFFFFF, 0x00100000, 0x0038B980, 0x00100250},
                "SetupThread arguments differ from the reviewed entry");
        require(runtime.thread_initialized && runtime.initial_sp == 0x01FFED60
                && runtime.stack_base == 0x01EFF000 && GPR_U32(&context, 29) == runtime.initial_sp - (sif_rpc_mode ? 464 : sif_dma_mode ? 96 : sif_mode ? 240 : osd_readback_mode ? 192 : readback_mode ? 128 : intc_mode ? 48 : timer2_mode ? 160 : timer_mode ? 112 : snapshot_mode ? 80 : 144),
                "SetupThread stack model was not applied");
        require(GPR_U32(&context, 28) == runtime.thread_arguments[0], "Unexpected gp after SetupThread");
        require(runtime.heap_initialized && runtime.heap_start == 0x0043058F
                && runtime.heap_end == runtime.stack_base,
                "Unexpected SetupHeap state or return");
        if (!snapshot_mode) require(runtime.guest_calls == 9 && runtime.custom_syscall_calls == 1 && runtime.returns == 5 && runtime.doubleword_writes == 10 && runtime.word_writes == 8
                && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0,
                "Generated callee did not save the caller return address");
        if (snapshot_mode) {
            require(runtime.guest_calls == (sif_rpc_mode ? 129u : sif_dma_mode ? 115u : sif_mode ? 108u : dma5_mode ? 105u : osd_readback_mode ? 99u : osd_mode ? 59u : readback_mode ? 58u : intc_mode ? 48u : timer2_mode ? 46u : timer_mode ? 43u : 12u) && runtime.custom_syscall_calls == (osd_readback_mode ? 17u : timer_mode ? 10u : 2u) && runtime.returns == (sif_rpc_mode ? 122u : sif_dma_mode ? 111u : sif_mode ? 102u : dma5_mode ? 100u : osd_readback_mode ? 95u : osd_mode ? 54u : readback_mode ? 53u : intc_mode ? 45u : timer_mode ? 41u : 10u)
                    && runtime.word_writes == (sif_rpc_mode ? 1093u : sif_dma_mode ? 1062u : sif_mode ? 1040u : dma5_mode ? 1031u : osd_readback_mode ? 1030u : osd_mode ? 701u : readback_mode ? 699u : timer2_mode ? 682u : timer_mode ? 681u : 9u) && runtime.doubleword_writes == (sif_rpc_mode ? 107u : sif_mode ? 76u : dma5_mode ? 69u : osd_readback_mode ? 66u : readback_mode ? 40u : intc_mode ? 26u : timer2_mode ? 24u : timer_mode ? 21u : 14u),
                    "Unexpected snapshot startup execution counts");
            require(runtime.read32(runtime.memory().data(), 0x0036AB40) == 0x80014D00,
                    "Two generated scans did not converge on the captured table base");
        }
        if (timer_mode) {
            require(runtime.intc_handlers.size() == 2 && runtime.intc_handler_order == std::vector<std::uint32_t>{2, 1}
                    && runtime.intc_handlers[0].cause == 12 && runtime.intc_handlers[0].address == 0x80076488
                    && runtime.intc_handlers[1].cause == 11 && runtime.intc_handlers[1].address == 0x00339118
                    && runtime.intc_handlers[1].arg == 0 && runtime.intc_handlers[1].gp == 0x0038BDF0
                    && runtime.read32(runtime.memory().data(), 0x0036BAD8) == 2 && context.cop0_status == (osd_readback_mode ? 0x10000u : 0u),
                    "Interrupt registration ID/state or generated status helper differs");
            require(runtime.timer3_mode_reads == 1, "Unexpected Timer 3 replay read count");
            require(runtime.kernel_word_writes == (osd_readback_mode ? 668u : 0x740 / 4) && runtime.cache_requests == (osd_readback_mode ? 6u : 2u) && runtime.instruction_cache_invalidations == (osd_readback_mode ? 2u : 1u),
                    "Unexpected copied kernel size/cache request count");
            require(runtime.kernel_dispatch_validations == (osd_readback_mode ? 12u : 6u), "Unexpected copied kernel dispatch count");
            std::array<bool, 4> registered_slots{};
            for (std::uint32_t index = 0; index < 4; ++index) {
                const auto slot = runtime.read32(runtime.memory().data(), 0x80076710 + index * 8);
                require(slot >= 0xFC && slot <= 0xFF && !registered_slots[slot - 0xFC] && runtime.syscall_handlers[slot]
                        == runtime.read32(runtime.memory().data(), 0x80076714 + index * 8),
                        "Generated kernel table lookup/registration mismatch");
                registered_slots[slot - 0xFC] = true;
            }
            require(runtime.read32(runtime.memory().data(), 0x80076730) == 0x12C
                    && runtime.read32(runtime.memory().data(), 0x80076734) == runtime.timer3_interrupt_handler
                    && runtime.timer3_interrupt_handler == 0x80076488 && runtime.timer3_interrupt_enable_requested
                    && runtime.extended_registrations == 1
                    && runtime.syscall_handlers[8] == runtime.read32(runtime.memory().data(), 0x8007673C),
                    "Completed kernel registrations differ from copied table");
            for (std::uint32_t offset = 0; offset < 0x740; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x80076000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036B328 + offset), "Kernel copy mismatch");
            for (std::uint32_t offset = 0; offset < 0x28; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x00082000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036BA68 + offset), "Main RAM copy mismatch");
        }
        if (osd_readback_mode) {
            for (std::uint32_t offset = 0; offset < 0x330; offset += 4)
                require(runtime.read32(runtime.memory().data(), 0x80075000 + offset)
                        == runtime.read32(runtime.memory().data(), 0x0036BAF8 + offset), "Startup kernel payload copy differs");
            require(runtime.syscall_handlers[0x5B] == 0x80075000, "Startup kernel entry not registered");
        }
        if (osd_readback_mode) {
            require(runtime.semaphore_waits == 1 && runtime.semaphore_signals == 1
                    && runtime.read32(runtime.memory().data(), 0x003A8830) == 1
                    && runtime.read32(runtime.memory().data(), 0x003A8838) == 0x003A8818
                    && runtime.read32(runtime.memory().data(), 0x003A8820) == 0x0036C148,
                    "Generated helper/semaphore list state differs");
            for (const auto offset : {0u, 4u, 12u, 16u, 20u})
                require(runtime.read32(runtime.memory().data(), 0x003A8818 + offset) == 0,
                        "Generated helper descriptor differs");
            for (std::uint32_t offset = 0x300; offset < 0x328; offset += 8) {
                const auto selector = runtime.read32(runtime.memory().data(), 0x80075000 + offset);
                const auto handler = runtime.read32(runtime.memory().data(), 0x80075004 + offset);
                require(runtime.syscall_handlers[selector] == handler, "Copied table registration differs");
            }
            require(runtime.read32(runtime.memory().data(), 0x0036BE70) == 0x80075330
                    && runtime.read32(runtime.memory().data(), 0x0036BAF0) == 0
                    && runtime.read32(runtime.memory().data(), 0x0036BAF4) == 0
                    && std::all_of(runtime.memory().begin() + 0x003AF0C8, runtime.memory().begin() + 0x003AF2C8,
                                   [](auto byte) { return byte == 0; }), "Generated startup cleanup differs");
        }
        require(runtime.dma_status_reads == (dma_status_mode ? 1u : 0u)
                && runtime.dma5_control_reads == (dma5_mode ? 1u : 0u), "Unexpected DMA checkpoint read counts");
        if (dma5_mode) {
            require(runtime.dmac_handlers.size() == 1 && runtime.dmac_handler_order == std::vector<std::uint32_t>{1}
                    && runtime.dmac_handlers[0].channel == 5 && runtime.dmac_handlers[0].address == 0x00331DF0
                    && runtime.dmac_handlers[0].arg == 0x20 && runtime.dmac_handlers[0].gp == 0x0038BDF0
                    && runtime.dmac_mask == 0x00A00000 && runtime.dmac_enable_calls == 1
                    && runtime.read32(runtime.memory().data(), 0x003A9594) == 1,
                    "Generated DMA registration/enable state differs");
        } else require(runtime.dmac_handlers.empty() && runtime.dmac_enable_calls == 0,
                       "DMA handler state advanced before checkpoint");
        require(runtime.sif_command_reads == (sif_mode ? 1u : 0u)
                && runtime.data_cache_lines == (sif_rpc_mode ? 2u : sif_mode ? 1u : 0u), "Unexpected SIF/cache checkpoint counts");
        if (sif_mode && !sif_dma_mode) {
            const auto frame = runtime.initial_sp - 240;
            const std::array<std::uint32_t, 4> descriptor{0x003A9580, 0x19600, 20, 0x44};
            for (unsigned index = 0; index < descriptor.size(); ++index)
                require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                        "Generated SIF DMA descriptor differs");
            require(runtime.read32(runtime.memory().data(), 0x003A95A0) == 0x19600
                    && runtime.read32(runtime.memory().data(), 0x003A9580) == 20
                    && runtime.read32(runtime.memory().data(), 0x003A9584) == 0
                    && runtime.read32(runtime.memory().data(), 0x003A9588) == 0x80000000
                    && runtime.read32(runtime.memory().data(), 0x003A9590) == 0x003A94C0
                    && runtime.read64(runtime.memory().data(), frame + 112) == 0x00331DA0
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 112) == 0x00332038
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                    "Generated SIF packet/current stack differs");
        }
        require(runtime.sif_dma_submissions == (sif_dma_mode ? 1u : 0u)
                && runtime.sif_rpc_reads == (sif_rpc_mode ? 1u : 0u)
                && runtime.uncached_word_accesses == (sif_rpc_mode ? 11u : 0u)
                && runtime.uncached_byte_accesses == (sif_rpc_mode ? 2u : 0u), "Unexpected receive/RPC/alias counts");
        if (sif_dma_mode) {
            const auto iop = runtime.sifIopMemory();
            const std::array<std::uint32_t, 5> consumed{0, 0, 0x80000000, 0, 0x3A94C0};
            require(iop.size() == PS2Runtime::sif_iop_size && runtime.sif_ee_receive_address == 0x3A94C0
                    && std::memcmp(iop.data() + 0x19600, consumed.data(), 20) == 0
                    && std::memcmp(iop.data() + 0x196C8, &consumed[4], 4) == 0,
                    "Bounded IOP receive effects differ from capture");
            const std::array<std::uint32_t, 4> handlers{0x3322E8, 0x3326C0, 0x3328E0, 0x3323F8};
            const std::array<unsigned, 4> ids{8, 9, 10, 12};
            for (unsigned index = 0; index < ids.size(); ++index) {
                const auto address = 0x3A95C0 + ids[index] * 12;
                require(runtime.read32(runtime.memory().data(), address) == handlers[index]
                        && runtime.read32(runtime.memory().data(), address + 4) == 0x3AAFC0
                        && runtime.read32(runtime.memory().data(), address + 8) == 0x38BDF0,
                        "Generated command handler registrations differ");
            }
            require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0,
                    "Persistent main frame changed");
        }
        if (sif_rpc_mode) {
            const auto frame = runtime.initial_sp - 464;
            const std::array<std::uint32_t, 4> descriptor{0x203A97C0, 0x19600, 64, 0x44};
            for (unsigned index = 0; index < descriptor.size(); ++index)
                require(runtime.read32(runtime.memory().data(), frame + index * 4) == descriptor[index],
                        "Pending RPC DMA descriptor differs");
            require(runtime.read32(runtime.memory().data(), 0x3A97C0) == 64
                    && runtime.read32(runtime.memory().data(), 0x3A97C8) == 0x80000009
                    && runtime.read32(runtime.memory().data(), 0x3A97D8) == 2
                    && runtime.read32(runtime.memory().data(), 0x3A97DC) == 0x3A87A8
                    && runtime.read32(runtime.memory().data(), 0x3A97E0) == 0x80000592
                    && runtime.read32(runtime.memory().data(), 0x3A87B0) == 3
                    && runtime.semaphores.size() == 4 && runtime.semaphores[3].count == 0 && runtime.semaphores[3].maximum == 1
                    && runtime.semaphores[3].option == 0x3838D8,
                    "Pending RPC bind packet/semaphore differs");
            require(runtime.read64(runtime.memory().data(), frame + 112) == 0x331DA0
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 336) == 0x33284C
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 224) == 0x31F984
                    && runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003AC,
                    "Active RPC bind frames differ");
        } else if (sif_dma_mode) require(runtime.read64(runtime.memory().data(), runtime.initial_sp - 48) == 0x1003A4,
                                      "Active RPC initialization frame differs");
        require(std::equal(kernel_before.begin(), kernel_before.end(), runtime.kernelBaseline().begin()),
                "Captured kernel baseline changed");
        if (timer_mode) {
            const auto frame = runtime.initial_sp - (osd_readback_mode ? 80 : readback_mode ? 128 : intc_mode ? 48 : 112);
            if (!sif_mode) require(runtime.read64(runtime.memory().data(), frame + (osd_readback_mode ? 48 : readback_mode ? 32 : intc_mode ? 16 : 80)) == (osd_readback_mode ? 0x00338568 : readback_mode ? 0x00338660 : intc_mode ? 0x00338550 : 0x00338548)
                    && (osd_readback_mode
                        ? runtime.read64(runtime.memory().data(), runtime.initial_sp - 8) == 0x00100240
                          && runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0
                        : runtime.read64(runtime.memory().data(), runtime.initial_sp - 16) == 0x001001F0),
                    "Current startup frames did not save return addresses");
            if (!intc_mode) for (const auto offset : {0u, 16u, 32u, 48u, 64u})
                require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved startup register");
            for (std::uint32_t index = 0; index < 128; ++index) {
                const auto block = 0x003ACCC0 + index * 64;
                require(runtime.read32(runtime.memory().data(), block) == (index == 127 ? 0 : block + 64),
                        "Generated buffer links differ from observed loop");
                require(std::all_of(runtime.memory().begin() + block + 4, runtime.memory().begin() + block + 64,
                                   [](auto byte) { return byte == 0; }), "Generated buffer payload is not zero");
            }
        } else {
            const auto frame = runtime.initial_sp - 144;
            require(runtime.read64(runtime.memory().data(), frame + 112) == (snapshot_mode ? 0x00338540 : 0x00338538),
                    "Current frame did not save return address");
            for (const auto offset : {0u, 16u, 32u, 48u, 64u, 80u, 96u})
                require(runtime.read64(runtime.memory().data(), frame + offset) == 0, "Unexpected saved callee register");
        }
        require(runtime.syscall_registrations == (osd_readback_mode ? 18u : timer_mode ? 10u : 2u) && runtime.syscall_handlers[0x83] == 0x00338388
                && runtime.syscall_handlers[0x5A] == (osd_readback_mode ? 0x0033A7C8u : timer_mode ? 0x003389B8u : 0x00338350u),
                "Expected syscall registrations missing");
        if (!snapshot_mode) require(args[0] == 0x80000000 && args[1] == 0x80080000 && args[2] == 0x00338388,
                "Unexpected reached guest call arguments");
        require(runtime.semaphores.size() == (sif_rpc_mode ? 4u : readback_mode ? 3u : 2u)
                && runtime.semaphores[0].count == 1 && runtime.semaphores[0].maximum == 1
                && runtime.semaphores[0].option == 0x00383BF0
                && runtime.semaphores[1].count == 1 && runtime.semaphores[1].maximum == 1
                && runtime.semaphores[1].option == 0x00383C00,
                "Unexpected created semaphore state");
        if (readback_mode) {
            require(runtime.threads.size() == 1 && runtime.current_thread_id == 1, "Dormant thread not recorded");
            const auto& thread = runtime.threads[0];
            require(thread.id == 2 && thread.entry == 0x0032F960 && thread.stack_base == 0x003A8840
                    && thread.stack_size == 0x400 && thread.gp == 0x0038BDF0 && thread.initial_priority == 0
                    && thread.heap_base == runtime.heap_start && thread.context_address == 0x003A89A0
                    && thread.initial_sp == 0x003A8C20
                    && runtime.read32(runtime.memory().data(), 0x0036AA84) == 2,
                    "Generated CreateThread descriptor/return differs");
            for (const auto index : {28u, 29u, 30u, 31u}) {
                const auto expected = index == 28 ? thread.gp : index == 31 ? 0x1FC03000 : thread.initial_sp;
                require(runtime.read32(runtime.memory().data(), thread.context_address + index * 16) == expected,
                        "Dormant thread context initialization differs");
            }
        }
        if (readback_mode) require(runtime.semaphores[2].count == 0 && runtime.semaphores[2].maximum == 255
                && runtime.semaphores[2].option == 0x00383660
                && runtime.read32(runtime.memory().data(), 0x003A8C40) == 2,
                "Generated semaphore creation/result differs");
        require(runtime.read32(runtime.memory().data(), 0x0036AB58) == 0
                && runtime.read32(runtime.memory().data(), 0x0036AB5C) == 1,
                "Generated code did not store semaphore IDs");
        std::ofstream report(argv[2]);
        if (!report) throw std::runtime_error("Could not write entry stop report");
        report << "{\"status\":\"" << (dma5_mode ? "blocked at unsupported syscall" : osd_readback_mode ? "blocked at unsupported guest memory" : readback_mode ? "blocked at unsupported syscall" : intc_mode ? "blocked at unsupported guest memory" : timer2_mode ? "blocked at unsupported syscall" : "blocked at unsupported guest memory") << "\",\"entry_executed\":true,"
               << "\"target\":" << stop_target << ",\"source_pc\":" << stop_source << ",\"resume_pc\":" << stop_resume << ","
               << "\"selector\":" << stop_selector << ","
               << "\"kernel_word_writes\":" << runtime.kernel_word_writes << ",\"access\":\"" << (dma5_mode ? "syscall" : osd_readback_mode ? "read" : readback_mode ? "syscall" : intc_mode ? "read" : timer2_mode ? "syscall" : "read") << "\",\"address\":" << stop_address << ",\"size\":" << stop_size
               << ",\"timer3_interrupt_handler\":" << runtime.timer3_interrupt_handler
               << ",\"timer3_interrupt_enable_requested\":" << (runtime.timer3_interrupt_enable_requested ? "true" : "false")
               << ",\"extended_registrations\":" << runtime.extended_registrations
               << ",\"startup_zero_verified\":" << (runtime.startup_zero_verified ? "true" : "false")
               << ",\"kernel_dispatch_validations\":" << runtime.kernel_dispatch_validations
               << ",\"sif_dma_submissions\":" << runtime.sif_dma_submissions << ",\"sif_rpc_reads\":" << runtime.sif_rpc_reads
               << ",\"uncached_word_accesses\":" << runtime.uncached_word_accesses << ",\"uncached_byte_accesses\":" << runtime.uncached_byte_accesses
               << ",\"sif_command_reads\":" << runtime.sif_command_reads << ",\"data_cache_lines\":" << runtime.data_cache_lines
               << ",\"dma_status_reads\":" << runtime.dma_status_reads << ",\"dma5_control_reads\":" << runtime.dma5_control_reads
               << ",\"dmac_handlers\":" << runtime.dmac_handlers.size() << ",\"dmac_mask\":" << runtime.dmac_mask
               << ",\"dmac_enable_calls\":" << runtime.dmac_enable_calls
               << ",\"cache_requests\":" << runtime.cache_requests << ",\"instruction_cache_invalidations\":" << runtime.instruction_cache_invalidations << ",\"syscall_selector\":" << (osd_readback_mode ? stop_selector : GPR_U32(&context, 3))
               << ",\"write_value\":" << (osd_readback_mode ? 0 : runtime.timer2_mode_writes ? 0x382 : 0)
               << ",\"osd_config_reads\":" << runtime.osd_config_reads << ",\"osd_config_sets\":" << runtime.osd_config_sets
               << ",\"main_thread_priority\":" << runtime.main_thread_priority << ",\"priority_changes\":" << runtime.priority_changes
               << ",\"intc_mask\":" << runtime.intc_mask << ",\"intc_enable_calls\":" << runtime.intc_enable_calls
               << ",\"timer2_mode_writes\":" << runtime.timer2_mode_writes
               << ",\"timer2_mode_after_write\":" << runtime.timer2_mode_after_write
               << ",\"timer2_mode_reads\":" << runtime.timer2_mode_reads
               << ",\"timer3_mode_reads\":" << runtime.timer3_mode_reads << ",\"kernel_snapshot_loaded\":" << (snapshot_mode ? "true" : "false") << ",\"handler\":" << (timer_mode ? stop_handler : runtime.syscall_handlers[0x83]) << ",\"pc\":" << context.pc << ",\"arguments\":[";
        for (unsigned index = 0; index < args.size(); ++index) report << (index ? "," : "") << args[index];
        report << "],\"quadword_writes\":" << runtime.quadword_writes
               << ",\"guest_calls\":" << runtime.guest_calls << ",\"doubleword_writes\":" << runtime.doubleword_writes
               << ",\"semaphore_waits\":" << runtime.semaphore_waits << ",\"semaphore_signals\":" << runtime.semaphore_signals
               << ",\"semaphores_created\":" << runtime.semaphores.size() << ",\"returns\":" << runtime.returns
               << ",\"syscall_registrations\":" << runtime.syscall_registrations
               << ",\"word_writes\":" << runtime.word_writes
               << ",\"byte_writes\":" << runtime.byte_writes << ",\"game_booted\":false}\n";
        report.close();
        require(!report.fail(), "Entry stop report write failed");
        std::cout << "BT2 entry CPU initialization, zeroing and bootstrap state verified.\n";
        if (sif_rpc_mode) std::cout << "Generated initialization returned; native RPC bind packet reached unsupported IOP service 0x80000592.\n";
        else if (sif_dma_mode) std::cout << "Validated CHANGE_SADDR receive effects completed; reached RPCINIT query selector 0x80000002.\n";
        else if (sif_mode) std::cout << "Captured SIF query returned; generated descriptor/cache path reached SifSetDma syscall 0x77.\n";
        else if (dma5_mode) std::cout << "DMA checkpoints replayed; channel 5 handler/enable request recorded; reached SifGetReg selector 0x80000000.\n";
        else if (dma_status_mode) std::cout << "Captured D_STAT replayed; reached channel 5 control read 0x1000C000.\n";
        else if (osd_readback_mode) std::cout << "Generated helper tail chain returned; DMA initialization reached unsupported D_STAT read 0x1000E010.\n";
        else if (osd_mode) std::cout << "Captured OSD configuration returned; reached SetOsdConfigParam syscall 0x4A.\n";
        else if (readback_mode) std::cout << "Dormant thread ID 2 created; generated setup helper returned; main priority changed to 1; reached GetOsdConfigParam syscall 0x4B.\n";
        else if (intc_mode) std::cout << "EnableIntc changed cause 11; timer startup returned; stopped at a second Timer 2 read, PC 0x00338DD8.\n";
        else if (timer2_mode) std::cout << "Timer 2 startup mode write completed; generated ErrorEPC/ERET returned; stopped at EnableIntc syscall 0x14, cause 11.\n";
        else if (timer_mode) std::cout << "Interrupt handler ID 2 stored; generated status helper returned; stopped at Timer 2 MMIO read 0x10001010 at PC 0x00338C58.\n";
        else if (snapshot_mode) std::cout << "Snapshot scans returned; FUN_00338A00 reached unsupported MMIO read 0x"
                                     << std::hex << stop_address << " at PC 0x" << context.pc << std::dec << "\n";
        else std::cout << "No kernel snapshot supplied; stopped at first kernel read 0x80000000.\n";
        std::cout << "Game boot remains incomplete.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Entry probe failed: " << error.what() << '\n';
        return 1;
    }
}
