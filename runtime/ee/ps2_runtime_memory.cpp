#include "ee/frame_profile.h"
#include "ee_runtime.h"
#include "dma_chain.h"

// EeRuntime guest memory access and memory-mapped I/O (DMA, GIF/VIF1, timers, GS privileged registers).
// Kept out of the header so edits rebuild this unit, not every generated unit that includes it.

void EeRuntime::write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value) {
    if(auto* spr=scratchpadAt(address,4)) {std::memcpy(spr,&value,4);return;}
    address = uncachedWordAddress(rdram, address);
    if(dmacStore(address,value) || vifGifControlStore(address,value)) {checkMemory(rdram,0,0);return;} // ee_hardware_registers.cpp
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

void EeRuntime::indexedDataCache(std::uint8_t* rdram, R5900Context* ctx,
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

void EeRuntime::frameGifChainStart(std::uint32_t value) {
    // Source chain (MOD 1, no tag transfer): the data of every tag, in order, is PATH3 GIF data.
    // GIF packets may continue across tags, so the data is joined and sent as one GS transaction.
    auto& g=gif_channel;
    if(g.qwc || (value&0xC0) || vif1.r.mskpath3)throw UnsupportedGuestMemory(0x1000A000,4); // Pending MADR data, TTE or TIE; PATH3 masked.
    const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),g.tadr,"GIF");
    std::vector<std::uint8_t> data;
    for(const auto& c:chain.segments)data.insert(data.end(),c.data.begin(),c.data.end());
    if(!data.empty())gs_sink.transfer(data);
    const auto& last=chain.segments.back();
    std::uint64_t last_tag;std::memcpy(&last_tag,memory_.data()+last.tag,8);
    g.madr=last.data.empty()?g.madr:std::uint32_t(last.data.data()+last.data.size()-memory_.data());
    g.tadr=chain.next_tadr;g.bytes+=data.size();g.chcr=(std::uint32_t(last_tag)&0xFFFF0000u)|(value&0xFEFFu);
    ++g.starts;++g.chain_starts;dma_cis|=4; // CIS2.
}

void EeRuntime::frameVif1Start(std::uint32_t value) {
    auto& v=frame_vif1;
    if(value!=0x145 || v.qwc)throw std::runtime_error("Unsupported VIF1 DMA start");
    // Walk the whole chain first. Each DMA tag contributes its upper 64 bits (two VIF codes) and then
    // its data qwords. The list is checked as a whole, so a code that is not modelled stops at the
    // channel start before any effect; then it runs in order: UNPACK and MPG fill VU1 memory,
    // microprograms run to their end and their XGKICK packets (PATH1) reach the GS between the
    // DIRECT data (PATH2).
    const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),v.tadr,"VIF1");
    // The list's words and the first word index and address of each tag; the buffers are kept between lists.
    auto& words=vif1_list_words_;auto& tags=vif1_list_tags_;words.clear();tags.clear();
    const auto append=[&](const std::uint8_t* bytes,std::size_t count) {
        const auto* from=reinterpret_cast<const std::uint32_t*>(bytes);words.insert(words.end(),from,from+count);
    };
    for(const auto& c:chain.segments) {
        tags.emplace_back(words.size(),c.tag);
        append(c.tag_words,2);append(c.data.data(),c.data.size()/4);
    }
    auto blocked=[&](std::uint32_t code,std::uint32_t tag) {
        v.blocked_code=code;v.blocked_tag=tag;v.blocked_tadr=v.tadr;++v.unsupported;
        throw UnsupportedGuestMemory(0x10009000,4); // Checked boundary at the channel start.
    };
    const auto codes=vif1.codes,directs=vif1.directs;const auto direct_qwords=vif1.direct_qwords;
    const Vu1::Kick to_gs=[this](std::span<const std::uint8_t> data) {FrameProfile::Scope timed(frame_profile.gs),of_vu1(frame_profile.vu1_gs);gs_sink.transfer(data);};
    if(on_vif1_transfer)on_vif1_transfer(words);
    try {FrameProfile::Scope timed(frame_profile.vif1);vif1.check(words);vif1_running=true;struct Done {bool& flag;~Done(){flag=false;}} done{vif1_running};vif1.run(words,to_gs,to_gs);}
    catch(const Vif1Unsupported& u) {
        std::uint32_t tag=v.tadr;
        for(const auto& [first,address]:tags)if(first<=u.index)tag=address;
        std::cerr<<u.what()<<'\n';blocked(u.code,tag);
    } catch(const std::runtime_error& e) {
        // The GS model refused a packet or a microprogram reached an instruction that is not modelled.
        std::cerr<<"VIF1 display list stopped: "<<e.what()<<'\n';blocked(0x50000000,v.tadr);
    }
    v.tadr=chain.next_tadr;v.tags+=unsigned(chain.segments.size());v.codes+=vif1.codes-codes;
    v.directs+=vif1.directs-directs;v.direct_qwords+=vif1.direct_qwords-direct_qwords;
    v.chcr=value&~0x100u;dma_cis|=2;++v.starts; // CIS1.
}

