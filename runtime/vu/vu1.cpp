// VU1 microprogram interpreter, written from the public VU instruction set description.
// Each instruction pair is one cycle: the upper result is computed from the state before the pair
// and committed after the lower instruction, so both read the old registers and the upper write
// wins. Floats follow the EE FPU model of this runtime (ps2_fpu.cpp): round toward zero, no
// denormals, infinities or NaNs. MAC and status flags are not computed (no modelled instruction
// reads them); the instructions that read them stop by name.
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <immintrin.h>
#include <span>
#include <stdexcept>
#include <vector>
// The float settings come before this project's headers: the compiler does not inline a function
// into code compiled under other float settings, and the small helpers of vu1.h and vu1_simd.h are
// used in every instruction.
#pragma fenv_access(on)
#pragma fp_contract(off)
#include "vu1.h"
#include "vu1_program.h"
#include "vu1_simd.h"

using namespace vu1_simd;

namespace {
constexpr std::uint64_t run_limit=Vu1::run_limit; // Instructions per start; a program that never ends stops here.

[[noreturn]] void refuse(const char* what,std::uint32_t word,std::uint32_t at) {
    char text[128];std::snprintf(text,sizeof text,"VU1 %s 0x%08X is not modelled (instruction %u)",what,word,at);
    throw std::runtime_error(text);
}
float value(std::uint32_t bits) {return std::bit_cast<float>(normal(bits));}
std::uint32_t bits(float v) {return normal(std::bit_cast<std::uint32_t>(v));}
std::uint32_t word(const std::uint8_t* at) {std::uint32_t w;std::memcpy(&w,at,4);return w;}
int imm11(std::uint32_t w) {return int(w&0x7FF)-int((w&0x400)<<1);}
unsigned viField(unsigned reg,std::uint32_t w,std::uint32_t at) {if(reg>15)refuse("integer register in",w,at);return reg;}
// The float mode of a run, set in MXCSR: round toward zero, denormal operands read as zero and tiny
// results flushed to zero. That is the model's treatment of denormals done by the processor; the
// infinities and NaNs are clamped in code (`clamp`).
constexpr unsigned vu_csr_mode=0x6000u|0x8000u|0x0040u;
struct ChopRounding {
    unsigned saved=_mm_getcsr();
    ChopRounding() {_mm_setcsr((saved&~0x6000u)|vu_csr_mode);}
    ~ChopRounding() {_mm_setcsr(saved);}
};
enum : unsigned {Add,Sub,Madd,Msub,Mul};       // Arithmetic families.
enum : unsigned {Vector,Field,RegisterQ,RegisterI}; // Second operand: ft, one field of ft, Q, I.
// Codes 0x1C-0x2D, shared by the register forms and the accumulator forms (ADDq/ADDAq ...).
bool sharedArithmetic(unsigned code,unsigned& operation,unsigned& source) {
    switch(code) {
        case 0x1C:operation=Mul;source=RegisterQ;return true;
        case 0x1E:operation=Mul;source=RegisterI;return true;
        case 0x2A:operation=Mul;source=Vector;return true;
        case 0x20:case 0x21:case 0x24:case 0x25:source=RegisterQ;break;
        case 0x22:case 0x23:case 0x26:case 0x27:source=RegisterI;break;
        case 0x28:case 0x29:case 0x2C:case 0x2D:source=Vector;break;
        default:return false;
    }
    operation=(code&4?Sub:Add)+(code&1?2:0); // Sub/Msub at 4-7 and C-D; the multiply-add forms are odd.
    return true;
}
}

void Vu1::settle(bool all) {
    if(q_pending_ && (all || q_ready_<=cycle_)) {q=q_next_;q_pending_=false;}
    unsigned kept=0;
    for(unsigned n=0;n<clip_count_;++n) {
        if(all || clip_queue_[n].ready<=cycle_)clip=clip_queue_[n].value; else clip_queue_[kept++]=clip_queue_[n];
    }
    clip_count_=kept;
}

std::uint32_t Vu1::readQ() {if(q_pending_)++early_q_reads;return q;}
std::uint32_t Vu1::readClip() {if(clip_count_)++early_clip_reads;return clip;}

