// Native pass "dof-weight": the depth of field's weight pass with a lighter strength (docs/plans/graphics/
// IMAGE_QUALITY_PLAN.md step 5b), the user's setting "Depth of field". BT2's fights blur the distance by blending a 2:1
// copy back over the frame by the frame's alpha ((Cs - Cd) x Ad + Cd); that alpha is written just before, by full-screen
// sprites that read the depth buffer's top byte as T8H through a palette the game uploads each frame and write only the
// frame's alpha. The palette's alphas are the game's blur per depth. Here the same weights are written scaled by
// BT2_DOF_STRENGTH (percent; the pass runs only below 100), so the game's own blend blurs less (shaders/dof_weight.comp).
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/dof_weight_spv.h"
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace {
using namespace ParallelGS;
struct Push {std::uint32_t area[4],buffers[4],texture[4],memory[4];};
int floorDiv16(int v) {return v>>4;}
int ceilDiv16(int v) {return (v+15)>>4;}
}

// The strength in 256ths of the game's weights: BT2_DOF_STRENGTH=<0..99> percent; 256 (the game's) without it.
std::uint32_t dofStrength() {
    const char* setting=std::getenv("BT2_DOF_STRENGTH");
    if(!setting || !*setting)return 256;
    const long percent=std::strtol(setting,nullptr,10);
    return std::uint32_t(std::clamp(percent,0L,100L)*256/100);
}

// Sprites with a T8H texture through a PSMCT32 palette in CSM1 order, DECAL with the texture's alpha, writing only the
// alpha of a PSMCT32 frame, without depth test or writes: the weight pass, when its texture is the depth buffer (run).
bool dofWeightDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    if(PRIMType(prim.PRIM)!=PRIMType::Sprite || !prim.TME || prim.FGE || prim.AA1)return false;
    const auto& tex0=ctx.tex0.desc;
    return ctx.frame.desc.PSM==PSMCT32 && std::uint32_t(ctx.frame.desc.FBMSK)==0x00FFFFFFu && tex0.PSM==PSMT8H && tex0.CPSM==PSMCT32
        && tex0.CSM==0 && tex0.CSA==0 && tex0.TFX==1 && tex0.TCC && ctx.zbuf.desc.ZMSK && (!ctx.test.desc.ZTE || ctx.test.desc.ZTST==1)
        && !ctx.test.desc.ATE && !ctx.test.desc.DATE && ctx.tex1.desc.MXL==0 && !ctx.fba.desc.FBA;
}

bool dofWeight(const NativeRenderPass& pass,NativeContext& context) {
    const std::uint32_t strength=dofStrength();
    if(strength>=256)return false;
    const auto& rp=pass.rp;
    if(pass.sampling_rate_x_log2!=pass.sampling_rate_y_log2 || pass.sampling_rate_x_log2>2
       || (pass.sampling_rate_x_log2 && !pass.super_sample_reference))return false;
    if(rp.num_instances!=1 || rp.num_textures!=1 || rp.feedback_mode!=RenderPass::None || !rp.num_primitives)return false;
    const auto& frame=rp.instances[0].fb.frame.desc;
    const auto& zbuf=rp.instances[0].fb.z.desc;
    const auto& tex0=pass.tex0[0];
    // The depth buffer's top byte, read where the game's depth buffer is; its palette read from GS memory.
    if(tex0.PSM!=PSMT8H || tex0.CPSM!=PSMCT32 || tex0.CSM!=0 || tex0.CSA!=0 || tex0.TBP0!=zbuf.ZBP*32u || frame.PSM!=PSMCT32)return false;

    // The sprites read their texels 1:1 at one offset and tile a rectangle.
    int x0=INT_MAX,y0=INT_MAX,x1=INT_MIN,y1=INT_MIN,offset_x=0,offset_y=0;
    std::uint64_t covered=0;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        if(!(prim.state&(1u<<STATE_BIT_SPRITE)) || prim.fbmsk!=0x00FFFFFFu)return false;
        const auto& p0=pass.positions[3*n].pos;const auto& p1=pass.positions[3*n+1].pos;
        const auto& a0=pass.attributes[3*n];const auto& a1=pass.attributes[3*n+1];
        if(int(a1.uv.x)-int(a0.uv.x)!=p1.x-p0.x || int(a1.uv.y)-int(a0.uv.y)!=p1.y-p0.y)return false;
        const int ox=floorDiv16(int(a0.uv.x)-p0.x),oy=floorDiv16(int(a0.uv.y)-p0.y);
        if(n && (ox!=offset_x || oy!=offset_y))return false;
        offset_x=ox;offset_y=oy;
        const int rx0=std::max(ceilDiv16(std::min(p0.x,p1.x)),int(prim.bb.x)),ry0=std::max(ceilDiv16(std::min(p0.y,p1.y)),int(prim.bb.y));
        const int rx1=std::min(ceilDiv16(std::max(p0.x,p1.x))-1,int(prim.bb.z)),ry1=std::min(ceilDiv16(std::max(p0.y,p1.y))-1,int(prim.bb.w));
        if(rx1<rx0 || ry1<ry0)return false;
        covered+=std::uint64_t(rx1-rx0+1)*std::uint64_t(ry1-ry0+1);
        x0=std::min(x0,rx0);y0=std::min(y0,ry0);x1=std::max(x1,rx1);y1=std::max(y1,ry1);
    }
    if(covered!=std::uint64_t(x1-x0+1)*std::uint64_t(y1-y0+1))return false; // Not a tiling (overlaps or gaps).

    auto& gs=context.gs;
    const std::uint32_t width=std::uint32_t(x1-x0+1),height=std::uint32_t(y1-y0+1);
    const std::uint32_t samples=1u<<(pass.sampling_rate_x_log2+pass.sampling_rate_y_log2);
    Push push{{std::uint32_t(x0),std::uint32_t(y0),width,height},{frame.FBP*32u,frame.FBW,tex0.TBP0,tex0.TBW},
              {std::uint32_t(offset_x),std::uint32_t(offset_y),tex0.CBP,strength},{gs.get_vram_size()/4u,pass.super_sample_reference?1u:0u,0,0}};
    auto* program=nativeComputeProgram(context.device,dof_weight_spv,1,sizeof push,0,0,1);
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        cmd.set_program(program);
        cmd.set_specialization_constant_mask(1);
        cmd.set_specialization_constant(0,samples);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.push_constants(&push,0,sizeof push);
        cmd.dispatch((width+7)/8,(height+7)/8,1);
    });
    return true;
}