void EeRuntime::countDisplayFlip() {
    if(!gs_display_flips)first_flip={gs_sink.state.regs,gif_channel.starts};
    ++gs_display_flips;
    if(on_display_flip)on_display_flip(); // Host presentation of the completed flip (viewer mode).
    // Probe frame budget: stop after this completed flip as a checked boundary.
    if(frame_limit && gs_display_flips==frame_limit) {frame_budget_reached=true;throw UnsupportedGuestMemory(0x120000E0,8);}
}

// INTC_STAT polled for VBLANK_START. Logical time: main finding it clear receives the next VBlank,
// delivered with its handlers when cause 2 is enabled, otherwise latched (VBLANK_START and END set,
// FIELD toggles, nothing runs). Other threads see the status as it is.
std::uint32_t EeRuntime::intcStatusPoll(std::uint8_t* rdram,R5900Context* context) {
    if(!(display_intc_stat&4) && current_thread_id==1 && !in_interrupt_) {
        if(intc_mask&4)deliverVblank(rdram,context);
        else {display_intc_stat|=0xC;display_field=!display_field;gs_csr_cleared=false;++polled_vblanks;advanceLogicalField();}
    }
    return display_intc_stat;
}

void EeRuntime::Store64(std::uint8_t* rdram, R5900Context*, std::uint32_t address, std::uint64_t value) {
    checkMemory(rdram,0,0);
    if(address==0x12001000 && value==0x200) {gsCsrReset();return;} // CSR RESET (ee_gs_privileged.cpp).
    if(!in_interrupt_ && gsReadbackStore64(address,value))return;
    // Privileged GS display registers outside interrupts: PMODE, SMODE2, DISPFB1/2, DISPLAY1/2,
    // EXTWRITE (off only) and BGCOLOR are recorded as display state for presentation.
    // CRT timing registers stay with SetGsCrt. A flip is counted at the library flip's last store
    // (BGCOLOR), or at the VBLANK ending a field whose display update did not include one (the
    // per-field update used while a movie plays).
    if(!in_interrupt_ && address>=0x12000000 && address<=0x120000E0 && !(address&15)) {
        const auto n=(address-0x12000000)/16;
        if(n!=0 && n!=2 && n!=7 && n!=8 && n!=9 && n!=10 && n!=13 && n!=14)throw UnsupportedGuestMemory(address,8);
        if(n==13 && value)throw UnsupportedGuestMemory(address,8); // EXTWRITE: write-back circuit stays off.
        gs_privileged[n]=value;++gs_display_writes;
        if(n==14)countDisplayFlip(); // The library flip ends with BGCOLOR.
        return;
    }
    throw UnsupportedGuestMemory(address,8);
}

std::uint64_t EeRuntime::Load64(std::uint8_t* rdram, R5900Context*, std::uint32_t address) {
    checkMemory(rdram,0,0);
    // IPU registers are 64-bit (IPU_CMD/IPU_TOP carry BUSY in bit 63): low word, then high word.
    if(address>=0x10002000 && address<0x10002040 && !(address&15)) {
        std::uint32_t low=0,high=0;
        if(ipuRead(address,low) && ipuRead(address+4,high))return low|(std::uint64_t(high)<<32);
    }
    if(address==0x12001000) {
        if(!gs_csr_cleared)++frame_csr_reads;
        return gsCsrRead(); // ee_gs_privileged.cpp
    }
    throw UnsupportedGuestMemory(address,8);
}

