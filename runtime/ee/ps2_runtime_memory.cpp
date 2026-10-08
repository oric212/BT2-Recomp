#include "ps2_runtime.h"

// PS2Runtime guest memory access and memory-mapped I/O (DMA, GIF/VIF1, timers, GS privileged registers).
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void PS2Runtime::write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
    address = uncachedWordAddress(rdram, address);
    if(address==0x10003000 && vif_startup.phase==5 && value==1 && !vif_startup.pending_mask) {
        checkMemory(rdram,0,0);
        if(gif_dma.starts!=2 || gif_dma.completions!=2 || gif_dma.qwc)throw UnsupportedGuestMemory(address,4);
        ++vif_startup.gif_resets;vif_startup.phase=6;return; // Idle GIF reset, not GS VRAM/drawing-state reset.
    }
    // The generated DMAC setup writes zero to STAT in its return delay slot.
    // STAT is W1C/toggle, so zero preserves the existing interrupt mask.
    if(address==0x1000E010 && dmac_startup_writes==2 && value==0 && heap_frees==1) {
        checkMemory(rdram,0,0);++dmac_startup_writes;return;
    }
    // Observed Timer 2 startup store only; no counter advancement or scheduling.
    if (address == 0xB0001010 && timer2_mode_supplied_ && timer2_mode_reads == 1
            && !timer2_mode_writes && value == 0x382) {
        checkMemory(rdram, 0, 0);
        timer2_mode_after_write = (timer2_mode_ & 0xC00 & ~value) | (value & 0x3FF);
        ++timer2_mode_writes;
        ++word_writes;
        return;
    }
    if (!startup_kernel_payload_.empty() && address >= 0x80075000 && address < 0x80075330) {
        if (memory_.empty() || rdram != memory_.data() || address % 4)
            throw UnsupportedGuestMemory(address, 4);
        const auto offset = address - 0x80075000;
        std::uint32_t expected;
        std::memcpy(&expected, startup_kernel_payload_.data() + offset, 4);
        if (value != expected || kernel_patch_words_.contains(address - 0x80000000))
            throw std::runtime_error("Startup kernel payload store differs or repeats");
        kernel_patch_words_[address - 0x80000000] = value;
        ++startup_kernel_word_writes;
        ++kernel_word_writes;
        ++word_writes;
        return;
    }
    if (address >= kernel_patch_start_ && address < kernel_patch_end_) {
        if (!kernel_patch_end_ || memory_.empty() || rdram != memory_.data() || address % 4
                || kernel_patch_end_ - address < 4) throw UnsupportedGuestMemory(address, 4);
        kernel_patch_words_[address - 0x80000000] = value;
        ++kernel_word_writes;
        ++word_writes;
        return;
    }
    checkMemory(rdram, address, 4);
    if (address % 4) throw std::runtime_error("Unsupported unaligned word store");
    std::memcpy(memory_.data() + address, &value, 4);
    ++word_writes;
}

void PS2Runtime::indexedDataCache(std::uint8_t* rdram, R5900Context* ctx,
                          std::uint32_t operation, std::uint32_t address) {
    const auto begin=static_cast<std::uint32_t>(ctx->r[4].low);
    const auto end=static_cast<std::uint32_t>(ctx->r[5].low);
    const auto way=address&1u;
    if ((address&~1u)>0xFC0 || (address&0x3Eu) || begin>end
        || (begin&63u) || (end&63u) || (ctx->cop0_status&0x10000u))
        throw std::runtime_error("Unsupported indexed cache range/status");
    checkMemory(rdram,begin,static_cast<std::size_t>(end)-begin+64u);
    if (operation==0x10u) {
        const auto slot=indexed_cache_tag_reads%128u;
        const auto expected=(slot/2u)*64u+(slot&1u);
        if (indexed_cache_tag_ready || address!=expected
            || (ctx->pc!=(way?0x32F500u:0x32F4C4u) && ctx->pc!=(way?0x32F640u:0x32F604u)))
            throw std::runtime_error("Indexed cache scan order differs");
        indexed_cache_tag_ready=true;
    } else if (operation==0x14u || operation==0x16u) {
        // Invalid coherent tags cannot select a nonzero owned EE range.
        throw std::runtime_error("Unexpected indexed writeback for invalid native tag");
    } else throw std::runtime_error("Unsupported indexed CACHE operation");
    ++indexed_cache_operations;
}

