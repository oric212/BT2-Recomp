// Native pass "glow": runs of textured triangles blended into the frame ((Cs - 0) x C / 128 + Cd, or (Cs - Cd) x C / 128
// + Cd), depth-tested without depth writes, as a super attack's glow and the layers between them draw them (docs/plans/graphics/HYBRID_RENDERING_PLAN.md step 3). paraLLEl-GS
// shades every sample of them, which at 16x costs up to 10 ms a frame; this pass shades each triangle once per game
// pixel and does coverage, the depth test and the blend per sample (shaders/glow.comp). With super-sampling only: at
// 1x the renderer draws them, exact by construction.
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/glow_spv.h"
#include <algorithm>
#include <climits>
#include <vector>

namespace {
using namespace ParallelGS;

constexpr std::uint32_t slotCount=8; // Texture bindings of shaders/glow.comp.
constexpr std::uint32_t flagGouraud=1,flagPerspective=2,flagTcc=4,flagColclamp=8,flagFix=16,flagZtest=32,flagZgreater=64,flagMinusDestination=128,flagParallelogram=256,flagPerSample=512;

struct Triangle {
    std::int32_t x[4],y[4];
    std::uint32_t z[4],rgba[4];
    float s[4],t[4],q[4];
    std::uint32_t info[4];
};
struct Slot {float scale[4],size[4],region[4];};
struct Uniforms {std::uint32_t area[4],buffers[4],memory[4],run[4];Slot slots[slotCount];};

// A texture as a slot binds it: its index in the render pass, which coordinates clamp, the filter.
struct SlotKey {
    std::uint32_t texture;bool clamp_s,clamp_t,linear;
    bool operator==(const SlotKey&) const=default;
};

std::uint32_t mode(std::uint32_t blend_mode,int offset,int bits) {return (blend_mode>>offset)&((1u<<bits)-1u);}
}

// A triangle, strip or fan with a texture, blended, depth-tested, into a PSMCT32 frame with the depth buffer masked; no fog,
// antialiasing, alpha or destination alpha test; blending (Cs - 0 or Cd) x (As or FIX) / 128 + Cd, MODULATE, one mip level.
bool glowDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    const auto type=PRIMType(prim.PRIM);
    if((type!=PRIMType::TriangleList && type!=PRIMType::TriangleStrip && type!=PRIMType::TriangleFan) || !prim.TME || !prim.ABE
       || prim.FGE || prim.AA1)return false;
    const auto& alpha=ctx.alpha.desc;
    // Depth-tested (greater or equal, or greater): the scene's effects, not the HUD's 2D quads, whose sharp texture edges
    // need the renderer's shading per sample.
    return ctx.frame.desc.PSM==PSMCT32 && ctx.zbuf.desc.ZMSK && ctx.test.desc.ZTE && ctx.test.desc.ZTST>=2 && !ctx.test.desc.ATE
        && !ctx.test.desc.DATE
        && alpha.A==0 && (alpha.B==1 || alpha.B==2) && (alpha.C==0 || alpha.C==2) && alpha.D==1 && ctx.tex0.desc.TFX==0 && ctx.tex1.desc.MXL==0
        && !ctx.fba.desc.FBA && !registers.dthe.desc.DTHE && !registers.pabe.desc.PABE;
}