void EeRuntime::Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value) {
    if(auto* ram=ramWord(rdram,address)) {std::memcpy(ram,&value,4);++word_writes;return;} // Main RAM first, as in Load32.
    if(ipuWrite(address,value)) {checkMemory(rdram,0,0);return;}
    if(timer1Store(address,value) || timer2Store(context,address,value)) {checkMemory(rdram,0,0);return;} // ee_hardware_registers.cpp
    if(!in_interrupt_ && (address==0x10009000 || address==0x10009010 || address==0x10009020 || address==0x10009030)) {
        checkMemory(rdram,0,0);auto& v=frame_vif1;
        if(v.chcr&0x100)throw UnsupportedGuestMemory(address,4);
        if(address==0x10009020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);v.qwc=value;return;}
        if(address==0x10009010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.madr=value&0x0FFFFFFF;return;} // No scratchpad transfers.
        if(address==0x10009030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);v.tadr=value&0x0FFFFFFF;return;}
        // Normal mode (the GS readback sequence): 0x101 sends VIF codes from memory, 0x100 receives
        // local->host data; a display list is a source chain with tag transfer (0x145).
        if(value==0x101)frameVif1Send(value);else if(value==0x100)frameVif1Receive(value);else frameVif1Start(value);
        return;
    }
    if(!in_interrupt_ && gsReadbackStore32(address,value)) {checkMemory(rdram,0,0);return;}
    if(!in_interrupt_ && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020 || address==0x1000A030)) {
        checkMemory(rdram,0,0);auto& g=gif_channel;
        if(g.chcr&0x100)throw UnsupportedGuestMemory(address,4); // Writes while a transfer runs.
        if(address==0x1000A020) {if(value>0xFFFF)throw UnsupportedGuestMemory(address,4);g.qwc=value;return;}
        if(address==0x1000A010) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.madr=value&0x0FFFFFFF;return;}
        if(address==0x1000A030) {if(value&0x8000000F)throw UnsupportedGuestMemory(address,4);g.tadr=value&0x0FFFFFFF;return;}
        // CHCR bits 16-31 (TAG) are read-only: a read-modify-write start carries the last tag back.
        if((value&0xFFFF)==0x105) {frameGifChainStart(value);return;}
        if((value&0xFFFF)!=0x101 || vif1.r.mskpath3 || !g.qwc || g.madr>=ram_size || std::size_t(g.qwc)*16>ram_size-g.madr)throw UnsupportedGuestMemory(address,4);
        gs_sink.transfer(std::span<const std::uint8_t>(memory_.data()+g.madr,std::size_t(g.qwc)*16));
        g.bytes+=std::size_t(g.qwc)*16;g.madr+=g.qwc*16;g.qwc=0;g.chcr=value&0xFEFFu;++g.starts;dma_cis|=4; // CIS2.
        if(display_reset_phase==6 && ++display_clear_transfers==2)display_reset_phase=7; // Diagnostic: both clear requests sent.
        return;
    }
    if(address==0x1000F000) {checkMemory(rdram,0,0);display_intc_stat&=~value;return;} // INTC_STAT: write-one-to-clear (the VBLANK bits are modelled).
    write32(rdram, address, value);
}

std::uint32_t EeRuntime::Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
    // Main RAM first: none of the device and checkpoint cases below is a RAM address.
    if(const auto* ram=ramWord(rdram,address)) {std::uint32_t value;std::memcpy(&value,ram,4);return value;}
    if(std::uint32_t value=0;ipuRead(address,value)) {checkMemory(rdram,0,0);return value;}
    if(!in_interrupt_ && (address==0x1000A000 || address==0x1000A010 || address==0x1000A020 || address==0x1000A030)) {
        checkMemory(rdram,0,0);if(context->pc==0x3105A4)++frame_syncs; // The GS library's sync (a diagnostic count).
        return address==0x1000A000?gif_channel.chcr:address==0x1000A010?gif_channel.madr:address==0x1000A020?gif_channel.qwc:gif_channel.tadr;
    }
    // GIF and VIF1 transfers complete when they start and VU1 runs each microprogram to its end, so
    // channel and path status reads see idle (VIF1_STAT 0x40 and GIF_STAT 0 as captured at the
    // startup path sync).
    if(!in_interrupt_) {
        if(address==0x10009000 || address==0x10009020 || address==0x10009030) {
            checkMemory(rdram,0,0);return address==0x10009000?frame_vif1.chcr:address==0x10009020?frame_vif1.qwc:frame_vif1.tadr;
        }
        if(address==0x10003C00) {checkMemory(rdram,0,0);return vif1Status();} // DBF follows the microprogram starts.
        if(address==0x10003020) {checkMemory(rdram,0,0);return 0;}
    }
    if(const auto timer=(address&0xFFFF0000u)==0xB0000000u?address&0x1FFFFFFFu:address;timer>=0x10000800 && timer<0x10001820) { // Timers 1 to 3 and their KSEG1 aliases (ee_hardware_registers.cpp).
        checkMemory(rdram,0,0);
        if(const auto value=timer1Load(rdram,context,address))return *value;
        if(const auto value=timer2Load(context,address))return *value;
        if(const auto value=timer3Load(address))return *value;
    }
    if(address==0x1000F000) {checkMemory(rdram,0,0);return intcStatusPoll(rdram,context);}
    return read32(rdram, address);
}