void PS2Runtime::frameVif1Start(std::uint32_t value) {
    auto& v=frame_vif1;
    if(value!=0x145 || v.qwc)throw std::runtime_error("Unsupported VIF1 DMA start");
    // Walk the whole chain first. Each DMA tag contributes its upper 64 bits (two VIF codes at
    // qword positions 2-3) and then its data qwords; codes are parsed with their real lengths.
    // NOP and FLUSH/FLUSHE/FLUSHA (no VU1 program or other path is running) have no effect;
    // DIRECT/DIRECTHL hand quadword-aligned PATH2 data to the GS. Any other code, or a DIRECT
    // payload that crosses a tag, stops at the channel start before any effect.
    struct Word {std::uint32_t value,tag;unsigned segment,phase;const std::uint8_t* at;};
    std::vector<Word> stream;std::uint32_t tag=v.tadr,next_tadr=0;unsigned tags=0,segment=0;
    for(unsigned n=0;;++n) {
        if(n>=65536 || tag%16 || tag>=ram_size-16)throw std::runtime_error("VIF1 DMA chain outside RAM");
        std::uint64_t lo;std::memcpy(&lo,memory_.data()+tag,8);
        const std::uint32_t qwc=std::uint32_t(lo&0xFFFF),id=std::uint32_t((lo>>28)&7),addr=std::uint32_t((lo>>32)&0x7FFFFFF0);
        if(lo&0x80000000)throw std::runtime_error("VIF1 DMA tag IRQ is not modelled");
        if(id==5 || id==6)throw std::runtime_error("VIF1 DMA call/ret tags are not modelled");
        ++segment;for(unsigned k=2;k<4;++k){std::uint32_t w;std::memcpy(&w,memory_.data()+tag+k*4,4);stream.push_back({w,tag,segment,k,memory_.data()+tag+k*4});}
        ++tags;
        if(qwc) {
            const auto data=(id==1 || id==2 || id==7)?tag+16:addr;
            if(data>=ram_size || std::size_t(qwc)*16>ram_size-data)throw std::runtime_error("VIF1 DMA data outside RAM");
            ++segment;
            for(std::uint32_t k=0;k<qwc*4;++k){std::uint32_t w;std::memcpy(&w,memory_.data()+data+k*4,4);stream.push_back({w,tag,segment,k%4,memory_.data()+data+k*4});}
        }
        if(id==0 || id==7) {next_tadr=id==7?tag+16+qwc*16:tag+16;break;}
        tag=id==1?tag+16+qwc*16:id==2?addr:tag+16;
    }
    std::vector<std::span<const std::uint8_t>> directs;std::uint64_t direct_qwords=0;
    try {
        for(std::size_t i=0;i<stream.size();) {
            const auto& w=stream[i];const unsigned cmd=(w.value>>24)&0x7F;
            if(cmd==0 || cmd==0x10 || cmd==0x11 || cmd==0x13) {++i;continue;}
            if(cmd==0x50 || cmd==0x51) {
                const std::size_t qwords=(w.value&0xFFFF)?(w.value&0xFFFF):65536,first=i+1,last=first+qwords*4;
                if(last>stream.size() || stream[first].phase || stream[first].segment!=stream[last-1].segment)
                    throw FrameVif1Unsupported{w.value,w.tag};
                directs.emplace_back(stream[first].at,qwords*16);direct_qwords+=qwords;i=last;continue;
            }
            throw FrameVif1Unsupported{w.value,w.tag};
        }
        gif_dma.gs.transferAll(directs); // One GS transaction: refused as a whole, without effects.
    } catch(const FrameVif1Unsupported& u) {
        v.blocked_code=u.code;v.blocked_tag=u.tag;v.blocked_tadr=v.tadr;++v.unsupported;
        throw UnsupportedGuestMemory(0x10009000,4); // Checked boundary at the channel start.
    } catch(const std::runtime_error& e) {
        std::cerr<<"VIF1 DIRECT refused by the GS model: "<<e.what()<<"\n";
        v.blocked_code=0x50000000;v.blocked_tag=v.tadr;v.blocked_tadr=v.tadr;++v.unsupported;
        throw UnsupportedGuestMemory(0x10009000,4);
    }
    v.tadr=next_tadr;v.tags+=tags;v.codes+=unsigned(stream.size());v.directs+=unsigned(directs.size());v.direct_qwords+=direct_qwords;
    v.chcr=value&~0x100u;v.cis=true;++v.starts;
}

