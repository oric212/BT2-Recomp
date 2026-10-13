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
    void arithmetic(Result& result,unsigned operation,unsigned source,std::uint32_t word);
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
    int host_rounding_=0; // Rounding mode outside the run, restored around XGKICK.
};
