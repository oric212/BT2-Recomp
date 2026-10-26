// Native pass "alpha-fill": a render pass of flat sprites that only write one alpha value into a PSMCT32 frame over a
// rectangle (BT2's fights clear the frame's alpha so before the ink outline, docs/plans/graphics/IMAGE_QUALITY_PLAN.md
// step 4). The groundwork's check of the native passes (docs/plans/graphics/HYBRID_RENDERING_PLAN.md step 1): GS
// memory must be what the renderer's drawing leaves, compared with BT2_GS_MEMORY_HASHES.
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/alpha_fill_spv.h"
#include <algorithm>
#include <climits>

namespace {
using namespace ParallelGS;

struct Push {std::uint32_t origin[2],size[2],block,width,plane,alpha,samples,reference;};

// Whether primitive n is a sprite with whole-pixel corners: it covers every sample of the pixels of its clipped box.
bool wholePixelSprite(const NativeRenderPass& pass,std::uint32_t n) {
    if(!(pass.primitives[n].state&(1u<<STATE_BIT_SPRITE)))return false;
    for(int v=0;v<2;++v) {
        const auto& p=pass.positions[3*n+v].pos;
        if((p.x&((1<<PGS_SUBPIXEL_BITS)-1)) || (p.y&((1<<PGS_SUBPIXEL_BITS)-1)))return false;
    }
    return true;
}
}

// A flat sprite without texture, fog, alpha or destination alpha test into a PSMCT32 frame with all but the alpha
// byte masked. Blending may be on: it changes only the colour, which is masked; the alpha written is the source's.
bool alphaFillDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    return PRIMType(prim.PRIM)==PRIMType::Sprite && !prim.TME && !prim.FGE && !prim.AA1
        && ctx.frame.desc.PSM==PSMCT32 && ctx.frame.desc.FBMSK==0x00FFFFFFu && !ctx.test.desc.ATE && !ctx.test.desc.DATE;
}

bool alphaFill(const NativeRenderPass& pass,NativeContext& context) {
    auto& gs=context.gs;
    const auto& rp=pass.rp;
    if(rp.num_instances!=1 || rp.num_textures || rp.has_aa1 || rp.has_scanmsk || rp.feedback_mode!=RenderPass::None || !rp.num_primitives)return false;
    const auto& inst=rp.instances[0];
    if(inst.fb.frame.desc.PSM!=PSMCT32 || inst.z_sensitive || inst.z_write || inst.channel_shuffle)return false;
    std::uint32_t alpha=UINT32_MAX,fba=UINT32_MAX;
    int lo_x=INT_MAX,lo_y=INT_MAX,hi_x=INT_MIN,hi_y=INT_MIN;
    std::uint64_t area=0;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        if(prim.fbmsk!=0x00FFFFFFu || !wholePixelSprite(pass,n))return false; // Only the alpha byte written.
        const auto& state=rp.states[(prim.state>>STATE_INDEX_BIT_OFFSET)&((1u<<STATE_INDEX_BIT_COUNT)-1u)];
        if(state.combiner&(COMBINER_TME_BIT|COMBINER_FOG_BIT))return false;
        if(state.blend_mode&(BLEND_MODE_ATE_BIT|BLEND_MODE_DATE_BIT))return false;
        const std::uint32_t prim_fba=(state.blend_mode&BLEND_MODE_FB_ALPHA_BIT)?0x80u:0u;
        // The sprite's colour is flat: both vertices must carry the same alpha.
        const std::uint32_t a0=pass.attributes[3*n].rgba>>24,a1=pass.attributes[3*n+1].rgba>>24;
        if(a0!=a1 || (alpha!=UINT32_MAX && (a0!=alpha || prim_fba!=fba)))return false;
        alpha=a0;fba=prim_fba;
        const auto& bb=prim.bb;
        if(bb.z<bb.x || bb.w<bb.y)return false;
        // No overlaps: the boxes must tile their union (checked by area below).
        for(std::uint32_t m=0;m<n;++m) {
            const auto& o=pass.primitives[m].bb;
            if(bb.x<=o.z && o.x<=bb.z && bb.y<=o.w && o.y<=bb.w)return false;
        }
        lo_x=std::min<int>(lo_x,bb.x);lo_y=std::min<int>(lo_y,bb.y);hi_x=std::max<int>(hi_x,bb.z);hi_y=std::max<int>(hi_y,bb.w);
        area+=std::uint64_t(bb.z-bb.x+1)*std::uint64_t(bb.w-bb.y+1);
    }
    if(area!=std::uint64_t(hi_x-lo_x+1)*std::uint64_t(hi_y-lo_y+1))return false;
    Push push{{std::uint32_t(lo_x),std::uint32_t(lo_y)},{std::uint32_t(hi_x-lo_x+1),std::uint32_t(hi_y-lo_y+1)},
              inst.fb.frame.desc.FBP*32u,inst.fb.frame.desc.FBW,gs.get_vram_size()/4u,(alpha|fba)&0xFFu,
              1u<<(inst.sampling_rate_x_log2+inst.sampling_rate_y_log2),pass.super_sample_reference?1u:0u};
    auto* program=nativeComputeProgram(context.device,alpha_fill_spv,1,sizeof push);
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        cmd.set_program(program);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.push_constants(&push,0,sizeof push);
        cmd.dispatch((push.size[0]+7)/8,(push.size[1]+7)/8,1);
    });
    return true;
}