void PS2Runtime::Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
    checkMemory(rdram,0,0);
    if(address==0x12001000 && vif_startup.phase==6 && !graph_startup_phase && value==0x200
        && context->pc==0x30FE44 && context->r[29].low==initial_sp-112 && gs_csr_reset_writes==1) {
        gif_dma.gs.resetDrawingState();graph_startup_phase=1;return;
    }
    if(address==0x12001000 && value==0x200 && (displayResetStep(context,0,0x30FE44,688) || displayResetStep(context,8,0x30FE44,128))) {
        gif_dma.gs.resetDrawingState();++display_reset_phase;return;
    }
    // Privileged GS display registers after display setup (main's thread): PMODE, SMODE2, DISPFB1/2,
    // DISPLAY1/2, EXTWRITE (off only) and BGCOLOR are recorded as display state for presentation.
    // CRT timing registers stay with SetGsCrt. The library flip (0x311860) is counted at its last
    // store. No presentation is claimed.
    if(current_thread_id==1 && !in_interrupt_ && display_reset_phase==13 && address>=0x12000000 && address<=0x120000E0 && !(address&15)) {
        const auto n=(address-0x12000000)/16;
        if(n!=0 && n!=2 && n!=7 && n!=8 && n!=9 && n!=10 && n!=13 && n!=14)throw UnsupportedGuestMemory(address,8);
        if(n==13 && value)throw UnsupportedGuestMemory(address,8); // EXTWRITE: write-back circuit stays off.
        if(!gs_display_flips && context->pc!=0x3118D0 && !gs_display_writes)throw UnsupportedGuestMemory(address,8); // First write is the library flip.
        gs_privileged[n]=value;++gs_display_writes;
        if(context->pc==0x311908 && n==14) {
            ++gs_display_flips;
            if(on_display_flip)on_display_flip(); // Host presentation of the completed flip (viewer mode).
            // Probe frame budget: stop after this completed flip (its store is applied) as a checked boundary.
            if(frame_limit && gs_display_flips==frame_limit) {frame_budget_reached=true;throw UnsupportedGuestMemory(address,8);}
        }
        return;
    }
    if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes || gs_csr_reads
            || context->pc!=0x30FE44 || context->r[29].low!=initial_sp-688 || value!=0x200)
        throw UnsupportedGuestMemory(address,8);
    // Reached reset accepted only with matching reference readback evidence.
    // Reset only the supported drawing-register defaults; VRAM stays unknown.
    gif_dma.gs.resetDrawingState();
    ++gs_csr_reset_writes;
}

