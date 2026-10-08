#include "native_runtime.h"

// IopRuntime memory-mapped hardware: the owned SPU2, SIO2 (memory card and pad bus),
// sound DMA and CDVD register reads and writes, checked against the captured startup states.

bool IopRuntime::ownedCriStartup() const {
    const auto server=rpc_servers.find(0x90000200);
    const auto child=std::find_if(threads.begin(),threads.end(),[](const auto& row){return row.second.entry==0x1AC890;});
    return server!=rpc_servers.end() && rpc_queues.contains(server->second.queue)
        && current_thread==rpc_queues.at(server->second.queue).thread && child!=threads.end()
        && child->second.waiting && !child->second.ready && child->second.wait_semaphore;
}

std::uint32_t IopRuntime::read(std::uint32_t address,unsigned size) {
    if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
    access_width=size;access_kind="read";access_value=0;
    const auto p=physical(address);
    if(size==4 && p==0x1F801014 && executing_pc==0x920B0 && spu2_auto_dma.configured
        && ownedCriStartup() && sound_bus.contains(p))return sound_bus.at(p);
    if(size==4 && p==0x1F8010C8 && executing_pc==0x92E34 && spu2_auto_dma.pending
        && ownedCriStartup() && !spu2_auto_dma.readbacks) {++spu2_auto_dma.readbacks;return spu2_auto_dma.chcr;}
    if(size==4 && p==0x1F8010C8 && executing_pc==0x92250 && !spu2_dma_idle_reads
        && spu2_fifo_halfwords==8 && spu2_reset_status_reads==2 && spu2_io_status_reads==1) {
        const auto server=rpc_servers.find(0x90000200);
        const auto child=std::find_if(threads.begin(),threads.end(),[](const auto& row){return row.second.entry==0x1AC890;});
        if(server==rpc_servers.end() || !rpc_queues.contains(server->second.queue)
            || current_thread!=rpc_queues.at(server->second.queue).thread || read(server->second.descriptor+36,4)!=0x40C
            || child==threads.end() || !child->second.waiting || child->second.ready || !child->second.wait_semaphore)
            throw std::runtime_error("SPU DMA idle query outside observed CRI startup");
        ++spu2_dma_idle_reads;return 0; // Captured initial CHCR; no DMA has been accepted by this owned device.
    }
    if(spu2_voice.enabled) {
        if(size==4 && spu2DmaCore(p,8)>=0)return spu2_voice.chcr[spu2DmaCore(p,8)];
        if(size==4 && spu2DmaCore(p,0)>=0)return spu2_voice.madr[spu2DmaCore(p,0)];
        if(size==2 && (p==0x1F900344 || p==0x1F900744))return 0x80; // No transfer in flight: data ready.
        if(size==2 && (p==0x1F9001A8 || p==0x1F9001AA || p==0x1F9005A8 || p==0x1F9005AA) && spu2_configuration.contains(p))
            return spu2_configuration.at(p); // Transfer start address readback.
        if(size==4 && (p==0x1F801014 || p==0x1F801414) && sound_bus.contains(p))return sound_bus.at(p);
    }
    if(size==1 && p==0x1F402005 && disc_type_known && executing_pc==0x1A3A38 && disc_type_reads==2 && !disc_ready_status_reads){++disc_ready_status_reads;return 0x4C;}
    if(size==1 && p==0x1F40200F && disc_type_known && ((executing_pc==0x1A3B1C && disc_type_reads==0) || (executing_pc==0x1A3A34 && disc_type_reads==1))){++disc_type_reads;return 0x14;}
    if(size==4 && abi_read_only_words.contains(p))return abi_read_only_words.at(p);
    if(size==2 && (p==0x1F900344 || p==0x1F900744)) {
        if(p==0x1F900344 && spu2_fifo_halfwords==8 && !spu2_io_status_reads && spu2_configuration.at(0x1F90019A)==0x8010
            && spu2_configuration.at(0x1F9001A8)==0 && spu2_configuration.at(0x1F9001AA)==0x2808) {
            ++spu2_io_status_reads;return 0x80; // Actual first I/O poll: data stored, busy bit 0x400 clear.
        }
        const std::map<std::uint32_t,std::uint32_t> reset={{0x1F9007C0,0x8000},{0x1F9007C6,0x900},{0x1F9007C8,0x200},{0x1F9007CA,8},
            {0x1F900760,0},{0x1F900762,0},{0x1F900788,0},{0x1F90078A,0},{0x1F9001B0,0},{0x1F9005B0,0},{0x1F90019A,0x8000},{0x1F90059A,0x8000}};
        if(spu2_configuration!=reset || spu2_reset_status_reads>=2 || p!=(spu2_reset_status_reads?0x1F900744u:0x1F900344u))throw std::runtime_error("SPU2 reset status prerequisites differ");
        ++spu2_reset_status_reads;return 0; // Direct local first-status checkpoint under this exact reset configuration.
    }
    if(size==2 && spu2_playback.enabled && (p==0x1F900340 || p==0x1F900342 || p==0x1F900740 || p==0x1F900742))
        return (spu2_playback.endx[p>=0x1F900400]>>((p&2)?16:0))&0xFFFF;
    if(size==2 && spu2_playback.enabled && p>=0x1F900000 && p<0x1F900800 && ((p-0x1F900000)&0x3FF)<0x180 && (p&0xF)==0xA)
        return static_cast<std::uint32_t>(spu2_playback.voices[p>=0x1F900400][((p-0x1F900000)&0x3FF)/16].envelope.level); // ENVX.
    if(size==2 && spu2_playback.enabled && p>=0x1F900000 && p<0x1F900800) {
        // LSA reads return the live loop address (block loop-start flags update it while a voice plays).
        const auto offset=(p-0x1F900000)&0x3FF;
        if(offset>=0x1C0 && offset<0x2E0 && ((offset-0x1C0)%12==4 || (offset-0x1C0)%12==6)) {
            const auto& v=spu2_playback.voices[p>=0x1F900400][(offset-0x1C0)/12];
            if(v.loop_known)return (offset-0x1C0)%12==4?(v.loop>>16)&0xF:v.loop&0xFFF8;
            if(spu2_configuration.contains(p))return spu2_configuration.at(p);
        }
    }
    if(size==2 && spu2_command_readbacks.contains(p)) {
        const auto record=spu2_command_readbacks.at(p);
        if(steps>record.instruction+1) {blocked_address=p;throw std::runtime_error("Late SPU2 dynamic register readback unsupported");}
        spu2_command_readbacks.erase(p);return record.value; // One observed immediate readback; no clocked voice-state read.
    }
    if(size==2 && p>=0x1F900000 && p<0x1F900800 && spu2_configuration.contains(p)) {
        const auto offset=(p-0x1F900000)&0x3FF;
        if((p>=0x1F900760 && p<=0x1F900796) || (offset>=0x1C0 && offset<0x2E0 && (offset-0x1C0)%12<=2) || offset==0x19A || offset==0x1B0 || (offset<0x180 && (offset%16==0 || offset%16==2 || offset%16==4 || offset%16==6 || offset%16==8)))
            return spu2_configuration.at(p); // Retained parameter register readback, no dynamic ENVX/status defaults.
    }
    if((p==0x1F8010F0 || p==0x1F801570) && size==4) {
        if(!sound_bus.contains(0x1F801404) || !sound_bus.contains(0x1F80140C) || !dma_channels.contains(11) || !dma_channels.contains(12)
            || !dma_channels.at(11).enabled || !dma_channels.at(12).enabled || dma_channels.at(11).priority!=3 || dma_channels.at(12).priority!=3)
            throw std::runtime_error("Sound DMA checkpoint prerequisites differ");
        if(!sound_bus.contains(p))sound_bus[p]=p==0x1F8010F0?0x0777FF77:0x07BBFF77;
        return sound_bus.at(p); // Guarded pre-LIBSD diagnostic checkpoint, not arbitrary hardware reset state.
    }
    if(p==0x1F808280 && size==4 && sio2_irq_status_known)return sio2_irq_status;
    if(p==0x1F80826C && size==4 && !sio2_receive.empty())return sio2_command_status;
    if(p==0x1F808270 && size==4 && !sio2_receive.empty())return 0xF;
    if(p==0x1F808274 && size==4 && !sio2_receive.empty())return 0;
    if(p==0x1F808264 && size==1) {
        if(sio2_receive_cursor>=sio2_receive.size())throw std::runtime_error("SIO2 receive without validated reply");
        return sio2_receive[sio2_receive_cursor++];
    }
    if(p==0x1F808268 && size==4 && sio2_control_known)return sio2_control;
    if(size==4 && sio2_parameters.contains(p))return sio2_parameters.at(p);
    check(p,size);
    if((size==2 && p%2) || (size==4 && p%4))throw std::runtime_error("Unaligned IOP load");
    std::uint32_t value=0;std::memcpy(&value,ram.data()+p,size);return value;
}

