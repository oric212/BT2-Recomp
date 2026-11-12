// Native pass "underwater": the picture under water (Planet Namek's lake, a fighter dived in). The game copies the frame
// (512 x 448) into a buffer half as tall with one bilinear sprite, then draws that copy back over the whole frame in five
// triangle strips whose texture coordinates wave, tinted (found 2026-11-12 in a GS dump of a CPU fight on stage 14,
// flip 1,800). With super-sampling the copy keeps none of the frame's samples, so the scene came back at 512 x 224,
// blurred against everything else (the user, 2026-11-12: the underwater render low resolution). This pass draws the
// strips' render pass from the frame's own samples at the place the copy took them from (shaders/underwater.comp):
// the game's wave and tint, the frame's detail. GS memory outside the frame (the copy) stays as the renderer draws it.
// With super-sampling only: at 1x the renderer draws the strips, exact by construction.
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/underwater_spv.h"
#include <algorithm>
#include <vector>

namespace {
using namespace ParallelGS;

struct Triangle {std::int32_t x[4],y[4];float u[4],v[4];};
struct Push {std::uint32_t block,width,plane,side,size[2],mode,count;float region[2];std::uint32_t colour,pad;};

// The copy the strips read: a PSMCT32 texture as wide as the frame, its 224 rows the frame's 448 halved; the strips hold
// it with REGION_CLAMP to texels 0..512 and 0..224 (one past the copy, as the game sets it).
constexpr std::uint32_t copyWidth=512,copyRows=224;
}

// Textured triangle strips without blending into an unmasked PSMCT32 frame 512 pixels wide, from a PSMCT32 texture of
// the same width held to the copy's region; MODULATE, the alpha from the vertex (TCC 0), bilinear, one level, texture
// coordinates in texels (FST); no fog, antialiasing, alpha or destination alpha test, no depth writes.
bool underwaterDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    if(PRIMType(prim.PRIM)!=PRIMType::TriangleStrip || !prim.TME || !prim.FST || prim.ABE || prim.FGE || prim.AA1)return false;
    const auto& frame=ctx.frame.desc;
    const auto& tex0=ctx.tex0.desc;
    const auto& tex1=ctx.tex1.desc;
    const auto& clamp=ctx.clamp.desc;
    return frame.PSM==PSMCT32 && frame.FBW==copyWidth/64 && frame.FBMSK==0 && ctx.zbuf.desc.ZMSK && !ctx.test.desc.ATE && !ctx.test.desc.DATE
        && tex0.PSM==PSMCT32 && tex0.TBW==copyWidth/64 && tex0.TW==9 && tex0.TBP0!=frame.FBP*32 && !tex0.TCC && tex0.TFX==0
        && tex1.MXL==0 && tex1.MMAG==1 && tex1.MMIN==1
        && clamp.WMS==CLAMPBits::REGION_CLAMP && clamp.WMT==CLAMPBits::REGION_CLAMP && clamp.MINU==0 && clamp.MAXU==copyWidth
        && clamp.MINV==0 && clamp.MAXV==copyRows
        && !ctx.fba.desc.FBA && !registers.dthe.desc.DTHE && !registers.pabe.desc.PABE;
}