std::uint64_t PS2Runtime::Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
    checkMemory(rdram,0,0);
    if(address==0x12001000 && graph_startup_phase==1 && context->pc==0x30FE50 && context->r[29].low==initial_sp-112) {
        graph_startup_phase=2;return 0x551B4000; // Independently captured second original CSR load; no native video clock.
    }
    if(address==0x12001000 && (displayResetStep(context,1,0x30FE50,688) || displayResetStep(context,9,0x30FE50,128))) {
        // Only the GS revision byte (bits 16..23) is consumed; the original keeps 0x1B.
        ++display_reset_phase;return 0x551B4000;
    }
    if(address==0x12001000 && (displayResetStep(context,5,0x3104F4,640) || displayResetStep(context,7,0x3104F4,80))) {
        // Only FIELD (bit 13) is consumed, after this wait's polled VBlank.
        if(polled_vblanks!=(display_reset_phase==5?1u:2u) || (display_intc_stat&4))throw UnsupportedGuestMemory(address,8); // One acknowledged VBlank per wait.
        ++display_reset_phase;if(display_reset_phase==6)clear_repeat_gif=gif_dma.repeat();
        return 0x551B4004|(display_field?0x2000u:0u);
    }
    if(address==0x12001000 && gs_field_supplied_ && gs_interrupt.acknowledgements==2 && !gs_field_reads
        && context->pc==0x3104F4 && context->r[29].low==initial_sp-640) {
        ++gs_field_reads;return 0x551B6004; // Independent captured field, not timed native GS state.
    }
    if(address!=0x12001000 || !gs_csr_supplied_ || gs_csr_reset_writes!=1 || gs_csr_reads
            || context->pc!=0x30FE50 || context->r[29].low!=initial_sp-688)
        throw UnsupportedGuestMemory(address,8);
    ++gs_csr_reads;return 0x551B4000; // Explicit captured reference readback, not native GS state.
}

