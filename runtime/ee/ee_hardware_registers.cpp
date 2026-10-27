#include "ee_runtime.h"

// EE hardware registers with one behavior wherever they are reached (RUNTIME_GENERALIZATION_PLAN.md,
// group 4): Timers 1 to 3, the DMAC's control registers, VIF1 and GIF control, the VU reset. The
// counters are diagnostics; none decides whether a request is accepted.

namespace {
// KSEG1 (uncached) aliases of the EE register window fold to the physical register.
std::uint32_t registerAddress(std::uint32_t address) {return (address&0xFFFF0000u)==0xB0000000u?address&0x1FFFFFFFu:address;}
}

// Timer 1 (COUNT 0x10000800, MODE 0x10000810), outside interrupts. A MODE write that sets CUE starts
// the count, one that clears it stops it.
bool EeRuntime::timer1Store(std::uint32_t address,std::uint32_t value) {
    if(address!=0x10000800 && address!=0x10000810)return false;
    if(in_interrupt_)throw UnsupportedGuestMemory(address,4);
    const bool counting=timer1.counting();
    try {if(address==0x10000810)timer1.writeMode(value);else timer1.writeCount(value);}
    catch(const std::runtime_error&) {throw UnsupportedGuestMemory(address,4);}
    if(!counting && timer1.counting())++timer1_starts;
    if(counting && !timer1.counting())++timer1_stops;
    timer1_wait_polls=0;
    return true;
}

// Logical time: a COUNT read of the counting H-BLANK clock on main's thread waits one line, and a
// crossed field boundary delivers that VBLANK_START first (interrupt handlers and woken threads run).
// Other reads do not advance time.
std::optional<std::uint32_t> EeRuntime::timer1Load(std::uint8_t* rdram,R5900Context* context,std::uint32_t address) {
    if(address!=0x10000800 && address!=0x10000810)return std::nullopt;
    if(in_interrupt_)throw UnsupportedGuestMemory(address,4);
    if(address==0x10000810)return timer1.mode();
    if(timer1.counting() && timer1.hblankClock() && current_thread_id==1) {
        ++timer1_polls;if(++timer1_wait_polls>100000)throw UnsupportedGuestMemory(address,4); // One wait never ends.
        const auto target=timer1.nextLine();
        if(timer1.crossesField(target)) {deliverVblank(rdram,context);++timer1_vblanks;}
        timer1.lineTo(target);
    }
    return timer1.count();
}

// Timer 2 (COUNT 0x10001000, MODE 0x10001010, COMP 0x10001020). Before it counts, MODE is the value
// the boot left (the supplied capture); a write acknowledges the flags (bits 10, 11) and sets bits 0
// to 9, and the first field after it starts the logical timer (advanceLogicalField). Once counting,
// main after its admission and the Timer 2 interrupt handler read and write the logical timer.
bool EeRuntime::timer2Store(const R5900Context* context,std::uint32_t address,std::uint32_t value) {
    address=registerAddress(address);
    if(address!=0x10001010 && address!=0x10001020)return false;
    if(timer2.started()) {
        if(!timer2Context(context))throw UnsupportedGuestMemory(address,4);
        try {if(address==0x10001010)timer2.writeMode(value);else timer2.writeComp(value);}
        catch(const std::runtime_error&) {throw UnsupportedGuestMemory(address,4);}
        return true;
    }
    if(address!=0x10001010 || !timer2_mode_supplied_ || value>0xFFFF)throw UnsupportedGuestMemory(address,4);
    timer2_mode_=(timer2_mode_&0xC00&~value)|(value&0x3FF);
    ++timer2_mode_writes;++word_writes;
    return true;
}

std::optional<std::uint32_t> EeRuntime::timer2Load(const R5900Context* context,std::uint32_t address) {
    address=registerAddress(address);
    if(address!=0x10001000 && address!=0x10001010 && address!=0x10001020)return std::nullopt;
    if(timer2.started()) {
        if(!timer2Context(context))throw UnsupportedGuestMemory(address,4);
        return address==0x10001000?timer2.read():address==0x10001010?timer2.mode():timer2.comp();
    }
    if(address!=0x10001010 || !timer2_mode_supplied_)throw UnsupportedGuestMemory(address,4);
    // A captured readback after the write must equal the modelled register.
    if(timer2_readback_supplied_ && timer2_mode_writes && timer2_mode_!=timer2_readback_expected_)
        throw std::runtime_error("Timer 2 modeled mode differs from captured readback");
    ++timer2_mode_reads;
    return timer2_mode_;
}

// Timer 3's MODE as the boot left it (the supplied capture); nothing else of Timer 3 is reached.
std::optional<std::uint32_t> EeRuntime::timer3Load(std::uint32_t address) {
    if(registerAddress(address)!=0x10001810)return std::nullopt;
    if(!timer3_mode_supplied_)throw UnsupportedGuestMemory(address,4);
    ++timer3_mode_reads;
    return timer3_mode_;
}

// The DMAC's control registers. D_CTRL with DMAE only (no stall control, cycle stealing or memory
// FIFO drain); D_PCR with every channel enabled when its priority control is on (transfers here are
// never held per channel); D_STAT's status bits are write-one-to-clear (CIS1 is the one modelled)
// and its mask bits toggle.
bool EeRuntime::dmacStore(std::uint32_t address,std::uint32_t value) {
    if(address!=0x1000E000 && address!=0x1000E010 && address!=0x1000E020)return false;
    if(address==0x1000E000) {
        if(value>1)throw UnsupportedGuestMemory(address,4);
        dmac_control=value;
    } else if(address==0x1000E020) {
        if((value&0x80000000u) && (value&0x03FF0000u)!=0x03FF0000u)throw UnsupportedGuestMemory(address,4);
        dmac_priority=value;
    } else {
        if(value&2)frame_vif1.cis=false;
        dmac_mask^=value&0x63FF0000u;
    }
    if(!gs_display_flips)++dmac_startup_writes;
    return true;
}

// VIF1 and GIF control with nothing in flight (every transfer completes when it starts): VIF1 FBRST's
// reset (RST) and stall cancel (STC), VIF1 ERR's error masks, GIF_CTRL's reset. A VIF1 force break or
// stop, and the GIF's temporary stop, would hold work this model never holds: refused. A GIF reset
// records VIF1 as the setup left it (the startup comparison reads it).
bool EeRuntime::vifGifControlStore(std::uint32_t address,std::uint32_t value) {
    if(address==0x10003C10) {
        if(value&~0x9u)throw UnsupportedGuestMemory(address,4);
        if(value&1)++vif1_resets;
        return true;
    }
    if(address==0x10003C20) {
        if(value>7)throw UnsupportedGuestMemory(address,4);
        vif1_error=value;return true;
    }
    if(address!=0x10003000)return false;
    if(value&~1u)throw UnsupportedGuestMemory(address,4);
    if(value) {
        ++gif_resets;
        vif_setup={vif1.r,vif1_error,vif1_fifo_writes,vif1_fifo_codes,vif1_resets,vu1_resets};
    }
    return true;
}

// VU FBRST (CTC2 vi28): VU1 runs each microprogram to its end inside a transfer and VU0 runs macro
// code only, so a reset or force break finds both idle. The debug and trap enables are kept.
void EeRuntime::writeVuFbrst(R5900Context* context,std::uint32_t value) {
    if(value&~0xF0Fu)throw std::runtime_error("Unsupported VU control write");
    if(value&0x200u)++vu1_resets;
    context->vu0_fbrst=value&0xC0C;
}