bool glow(const NativeRenderPass& pass,NativeContext& context) {
    const auto& rp=pass.rp;
    // With super-sampling on a square grid only (4x, 16x).
    if(!pass.sampling_rate_x_log2 || pass.sampling_rate_x_log2!=pass.sampling_rate_y_log2 || !pass.super_sample_reference)return false;
    if(rp.num_instances!=1 || !rp.num_textures || rp.has_aa1 || rp.has_scanmsk || rp.feedback_mode!=RenderPass::None || !rp.num_primitives)
        return false;
    const auto& inst=rp.instances[0];
    const auto& frame=inst.fb.frame.desc;
    const auto& zbuf=inst.fb.z.desc;
    if(frame.PSM!=PSMCT32 || inst.z_write || (zbuf.PSM|0x30u)!=PSMZ24 || zbuf.ZBP==frame.FBP
       || inst.sampling_rate_x_log2!=pass.sampling_rate_x_log2 || inst.sampling_rate_y_log2!=pass.sampling_rate_y_log2)return false;

    std::vector<Triangle> triangles;triangles.reserve(rp.num_primitives);
    std::vector<SlotKey> keys; // Per triangle.
    int lo_x=INT_MAX,lo_y=INT_MAX,hi_x=INT_MIN,hi_y=INT_MIN;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        constexpr std::uint32_t refused=(1u<<STATE_BIT_SPRITE)|(1u<<STATE_BIT_LINE)
            |(1u<<STATE_BIT_MULTISAMPLE)|(1u<<STATE_BIT_SCANMSK_EVEN)|(1u<<STATE_BIT_SCANMSK_ODD)|(1u<<STATE_BIT_Z_WRITE);
        if(prim.state&refused)return false;
        const auto& state=rp.states[(prim.state>>STATE_INDEX_BIT_OFFSET)&((1u<<STATE_INDEX_BIT_COUNT)-1u)];
        if(!(state.combiner&COMBINER_TME_BIT) || (state.combiner&COMBINER_FOG_BIT)
           || mode(state.combiner,COMBINER_MODE_OFFSET,COMBINER_MODE_BITS)!=COMBINER_MODULATE)return false;
        const auto blend=state.blend_mode;
        if(!(blend&BLEND_MODE_ABE_BIT) || (blend&(BLEND_MODE_ATE_BIT|BLEND_MODE_DATE_BIT|BLEND_MODE_PABE_BIT|BLEND_MODE_FB_ALPHA_BIT|BLEND_MODE_DTHE_BIT)))
            return false;
        const auto b=mode(blend,BLEND_MODE_B_MODE_OFFSET,BLEND_MODE_B_MODE_BITS),c=mode(blend,BLEND_MODE_C_MODE_OFFSET,BLEND_MODE_C_MODE_BITS);
        if(mode(blend,BLEND_MODE_A_MODE_OFFSET,BLEND_MODE_A_MODE_BITS)!=BLEND_RGB_SOURCE || (b!=BLEND_RGB_ZERO && b!=BLEND_RGB_DEST)
           || mode(blend,BLEND_MODE_D_MODE_OFFSET,BLEND_MODE_D_MODE_BITS)!=BLEND_RGB_DEST || (c!=BLEND_ALPHA_SOURCE && c!=BLEND_ALPHA_FIX))return false;
        // One level, sampled from the renderer's own image of the texture (not one made of super-sampled pixels). A
        // replacement's coordinates per sample (the renderer's 2D art) are taken per pixel here as all others.
        if(prim.tex&(TEX_SAMPLE_MAPPING_BIT|TEX_SAMPLE_RESOLVED_BIT|TEX_SAMPLER_MIPMAP_LINEAR_BIT
                     |(((1u<<TEX_MAX_MIP_LEVEL_BITS)-1u)<<TEX_MAX_MIP_LEVEL_OFFSET)))return false;
        if((prim.tex&TEX_PER_SAMPLE_BIT) && !(prim.tex&TEX_REPLACEMENT_BIT))return false;
        const std::uint32_t texture=(prim.tex>>TEX_TEXTURE_INDEX_OFFSET)&((1u<<TEX_TEXTURE_INDEX_BITS)-1u);
        if(texture>=rp.num_textures || rp.textures[texture].info.arrayed)return false;
        const bool clamp_s=(prim.tex&TEX_SAMPLER_CLAMP_S_BIT)!=0,clamp_t=(prim.tex&TEX_SAMPLER_CLAMP_T_BIT)!=0;
        const auto& info=rp.textures[texture].info;
        const auto& tex0=pass.tex0[texture];
        // A coordinate that repeats needs the image to be the whole texture along it; a clamped one is held to the
        // texture entry's region (CLAMP: the image; REGION_CLAMP and the renderer's pieces of 2D art: their texels).
        if((!clamp_s && (info.bias.x!=0.0f || info.sizes.z*float(1u<<tex0.TW)!=1.0f))
           || (!clamp_t && (info.bias.y!=0.0f || info.sizes.w*float(1u<<tex0.TH)!=1.0f)))return false;
        keys.push_back({texture,clamp_s,clamp_t,(prim.tex&TEX_SAMPLER_MAG_LINEAR_BIT)!=0});

        Triangle tri{};
        std::uint32_t flags=(c==BLEND_ALPHA_FIX?flagFix:0u)|(b==BLEND_RGB_DEST?flagMinusDestination:0u)|((state.combiner&COMBINER_TCC_BIT)?flagTcc:0u)|((blend&BLEND_MODE_COLCLAMP_BIT)?flagColclamp:0u);
        if(prim.state&(1u<<STATE_BIT_IIP))flags|=flagGouraud;
        if(prim.state&(1u<<STATE_BIT_PERSPECTIVE))flags|=flagPerspective;
        if(prim.tex&TEX_PER_SAMPLE_BIT)flags|=flagPerSample;
        if(!(prim.state&(1u<<STATE_BIT_Z_TEST)))return false; // Depth-tested only (glowDraw).
        flags|=flagZtest|((prim.state&(1u<<STATE_BIT_Z_TEST_GREATER))?flagZgreater:0u);
        // A parallelogram (two triangles the renderer joined): its provoking vertex first, the fourth corner opposite.
        int first=0;
        if(prim.state&(1u<<STATE_BIT_PARALLELOGRAM)) {
            flags|=flagParallelogram;
            first=int((prim.state>>STATE_PARALLELOGRAM_PROVOKING_OFFSET)&((1u<<STATE_PARALLELOGRAM_PROVOKING_COUNT)-1u));
            if(first>2)return false;
        }
        for(int v=0;v<3;++v) {
            const auto& position=pass.positions[3*n+(v+first)%3];
            const auto& attribute=pass.attributes[3*n+(v+first)%3];
            tri.x[v]=position.pos.x;tri.y[v]=position.pos.y;tri.z[v]=std::min(position.z,0xFFFFFFu);
            // Flat: the provoking vertex's colour (the renderer's vertex 0) for all three.
            tri.rgba[v]=(flags&flagGouraud)?attribute.rgba:pass.attributes[3*n].rgba;
            if(flags&flagPerspective) {tri.s[v]=attribute.st.x;tri.t[v]=attribute.st.y;tri.q[v]=attribute.q;}
            else {tri.s[v]=float(attribute.uv.x)/16.0f;tri.t[v]=float(attribute.uv.y)/16.0f;tri.q[v]=1.0f;}
        }
        const auto& bb=prim.bb;
        if(bb.z<bb.x || bb.w<bb.y)return false;
        tri.x[3]=bb.x;tri.y[3]=bb.y;tri.z[3]=std::uint32_t(bb.z)|(std::uint32_t(bb.w)<<16);
        tri.info[0]=flags;tri.info[1]=(prim.alpha>>ALPHA_AFIX_OFFSET)&0xFFu;tri.info[2]=prim.fbmsk;
        triangles.push_back(tri);
        lo_x=std::min<int>(lo_x,bb.x);lo_y=std::min<int>(lo_y,bb.y);hi_x=std::max<int>(hi_x,bb.z);hi_y=std::max<int>(hi_y,bb.w);
    }

    // Runs of consecutive triangles that use at most slotCount textures: one dispatch each, in order.
    struct Run {std::uint32_t first,count;std::vector<SlotKey> slots;};
    std::vector<Run> runs;
    for(std::uint32_t n=0;n<triangles.size();++n) {
        if(runs.empty())runs.push_back({n,0,{}});
        auto* run=&runs.back();
        auto at=std::find(run->slots.begin(),run->slots.end(),keys[n]);
        if(at==run->slots.end() && run->slots.size()==slotCount) {runs.push_back({n,0,{}});run=&runs.back();at=run->slots.end();}
        if(at==run->slots.end()) {run->slots.push_back(keys[n]);at=run->slots.end()-1;}
        triangles[n].info[3]=std::uint32_t(at-run->slots.begin());
        ++run->count;
    }

    auto& gs=context.gs;
    auto& device=context.device;
    Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::LinkedDeviceHost;
    info.size=triangles.size()*sizeof(Triangle);info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto buffer=device.create_buffer(info,triangles.data());
    const std::uint32_t width=std::uint32_t(hi_x-lo_x+1),height=std::uint32_t(hi_y-lo_y+1);
    Uniforms uniforms{};
    uniforms.area[0]=std::uint32_t(lo_x);uniforms.area[1]=std::uint32_t(lo_y);uniforms.area[2]=width;uniforms.area[3]=height;
    uniforms.buffers[0]=frame.FBP*32u;uniforms.buffers[1]=frame.FBW;uniforms.buffers[2]=zbuf.ZBP*32u;uniforms.buffers[3]=frame.FBW;
    uniforms.memory[0]=gs.get_vram_size()/4u;uniforms.memory[1]=1u<<(pass.sampling_rate_x_log2+pass.sampling_rate_y_log2);
    uniforms.memory[2]=1;uniforms.memory[3]=1; // Reference planes present; a 24-bit depth buffer.
    auto* program=nativeComputeProgram(device,glow_spv,2,0,1,slotCount,1);
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        cmd.set_program(program);
        cmd.set_specialization_constant_mask(1);
        cmd.set_specialization_constant(0,uniforms.memory[1]);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_storage_buffer(0,1,*buffer);
        for(std::size_t r=0;r<runs.size();++r) {
            const auto& run=runs[r];
            if(r)cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            Uniforms u=uniforms;
            u.run[0]=run.first;u.run[1]=run.count;
            for(std::uint32_t s=0;s<slotCount;++s) {
                const auto& key=run.slots[std::min<std::size_t>(s,run.slots.size()-1)];
                const auto& texture=rp.textures[key.texture];
                const auto& tex0=pass.tex0[key.texture];
                const auto& region=texture.info.region;
                u.slots[s]={{texture.info.sizes.z,texture.info.sizes.w,texture.info.bias.x,texture.info.bias.y},
                            {float(1u<<tex0.TW),float(1u<<tex0.TH),key.clamp_s?1.0f:0.0f,key.clamp_t?1.0f:0.0f},
                            {region.x,region.y,region.z,region.w}};
                const bool clamp=key.clamp_s || key.clamp_t;
                const auto sampler=key.linear?(clamp?Vulkan::StockSampler::LinearClamp:Vulkan::StockSampler::LinearWrap)
                                             :(clamp?Vulkan::StockSampler::NearestClamp:Vulkan::StockSampler::NearestWrap);
                cmd.set_texture(0,3+s,*texture.view,sampler);
            }
            *static_cast<Uniforms*>(cmd.allocate_constant_data(0,2,sizeof u))=u;
            cmd.dispatch((width+7)/8,(height+7)/8,1);
        }
    });
    return true;
}