void PS2Runtime::Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {
    if(address==0x10000810 && context->pc==0x28C770 && context->r[29].low==initial_sp-112
        && texture_gif.phase==8 && !timer1_setup_writes && value==3) {
        checkMemory(rdram,0,0);timer1_mode=value;timer1.configure(value);++timer1_setup_writes;return; // Disabled Timer 1 configuration only.
    }
    if(frameGifContext() && (address==0x10009000 || address==0x10009020 || address==0x10009030 || (address==0x1000E010 && value==2))) {
        checkMemory(rdram,0,0);auto& v=frame_vif1;
        if(address==0x1000E010) {v.cis=false;return;} // W1C of CIS1 only.
        if(v.chcr&0x100)throw UnsupportedGuestMemory(address,4);
        if(address==0x10009020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);v.qwc=value;return;}
        if(address==0x10009030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.tadr=value&0x0FFFFFFF;return;}
        frameVif1Start(value);return;
    }
    if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020)) {
        checkMemory(rdram,0,0);auto& g=frame_gif;
        if(g.chcr&0x100)throw UnsupportedGuestMemory(address,4); // Writes while a transfer runs.
        if(address==0x1000A020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);g.qwc=value;return;}
        if(address==0x1000A010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.madr=value&0x0FFFFFFF;return;}
        if(value!=0x101 || !g.qwc || g.madr>=ram_size || std::size_t(g.qwc)*16>ram_size-g.madr)throw UnsupportedGuestMemory(address,4);
        gif_dma.gs.transfer(std::span<const std::uint8_t>(memory_.data()+g.madr,std::size_t(g.qwc)*16));
        g.bytes+=std::size_t(g.qwc)*16;g.madr+=g.qwc*16;g.qwc=0;g.chcr=value&~0x100u;++g.starts;return;
    }
    // Main's H-BLANK delay helpers (0x28C778 start, 0x28C790 wait >= 400 and stop): logical Timer 1.
    if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
        if(address==0x10000810 && context->pc==0x28C78C && value==(timer1.mode()|0x80)) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_starts;timer1_wait_polls=0;return;}
        if(address==0x10000810 && context->pc==0x28C7CC && value==(timer1.mode()&~0x80u) && timer1.counting()) {checkMemory(rdram,0,0);timer1.writeMode(value);++timer1_stops;return;}
        if(address==0x10000800 && context->pc==0x28C7D4 && !value && !timer1.counting()) {checkMemory(rdram,0,0);timer1.writeCount(0);return;}
    }
    if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
        if(address==0x1000A020 && context->pc==0x28B96C) {checkMemory(rdram,0,0);texture_gif.count(value);return;}
        if(address==0x1000A010 && context->pc==0x28B978) {checkMemory(rdram,0,0);texture_gif.address(value,initial_sp-112,memory());return;}
        if(address==0x1000A000 && context->pc==0x28B98C) {checkMemory(rdram,0,0);texture_gif.start(value,memory(),gif_dma.gs);return;}
    }
    if(dmac_startup_writes==3 && heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
        if(address==0x10003C10 && context->pc==0x30FF70 && !vif_startup.phase && value==1) {
            checkMemory(rdram,0,0);vif_startup.phase=1;return;
        }
        if(address==0x10003C20 && context->pc==0x30FF80 && vif_startup.phase==1 && value==2) {
            checkMemory(rdram,0,0);vif_startup.error=value;vif_startup.phase=2;return;
        }
    }
    if(heap_frees==1 && native_module_loads==14 && context->r[29].low==initial_sp-32) {
        if(address==0x1000E000 && context->pc==0x28B2B4 && !dmac_startup_writes && value==1) {
            checkMemory(rdram,0,0);dmac_control=value;++dmac_startup_writes;return;
        }
        if(address==0x1000E020 && context->pc==0x28B2B8 && dmac_startup_writes==1 && value==0x83FF0002) {
            checkMemory(rdram,0,0);dmac_priority=value;++dmac_startup_writes;return;
        }
    }
    if((address==0xB0001010 || address==0xB0001020) && context->pc==0x338B30 && timer2Context(context)) {
        checkMemory(rdram,0,0);if(address==0xB0001010)timer2.writeMode(value);else timer2.writeComp(value);return;
    }
    if(address==0x1000F000 && value==4 && (displayVsyncWait(context,0x32F10C,0) || displayVsyncWait(context,0x32F160,0) || (frameGifContext() && timer1_stops && (context->pc==0x32F1AC || context->pc==0x32F1FC)))) {
        checkMemory(rdram,0,0);display_intc_stat&=~value;return; // Write-one-to-clear VBLANK_START.
    }
    if(clear_repeat_gif && display_reset_phase==6 && context->r[29].low==initial_sp-624 && displayResetStep(context,6,context->pc,624)) {
        if(address==0x1000A000 && context->pc==0x3101A8) {checkMemory(rdram,0,0);clear_repeat_gif->start(value,memory(),gif_dma.gs);return;}
        if(address==0x1000A020 && context->pc==0x310148) {checkMemory(rdram,0,0);clear_repeat_gif->setCount(value);return;}
        if(address==0x1000A010 && context->pc==0x310194) {
            checkMemory(rdram,0,0);clear_repeat_gif->setAddress(value,initial_sp-(clear_repeat_gif->starts?288u:528u),memory());return;
        }
    }
    if(address==0x1000F000 && gs_interrupt_supplied_ && gs_crt_calls==1
        && context->r[29].low==initial_sp-656 && value==4
        && ((context->pc==0x32F10C && !gs_interrupt.acknowledgements)
            || (context->pc==0x32F160 && gs_interrupt.acknowledgements==1))) {
        checkMemory(rdram,0,0);
        if(!gs_interrupt.acknowledgements) {
            if(intc_mask!=0x1802)throw std::runtime_error("INTC mask differs from checkpoint");
            gs_interrupt.arm(12,intc_mask);
        }
        gs_interrupt.acknowledge(value);return;
    }
    if(gif_dma_supplied_ && gs_field_reads==1 && context->r[29].low==initial_sp-624) {
        if(address==0x1000A000 && context->pc==0x3101A8) {
            checkMemory(rdram,0,0);gif_dma.start(value,memory());return;
        }
        if(address==0x1000A020 && context->pc==0x310148) {
            checkMemory(rdram,0,0);gif_dma.setCount(value);return;
        }
        if(address==0x1000A010 && context->pc==0x310194) {
            checkMemory(rdram,0,0);gif_dma.setAddress(value,initial_sp-(gif_dma.starts?288u:528u),memory());return;
        }
    }
    write32(rdram, address, value);
}