std::uint32_t EeRuntime::read32(std::uint8_t* rdram, std::uint32_t address) {
    if(auto* spr=scratchpadAt(address,4)) {std::uint32_t value;std::memcpy(&value,spr,4);return value;}
    address = uncachedWordAddress(rdram, address);
    // D_STAT: the channel status bits and masks; SIF0's CHCR (services/sif_system.cpp).
    if (address == 0x1000E010) { checkMemory(rdram, 0, 0); ++dma_status_reads; return dma_cis | dmac_mask; }
    if (address == 0x1000C000) { checkMemory(rdram, 0, 0); ++dma5_control_reads; return sif0_chcr; }
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

bool EeRuntime::ipuRead(std::uint32_t address,std::uint32_t& value) {
    if(!ipuContext())return false;
    if(address==0x1000F520) {value=dmac_enabler;return true;}
    if(const auto spr=scratchpad_dma.read32(address)) {value=*spr;return true;}
    const auto ipu_value=ipu.read32(address);
    if(!ipu_value)return false;
    value=*ipu_value;return true;
}

bool EeRuntime::ipuWrite(std::uint32_t address,std::uint32_t value) {
    if(!ipuContext())return false;
    if(address==0x1000F590) {
        if((value&~0x10000u)!=(dmac_enabler&~0x10000u))throw UnsupportedGuestMemory(address,4);
        if(value&0x10000u)++dmac_suspends;
        dmac_enabler=value;return true; // D_ENABLEW mirrors into D_ENABLER; no channel is running.
    }
    if((address==0x1000D000 || address==0x1000D400) && (value&0x100u)) {scratchpadTransfer(address,value);return true;}
    ipu.ram=std::span<const std::uint8_t>(memory_.data(),ram_size);
    ipu.output_ram=std::span<std::uint8_t>(memory_.data(),ram_size);ipu.output_scratchpad=scratchpad_;
    try {return scratchpad_dma.write32(address,value) || ipu.write32(address,value);}
    catch(const std::runtime_error& e) {std::cerr<<"IPU refused: "<<e.what()<<std::endl;throw UnsupportedGuestMemory(address,4);}
}

// Scratchpad DMA in normal mode (MOD 0), completed synchronously: fromSPR (0x1000D000) copies QWC
// quadwords from SADR to main RAM at MADR; toSPR (0x1000D400) copies the other way, or in source chain
// mode (MOD 1) copies each tag's data from TADR's chain to SADR onwards. Other modes, tag transfer,
// transfers that wrap the 16 KB scratchpad and channel interrupts are refused.
void EeRuntime::scratchpadTransfer(std::uint32_t address,std::uint32_t value) {
    auto& c=scratchpad_dma.channels.at(address);
    const bool from_spr=address==0x1000D000;
    if(!from_spr && (value&0xFFu)==0x05 && !c.qwc) {
        const auto chain=walkDmaSourceChain(std::span<const std::uint8_t>(memory_.data(),ram_size),c.tadr,"toSPR");
        std::uint32_t sadr=c.sadr;
        for(const auto& segment:chain.segments) {
            if(sadr+segment.data.size()>scratchpad_size)throw UnsupportedGuestMemory(address,4);
            std::copy(segment.data.begin(),segment.data.end(),scratchpad_.begin()+sadr);sadr+=std::uint32_t(segment.data.size());
        }
        c.sadr=sadr&0x3FF0u;c.tadr=chain.next_tadr;c.chcr=value&~0x100u;++scratchpad_transfers;
        return;
    }
    const std::uint32_t madr=c.madr&0x0FFFFFF0u,bytes=c.qwc*16;
    if(((value>>2)&3) || (value&0xC0u) || !c.qwc || (c.madr&0x80000000u) || c.madr%16 || madr+bytes>ram_size || c.sadr+bytes>scratchpad_size)
        throw UnsupportedGuestMemory(address,4);
    auto ram=memory().subspan(madr,bytes);auto spr=scratchpad().subspan(c.sadr,bytes);
    if(from_spr)std::copy(spr.begin(),spr.end(),ram.begin());else std::copy(ram.begin(),ram.end(),spr.begin());
    c.madr+=bytes;c.sadr=(c.sadr+bytes)&0x3FF0u;c.qwc=0;c.chcr=value&~0x100u;++scratchpad_transfers;
}
