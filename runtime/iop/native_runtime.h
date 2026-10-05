#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

struct IopYield : std::exception {const char* what() const noexcept override {return "IOP thread waiting";}};

struct IopContext {
    std::array<std::uint32_t,32> r{};
    std::uint32_t pc=0,hi=0,lo=0,branch_target=0,load_value=0;
    unsigned load_register=0;
    bool branch_pending=false,delay_instruction=false;
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
    std::uint32_t cdvd_interrupt_event=0;
    unsigned data_cache_flushes=0;
    std::map<std::uint32_t,std::uint32_t> spu2_configuration;
    std::set<unsigned> enabled_interrupts;
    std::array<std::uint32_t,2> spu2_pending_key_off{},spu2_pending_key_on{};
    unsigned spu2_reset_status_reads=0,spu2_fifo_halfwords=0,spu2_io_status_reads=0;
    std::map<std::uint32_t,std::uint16_t> spu2_sound_ram;
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
    struct Thread {std::uint32_t attr,option,entry,stack,stacksize,priority,gp; IopContext context; bool ready=false,waiting=false,wait_satisfied=false; unsigned wait_event=0;std::uint32_t wait_bits=0,wait_mode=0,wait_result=0,wait_rpc=0;std::uint64_t wait_order=0;};
    std::map<unsigned,Thread> threads;
    std::uint64_t next_wait_order=0;
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
    std::uint32_t sio2_command_status=0;
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
    std::uint32_t read(std::uint32_t address,unsigned size) {
        if(size!=1 && size!=2 && size!=4)throw std::runtime_error("Invalid IOP access width");
        access_width=size;access_kind="read";access_value=0;
        const auto p=physical(address);
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
        if(size==2 && spu2_command_readbacks.contains(p)) {
            const auto record=spu2_command_readbacks.at(p);
            if(steps>record.instruction+1)throw std::runtime_error("Late SPU2 dynamic register readback unsupported");
            spu2_command_readbacks.erase(p);return record.value; // One observed immediate readback; no clocked voice-state read.
        }
        if(size==2 && p>=0x1F900000 && p<0x1F900800 && spu2_configuration.contains(p)) {
            const auto offset=(p-0x1F900000)&0x3FF;
            if((p>=0x1F900760 && p<=0x1F900796) || (offset>=0x1C0 && offset<0x2E0 && (offset-0x1C0)%12<=2) || offset==0x19A || (offset<0x180 && (offset%16==0 || offset%16==2 || offset%16==4 || offset%16==6 || offset%16==8)))
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
        if(size==2 && p>=0x1F900000 && p<0x1F900800 && spu2_io_transfer_completed) {
            const auto offset=(p-0x1F900000)&0x3FF;
            const bool routing=offset>=0x188 && offset<=0x196 && !(offset&1) && value==((offset&2)?0xFFu:0xFFFFu);
            const bool core=(offset==0x19A && (value==0xC000 || value==0xC001));
            const bool effects_end=offset==0x33C && value==(p<0x1F900400?0xEu:0xFu);
            const bool volume=((p>=0x1F900760 && p<=0x1F90076E) || (p>=0x1F900788 && p<=0x1F900796)) && !(p&1)
                && (value==0 || ((p==0x1F900760 || p==0x1F900762 || p==0x1F900788 || p==0x1F90078A) && value==0x3FFF) || ((p==0x1F900790 || p==0x1F900792) && value==0x7FFF));
            if(routing || core || effects_end || volume || (p==0x1F9007C0 && value==0xC032)
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
                // Retain a command only for the initialized zero-volume voices. No samples/ADSR advancement.
                for(unsigned voice=0;voice<24;++voice)for(unsigned side=0;side<2;++side)
                    if(!spu2_configuration.contains(0x1F900000+core*0x400+voice*16+side*2)
                        || spu2_configuration.at(0x1F900000+core*0x400+voice*16+side*2)!=0)
                        throw std::runtime_error("SPU2 voice activation without muted initialization unsupported");
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
                const auto port=command->second&1;
                if((command->second&~1u)!=0x140540 || sio2_transmit!=std::vector<std::uint8_t>({1,0x42,0,0,0})
                    || sio2_parameters.at(0x1F808240+port*8)!=0xFFC00505 || sio2_parameters.at(0x1F808244+port*8)!=0x20014)
                    throw std::runtime_error("Unsupported SIO2 device packet/configuration");
                sio2_receive=port?std::vector<std::uint8_t>(5,0xFF):std::vector<std::uint8_t>({0xFF,0x41,0x5A,0xFF,0xFF});
                sio2_command_status=port?0x2D100:0x1100;sio2_receive_cursor=0;
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
            if(sio2_receive.size()!=5 || !current_thread || !execute || irq==interrupts.end() || !irq->second.enabled || in_interrupt || !cpu_interrupts_enabled)
                throw std::runtime_error("Unsupported SIO2 transfer completion state");
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
        check(p,size);
        if((size==2 && p%2) || (size==4 && p%4))throw std::runtime_error("Unaligned IOP store");
        std::memcpy(ram.data()+p,&value,size);
    }
    void tick(IopContext& c) {executing_pc=c.pc;if(++steps>100000)throw std::runtime_error("IOP diagnostic budget exceeded");}
    std::uint32_t import(const char* library,unsigned ordinal,IopContext& c) {
        const auto key=std::make_pair(std::string(library),ordinal);
        imports.push_back(key.first+":"+std::to_string(ordinal));
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