void Vu1::setVi(unsigned reg,std::uint16_t v,bool flag_instruction) {
    if(!reg)return;
    vi[reg]=v;
    if(!flag_instruction) {vi_written_=reg;vi_written_cycle_=cycle_;}
}

// A branch reads its integer registers earlier in the pipeline than the previous instruction writes
// them. The flag instructions (FCAND, FCOR ...) are followed directly by their branch throughout the
// game's programs and are read as written; the delayed value after any other writer is not modelled.
std::uint16_t Vu1::branchVi(unsigned reg,std::uint32_t at) const {
    if(reg && vi_written_==reg && vi_written_cycle_+1==cycle_) {
        char text[128];std::snprintf(text,sizeof text,"VU1 branch on VI%u, written by the previous instruction, is not modelled (instruction %u)",reg,at);
        throw std::runtime_error(text);
    }
    return vi[reg];
}

Vu1::Result Vu1::upper(std::uint32_t w,std::uint32_t at) {
    const unsigned ft=(w>>16)&31,fs=(w>>11)&31,op=w&0x3F;
    // The arithmetic forms (to a register below 0x3C, to the accumulator above) are decoded and run
    // by the run loop; what reaches this function with such a code is not an instruction of the model.
    Result result;result.mask=(w>>21)&15;
    if(op<0x3C)refuse("upper instruction",w,at);
    const unsigned code=(((w>>6)&0x1F)<<2)|(w&3);
    if(code==0x2F)return result; // NOP.
    if(code>=0x10 && code<0x18) {
        // ITOF0/4/12/15 and FTOI0/4/12/15: ft from fs, with 0, 4, 12 or 15 fraction bits.
        static constexpr unsigned fraction[4]={0,4,12,15};
        const double scale=double(1u<<fraction[code&3]);
        result.target=Result::Register;result.reg=ft;
        for(unsigned lane=0;lane<4;++lane) {
            if(!(result.mask&(8u>>lane)))continue;
            if(code<0x14)result.value[lane]=bits(float(double(std::int32_t(vf[fs][lane]))/scale));
            else {
                const double scaled=double(value(vf[fs][lane]))*scale; // Truncates and saturates.
                result.value[lane]=scaled>=2147483648.0?0x7FFFFFFFu:scaled<=-2147483648.0?0x80000000u:std::uint32_t(std::int32_t(scaled));
            }
        }
        return result;
    }
    if(code==0x1D) { // ABS.
        result.target=Result::Register;result.reg=ft;
        for(unsigned lane=0;lane<4;++lane)result.value[lane]=vf[fs][lane]&0x7FFFFFFFu;
        return result;
    }
    if(code==0x1F) {
        // CLIP: x, y and z of fs against +-|w| of ft; six new flags shift in below the earlier ones.
        const float limit=std::fabs(value(vf[ft][3]));
        std::uint32_t flags=clip_latest_<<6;
        for(unsigned lane=0;lane<3;++lane) {
            const float v=value(vf[fs][lane]);
            if(v>limit)flags|=1u<<(lane*2);
            if(v<-limit)flags|=2u<<(lane*2);
        }
        clip_latest_=flags&0xFFFFFFu;
        if(clip_count_==clip_queue_.size())refuse("clip flag pipeline overflow at",w,at);
        clip_queue_[clip_count_++]={cycle_+4,clip_latest_};
        return result;
    }
    refuse("upper instruction",w,at);
}

