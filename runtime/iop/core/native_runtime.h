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
#include "iop/devices/spu2_playback.h"
#include "iop/devices/dualshock2.h"

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
    struct Event {std::uint32_t attr,option,bits,init_bits=0;};
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
    // Pad bus: a neutral DualShock 2 on port 1 (SIO2 port 0); port 2 stays absent.
    DualShock2 pad;bool sio2_pad_reply=false;
    unsigned sio2_card_sends=0,sio2_card_receives=0,sio2_card_fifo_packets=0;
    // Each absent-card query repeats MCMAN's PS2 then PS1 probe sequence (the card type stays "no card"),
    // so the card path is validated against counters relative to the start of the current query.
    struct CardQueryBase {unsigned sends=0,receives=0,fifo=0,waits=0,wakes=0,alarm_sets=0,alarm_fires=0,alarm_waits=0;} card_base;
    void beginCardQuery() {card_base={sio2_card_sends,sio2_card_receives,sio2_card_fifo_packets,timed_waits,timed_wakes,card_alarm_sets,card_alarm_fires,card_alarm_waits};}
    unsigned cardSends() const {return sio2_card_sends-card_base.sends;}
    unsigned cardReceives() const {return sio2_card_receives-card_base.receives;}
    unsigned cardFifo() const {return sio2_card_fifo_packets-card_base.fifo;}
    unsigned cardWaits() const {return timed_waits-card_base.waits;}
    unsigned cardWakes() const {return timed_wakes-card_base.wakes;}
    unsigned cardAlarmSets() const {return card_alarm_sets-card_base.alarm_sets;}
    unsigned cardAlarmFires() const {return card_alarm_fires-card_base.alarm_fires;}
    unsigned cardAlarmWaits() const {return card_alarm_waits-card_base.alarm_waits;}
    std::vector<std::uint32_t> sio2_card_controls;
    std::uint32_t sio2_command_status=0;
    // One captured command-14 readiness profile; disabled outside that owned request.
    bool disc_type_known=false;
    unsigned disc_type_reads=0,disc_ready_status_reads=0;
    unsigned steps=0,access_width=0;
    std::uint32_t access_value=0;
    std::string access_kind;
    unsigned card_alarm_idle_fires=0; // Pending card alarms fired at a field advance (advanceLogicalField).
    unsigned vblank_edges=0;
    std::uint32_t watch_address=0; // Diagnostic only (BT2_IOP_WATCH); zero disables.
    IopContext* live_context=nullptr; // The context currently executing instructions.

    // Owned memory and hardware registers (native_runtime.cpp, native_runtime_io.cpp).
    void map(std::uint32_t base,std::span<const std::uint8_t> data);
    void check(std::uint32_t address,unsigned size);
    static std::uint32_t physical(std::uint32_t address) {return address&0x1FFFFFFF;}
    bool ownedCriStartup() const;
    std::uint32_t read(std::uint32_t address,unsigned size);
    void write(std::uint32_t address,std::uint32_t value,unsigned size);
    bool writeSpu2Voice(std::uint32_t p,std::uint32_t value,unsigned size);

    // Logical time (native_runtime_timing.cpp).
    void waitCardAlarm(IopContext& c,unsigned semaphore);
    std::uint32_t delayCurrent(IopContext& c,unsigned usec);
    // Logical IOP time also advances by one NTSC field (16,683 us) at each VBlank, outside service calls.
    // A card timeout alarm still pending when its query returned then fires once its deadline passes:
    // the callback runs in interrupt context on the owned interrupt stack and signals the card
    // semaphore, so the next query's wait on it completes at once, as on hardware.
    void advanceLogicalField();
    // One NTSC field of IOP system clock (36.864 MHz x 1001/60 ms = 615,014.4 clocks, in tenths).
    // Each elapsed compare interval runs the registered handler in interrupt context on an owned
    // stack (it returns the next interval; 0 stops), then woken workers run until they park.
    void advanceSoundTimerField();
    // IOP VBLANK (IRQ 0) and EVBLANK (IRQ 11) edges as the public vblank module signals its event
    // flag: start sets START|VBLANK (1|2) then clears START/NON_VBLANK (1|8); end sets END|NON_VBLANK
    // (4|8) then clears VBLANK/END (2|4). Waiters released by the set run at once in priority order
    // (interrupt return). The flag exists only after a thread first waits on it.
    void deliverVblankEdge(bool start);

    // Execution (native_runtime.cpp). Lifetime runaway guard in tick; repeated owned disc requests
    // legitimately accumulate steps.
    void tick(IopContext& c) {live_context=&c;executing_pc=c.pc;if(++steps>50000000)throw std::runtime_error("IOP diagnostic budget exceeded");}
    std::uint32_t import(const char* library,unsigned ordinal,IopContext& c);
    // Runs a handler in interrupt context (CPU interrupts disabled, as on IOP exception entry) until it
    // returns or throws; the interrupt state is restored either way.
    void runInterrupt(IopContext& handler);
    // A handler context derived from `base`: no pending branch or load, $a0=argument, $gp and $ra=sentinel.
    static IopContext interruptContext(IopContext base,std::uint32_t pc,std::uint32_t argument,std::uint32_t gp);
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
