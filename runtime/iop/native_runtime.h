#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <deque>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "spu2_playback.h"

struct IopYield : std::exception {const char* what() const noexcept override {return "IOP thread waiting";}};

struct IopContext {
    std::array<std::uint32_t,32> r{};
    std::uint32_t pc=0,hi=0,lo=0,branch_target=0,load_value=0;
    unsigned load_register=0;
    bool branch_pending=false,delay_instruction=false;
    bool operator==(const IopContext&) const = default;
    std::uint32_t get(unsigned n) const {return n?r.at(n):0;}
    void set(unsigned n,std::uint32_t value) {if(n)r.at(n)=value;}
    std::uint32_t merge(unsigned n) const {return load_register==n && n?load_value:get(n);}
    void advance() {
        const auto next=branch_pending?branch_target:pc+4;
        delay_instruction=branch_pending;branch_pending=false;
        if(load_register)set(load_register,load_value);
        load_register=0;pc=next;r[0]=0;
    }
    void load(unsigned n,std::uint32_t value) {load_register=n;load_value=value;}
    void branch(std::uint32_t target) {
        if(delay_instruction)throw std::runtime_error("IOP branch in delay slot unsupported");
        branch_pending=true;branch_target=target;
    }
};

class IopRuntime {
public:
    static constexpr std::uint32_t ram_size=2*1024*1024,sentinel=0xFFFFFFFC;
    using Provider=std::function<std::uint32_t(IopRuntime&,IopContext&)>;
    std::vector<std::uint8_t> ram=std::vector<std::uint8_t>(ram_size);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> ranges;
    std::map<std::pair<std::string,unsigned>,Provider> providers;
    std::vector<std::string> kernel_messages,stdio_messages;
    std::array<std::uint32_t,4> last_import_arguments{};
    std::uint32_t last_import_ra=0,last_import_sp=0;
    std::uint32_t cdvd_interrupt_event=0;
    unsigned data_cache_flushes=0;
    unsigned spu2_dma_idle_reads=0;
    struct Spu2AutoDma {
        bool configured=false,pending=false;
        unsigned owner=0,program_stage=0,readbacks=0;
        std::uint32_t source=0,madr=0,bcr=0,chcr=0;
        std::vector<std::uint8_t> initial_half;
    } spu2_auto_dma;
    std::map<std::uint32_t,std::uint32_t> spu2_configuration;
    std::set<unsigned> enabled_interrupts;
    std::array<std::uint32_t,2> spu2_pending_key_off{},spu2_pending_key_on{};
    unsigned spu2_reset_status_reads=0,spu2_fifo_halfwords=0,spu2_io_status_reads=0;
    std::map<std::uint32_t,std::uint16_t> spu2_sound_ram;
    Spu2Playback spu2_playback;
    // Owned SPU2 voice transfers (IOP -> sound RAM through DMA4/DMA7), enabled only while a later
    // sound-driver command runs. Transfers complete in logical time; LIBSD's IRQ 36/40 handler runs
    // at once when CPU interrupts are enabled. No audio output, read-back transfers or physical DMA timing.
    unsigned sound_worker_starts=0;
    // Message boxes (thmsgbx) used by FILEIO's per-file workers: queued message pointers.
    std::map<unsigned,std::deque<std::uint32_t>> mailboxes;unsigned next_mailbox=1;
    // One IOP hardware timer allocated by the SOUNDS worker (captured: root counter 5, id 0x5BF80149).
    // Logical time: it advances one NTSC field per EE logical VBlank at the 36.864 MHz system clock.
    struct HardTimer {bool allocated=false,started=false;std::uint32_t id=0,compare=0,handler=0,common=0,gp=0;std::uint64_t tenths=0;unsigned ticks=0;} sound_timer;
    struct Spu2VoiceDma {bool enabled=false;std::array<std::uint32_t,2> madr{},bcr{},chcr{};unsigned transfers=0,irqs=0;std::uint64_t bytes=0;} spu2_voice;
    static int spu2DmaCore(std::uint32_t p,std::uint32_t offset) {return p==0x1F8010C0+offset?0:p==0x1F801500+offset?1:-1;} // DMA4 (core 0), DMA7 (core 1).
    struct Spu2Readback {std::uint16_t value;std::uint64_t instruction;};
    std::map<std::uint32_t,Spu2Readback> spu2_command_readbacks;
    bool spu2_io_transfer_completed=false;
    std::map<std::string,std::uint32_t> native_export_results;
    std::map<std::uint32_t,std::uint32_t> abi_read_only_words;
    struct DmaChannel {unsigned priority=0;bool priority_known=false,enabled=false,enabled_known=false;
        std::uint32_t madr=0,bcr=0,chcr=0;bool slice_known=false;};
    std::map<unsigned,DmaChannel> dma_channels;
    std::map<unsigned,std::uint32_t> security_handlers;
    std::uint32_t kelf_path_handler=0;
    struct Driver {std::uint32_t descriptor=0,operations=0,gp=0;std::string name;};
    std::array<Driver,16> drivers{};
    bool driver_callback_active=false;
    bool sif_transport_attached=false,rpc_initialized=false;
    struct RpcQueue {unsigned thread;std::vector<std::uint32_t> servers;};
    struct RpcServer {std::uint32_t descriptor,id,handler,buffer,queue,gp;};
    std::map<std::uint32_t,RpcQueue> rpc_queues;
    std::uint32_t rpc_queue_head=0,rpc_queue_tail=0;
    std::map<std::uint32_t,RpcServer> rpc_servers;
    bool cpu_interrupts_enabled=true; // Owned diagnostic kernel starts dispatch enabled.
    struct Interrupt {std::uint32_t mode,handler,argument; bool enabled=false;std::uint32_t gp=0;};
    std::map<unsigned,Interrupt> interrupts;
    struct Thread {std::uint32_t attr,option,entry,stack,stacksize,priority,gp; IopContext context; bool ready=false,waiting=false,wait_satisfied=false; unsigned wait_event=0;std::uint32_t wait_bits=0,wait_mode=0,wait_result=0,wait_rpc=0;std::uint64_t wait_order=0;bool wait_ee_bind=false;unsigned wait_semaphore=0;bool wait_sleep=false;unsigned wakeups=0;unsigned wait_mbx=0;std::uint32_t wait_mbx_out=0,mbx_message=0;bool exited=false;};
    std::map<unsigned,Thread> threads;
    std::uint64_t next_wait_order=0;
    std::uint64_t logical_time_us=0,timed_deadline_us=0;
    unsigned timed_wait_owner=0,timed_waits=0,timed_wakes=0,timed_dispatches=0;
    struct EeBind {unsigned thread=0;std::uint32_t client=0,server_id=0,packet=0,rpc_id=0;};
    EeBind pending_ee_bind;
    IopContext timed_context;
    struct Alarm {std::uint64_t deadline=0;std::uint32_t callback=0,argument=0,gp=0;};
    Alarm card_alarm;unsigned card_alarm_sets=0,card_alarm_fires=0,card_alarm_waits=0;
    unsigned next_thread=0x20001,current_thread=0;
    std::function<void(IopRuntime&,IopContext&)> execute;
    std::uint32_t stack_cursor=0x1E0000;
    struct Semaphore {std::uint32_t attr,option,count,maximum;};
    std::map<unsigned,Semaphore> semaphores;
    unsigned next_semaphore=0x30001;
    struct Event {std::uint32_t attr,option,bits;};
    std::map<unsigned,Event> events;
    unsigned next_event=0x10001,vblank_event=0;
    struct Library {std::uint32_t table; unsigned version; std::vector<std::uint32_t> slots; std::uint32_t gp=0;};
    std::map<std::string,Library> libraries;
    std::map<std::uint32_t,unsigned> export_bounds;
    std::uint32_t library_head=0;
    std::set<std::uint32_t> entries,entry_targets;
    std::vector<std::string> imports;
    std::uint32_t executing_pc=0,blocked_address=0;
    bool sio2_control_known=false,in_interrupt=false;
    std::uint32_t sio2_irq_status=0;
    bool sio2_irq_status_known=false;
    std::uint32_t sio2_control=0;
    unsigned sio2_reset_writes=0;
    std::map<std::uint32_t,std::uint32_t> sound_bus;
    std::map<std::uint32_t,std::uint32_t> sio2_parameters;
    std::vector<std::uint8_t> sio2_transmit,sio2_receive;
    unsigned sio2_receive_cursor=0;
    bool sio2_card_absent=true; // Explicit initial device policy; mounted cards are not implemented.
    unsigned sio2_card_sends=0,sio2_card_receives=0,sio2_card_fifo_packets=0;
    std::vector<std::uint32_t> sio2_card_controls;
    std::uint32_t sio2_command_status=0;
    // One captured command-14 readiness profile; disabled outside that owned request.
    bool disc_type_known=false;
    unsigned disc_type_reads=0,disc_ready_status_reads=0;
    unsigned steps=0,access_width=0;
    std::uint32_t access_value=0;
    std::string access_kind;
    void map(std::uint32_t base,std::span<const std::uint8_t> data) {
        if(!data.size() || base>=ram_size || data.size()>ram_size-base)throw std::runtime_error("IOP mapping outside owned RAM");
        for(auto [lo,hi]:ranges)if(base<hi && lo<base+data.size())throw std::runtime_error("Overlapping IOP ownership");
        std::copy(data.begin(),data.end(),ram.begin()+base);ranges.emplace_back(base,base+static_cast<std::uint32_t>(data.size()));
    }
    void check(std::uint32_t address,unsigned size) {
        // Cached/uncached RAM aliases are normalized only within physical IOP RAM.
        for(auto [lo,hi]:ranges)if(address>=lo && address<=hi && size<=hi-address)return;
        blocked_address=address;throw std::runtime_error("Unimplemented IOP memory/hardware access");
    }
    static std::uint32_t physical(std::uint32_t address) {return address&0x1FFFFFFF;}
    bool ownedCriStartup() const {
        const auto server=rpc_servers.find(0x90000200);
        const auto child=std::find_if(threads.begin(),threads.end(),[](const auto& row){return row.second.entry==0x1AC890;});
        return server!=rpc_servers.end() && rpc_queues.contains(server->second.queue)
            && current_thread==rpc_queues.at(server->second.queue).thread && child!=threads.end()
            && child->second.waiting && !child->second.ready && child->second.wait_semaphore;
    }
    std::uint32_t read(std::uint32_t address,unsigned size) {
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
    void write(std::uint32_t address,std::uint32_t value,unsigned size) {
        if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
        access_width=size;access_kind="write";access_value=value;
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
            const bool routing=offset>=0x188 && offset<=0x196 && !(offset&1) && value==((offset&2)?0xFFu:0xFFFFu);
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
            const bool parameter=offset<0x180 && !(offset&1) && ((offset%16==4 && value==0x3FFF)
                || ((offset%16==0 || offset%16==2 || offset%16==6 || offset%16==8) && value==0));
            const bool start=offset>=0x1C0 && offset<0x2E0 && (((offset-0x1C0)%12==0 && value==0)
                || ((offset-0x1C0)%12==2 && value==0x2800));
            if(parameter || start || ((offset==0x340 || offset==0x342) && value==0)) {
                spu2_configuration[p]=value;if(offset==0x340 || offset==0x342)spu2_command_readbacks[p]={0,steps};return;
            }
            if(offset==0x1A0 || offset==0x1A2) {
                const unsigned core=p>=0x1F900400,shift=(offset&2)?16:0;
                // Admit key-on only for initialized zero-volume voices. Position/ENDX advance; no samples or ADSR.
                for(unsigned voice=0;voice<24;++voice)for(unsigned side=0;side<2;++side)
                    if(!spu2_configuration.contains(0x1F900000+core*0x400+voice*16+side*2)
                        || spu2_configuration.at(0x1F900000+core*0x400+voice*16+side*2)!=0)
                        throw std::runtime_error("SPU2 voice activation without muted initialization unsupported");
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
                    && timed_wakes==sio2_card_fifo_packets+1 && sio2_card_fifo_packets<5 && sio2_card_sends==6 && sio2_card_receives==6
                    && sio2_parameters.at(0x1F808250)==0xFFC00505 && sio2_parameters.at(0x1F808254)==0x201F4) {
                    sio2_receive.assign(3,0xFF);sio2_command_status=0x1D100;sio2_receive_cursor=0;++sio2_card_fifo_packets;
                } else {
                const auto port=command->second&1;
                if((command->second&~1u)!=0x140540 || sio2_transmit!=std::vector<std::uint8_t>({1,0x42,0,0,0})
                    || sio2_parameters.at(0x1F808240+port*8)!=0xFFC00505 || sio2_parameters.at(0x1F808244+port*8)!=0x20014)
                    throw std::runtime_error("Unsupported SIO2 device packet/configuration: command="+std::to_string(command->second)+" first="+std::to_string(sio2_transmit[0])+" second="+std::to_string(sio2_transmit[1])+" length="+std::to_string(length));
                sio2_receive=port?std::vector<std::uint8_t>(5,0xFF):std::vector<std::uint8_t>({0xFF,0x41,0x5A,0xFF,0xFF});
                sio2_command_status=port?0x2D100:0x1100;sio2_receive_cursor=0;
                }
            }
            return;
        }
        if(p>=0x1F808200 && p<0x1F808260 && p%4==0 && size==4) {
            if(p==0x1F808200 && !sio2_transmit.empty()) {
                if(sio2_receive_cursor!=sio2_receive.size())throw std::runtime_error("SIO2 command reset with unread reply");
                sio2_transmit.clear();sio2_receive.clear();sio2_receive_cursor=0;
            }
            sio2_parameters[p]=value;return; // Command/port configuration only; no transfer/device reply.
        }
        if(p==0x1F808280 && size==4 && sio2_irq_status_known) {sio2_irq_status&=~value;return;}
        if(p==0x1F808268 && size==4 && value==0x3BD) {
            const auto irq=interrupts.find(17);
            if((sio2_receive.size()!=5 && sio2_receive.size()!=144 && !(sio2_receive.size()==3 && sio2_card_fifo_packets)) || !current_thread || !execute || irq==interrupts.end() || !irq->second.enabled || in_interrupt || !cpu_interrupts_enabled)
                throw std::runtime_error("Unsupported SIO2 transfer completion state");
            if(sio2_receive.size()==144) {
                if(sio2_transmit.size()!=144 || sio2_receive_cursor!=144
                    || (dma_channels.at(11).chcr!=0x1000201 && dma_channels.at(11).chcr!=0x201)
                    || (dma_channels.at(12).chcr!=0x41000200 && dma_channels.at(12).chcr!=0x40000200))
                    throw std::runtime_error("SIO2 card completion before both transfers");
                sio2_card_controls.push_back(value);
                dma_channels.at(11).chcr&=~0x1000000u;dma_channels.at(12).chcr&=~0x1000000u;
            }
            sio2_control=value;sio2_control_known=true;sio2_irq_status=1;sio2_irq_status_known=true;
            IopContext callback=threads.at(current_thread).context;callback.pc=irq->second.handler;
            callback.branch_pending=false;callback.delay_instruction=false;callback.load_register=0;
            callback.set(4,irq->second.argument);callback.set(28,irq->second.gp);callback.set(31,sentinel);
            in_interrupt=true;cpu_interrupts_enabled=false;
            try {execute(*this,callback);in_interrupt=false;cpu_interrupts_enabled=true;}
            catch(...) {in_interrupt=false;cpu_interrupts_enabled=true;throw;}
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
    bool writeSpu2Voice(std::uint32_t p,std::uint32_t value,unsigned size) {
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
            IopContext callback=*live_context;callback.pc=irq->second.handler;callback.set(29,(live_context->get(29)-256)&~15u);
            callback.branch_pending=false;callback.delay_instruction=false;callback.load_register=0;
            callback.set(4,irq->second.argument);callback.set(28,irq->second.gp);callback.set(31,sentinel);
            in_interrupt=true;cpu_interrupts_enabled=false;
            try {execute(*this,callback);in_interrupt=false;cpu_interrupts_enabled=true;}
            catch(...) {in_interrupt=false;cpu_interrupts_enabled=true;throw;}
            if(callback.pc!=sentinel || (spu2_configuration.at(attr_address)&0x30))throw std::runtime_error("SPU2 DMA interrupt did not complete the transfer");
            ++spu2_voice.irqs;return true;
        }
        return false;
    }
    void waitCardAlarm(IopContext& c,unsigned semaphore) {
        if(!card_alarm.callback || card_alarm.deadline<=logical_time_us || !cpu_interrupts_enabled || in_interrupt
            || !execute || !current_thread || !semaphores.contains(semaphore) || semaphores.at(semaphore).count)
            throw std::runtime_error("Card alarm wait outside owned pending timeout");
        for(const auto& [id,t]:threads)if(id!=current_thread && t.ready && !t.waiting)
            throw std::runtime_error("Card alarm wait has an undispatched ready worker");
        ++card_alarm_waits;logical_time_us=card_alarm.deadline;
        const auto saved=c;IopContext callback=c;callback.pc=card_alarm.callback;
        callback.branch_pending=false;callback.delay_instruction=false;callback.load_register=0;
        callback.set(4,card_alarm.argument);callback.set(28,card_alarm.gp);callback.set(31,sentinel);
        in_interrupt=true;cpu_interrupts_enabled=false;
        try {execute(*this,callback);in_interrupt=false;cpu_interrupts_enabled=true;}
        catch(...) {in_interrupt=false;cpu_interrupts_enabled=true;throw;}
        if(callback.pc!=sentinel || callback.get(2)!=0 || c!=saved
            || semaphores.at(semaphore).count!=1)throw std::runtime_error("Native card alarm callback/completion differs");
        card_alarm={};++card_alarm_fires;
    }
    std::uint32_t delayCurrent(IopContext& c,unsigned usec) {
        const auto owner=current_thread;
        if(!owner || !threads.contains(owner) || !usec || usec>1000000 || timed_wait_owner
            || !cpu_interrupts_enabled || in_interrupt || !execute || logical_time_us>UINT64_MAX-usec)
            throw std::runtime_error("IOP timed wait outside owned enabled context");
        // Keep the live CPU and every native export/RPC frame on the host stack.
        // Ready workers have separate retained CPUs; the blocked caller cannot run.
        timed_context=c;timed_wait_owner=owner;timed_deadline_us=logical_time_us+usec;++timed_waits;
        try {
            unsigned dispatch_budget=0;
            for(;;) {
                auto next=threads.end();
                for(auto t=threads.begin();t!=threads.end();++t)
                    if(t->first!=owner && t->second.ready && !t->second.waiting
                        && (next==threads.end() || t->second.priority<next->second.priority))next=t;
                if(next==threads.end())break;
                if(++dispatch_budget>32)throw std::runtime_error("IOP timed dispatch budget exceeded");
                current_thread=next->first;++timed_dispatches;
                try {execute(*this,next->second.context);throw std::runtime_error("Unexpected timed IOP worker completion");}
                catch(const IopYield&) {if(!next->second.waiting || next->second.ready)throw std::runtime_error("Timed IOP worker yielded without owned wait");}
                current_thread=owner;
            }
            // Logical idle advance only once all eligible ready workers have parked.
            logical_time_us=timed_deadline_us;
            if(c!=timed_context
                || !cpu_interrupts_enabled || in_interrupt)throw std::runtime_error("Suspended IOP timed CPU/context changed");
            timed_wait_owner=0;++timed_wakes;current_thread=owner;return 0u;
        } catch(...) {current_thread=owner;throw;} // Keep the pending snapshot/deadline for diagnosis.
    }
    // Lifetime runaway guard; repeated owned disc requests legitimately accumulate steps.
    IopContext* live_context=nullptr; // The context currently executing instructions.
    // One NTSC field of IOP system clock (36.864 MHz x 1001/60 ms = 615,014.4 clocks, in tenths).
    // Each elapsed compare interval runs the registered handler in interrupt context on an owned
    // stack (it returns the next interval; 0 stops), then woken workers run until they park.
    void advanceSoundTimerField() {
        auto& t=sound_timer;
        if(!t.started)return;
        std::uint64_t elapsed=6150144;
        while(t.compare && elapsed) {
            const auto period=std::uint64_t(t.compare)*10;
            if(t.tenths>=period)throw std::runtime_error("IOP timer elapsed state invalid");
            const auto until_tick=period-t.tenths;
            if(elapsed<until_tick)break;
            if(in_interrupt || !execute)throw std::runtime_error("IOP timer tick outside an idle point");
            spu2_playback.advanceTenths(until_tick,spu2_sound_ram);
            elapsed-=until_tick;t.tenths=0;
            IopContext handler;handler.pc=t.handler;handler.set(4,t.common);handler.set(28,t.gp);handler.set(29,0x1FFE00);handler.set(31,sentinel);
            const auto previous=current_thread;current_thread=0;
            in_interrupt=true;cpu_interrupts_enabled=false;
            try {execute(*this,handler);in_interrupt=false;cpu_interrupts_enabled=true;}
            catch(...) {in_interrupt=false;cpu_interrupts_enabled=true;current_thread=previous;throw;}
            if(handler.pc!=sentinel)throw std::runtime_error("IOP timer handler did not return");
            t.compare=handler.get(2);++t.ticks;
            for(;;) {
                auto next=threads.end();
                for(auto c=threads.begin();c!=threads.end();++c)
                    if(c->second.ready && !c->second.waiting && c->second.wait_satisfied && (next==threads.end() || c->second.priority<next->second.priority))next=c;
                if(next==threads.end())break;
                current_thread=next->first;
                try {execute(*this,next->second.context);next->second.ready=false;}
                catch(const IopYield&) {if(!next->second.waiting){current_thread=previous;throw std::runtime_error("IOP worker yielded without an owned wait");}}
                catch(...) {current_thread=previous;throw;}
            }
            current_thread=previous;
        }
        spu2_playback.advanceTenths(elapsed,spu2_sound_ram);
        t.tenths+=elapsed;
    }
    void tick(IopContext& c) {live_context=&c;executing_pc=c.pc;if(++steps>50000000)throw std::runtime_error("IOP diagnostic budget exceeded");}
    std::uint32_t import(const char* library,unsigned ordinal,IopContext& c) {
        const auto key=std::make_pair(std::string(library),ordinal);
        imports.push_back(key.first+":"+std::to_string(ordinal));
        for(unsigned n=0;n<4;++n)last_import_arguments[n]=c.get(4+n);
        last_import_ra=c.get(31);last_import_sp=c.get(29);
        const auto found=providers.find(key);
        if(found==providers.end()) {
            const auto library_entry=libraries.find(library);
            if(library_entry==libraries.end() || ordinal>=library_entry->second.slots.size() || !library_entry->second.slots[ordinal] || !execute)
                throw std::runtime_error("Unresolved IOP import: "+imports.back());
            if(c.load_register || c.branch_pending)throw std::runtime_error("Pending CPU state at native IOP export call unsupported");
            const auto target=library_entry->second.slots[ordinal];check(physical(target),4);
            IopContext callee=c;callee.pc=target;callee.delay_instruction=false;callee.set(28,library_entry->second.gp);callee.set(31,sentinel);
            entry_targets.insert(target);
            try {execute(*this,callee);}
            catch(const IopYield&) {throw std::runtime_error("Native IOP export suspension requires retained call frames");}
            if(callee.pc!=sentinel)throw std::runtime_error("Native IOP export did not return");
            const auto ra=c.get(31),gp=c.get(28);c.r=callee.r;c.set(31,ra);c.set(28,gp);c.hi=callee.hi;c.lo=callee.lo;
            c.load_register=callee.load_register;c.load_value=callee.load_value;
            native_export_results[key.first+":"+std::to_string(ordinal)]=callee.get(2);
            return callee.get(2);
        }
        return found->second(*this,c);
    }
};
inline std::int32_t iop_signed(std::uint32_t value) {return std::bit_cast<std::int32_t>(value);}

inline std::uint32_t iop_add(std::uint32_t a,std::uint32_t b) {
    const auto v=static_cast<std::int64_t>(iop_signed(a))+iop_signed(b);
    if(v<INT32_MIN || v>INT32_MAX)throw std::runtime_error("IOP signed overflow exception unsupported");
    return static_cast<std::uint32_t>(v);
}
inline std::uint32_t iop_sub(std::uint32_t a,std::uint32_t b) {
    const auto v=static_cast<std::int64_t>(iop_signed(a))-iop_signed(b);
    if(v<INT32_MIN || v>INT32_MAX)throw std::runtime_error("IOP signed overflow exception unsupported");
    return static_cast<std::uint32_t>(v);
}
inline std::pair<std::uint32_t,std::uint32_t> iop_div(std::uint32_t a,std::uint32_t b,bool sign) {
    if(!b)return {sign && iop_signed(a)<0?1u:UINT32_MAX,a};
    if(sign) {
        if(a==0x80000000 && b==UINT32_MAX)return {a,0};
        return {static_cast<std::uint32_t>(iop_signed(a)/iop_signed(b)),static_cast<std::uint32_t>(iop_signed(a)%iop_signed(b))};
    }
    return {a/b,a%b};
}