void Vu1::lower(std::uint32_t w,std::uint32_t at,const Kick& kick) {
    const unsigned op=w>>25,it=(w>>16)&31,is=(w>>11)&31,dest=(w>>21)&15;
    auto load=[&](unsigned reg,std::uint32_t address) {
        if(!reg)return;
        const auto* from=cell(address);
        for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[reg][lane]=word(from+lane*4);
    };
    auto store=[&](std::uint32_t address,const std::array<std::uint32_t,4>& v) {
        auto* to=cell(address);
        for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))std::memcpy(to+lane*4,&v[lane],4);
    };
    auto branchTo=[&](std::uint32_t target) {branch_=true;branch_target_=target&program_mask;};
    auto integer=[&](unsigned reg) {return viField(reg,w,at);};
    switch(op) {
        case 0x00:load(it,vi[integer(is)]+imm11(w));return;                 // LQ ft, imm(is)
        case 0x01:store(vi[integer(it)]+imm11(w),vf[is]);return;            // SQ fs, imm(it)
        case 0x04: {                                                        // ILW it, imm(is): the dest field
            if(dest!=8 && dest!=4 && dest!=2 && dest!=1)refuse("ILW field mask in",w,at);
            const unsigned lane=dest==8?0:dest==4?1:dest==2?2:3;
            setVi(integer(it),std::uint16_t(word(cell(vi[integer(is)]+imm11(w))+lane*4)));return;
        }
        case 0x05: {const std::uint32_t v=vi[integer(it)];store(vi[integer(is)]+imm11(w),{v,v,v,v});return;} // ISW
        case 0x08:case 0x09: {                                              // IADDIU, ISUBIU it, is, imm15
            const std::uint32_t imm=((w>>10)&0x7800)|(w&0x7FF),base=vi[integer(is)];
            setVi(integer(it),std::uint16_t(op==8?base+imm:base-imm));return;
        }
        case 0x12:setVi(1,(readClip()&w&0xFFFFFFu)?1:0,true);return;        // FCAND
        case 0x13:setVi(1,((readClip()|w)&0xFFFFFFu)==0xFFFFFFu?1:0,true);return; // FCOR
        case 0x1C:setVi(integer(it),std::uint16_t(readClip()&0xFFF),true);return; // FCGET
        case 0x20:branchTo(at+1+imm11(w));return;                           // B
        case 0x21:setVi(integer(it),std::uint16_t(at+2));branchTo(at+1+imm11(w));return; // BAL
        case 0x24:branchTo(branchVi(integer(is),at));return;                // JR
        case 0x28:case 0x29: {                                              // IBEQ, IBNE
            const bool equal=branchVi(integer(it),at)==branchVi(integer(is),at);
            if(equal==(op==0x28))branchTo(at+1+imm11(w));
            return;
        }
        case 0x2C:case 0x2D:case 0x2E:case 0x2F: {                          // IBLTZ, IBGTZ, IBLEZ, IBGEZ
            const auto v=std::int16_t(branchVi(integer(is),at));
            if(op==0x2C?v<0:op==0x2D?v>0:op==0x2E?v<=0:v>=0)branchTo(at+1+imm11(w));
            return;
        }
        case 0x40:break;
        default:refuse("lower instruction",w,at);
    }
    const unsigned function=w&0x3F,id=(w>>6)&31;
    if(function<0x3C) {
        const std::uint32_t a=vi[integer(is)],b=vi[integer(it)];
        switch(function) {
            case 0x30:setVi(integer(id),std::uint16_t(a+b));return;         // IADD
            case 0x31:setVi(integer(id),std::uint16_t(a-b));return;         // ISUB
            case 0x32:setVi(integer(it),std::uint16_t(a+id-((id&16)<<1)));return; // IADDI it, is, imm5
            case 0x34:setVi(integer(id),std::uint16_t(a&b));return;         // IAND
            case 0x35:setVi(integer(id),std::uint16_t(a|b));return;         // IOR
            default:refuse("lower instruction",w,at);
        }
    }
    switch((id<<2)|(w&3)) {
        case 0x30:if(it)for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[it][lane]=vf[is][lane];return; // MOVE
        case 0x31: {                                                        // MR32: x, y, z, w from y, z, w, x
            const auto from=vf[is];
            if(it)for(unsigned lane=0;lane<4;++lane)if(dest&(8u>>lane))vf[it][lane]=from[(lane+1)&3];
            return;
        }
        case 0x34:load(it,vi[integer(is)]);setVi(is,std::uint16_t(vi[is]+1));return;               // LQI ft, (is++)
        case 0x35:store(vi[integer(it)],vf[is]);setVi(it,std::uint16_t(vi[it]+1));return;          // SQI fs, (it++)
        case 0x36:setVi(integer(is),std::uint16_t(vi[is]-1));load(it,vi[is]);return;               // LQD ft, (--is)
        case 0x37:setVi(integer(it),std::uint16_t(vi[it]-1));store(vi[it],vf[is]);return;          // SQD fs, (--it)
        case 0x38: {
            // DIV: Q = fs.fsf / ft.ftf after 7 cycles; a division by zero gives the largest magnitude.
            // A division still in progress finishes first.
            if(q_pending_) {q=q_next_;cycle_=std::max(cycle_,q_ready_);}
            const std::uint32_t n=normal(vf[is][(w>>21)&3]),d=normal(vf[it][(w>>23)&3]);
            q_next_=(d&0x7FFFFFFFu)?bits(std::bit_cast<float>(n)/std::bit_cast<float>(d)):((n^d)&0x80000000u)|0x7F7FFFFFu;
            q_pending_=true;q_ready_=cycle_+7;return;
        }
        case 0x3B:if(q_pending_) {q=q_next_;q_pending_=false;cycle_=std::max(cycle_,q_ready_);}return; // WAITQ
        case 0x3E: {                                                        // ILWR it, (is): the dest field
            if(dest!=8 && dest!=4 && dest!=2 && dest!=1)refuse("ILWR field mask in",w,at);
            const unsigned lane=dest==8?0:dest==4?1:dest==2?2:3;
            setVi(integer(it),std::uint16_t(word(cell(vi[integer(is)])+lane*4)));return;
        }
        case 0x3F: {const std::uint32_t v=vi[integer(it)];store(vi[integer(is)],{v,v,v,v});return;} // ISWR
        case 0x68:setVi(integer(it),std::uint16_t(top));return;             // XTOP
        case 0x69:setVi(integer(it),std::uint16_t(itop));return;            // XITOP
        case 0x6C: {                                                        // XGKICK is
            // The packet in place where it lies in one piece, otherwise (wrapping around the end of
            // data memory) as a copy.
            const std::uint32_t first=vi[integer(is)]&data_mask,qwords=gifPacketQwords(first);
            std::span<const std::uint8_t> packet(data.data()+first*16,std::size_t(qwords)*16);
            if(first+qwords>data_mask+1) {gifPacketInto(packet_,first);packet=packet_;}
            ++kicks;
            for(std::size_t n=0;n+8<=packet.size();n+=8) {std::uint64_t part;std::memcpy(&part,packet.data()+n,8);kick_hash=(kick_hash^part)*0x100000001B3ull;}
            if(kick) {const unsigned mode=_mm_getcsr();_mm_setcsr(host_csr_);kick(packet);_mm_setcsr(mode);} // The GS runs with the host's float mode.
            return;
        }
        default:refuse("lower instruction",w,at);
    }
}

