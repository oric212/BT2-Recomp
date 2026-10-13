// VU1 microprogram interpreter, written from the public VU instruction set description.
// Each instruction pair is one cycle: the upper result is computed from the state before the pair
// and committed after the lower instruction, so both read the old registers and the upper write
// wins. Floats follow the EE FPU model of this runtime (ps2_fpu.cpp): round toward zero, no
// denormals, infinities or NaNs. MAC and status flags are not computed (no modelled instruction
// reads them); the instructions that read them stop by name.
#include "vu1.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
#pragma fenv_access(on)
#pragma fp_contract(off)

namespace {
constexpr std::uint64_t run_limit=16'000'000; // Instructions per start; a program that never ends stops here.

[[noreturn]] void refuse(const char* what,std::uint32_t word,std::uint32_t at) {
    char text[128];std::snprintf(text,sizeof text,"VU1 %s 0x%08X is not modelled (instruction %u)",what,word,at);
    throw std::runtime_error(text);
}
// Denormals read as signed zero; infinities and NaNs as the largest magnitude of their sign.
std::uint32_t normal(std::uint32_t bits) {
    const auto exponent=bits&0x7F800000u;
    return !exponent?bits&0x80000000u:exponent==0x7F800000u?(bits&0x80000000u)|0x7F7FFFFFu:bits;
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
// Four lanes at once: infinities and NaNs become the largest magnitude of their sign.
inline __m128 clamp(__m128 v) {
    const __m128i x=_mm_castps_si128(v),exponent=_mm_set1_epi32(0x7F800000);
    const __m128i special=_mm_cmpeq_epi32(_mm_and_si128(x,exponent),exponent);
    const __m128i largest=_mm_or_si128(_mm_and_si128(x,_mm_set1_epi32(int(0x80000000u))),_mm_set1_epi32(0x7F7FFFFF));
    return _mm_castsi128_ps(_mm_or_si128(_mm_andnot_si128(special,x),_mm_and_si128(special,largest)));
}
inline __m128 lanes(const std::array<std::uint32_t,4>& v) {return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data())));}
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

void Vu1::arithmetic(Result& result,unsigned operation,unsigned source,std::uint32_t w) {
    const unsigned ft=(w>>16)&31,fs=(w>>11)&31;
    // All four lanes are computed; the caller commits the masked ones. Denormals are handled by the
    // run's float mode, so the model's per-lane normalization reduces to the clamp.
    const __m128 a=clamp(lanes(vf[fs]));
    const __m128 b=clamp(source==Vector?lanes(vf[ft]):_mm_castsi128_ps(_mm_set1_epi32(int(source==Field?vf[ft][w&3]:source==RegisterQ?readQ():i))));
    __m128 r;
    switch(operation) {
        case Add:r=_mm_add_ps(a,b);break;
        case Sub:r=_mm_sub_ps(a,b);break;
        case Mul:r=_mm_mul_ps(a,b);break;
        default: {
            const __m128 product=clamp(_mm_mul_ps(a,b)),sum=clamp(lanes(acc));
            r=operation==Madd?_mm_add_ps(sum,product):_mm_sub_ps(sum,product);
        }
    }
    _mm_storeu_si128(reinterpret_cast<__m128i*>(result.value.data()),_mm_castps_si128(clamp(r)));
}