void IopRuntime::write(std::uint32_t address,std::uint32_t value,unsigned size) {
    if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
    if(size<4)value&=(1u<<(size*8))-1; // SB/SH store only the low byte/halfword of the register.
    access_width=size;access_kind="write";access_value=value;
    // Diagnostic write watchpoint (BT2_IOP_WATCH): stops on the first nonzero store touching that address.
    if(watch_address && value && physical(address)<=watch_address && watch_address<physical(address)+size)
        throw std::runtime_error("IOP watched write: address "+std::to_string(physical(address))+" value "+std::to_string(value)
            +" pc "+std::to_string(executing_pc)+" thread "+std::to_string(current_thread)+" interrupt "+std::to_string(in_interrupt));
    const auto p=physical(address);
    if(size==2 && spu2_playback.enabled && spu2_io_transfer_completed) {
        const auto offset=(p-0x1F900000)&0x3FF;
        if(p>=0x1F900000 && p<0x1F900800 && (offset==0x340 || offset==0x342)) {
            const unsigned core=p>=0x1F900400,mask=(offset&2)?0xFF0000:0xFFFF;
            spu2_playback.endx[core]&=~mask;spu2_configuration[p]=0;spu2_command_readbacks.erase(p);return;
        }
    }
    if(spu2_voice.enabled && writeSpu2Voice(p,value,size))return;
    if(size==2 && p==0x1F9001B0 && value==1 && executing_pc==0x92DB8) {
        if(!ownedCriStartup() || !spu2_io_transfer_completed || spu2_dma_idle_reads!=1 || spu2_auto_dma.configured
            || cpu_interrupts_enabled || in_interrupt || spu2_configuration.at(p)!=0
            || read(0x95328,4)!=0x1600 || read(0x95970,4)!=0 || read(0x95980,4)!=2048)
            throw std::runtime_error("AutoDMA enable outside observed CRI loop setup");
        const auto source=read(0x95978,4);check(source,4096);
        if(!interrupts.contains(36) || !interrupts.at(36).enabled || interrupts.at(36).handler!=0x927E8
            || interrupts.at(36).argument!=0x95328)throw std::runtime_error("AutoDMA lacks owned LIBSD completion handler");
        spu2_auto_dma.configured=true;spu2_auto_dma.owner=current_thread;spu2_auto_dma.source=source;
        spu2_configuration[p]=1;return;
    }
    if(spu2_auto_dma.configured && ownedCriStartup() && current_thread==spu2_auto_dma.owner) {
        auto& d=spu2_auto_dma;
        if(size==4 && p==0x1F801014 && executing_pc==0x920C0 && sound_bus.contains(p)
            && value==(sound_bus.at(p)&0xF0FFFFFF)) {sound_bus[p]=value;return;}
        if(size==4 && p==0x1F8010C0 && executing_pc==0x92DE8 && d.program_stage==0 && value==d.source) {
            check(value,2048);d.madr=value;d.program_stage=1;return;
        }
        if(size==2 && p==0x1F8010C4 && executing_pc==0x92DF4 && d.program_stage==1 && value==16) {
            d.bcr=value;d.program_stage=2;return;
        }
        if(size==2 && p==0x1F8010C6 && executing_pc==0x92E2C && d.program_stage==2 && value==32) {
            d.bcr|=value<<16;d.program_stage=3;return;
        }
        if(size==4 && p==0x1F8010C8 && executing_pc==0x92E30 && d.program_stage==3 && !d.pending && value==0x1000201) {
            check(d.madr,2048);d.initial_half.assign(ram.begin()+d.madr,ram.begin()+d.madr+2048);
            d.chcr=value;d.pending=true;d.program_stage=4;return;
        }
    }
    if(size==2 && p>=0x1F900000 && p<0x1F900800 && spu2_io_transfer_completed) {
        const auto offset=(p-0x1F900000)&0x3FF;
        // Voice routing masks (VMIXL/VMIXEL/VMIXR/VMIXER; 24 voice bits over two halves). Retained only.
        const bool routing=offset>=0x188 && offset<=0x196 && !(offset&1) && !(value&~((offset&2)?0xFFu:0xFFFFu));
        const bool core_parameter=(offset==0x19A && (value==0xC000 || value==0xC001));
        const bool effects_end=offset==0x33C && value==(p<0x1F900400?0xEu:0xFu);
        if((p==0x1F90076C || p==0x1F90076E) && value==0x7FFF && executing_pc==0x936E8
            && spu2_auto_dma.pending && spu2_auto_dma.readbacks==1 && ownedCriStartup() && cpu_interrupts_enabled) {
            spu2_configuration[p]=value;return;
        }
        const bool volume=((p>=0x1F900760 && p<=0x1F90076E) || (p>=0x1F900788 && p<=0x1F900796)) && !(p&1)
            && (value==0 || ((p==0x1F900760 || p==0x1F900762 || p==0x1F900788 || p==0x1F90078A) && value==0x3FFF) || ((p==0x1F900790 || p==0x1F900792) && value==0x7FFF));
        if(routing || core_parameter || effects_end || volume || (p==0x1F9007C0 && value==0xC032)
            || (offset==0x198 && value==(p<0x1F900400?0xFF0u:0xFFCu))) {spu2_configuration[p]=value;return;}
        // Voice volumes, pitch, ADSR1/2 and start/loop addresses take any value. No mixing is modeled, so
        // volumes are retained only; a playing voice follows its pitch, ADSR and loop address.
        const bool parameter=offset<0x180 && !(offset&1) && offset%16<=8;
        const bool start=offset>=0x1C0 && offset<0x2E0 && !(offset&1) && (offset-0x1C0)%12<=6;
        if(parameter || start || ((offset==0x340 || offset==0x342) && value==0)) {
            spu2_configuration[p]=value;if(offset==0x340 || offset==0x342)spu2_command_readbacks[p]={0,steps};
            if((parameter || start) && spu2_playback.enabled)spu2_playback.registerWritten(p>=0x1F900400,offset,spu2_configuration);
            return;
        }
        if(offset==0x1A0 || offset==0x1A2) {
            const unsigned core=p>=0x1F900400,shift=(offset&2)?16:0;
            // Key-on requires the keyed voices' volumes to be owned (pitch, ADSR and start address are checked
            // by the playback model). Position, ENDX and the envelope advance; no samples are mixed.
            for(unsigned voice=0;voice<24;++voice)if(((value<<shift)>>voice)&1)for(unsigned side=0;side<2;++side)
                if(!spu2_configuration.contains(0x1F900000+core*0x400+voice*16+side*2))
                    throw std::runtime_error("SPU2 voice activation without owned volumes");
            if(spu2_playback.enabled)spu2_playback.keyOn(core,(value<<shift)&0xFFFFFF,spu2_configuration);
            spu2_pending_key_on[core]|=(value<<shift)&0xFFFFFF;spu2_command_readbacks[p]={static_cast<std::uint16_t>(value),steps};return;
        }
    }
    if(size==2 && p==0x1F9001AC) {
        if(enabled_interrupts.contains(9) || enabled_interrupts.contains(36) || enabled_interrupts.contains(40) || spu2_reset_status_reads!=2 || spu2_fifo_halfwords>=8 || spu2_configuration.at(0x1F90019A)!=0x8000
            || spu2_configuration.at(0x1F9001B0)!=0 || spu2_configuration.at(0x1F90059A)!=0x8000)
            throw std::runtime_error("Unsupported SPU2 direct transfer state");
        const auto sound_address=(spu2_configuration.at(0x1F9001A8)<<16)|spu2_configuration.at(0x1F9001AA);
        if(sound_address!=0x2800+spu2_fifo_halfwords)throw std::runtime_error("SPU2 initialization destination differs");
        spu2_sound_ram[sound_address]=static_cast<std::uint16_t>(value);++spu2_fifo_halfwords;
        const auto next=(sound_address+1)&0xFFFFF;spu2_configuration[0x1F9001A8]=next>>16;spu2_configuration[0x1F9001AA]=next&0xFFFF;
        return;
    }
    if(size==2 && p==0x1F90019A && value==0x8010 && spu2_fifo_halfwords==8 && !spu2_io_status_reads) {
        spu2_configuration[p]=value;return;
    }
    if(size==2 && p==0x1F90019A && value==0x8000 && spu2_io_status_reads==1)spu2_io_transfer_completed=true;
    if(size==2 && (p==0x1F9001A8 || p==0x1F9001AA || p==0x1F9005A8 || p==0x1F9005AA || p==0x1F9002E0 || p==0x1F9002E2 || p==0x1F9006E0 || p==0x1F9006E2)) {
        spu2_configuration[p]=value&0xFFFF;return; // Transfer/effects addresses only; no transfer completion or effects processing.
    }
    if(size==2 && (p==0x1F9001A4 || p==0x1F9001A6 || p==0x1F9005A4 || p==0x1F9005A6) && spu2_reset_status_reads>=2) {
        const unsigned core=p>=0x1F900400,shift=(p&2)?16:0;
        if(spu2_playback.enabled)spu2_playback.keyOff(core,(value<<shift)&0xFFFFFF);
        spu2_pending_key_off[core]|=(value<<shift)&0xFFFFFF;
        if(spu2_io_transfer_completed) {spu2_pending_key_on[core]&=~((value<<shift)&0xFFFFFF);spu2_command_readbacks[p]={static_cast<std::uint16_t>(value),steps};}
        return; // Retain release command; no samples or completed envelopes claimed.
    }
    if(size==2 && value==0 && ((p>=0x1F900180 && p<=0x1F900196 && !(p&1))
        || (p>=0x1F900580 && p<=0x1F900596 && !(p&1)))) {spu2_configuration[p]=0;return;}
    if(size==2 && (((p==0x1F9001B0 || p==0x1F9005B0) && value==0)
        || ((p==0x1F90019A || p==0x1F90059A) && (value==0 || value==0x8000)))) {
        spu2_configuration[p]=static_cast<std::uint16_t>(value);return;
    }
    if(size==2 && value==0 && (p==0x1F900760 || p==0x1F900762 || p==0x1F900788 || p==0x1F90078A)) {spu2_configuration[p]=0;return;}
    if(size==2 && ((p==0x1F9007C0 && (value==0 || value==0x8000)) || (p==0x1F9007C6 && value==0x900)
        || (p==0x1F9007C8 && value==0x200) || (p==0x1F9007CA && value==8))) {
        spu2_configuration[p]=static_cast<std::uint16_t>(value);return;
    }
    if(size==4 && ((p==0x1F8010F0 && sound_bus.contains(p) && value==(sound_bus.at(p)|0x80000))
        || (p==0x1F801570 && sound_bus.contains(p) && value==(sound_bus.at(p)|8))
        || ((p==0x1F801014 || p==0x1F801414) && value==0x200B31E1))) {sound_bus[p]=value;return;}
    if(size==4 && ((p==0x1F801404 && value==0xBF900000) || (p==0x1F80140C && value==0xBF900800))) {sound_bus[p]=value;return;}
    if(p==0x1F808260 && size==1) {
        const auto command=sio2_parameters.find(0x1F808200);
        if(command==sio2_parameters.end())throw std::runtime_error("SIO2 transmit without owned command");
        const auto length=(command->second>>8)&0x1FF;
        if(!length || sio2_transmit.size()>=length)throw std::runtime_error("SIO2 transmit length/state unsupported");
        sio2_transmit.push_back(static_cast<std::uint8_t>(value));
        if(sio2_transmit.size()==length) {
            if(sio2_card_absent && command->second==0xC0342 && sio2_transmit==std::vector<std::uint8_t>({0x81,0x52,0})
                && cardWakes()==cardFifo()+1 && cardFifo()<5 && cardSends()==6 && cardReceives()==6
                && sio2_parameters.at(0x1F808250)==0xFFC00505 && sio2_parameters.at(0x1F808254)==0x201F4) {
                sio2_receive.assign(3,0xFF);sio2_command_status=0x1D100;sio2_receive_cursor=0;++sio2_card_fifo_packets;
            } else {
            // Pad packets: command word 0x40|port with equal send/receive lengths (5, 9 or 21 bytes)
            // on an admitted pad timing configuration. Port 1 answers from the DualShock 2 model (the
            // captured startup poll reply is its digital-mode answer); port 2 is absent.
            const auto port=command->second&1;const auto received=(command->second>>18)&0x1FF;
            // PORT_CTRL1/2 select the per-packet bus timing (the driver uses different timing for its
            // first poll, configuration and full polls); timing is not modelled, so the observed pairs
            // are admitted explicitly.
            constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,3> pad_timings{{{0xFFC00505,0x20014},{0xFFC00505,0x0103000A},{0xFF600A0A,0x01030014}}};
            const auto pad_bus=port<2 && sio2_parameters.contains(0x1F808240+port*8) && sio2_parameters.contains(0x1F808244+port*8)
                && std::find(pad_timings.begin(),pad_timings.end(),std::pair{sio2_parameters.at(0x1F808240+port*8),sio2_parameters.at(0x1F808244+port*8)})!=pad_timings.end();
            if((command->second&0x3F8000FEu)!=0x40 || received!=length || (length!=5 && length!=9 && length!=21) || !pad_bus || sio2_transmit[0]!=1)
                throw std::runtime_error("Unsupported SIO2 device packet/configuration: command="+std::to_string(command->second)+" first="+std::to_string(sio2_transmit[0])+" second="+std::to_string(sio2_transmit[1])+" length="+std::to_string(length)+" port_ctrl="+(sio2_parameters.contains(0x1F808240+port*8)?std::to_string(sio2_parameters.at(0x1F808240+port*8)):"-")+"/"+(sio2_parameters.contains(0x1F808244+port*8)?std::to_string(sio2_parameters.at(0x1F808244+port*8)):"-"));
            sio2_receive=port?std::vector<std::uint8_t>(length,0xFF):pad.transfer(sio2_transmit);
            sio2_command_status=port?0x2D100:0x1100;sio2_receive_cursor=0;sio2_pad_reply=true;
            }
        }
        return;
    }
    if(p>=0x1F808200 && p<0x1F808260 && p%4==0 && size==4) {
        if(p==0x1F808200 && !sio2_transmit.empty()) {
            if(sio2_receive_cursor!=sio2_receive.size())throw std::runtime_error("SIO2 command reset with unread reply");
            sio2_transmit.clear();sio2_receive.clear();sio2_receive_cursor=0;sio2_pad_reply=false;
        }
        sio2_parameters[p]=value;return; // Command/port configuration only; no transfer/device reply.
    }
    if(p==0x1F808280 && size==4 && sio2_irq_status_known) {sio2_irq_status&=~value;return;}
    if(p==0x1F808268 && size==4 && value==0x3BD) {
        const auto irq=interrupts.find(17);
        if((!sio2_pad_reply && sio2_receive.size()!=144 && !(sio2_receive.size()==3 && sio2_card_fifo_packets)) || !current_thread || !execute || irq==interrupts.end() || !irq->second.enabled || in_interrupt || !cpu_interrupts_enabled)
            throw std::runtime_error("Unsupported SIO2 transfer completion state: receive="+std::to_string(sio2_receive.size())+" pad="+std::to_string(sio2_pad_reply)
                +" fifo="+std::to_string(cardFifo())+" thread="+std::to_string(current_thread)+" irq="+std::to_string(irq!=interrupts.end() && irq->second.enabled)
                +" in_interrupt="+std::to_string(in_interrupt)+" enabled="+std::to_string(cpu_interrupts_enabled));
        if(sio2_receive.size()==144) {
            if(sio2_transmit.size()!=144 || sio2_receive_cursor!=144
                || (dma_channels.at(11).chcr!=0x1000201 && dma_channels.at(11).chcr!=0x201)
                || (dma_channels.at(12).chcr!=0x41000200 && dma_channels.at(12).chcr!=0x40000200))
                throw std::runtime_error("SIO2 card completion before both transfers");
            sio2_card_controls.push_back(value);
            dma_channels.at(11).chcr&=~0x1000000u;dma_channels.at(12).chcr&=~0x1000000u;
        }
        sio2_control=value;sio2_control_known=true;sio2_irq_status=1;sio2_irq_status_known=true;
        auto callback=interruptContext(threads.at(current_thread).context,irq->second.handler,irq->second.argument,irq->second.gp);
        runInterrupt(callback);
        if(callback.pc!=sentinel)throw std::runtime_error("Native SIO2 interrupt did not return");
        return;
    }
    if(p==0x1F808268 && size==4 && value==0x3BC) {
        sio2_control=value;sio2_control_known=true;++sio2_reset_writes;return;
    }
    if(p==0x1F808268)throw std::runtime_error("Unsupported SIO2 control write: "+std::to_string(value));
    check(p,size);
    if((size==2 && p%2) || (size==4 && p%4))throw std::runtime_error("Unaligned IOP store");
    std::memcpy(ram.data()+p,&value,size);
}