std::uint32_t PS2Runtime::Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
    if(frameGifContext() && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020)) {
        checkMemory(rdram,0,0);if(context->pc==0x3105A4)++frame_syncs;
        return address==0x1000A000?frame_gif.chcr:address==0x1000A010?frame_gif.madr:frame_gif.qwc;
    }
    // Frame phase: GIF and VIF1 transfers complete synchronously and no VU1 work is started, so
    // channel and path status reads see idle (VIF1_STAT 0x40 and GIF_STAT 0 as captured at the
    // startup path sync).
    if(frameGifContext() && !(frame_gif.chcr&0x100) && !(frame_vif1.chcr&0x100)) {
        if(address==0x10009000 || address==0x10009020 || address==0x10009030) {
            checkMemory(rdram,0,0);return address==0x10009000?frame_vif1.chcr:address==0x10009020?frame_vif1.qwc:frame_vif1.tadr;
        }
        if(address==0x10003C00) {checkMemory(rdram,0,0);return 0x40;}
        if(address==0x10003020) {checkMemory(rdram,0,0);return 0;}
    }
    if(timer1_setup_writes==1 && current_thread_id==1 && !in_interrupt_) {
        if(address==0x10000810 && (context->pc==0x28C780 || context->pc==0x28C7C0)) {checkMemory(rdram,0,0);return timer1.mode();}
        if(address==0x10000800 && context->pc==0x28C798 && timer1.counting()) {
            // Logical time: each poll of the delay loop waits one line; a crossed field boundary
            // delivers that VBLANK_START first (interrupt handlers and woken threads run).
            checkMemory(rdram,0,0);
            ++timer1_polls;if(++timer1_wait_polls>100000)throw UnsupportedGuestMemory(address,4); // One wait never ends.
            const auto target=timer1.nextLine();
            if(timer1.crossesField(target)) {deliverVblank(rdram,context);++timer1_vblanks;}
            timer1.lineTo(target);return timer1.count();
        }
    }
    if((address==0x10001000 || address==0x10001010) && timer2Context(context)) {
        checkMemory(rdram,0,0);return address==0x10001000?timer2.count():timer2.mode();
    }
    if(address==0x1000F000 && frameGifContext() && timer1_stops && context->pc==0x32F1D0 && vsync_flag) {
        checkMemory(rdram,0,0);deliverVblank(rdram,context);return display_intc_stat;
    }
    if(address==0x1000F000 && displayVsyncWait(context,0x32F130,0)) {
        checkMemory(rdram,0,0);
        // Logical time: a poll that finds VBLANK_START clear advances one VBlank (START and END latch).
        // Cause 2 is disabled here, so no handler runs; FIELD toggles in interlaced mode.
        if(!(display_intc_stat&4)) {display_intc_stat|=0xC;display_field=!display_field;++polled_vblanks;advanceLogicalField();}
        return display_intc_stat;
    }
    if(clear_repeat_gif && displayResetStep(context,6,context->pc,624)) {
        auto& g=*clear_repeat_gif;
        if(address==0x1000A000 && context->pc==0x3100E4) {checkMemory(rdram,0,0);return g.readIdle();}
        if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return g.readVifIdle();}
        if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {checkMemory(rdram,0,0);return g.readStarted(context->pc==0x3105CC);}
        if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return g.readVifStatus();}
        if(address==0x10003020 && context->pc==0x310664) {
            checkMemory(rdram,0,0);const auto status=g.readGifStatus();
            if(g.starts==2)display_reset_phase=7; // Both clear requests completed.
            return status;
        }
    }
    if(texture_gif.supplied && graph_startup_phase==4 && context->r[29].low==initial_sp-128) {
        if(address==0x1000A000 && context->pc==0x28B928) {checkMemory(rdram,0,0);return texture_gif.idle();}
        if(address==0x10003020 && context->pc==0x28B944) {checkMemory(rdram,0,0);return texture_gif.pathIdle();}
        if(address==0x1000A000 && context->pc==0x28B998) {checkMemory(rdram,0,0);return texture_gif.poll();}
        if(address==0x10003020 && context->pc==0x28B9B4) {checkMemory(rdram,0,0);return texture_gif.pathComplete();}
    }
    if(post_sif.supplied && gif_dma.completions==2 && iop_epoch==1 && context->r[29].low==initial_sp-192) {
        if(address==0x1000E010 && context->pc==0x3319CC) {checkMemory(rdram,0,0);return post_sif.status();}
        if(address==0x1000C000 && context->pc==0x3319EC) {checkMemory(rdram,0,0);return post_sif.channel();}
    }
    if(gif_dma.syncSupplied() && gif_dma.starts>=1 && gif_dma.starts<=2 && context->r[29].low==initial_sp-624) {
        if(address==0x10009000 && context->pc==0x310560) {checkMemory(rdram,0,0);return gif_dma.readVifIdle();}
        if(address==0x1000A000 && (context->pc==0x3105A4 || context->pc==0x3105CC)) {
            checkMemory(rdram,0,0);return gif_dma.readStarted(context->pc==0x3105CC);
        }
        if(address==0x10003020 && context->pc==0x310664) {checkMemory(rdram,0,0);return gif_dma.readGifStatus();}
        if(address==0x10003C00 && context->pc==0x3105EC) {checkMemory(rdram,0,0);return gif_dma.readVifStatus();}
    }
    if(address==0x1000A000 && gif_dma_supplied_ && gs_field_reads==1 && context->pc==0x3100E4
        && context->r[29].low==initial_sp-624) {
        checkMemory(rdram,0,0);return gif_dma.readIdle();
    }
    if(address==0x1000F000 && gs_interrupt_supplied_ && context->pc==0x32F130
        && context->r[29].low==initial_sp-656) {
        checkMemory(rdram,0,0);return gs_interrupt.poll();
    }
    if (address == 0x10001810 && timer3_mode_supplied_ && !timer3_mode_reads) {
        checkMemory(rdram, 0, 0);
        ++timer3_mode_reads;
        return timer3_mode_;
    }
    if (address == 0x10001010 && timer2_mode_supplied_ && !timer2_mode_reads) {
        checkMemory(rdram, 0, 0);
        ++timer2_mode_reads;
        return timer2_mode_;
    }
    if (address == 0x10001010 && timer2_readback_supplied_ && timer2_mode_reads == 1 && timer2_mode_writes == 1) {
        checkMemory(rdram, 0, 0);
        if (timer2_mode_after_write != timer2_readback_expected_)
            throw std::runtime_error("Timer 2 modeled mode differs from captured readback");
        ++timer2_mode_reads;
        return timer2_mode_after_write;
    }
    return read32(rdram, address);
}