void Vu1::decode(Decoded& d,std::uint32_t low,std::uint32_t high) const {
    d=Decoded{};d.low=low;d.high=high;d.valid=true;
    d.i_bit=(high&0x80000000u)!=0;d.ending=((high>>30)&1)!=0;d.debug_bits=(high&0x18000000u)!=0;
    const unsigned ft=(high>>16)&31,fs=(high>>11)&31,fd=(high>>6)&31,op=high&0x3F;
    d.fs=std::uint8_t(fs);d.ft=std::uint8_t(ft);d.field=std::uint8_t(high&3);d.mask=std::uint8_t((high>>21)&15);
    unsigned operation=0,source=0,target=0; // `target`: the float register the upper instruction writes.
    if(op<0x3C) {
        // ADDbc, SUBbc, MADDbc, MSUBbc (0x00-0x0F), MULbc (0x18-0x1B) and the q, i and vector forms, to fd.
        bool known=true;
        if(op<0x10) {operation=op>>2;source=Field;}
        else if(op>=0x18 && op<0x1C) {operation=Mul;source=Field;}
        else known=sharedArithmetic(op,operation,source);
        if(known) {d.upper=Decoded::UpperRegister;d.reg=std::uint8_t(fd);target=fd;}
    } else {
        const unsigned code=(((high>>6)&0x1F)<<2)|(high&3);
        if(code==0x2F)d.upper=Decoded::UpperNop;
        else if((code>=0x10 && code<0x18) || code==0x1D)target=ft; // ITOF, FTOI, ABS: through `upper`.
        else if(code!=0x1F) {                                        // Not CLIP: the accumulator forms.
            bool known=true;
            if(code<0x10) {operation=code>>2;source=Field;}
            else if(code>=0x18 && code<0x1C) {operation=Mul;source=Field;}
            else known=sharedArithmetic(code,operation,source);
            if(known)d.upper=Decoded::UpperAccumulator;
        }
    }
    d.operation=std::uint8_t(operation);d.source=std::uint8_t(source);
    if(d.i_bit)return; // The lower word is a float for I.
    const unsigned lop=low>>25,it=(low>>16)&31,is=(low>>11)&31;
    const unsigned lower_code=lop==0x40 && (low&0x3F)>=0x3C?(((low>>6)&31)<<2)|(low&3):0x100u;
    // A lower instruction writing the float register the upper one writes is discarded.
    const bool writes_float=lop==0 || lower_code==0x30 || lower_code==0x31 || lower_code==0x34 || lower_code==0x36;
    d.skip_lower=target && writes_float && it==target;
    d.lower_mask=std::uint8_t((low>>21)&15);d.lower_imm=std::int16_t(imm11(low));
    if(lop==0x00 && is<16) {d.lower=Decoded::LowerLq;d.lower_vf=std::uint8_t(it);d.lower_vi=std::uint8_t(is);}
    else if(lop==0x01 && it<16) {d.lower=Decoded::LowerSq;d.lower_vf=std::uint8_t(is);d.lower_vi=std::uint8_t(it);}
    else if(lower_code==0x34 && is<16) {d.lower=Decoded::LowerLqi;d.lower_vf=std::uint8_t(it);d.lower_vi=std::uint8_t(is);}
    else if(lower_code==0x35 && it<16) {d.lower=Decoded::LowerSqi;d.lower_vf=std::uint8_t(is);d.lower_vi=std::uint8_t(it);}
    else if(lower_code==0x30 && !it)d.lower=Decoded::LowerNop; // MOVE to VF0.
}