Vu1::Result Vu1::upper(std::uint32_t w,std::uint32_t at) {
    const unsigned ft=(w>>16)&31,fs=(w>>11)&31,fd=(w>>6)&31,op=w&0x3F;
    Result result;result.mask=(w>>21)&15;
    unsigned operation=0,source=0;
    if(op<0x3C) {
        // ADDbc, SUBbc, MADDbc, MSUBbc (0x00-0x0F), MULbc (0x18-0x1B) and the q, i and vector forms.
        if(op<0x10) {operation=op>>2;source=Field;}
        else if(op>=0x18 && op<0x1C) {operation=Mul;source=Field;}
        else if(!sharedArithmetic(op,operation,source))refuse("upper instruction",w,at);
        result.target=Result::Register;result.reg=fd;
        arithmetic(result,operation,source,w);
        return result;
    }
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
    // ADDAbc ... MSUBAbc (0x00-0x0F), MULAbc (0x18-0x1B) and the q, i and vector forms, into ACC.
    if(code<0x10) {operation=code>>2;source=Field;}
    else if(code>=0x18 && code<0x1C) {operation=Mul;source=Field;}
    else if(!sharedArithmetic(code,operation,source))refuse("upper instruction",w,at);
    result.target=Result::Accumulator;
    arithmetic(result,operation,source,w);
    return result;
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
            gifPacketInto(packet_,vi[integer(is)]);const auto& packet=packet_;
            ++kicks;
            for(std::size_t n=0;n+8<=packet.size();n+=8) {std::uint64_t part;std::memcpy(&part,packet.data()+n,8);kick_hash=(kick_hash^part)*0x100000001B3ull;}
            if(kick) {const unsigned mode=_mm_getcsr();_mm_setcsr(host_csr_);kick(packet);_mm_setcsr(mode);} // The GS runs with the host's float mode.
            return;
        }
        default:refuse("lower instruction",w,at);
    }
}

void Vu1::run(std::uint32_t start,const Kick& kick) {
    const ChopRounding rounding;host_csr_=rounding.saved;
    pc=start&program_mask;++runs;branch_=false;
    bool ending=false;
    for(std::uint64_t executed=0;;++executed) {
        if(executed>=run_limit)throw std::runtime_error("VU1 microprogram does not end");
        const std::uint32_t at=pc,low=word(program.data()+at*8),high=word(program.data()+at*8+4);
        if(high&0x18000000u)refuse("D/T bit in",high,at);
        pc=(pc+1)&program_mask;
        const bool last=ending,branching=branch_;const std::uint32_t target=branch_target_;
        ending=(high>>30)&1;branch_=false;
        settle(false);
        if(high&0x80000000u)i=low; // I bit: the lower word is a float for I; the upper instruction sees it.
        const Result result=upper(high,at);
        if(!(high&0x80000000u)) {
            // A lower instruction writing the float register the upper one writes is discarded.
            const unsigned lower_code=(low>>25)==0x40 && (low&0x3F)>=0x3C?(((low>>6)&31)<<2)|(low&3):0x100u;
            const bool writes_float=(low>>25)==0 || lower_code==0x30 || lower_code==0x31 || lower_code==0x34 || lower_code==0x36;
            const bool same=result.target==Result::Register && result.reg && writes_float && ((low>>16)&31)==result.reg;
            if(!same)lower(low,at,kick);
        }
        if(result.target==Result::Register && result.reg) {
            for(unsigned lane=0;lane<4;++lane)if(result.mask&(8u>>lane))vf[result.reg][lane]=result.value[lane];
        } else if(result.target==Result::Accumulator) {
            for(unsigned lane=0;lane<4;++lane)if(result.mask&(8u>>lane))acc[lane]=result.value[lane];
        }
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

void Vu1::gifPacketInto(std::vector<std::uint8_t>& packet,std::uint32_t address) const {
    packet.clear();
    auto take=[&](std::uint32_t qwords) {
        if(packet.size()/16+qwords>data_mask+1)throw std::runtime_error("VU1 XGKICK packet exceeds VU1 memory");
        for(;qwords;--qwords,++address) {const auto* from=data.data()+(address&data_mask)*16;packet.insert(packet.end(),from,from+16);}
    };
    for(;;) {
        std::uint64_t tag;std::memcpy(&tag,data.data()+(address&data_mask)*16,8);
        take(1);
        const std::uint32_t nloop=std::uint32_t(tag&0x7FFF),flg=std::uint32_t((tag>>58)&3),regs=std::uint32_t((tag>>60)&15);
        const std::uint32_t nreg=regs?regs:16;
        take(flg==0?nloop*nreg:flg==1?(nloop*nreg+1)/2:nloop); // PACKED, REGLIST, IMAGE.
        if(tag&0x8000)return; // EOP.
    }
}
