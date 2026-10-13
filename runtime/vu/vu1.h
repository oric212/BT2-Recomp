// VU1: 16 KB of data memory, 16 KB of microprogram memory and the microprogram interpreter
// (docs/plans/graphics/VU1_PIPELINE_PLAN.md). VIF1 fills both memories (UNPACK, MPG) and starts
// programs (MSCAL, MSCALF, MSCNT); a program runs synchronously to its E bit and sends GS packets
// with XGKICK (PATH1). Only the instructions the game's microprograms use are modelled; any other
// stops by name.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

class Vu1 {
public:
    // Receives each XGKICK packet (whole GIF packets up to EOP, copied out of data memory).
    using Kick=std::function<void(std::span<const std::uint8_t>)>;
    static constexpr std::uint32_t data_mask=0x3FF,program_mask=0x7FF; // Quadwords; instructions.

    std::array<std::uint8_t,16384> data{},program{};
    std::array<std::array<std::uint32_t,4>,32> vf{}; // Bit patterns (x, y, z, w); VF0 is (0, 0, 0, 1).
    std::array<std::uint16_t,16> vi{};               // VI0 is 0.
    std::array<std::uint32_t,4> acc{};
    std::uint32_t q=0,i=0,clip=0; // Q and I as bit patterns; the 24 clipping flags.
    std::uint32_t pc=0;           // Instruction index where MSCNT continues.
    std::uint32_t top=0,itop=0;   // Set by VIF1 at each start (XTOP, XITOP).
    std::uint64_t instructions=0;
    unsigned runs=0,kicks=0;
    std::uint64_t kick_hash=0xCBF29CE484222325ull; // Over every XGKICK packet's bytes, in order: equal hashes mean equal output.
    // Reads of Q or the clipping flags while a newer result was still in the pipeline. The model
    // counts one cycle per instruction (no stalls), so a nonzero count marks timing-sensitive code.
    unsigned early_q_reads=0,early_clip_reads=0;

    Vu1() {vf[0][3]=0x3F800000;}
    void run(std::uint32_t start,const Kick& kick); // MSCAL/MSCALF: from instruction `start`.
    void resume(const Kick& kick) {run(pc,kick);}   // MSCNT.
    // The GIF packets at data address `address` (quadwords, wrapping), up to and including EOP.
    std::vector<std::uint8_t> gifPacket(std::uint32_t address) const;

private:
    struct Result {enum Target {None,Register,Accumulator} target=None;unsigned reg=0,mask=0;std::array<std::uint32_t,4> value{};};
    Result upper(std::uint32_t word,std::uint32_t at);
    void lower(std::uint32_t word,std::uint32_t at,const Kick& kick);
    void setVi(unsigned reg,std::uint16_t value,bool flag_instruction=false);
    std::uint16_t branchVi(unsigned reg,std::uint32_t at) const;
    std::uint8_t* cell(std::uint32_t address) {return data.data()+(address&data_mask)*16;}
    std::uint32_t readQ();
    std::uint32_t readClip();
    void settle(bool all); // Publishes pipeline results that are due (all of them at the end of a run).

    std::uint64_t cycle_=0;
    bool q_pending_=false;std::uint64_t q_ready_=0;std::uint32_t q_next_=0; // DIV: 7 cycles.
    struct ClipEntry {std::uint64_t ready;std::uint32_t value;};
    std::array<ClipEntry,4> clip_queue_{};unsigned clip_count_=0;std::uint32_t clip_latest_=0; // CLIP: 4 cycles.
    unsigned vi_written_=0;std::uint64_t vi_written_cycle_=0; // Last VI write other than by a flag instruction.
    bool branch_=false;std::uint32_t branch_target_=0;
    unsigned host_csr_=0; // The float mode outside the run, restored around XGKICK.
    // An instruction pair decoded once: what the run loop needs without taking the words apart again.
    // An entry is current while its words equal program memory, so no writer has to invalidate it.
    // `Slow` kinds go through `upper` and `lower`, which also refuse what is not modelled.
    struct Decoded {
        enum Upper : std::uint8_t {UpperSlow,UpperNop,UpperRegister,UpperAccumulator};
        enum Lower : std::uint8_t {LowerSlow,LowerNop,LowerLq,LowerSq,LowerLqi,LowerSqi};
        std::uint32_t low=0,high=0;
        Upper upper=UpperSlow;Lower lower=LowerSlow;
        std::uint8_t operation=0,source=0,fs=0,ft=0,reg=0,field=0,mask=0; // Upper arithmetic.
        std::uint8_t lower_vf=0,lower_vi=0,lower_mask=0;std::int16_t lower_imm=0; // Lower loads and stores.
        bool valid=false,i_bit=false,ending=false,debug_bits=false,skip_lower=false;
    };
    std::array<Decoded,program_mask+1> decoded_{};
    void decode(Decoded& d,std::uint32_t low,std::uint32_t high) const;
    std::vector<std::uint8_t> packet_; // The XGKICK packet being sent; kept to reuse its storage.
    void gifPacketInto(std::vector<std::uint8_t>& packet,std::uint32_t address) const;
};