void Vu1::run(std::uint32_t start,const Kick& kick) {
    const ChopRounding rounding;host_csr_=rounding.saved;
    pc=start&program_mask;++runs;branch_=false;
    if(entry_generation_[pc]!=program_generation) {
        // The first start from here since program memory changed: which program is it?
        entry_generation_[pc]=program_generation;
        native_[pc]=nullptr;native_by_hand_[pc]=false;
        const auto natives=nativePrograms(),by_hand=use_hand_written?handWrittenPrograms():std::span<const NativeEntry>{};
        if(use_native && (!natives.empty() || !by_hand.empty() || !extra_programs.empty())) {
            // Tests' own programs, then the hand-written ones, then the translations.
            const auto key=Vu1Program::reachable(program,pc).key;
            for(const auto& n:extra_programs)if(n.key==key && !native_[pc])native_[pc]=n.run;
            for(const auto& n:by_hand)if(n.key==key && !native_[pc]) {native_[pc]=n.run;native_by_hand_[pc]=true;}
            for(const auto& n:natives)if(n.key==key && !native_[pc])native_[pc]=n.run;
        }
        if(on_entry)on_entry(pc);
    }
    // A native translation runs the program to its end, or returns early with `pc` at a pair it
    // does not contain; the interpreter goes on from there.
    if(native_[pc]) {
        ++native_runs;hand_written_runs+=native_by_hand_[pc];
        if(verify_native) {runVerified(native_[pc],kick);return;}
        if(native_[pc](*this,kick)) {settle(true);clip_latest_=clip;vi_written_=0;return;}
    }
    interpret(kick);
}