std::uint32_t PS2Runtime::read32(std::uint8_t* rdram, std::uint32_t address) {
    address = uncachedWordAddress(rdram, address);
    if (address == 0x1000E010 && dma_status_supplied_ && !dma_status_reads) {
        checkMemory(rdram, 0, 0);
        ++dma_status_reads;
        return dma_status_word_;
    }
    if (address == 0x1000C000 && dma5_control_supplied_ && dma_status_reads == 1 && !dma5_control_reads) {
        checkMemory(rdram, 0, 0);
        ++dma5_control_reads;
        return dma5_control_word_;
    }
    // Read-only KSEG0 view of locally supplied kernel bytes; no fabricated kernel.
    if (address >= 0x80000000 && address < 0x80000000 + kernel_snapshot_size) {
        const auto offset = address - 0x80000000;
        if (kernel_snapshot_.empty() || memory_.empty() || rdram != memory_.data()
            || offset % 4 || offset > kernel_snapshot_.size() - 4)
            throw UnsupportedGuestMemory(address, 4);
        if (const auto patched = kernel_patch_words_.find(offset); patched != kernel_patch_words_.end())
            return patched->second;
        std::uint32_t value;
        std::memcpy(&value, kernel_snapshot_.data() + offset, 4);
        return value;
    }
    checkMemory(rdram, address, 4);
    if (address % 4) throw std::runtime_error("Unsupported unaligned word read");
    std::uint32_t value;
    std::memcpy(&value, memory_.data() + address, sizeof(value));
    return value;
}