bool underwater(const NativeRenderPass& pass,NativeContext& context) {
    const auto& rp=pass.rp;
    // With super-sampling on a square grid only (4x, 16x).
    if(!pass.sampling_rate_x_log2 || pass.sampling_rate_x_log2!=pass.sampling_rate_y_log2 || pass.sampling_rate_x_log2>2
       || !pass.super_sample_reference)return false;
    if(rp.num_instances!=1 || rp.num_textures!=1 || rp.has_aa1 || rp.has_scanmsk || rp.feedback_mode!=RenderPass::None || !rp.num_primitives)
        return false;
    const auto& inst=rp.instances[0];
    const auto& frame=inst.fb.frame.desc;
    if(frame.PSM!=PSMCT32 || frame.FBW!=copyWidth/64 || inst.z_write || inst.channel_shuffle
       || inst.sampling_rate_x_log2!=pass.sampling_rate_x_log2 || inst.sampling_rate_y_log2!=pass.sampling_rate_y_log2)return false;

    std::vector<Triangle> triangles;triangles.reserve(rp.num_primitives);
    std::uint32_t colour=0;
    int hi_x=0,hi_y=0;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        constexpr std::uint32_t refused=(1u<<STATE_BIT_SPRITE)|(1u<<STATE_BIT_LINE)|(1u<<STATE_BIT_MULTISAMPLE)
            |(1u<<STATE_BIT_SCANMSK_EVEN)|(1u<<STATE_BIT_SCANMSK_ODD)|(1u<<STATE_BIT_Z_WRITE)|(1u<<STATE_BIT_PERSPECTIVE)|(1u<<STATE_BIT_IIP);
        if((prim.state&refused) || prim.fbmsk)return false;
        const auto& state=rp.states[(prim.state>>STATE_INDEX_BIT_OFFSET)&((1u<<STATE_INDEX_BIT_COUNT)-1u)];
        if(!(state.combiner&COMBINER_TME_BIT) || (state.combiner&(COMBINER_FOG_BIT|COMBINER_TCC_BIT))
           || ((state.combiner>>COMBINER_MODE_OFFSET)&((1u<<COMBINER_MODE_BITS)-1u))!=COMBINER_MODULATE)return false;
        if(state.blend_mode&(BLEND_MODE_ABE_BIT|BLEND_MODE_ATE_BIT|BLEND_MODE_DATE_BIT|BLEND_MODE_PABE_BIT|BLEND_MODE_FB_ALPHA_BIT|BLEND_MODE_DTHE_BIT)
           || !(state.blend_mode&BLEND_MODE_COLCLAMP_BIT))return false;
        if(prim.tex&(TEX_SAMPLE_MAPPING_BIT|TEX_SAMPLE_RESOLVED_BIT|TEX_SAMPLER_MIPMAP_LINEAR_BIT|TEX_REPLACEMENT_BIT
                     |(((1u<<TEX_MAX_MIP_LEVEL_BITS)-1u)<<TEX_MAX_MIP_LEVEL_OFFSET)) || !(prim.tex&TEX_SAMPLER_MAG_LINEAR_BIT))return false;
        // Flat: one colour for the whole pass (the game's tint).
        const std::uint32_t rgba=pass.attributes[3*n].rgba;
        if(n && rgba!=colour)return false;
        colour=rgba;
        Triangle tri{};
        int first=0;
        if(prim.state&(1u<<STATE_BIT_PARALLELOGRAM)) {
            first=int((prim.state>>STATE_PARALLELOGRAM_PROVOKING_OFFSET)&((1u<<STATE_PARALLELOGRAM_PROVOKING_COUNT)-1u));
            if(first>2)return false;
            tri.x[3]=1;
        }
        for(int v=0;v<3;++v) {
            const auto& position=pass.positions[3*n+(v+first)%3];
            const auto& attribute=pass.attributes[3*n+(v+first)%3];
            tri.x[v]=position.pos.x;tri.y[v]=position.pos.y;
            tri.u[v]=float(attribute.uv.x)/16.0f;tri.v[v]=float(attribute.uv.y)/16.0f;
        }
        const auto& bb=prim.bb;
        if(bb.z<bb.x || bb.w<bb.y || bb.x<0 || bb.y<0)return false;
        hi_x=std::max<int>(hi_x,bb.z);hi_y=std::max<int>(hi_y,bb.w);
        triangles.push_back(tri);
    }
    // The whole frame, as the game draws it (512 x 448).
    if(hi_x+1!=int(copyWidth) || hi_y+1!=int(2*copyRows))return false;

    auto& gs=context.gs;
    auto& device=context.device;
    const std::uint32_t side=1u<<pass.sampling_rate_x_log2;
    const std::uint32_t width=std::uint32_t(hi_x+1),height=std::uint32_t(hi_y+1);
    const auto& source=context.scratch(VkDeviceSize(width)*side*height*side*4u);
    Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::LinkedDeviceHost;
    info.size=triangles.size()*sizeof(Triangle);info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto buffer=device.create_buffer(info,triangles.data());
    Push push{frame.FBP*32u,frame.FBW,gs.get_vram_size()/4u,side,{width,height},0,std::uint32_t(triangles.size()),
              {float(copyWidth),float(copyRows)},colour,0};
    auto* program=nativeComputeProgram(device,underwater_spv,3,sizeof push);
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        cmd.set_program(program);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_storage_buffer(0,1,source);
        cmd.set_storage_buffer(0,2,*buffer);
        cmd.push_constants(&push,0,sizeof push);
        cmd.dispatch((width+7)/8,(height+7)/8,1);
        cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        Push draw=push;draw.mode=1;
        cmd.push_constants(&draw,0,sizeof draw);
        cmd.dispatch((width+7)/8,(height+7)/8,1);
    });
    return true;
}