// What a program leaves behind that a later program, VIF1 or the report can see.
namespace {
struct Visible {
    std::array<std::uint8_t,16384> data;std::array<std::array<std::uint32_t,4>,32> vf;std::array<std::uint16_t,16> vi;
    std::array<std::uint32_t,4> acc;std::uint32_t q,i,clip,pc;std::uint64_t instructions;unsigned kicks;
    explicit Visible(const Vu1& vu):data(vu.data),vf(vu.vf),vi(vu.vi),acc(vu.acc),q(vu.q),i(vu.i),clip(vu.clip),pc(vu.pc),instructions(vu.instructions),kicks(vu.kicks) {}
    void restore(Vu1& vu) const {vu.data=data;vu.vf=vf;vu.vi=vi;vu.acc=acc;vu.q=q;vu.i=i;vu.clip=clip;vu.pc=pc;vu.instructions=instructions;vu.kicks=kicks;}
    const char* difference(const Visible& o) const {
        return data!=o.data?"data memory":vf!=o.vf?"a float register":vi!=o.vi?"an integer register":acc!=o.acc?"ACC":q!=o.q?"Q":i!=o.i?"I":clip!=o.clip?"the clipping flags"
            :pc!=o.pc?"the stopping address":instructions!=o.instructions?"the pair count":kicks!=o.kicks?"the packet count":nullptr;
    }
};
}

void Vu1::runVerified(NativeProgram native,const Kick& kick) {
    const std::uint32_t start=pc;
    const auto fail=[&](const char* what) {
        char text[160];std::snprintf(text,sizeof text,"VU1 native program %016llx (start %u) differs from the interpreter in %s",
            static_cast<unsigned long long>(Vu1Program::reachable(program,start).key),start,what);
        throw std::runtime_error(text);
    };
    // The interpreter's run: its packets are kept, not sent, and the output hash is left alone.
    const Visible before(*this);const auto hash=kick_hash;const auto cycle=cycle_;
    std::vector<std::vector<std::uint8_t>> expected;
    const Kick keep=[&](std::span<const std::uint8_t> packet) {expected.emplace_back(packet.begin(),packet.end());};
    interpret(keep);
    const Visible reference(*this);
    // The native run from the same state: each packet must be the interpreter's, then goes out.
    before.restore(*this);kick_hash=hash;cycle_=cycle;branch_=false;clip_latest_=clip; // As every run begins: the pipelines empty.
    std::size_t sent=0;
    const Kick check=[&](std::span<const std::uint8_t> packet) {
        if(sent>=expected.size() || packet.size()!=expected[sent].size() || std::memcmp(packet.data(),expected[sent].data(),packet.size()))fail("an output packet");
        ++sent;if(kick)kick(packet);
    };
    if(native(*this,check)) {settle(true);clip_latest_=clip;vi_written_=0;} else interpret(check);
    if(sent!=expected.size())fail("the number of output packets");
    if(const char* what=Visible(*this).difference(reference))fail(what);
    ++verified_runs;
}