bool IopRuntime::writeSpu2Voice(std::uint32_t p,std::uint32_t value,unsigned size) {
    if(size==4 && (p==0x1F801014 || p==0x1F801414)) {
        // DMA timing nibble only (bits 24..27); the rest of the SSBUS configuration is retained.
        if(!sound_bus.contains(p) || (value&~0x0F000000u)!=(sound_bus.at(p)&~0x0F000000u))throw std::runtime_error("SPU2 bus configuration change unsupported");
        sound_bus[p]=value;return true;
    }
    if(size==2 && (p==0x1F90019A || p==0x1F90059A)) {
        if(!spu2_configuration.contains(p) || (value&~0x30u)!=(spu2_configuration.at(p)&~0x30u) || (value&0x30)==0x10)
            throw std::runtime_error("SPU2 attribute change outside transfer mode unsupported");
        spu2_configuration[p]=value;return true;
    }
    if(size==4 && spu2DmaCore(p,0)>=0) {spu2_voice.madr[spu2DmaCore(p,0)]=value;return true;}
    if(size==2 && spu2DmaCore(p,4)>=0) {auto& b=spu2_voice.bcr[spu2DmaCore(p,4)];b=(b&0xFFFF0000u)|value;return true;}
    if(size==2 && spu2DmaCore(p,6)>=0) {auto& b=spu2_voice.bcr[spu2DmaCore(p,6)];b=(b&0xFFFFu)|(value<<16);return true;}
    if(size==4 && spu2DmaCore(p,8)>=0) {
        const unsigned core=spu2DmaCore(p,8);const auto attr_address=0x1F90019Au+core*0x400;
        const auto words=spu2_voice.bcr[core]&0xFFFF,blocks=spu2_voice.bcr[core]>>16,madr=spu2_voice.madr[core]&0xFFFFFF;
        if(value!=0x01000201 || words!=16 || !blocks || (spu2_voice.chcr[core]&0x01000000)
            || !spu2_configuration.contains(attr_address) || (spu2_configuration.at(attr_address)&0x30)!=0x20)
            throw std::runtime_error("Unsupported SPU2 DMA transfer (only IOP to SPU2 writes)");
        const auto bytes=blocks*words*4;check(madr,bytes);
        const auto high=spu2_configuration.find(0x1F9001A8u+core*0x400),low=spu2_configuration.find(0x1F9001AAu+core*0x400);
        if(high==spu2_configuration.end() || low==spu2_configuration.end())throw std::runtime_error("SPU2 transfer address unset");
        const auto tsa=((high->second<<16)|low->second)&0xFFFFF;
        for(unsigned n=0;n<bytes/2;++n) {std::uint16_t half;std::memcpy(&half,ram.data()+madr+n*2,2);spu2_sound_ram[(tsa+n)&0xFFFFF]=half;}
        spu2_voice.chcr[core]=value&~0x01000000u;++spu2_voice.transfers;spu2_voice.bytes+=bytes;
        // Completion: run LIBSD's registered DMA interrupt handler now (logical time, no DMA duration).
        const auto irq=interrupts.find(core?40:36);
        if(irq==interrupts.end() || !irq->second.enabled || !enabled_interrupts.contains(core?40:36) || in_interrupt
            || !cpu_interrupts_enabled || !current_thread || !execute || !live_context)throw std::runtime_error("SPU2 DMA completion without an eligible interrupt");
        // The handler runs below the live stack pointer; the issuing code's frames stay intact.
        auto callback=interruptContext(*live_context,irq->second.handler,irq->second.argument,irq->second.gp);
        callback.set(29,(live_context->get(29)-256)&~15u);
        runInterrupt(callback);
        if(callback.pc!=sentinel || (spu2_configuration.at(attr_address)&0x30))throw std::runtime_error("SPU2 DMA interrupt did not complete the transfer");
        ++spu2_voice.irqs;return true;
    }
    return false;
}