void Vu1::interpret(const Kick& kick) {
    bool ending=false;
    for(std::uint64_t executed=0;;++executed) {
        if(executed>=run_limit)throw std::runtime_error("VU1 microprogram does not end");
        const std::uint32_t at=pc,low=word(program.data()+at*8),high=word(program.data()+at*8+4);
        auto& d=decoded_[at];
        if(!d.valid || d.low!=low || d.high!=high)decode(d,low,high);
        if(d.debug_bits)refuse("D/T bit in",high,at);
        pc=(pc+1)&program_mask;
        const bool last=ending,branching=branch_;const std::uint32_t target=branch_target_;
        ending=d.ending;branch_=false;
        if(q_pending_ || clip_count_)settle(false);
        if(d.i_bit)i=low; // I bit: the lower word is a float for I; the upper instruction sees it.
        // The upper result is computed from the state before the pair and committed after the lower instruction.
        __m128i value=_mm_setzero_si128();unsigned commit=Result::None,commit_reg=0,commit_mask=d.mask;
        switch(d.upper) {
            case Decoded::UpperNop:break;
            case Decoded::UpperRegister:case Decoded::UpperAccumulator: {
                const __m128 a=clamp(lanes(vf[d.fs]));
                const __m128 b=clamp(d.source==Vector?lanes(vf[d.ft]):_mm_castsi128_ps(_mm_set1_epi32(int(d.source==Field?vf[d.ft][d.field]:d.source==RegisterQ?readQ():i))));
                __m128 r;
                switch(d.operation) {
                    case Add:r=_mm_add_ps(a,b);break;
                    case Sub:r=_mm_sub_ps(a,b);break;
                    case Mul:r=_mm_mul_ps(a,b);break;
                    default: {
                        const __m128 product=clamp(_mm_mul_ps(a,b)),sum=clamp(lanes(acc));
                        r=d.operation==Madd?_mm_add_ps(sum,product):_mm_sub_ps(sum,product);
                    }
                }
                value=_mm_castps_si128(clamp(r));
                commit=d.upper==Decoded::UpperRegister?Result::Register:Result::Accumulator;commit_reg=d.reg;
                break;
            }
            default: {
                const Result result=upper(high,at);
                value=load4(result.value.data());commit=result.target;commit_reg=result.reg;commit_mask=result.mask;
            }
        }
        if(!d.i_bit && !d.skip_lower) {
            switch(d.lower) {
                case Decoded::LowerNop:break;
                case Decoded::LowerLq:
                    if(d.lower_vf)store4(vf[d.lower_vf].data(),select(load4(vf[d.lower_vf].data()),load4(cell(vi[d.lower_vi]+d.lower_imm)),laneMask(d.lower_mask)));
                    break;
                case Decoded::LowerSq: {
                    auto* to=cell(vi[d.lower_vi]+d.lower_imm);
                    store4(to,select(load4(to),load4(vf[d.lower_vf].data()),laneMask(d.lower_mask)));
                    break;
                }
                case Decoded::LowerLqi:
                    if(d.lower_vf)store4(vf[d.lower_vf].data(),select(load4(vf[d.lower_vf].data()),load4(cell(vi[d.lower_vi])),laneMask(d.lower_mask)));
                    setVi(d.lower_vi,std::uint16_t(vi[d.lower_vi]+1));
                    break;
                case Decoded::LowerSqi: {
                    auto* to=cell(vi[d.lower_vi]);
                    store4(to,select(load4(to),load4(vf[d.lower_vf].data()),laneMask(d.lower_mask)));
                    setVi(d.lower_vi,std::uint16_t(vi[d.lower_vi]+1));
                    break;
                }
                default:lower(low,at,kick);
            }
        }
        if(commit==Result::Register && commit_reg)store4(vf[commit_reg].data(),select(load4(vf[commit_reg].data()),value,laneMask(commit_mask)));
        else if(commit==Result::Accumulator)store4(acc.data(),select(load4(acc.data()),value,laneMask(commit_mask)));
        ++cycle_;++instructions;
        if(branching) {
            if(branch_)refuse("branch in a branch delay slot,",low,at);
            pc=target;
        }
        if(last)break; // The instruction after the E bit has run.
    }
    settle(true);clip_latest_=clip;vi_written_=0;
}

std::vector<std::uint8_t> Vu1::gifPacket(std::uint32_t address) const {
    std::vector<std::uint8_t> packet;gifPacketInto(packet,address);return packet;
}

std::uint32_t Vu1::gifPacketQwords(std::uint32_t address) const {
    std::uint32_t total=0;
    for(;;) {
        std::uint64_t tag;std::memcpy(&tag,data.data()+((address+total)&data_mask)*16,8);
        const std::uint32_t nloop=std::uint32_t(tag&0x7FFF),flg=std::uint32_t((tag>>58)&3),regs=std::uint32_t((tag>>60)&15);
        const std::uint32_t nreg=regs?regs:16;
        total+=1+(flg==0?nloop*nreg:flg==1?(nloop*nreg+1)/2:nloop); // The tag; PACKED, REGLIST, IMAGE.
        if(total>data_mask+1)throw std::runtime_error("VU1 XGKICK packet exceeds VU1 memory");
        if(tag&0x8000)return total; // EOP.
    }
}

void Vu1::gifPacketInto(std::vector<std::uint8_t>& packet,std::uint32_t address) const {
    const std::uint32_t first=address&data_mask,qwords=gifPacketQwords(first),straight=std::min(qwords,data_mask+1-first);
    packet.resize(std::size_t(qwords)*16);
    std::memcpy(packet.data(),data.data()+first*16,std::size_t(straight)*16);
    std::memcpy(packet.data()+std::size_t(straight)*16,data.data(),std::size_t(qwords-straight)*16);
}
