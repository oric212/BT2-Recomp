// Native pass "ink-edges": the render pass that makes the ink outline's edge buffer (docs/plans/graphics/
// HYBRID_RENDERING_PLAN.md step 2). BT2's fights draw it as sprites into a PSMCT16 buffer: a clear, then the
// material ids (the top byte of a PSMCT32 buffer, read as a T8H texture) through a PSMCT32 palette, then the same
// palette values a whole texel away subtracted from it per channel; where a neighbour's value differs, the channel
// is non-zero. The renderer draws this pass single-sampled (parallel_gs_super_sampled_effects.patch), which gives the
// game's band per game pixel. Here every sample is computed from the ids of the same sample position, so the edge
// follows the geometry at the super-sampled resolution, and each edge sample is extended down within its game pixel
// so the copies one and two game pixels up join into the original's band (shaders/ink_edges_resolve.comp). At 1x the
// result is the renderer's exactly.
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/ink_edges_resolve_spv.h"
#include "gs/gpu/native/shaders/ink_edges_spv.h"
#include <algorithm>
#include <climits>

namespace {
using namespace ParallelGS;

constexpr std::uint32_t maxLayers=8,maxSprites=256;
constexpr std::uint32_t flagTextured=1,flagTcc=2,flagBlend=4,flagColclamp=8; // shaders/ink_edges.comp

struct Layer {std::uint32_t rect[4],state[4];std::int32_t offset[4];};
struct Uniforms {std::uint32_t area[4],buffers[4],texture[4],memory[4];Layer layers[maxLayers];};
struct ResolvePush {std::uint32_t origin[2],size[2],block,width,plane,samples,reference;};

struct Rect {int x0,y0,x1,y1;};
// One layer: consecutive sprites drawn with the same state, colour, texture and texel offset.
struct Collected {
    std::uint32_t state_index,fbmsk,rgba,alpha,tex,first,end;
    int offset_x,offset_y;
};

int floorDiv16(int v) {return v>>4;} // Arithmetic: rounds toward minus infinity.
int ceilDiv16(int v) {return (v+15)>>4;}

// A range of GS memory in words, [begin, end): rows y0..y1 of a buffer as whole rows of pages from its base block.
struct Words {std::uint64_t begin,end;};
bool overlap(Words a,Words b) {return a.begin<b.end && b.begin<a.end;}
Words pageRows(std::uint32_t block,std::uint32_t width,int y0,int y1,int page_height) {
    const std::uint64_t base=std::uint64_t(block)*64u,row=std::uint64_t(std::max(width,1u))*2048u;
    return {base+std::uint64_t(y0/page_height)*row,base+std::uint64_t(y1/page_height+1)*row};
}

// GS blend inputs (A, B, D: source, destination, zero; C: source alpha, destination alpha, FIX) as the renderer
// keeps them, in the order the GS ALPHA register has them.
std::uint32_t blendFields(std::uint32_t mode,std::uint32_t fix) {
    const auto field=[mode](int offset) {return (mode>>offset)&3u;};
    return field(BLEND_MODE_A_MODE_OFFSET)|field(BLEND_MODE_B_MODE_OFFSET)<<2|field(BLEND_MODE_C_MODE_OFFSET)<<4
        |field(BLEND_MODE_D_MODE_OFFSET)<<6|(fix&0xFFu)<<8;
}
}

// A sprite into a PSMCT16 frame, flat or with a T8H texture through a PSMCT32 palette in CSM1 order at its start;
// no antialiasing, fog, alpha or destination alpha test.
bool inkEdgesDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    if(PRIMType(prim.PRIM)!=PRIMType::Sprite || prim.FGE || prim.AA1 || ctx.frame.desc.PSM!=PSMCT16
       || ctx.test.desc.ATE || ctx.test.desc.DATE)return false;
    if(!prim.TME)return true;
    const auto& tex0=ctx.tex0.desc;
    return tex0.PSM==PSMT8H && tex0.CPSM==PSMCT32 && tex0.CSM==0 && tex0.CSA==0;
}

bool inkEdges(const NativeRenderPass& pass,NativeContext& context) {
    const auto& rp=pass.rp;
    if(rp.num_instances!=1 || rp.num_textures!=1 || rp.has_aa1 || rp.has_scanmsk || rp.feedback_mode!=RenderPass::None
       || !rp.num_primitives || rp.num_primitives>maxSprites)return false;
    // Samples on a square grid, 1, 4 or 16 (the resolve extends edges down its columns).
    if(pass.sampling_rate_x_log2!=pass.sampling_rate_y_log2)return false;
    const auto& inst=rp.instances[0];
    const auto& frame=inst.fb.frame.desc;
    if(frame.PSM!=PSMCT16 || inst.z_sensitive || inst.z_write)return false;

    Rect rects[maxSprites];
    Collected layers[maxLayers];
    std::uint32_t count=0;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        if(!(prim.state&(1u<<STATE_BIT_SPRITE)))return false;
        const std::uint32_t state_index=(prim.state>>STATE_INDEX_BIT_OFFSET)&((1u<<STATE_INDEX_BIT_COUNT)-1u);
        const auto& state=rp.states[state_index];
        if(state.combiner&COMBINER_FOG_BIT)return false;
        if(state.blend_mode&(BLEND_MODE_ATE_BIT|BLEND_MODE_DATE_BIT|BLEND_MODE_PABE_BIT|BLEND_MODE_FB_ALPHA_BIT|BLEND_MODE_DTHE_BIT))
            return false;
        const bool textured=(state.combiner&COMBINER_TME_BIT)!=0;
        const auto& p0=pass.positions[3*n].pos;
        const auto& p1=pass.positions[3*n+1].pos;
        const auto& a0=pass.attributes[3*n];
        const auto& a1=pass.attributes[3*n+1];
        // Whole or half pixel corners, one flat colour.
        if(((p0.x|p0.y|p1.x|p1.y)&7) || a0.rgba!=a1.rgba)return false;
        int offset_x=0,offset_y=0;
        std::uint32_t tex=UINT32_MAX;
        if(textured) {
            if((state.combiner&((1u<<COMBINER_MODE_BITS)-1u))!=COMBINER_DECAL || (prim.state&(1u<<STATE_BIT_PERSPECTIVE)))return false;
            if(prim.tex&(TEX_SAMPLER_MAG_LINEAR_BIT|TEX_SAMPLER_MIN_LINEAR_BIT|TEX_SAMPLER_MIPMAP_LINEAR_BIT|TEX_REPLACEMENT_BIT))
                return false;
            // Texture coordinates spanning what the corners span: each pixel reads one texel a whole number of
            // texels from it.
            if(int(a1.uv.x)-int(a0.uv.x)!=p1.x-p0.x || int(a1.uv.y)-int(a0.uv.y)!=p1.y-p0.y)return false;
            offset_x=floorDiv16(int(a0.uv.x)-p0.x);offset_y=floorDiv16(int(a0.uv.y)-p0.y);
            tex=(prim.tex>>TEX_TEXTURE_INDEX_OFFSET)&((1u<<TEX_TEXTURE_INDEX_BITS)-1u);
        }
        // The pixels the GS covers (top-left rule) within the scissor (which clipped the box).
        Rect& r=rects[n];
        r={std::max(ceilDiv16(std::min(p0.x,p1.x)),int(prim.bb.x)),std::max(ceilDiv16(std::min(p0.y,p1.y)),int(prim.bb.y)),
           std::min(ceilDiv16(std::max(p0.x,p1.x))-1,int(prim.bb.z)),std::min(ceilDiv16(std::max(p0.y,p1.y))-1,int(prim.bb.w))};
        if(r.x1<r.x0 || r.y1<r.y0)return false;
        Collected* layer=count?&layers[count-1]:nullptr;
        if(!layer || layer->state_index!=state_index || layer->fbmsk!=prim.fbmsk || layer->rgba!=a0.rgba
           || layer->alpha!=prim.alpha || layer->tex!=tex || layer->offset_x!=offset_x || layer->offset_y!=offset_y) {
            if(count==maxLayers)return false;
            layer=&layers[count++];
            *layer={state_index,prim.fbmsk,a0.rgba,prim.alpha,tex,n,n,offset_x,offset_y};
        }
        layer->end=n+1;
    }

    // The ids texture and its palette, read from GS memory as the renderer's upload and CLUT load read them.
    const auto& tex0=pass.tex0[0];
    if(tex0.PSM!=PSMT8H || tex0.CPSM!=PSMCT32 || tex0.CSM!=0 || tex0.CSA!=0 || tex0.CLD==TEX0Bits::CLD_IGNORE)return false;
    const int tex_w=1<<tex0.TW,tex_h=1<<tex0.TH;

    Uniforms uniforms{};
    Rect area{INT_MAX,INT_MAX,INT_MIN,INT_MIN};
    int read_y0=INT_MAX,read_y1=INT_MIN;
    for(std::uint32_t l=0;l<count;++l) {
        const auto& layer=layers[l];
        // The layer's sprites tile a rectangle: no two overlap, their areas add up to the box.
        Rect box{INT_MAX,INT_MAX,INT_MIN,INT_MIN};
        std::uint64_t sum=0;
        for(std::uint32_t n=layer.first;n<layer.end;++n) {
            const Rect& r=rects[n];
            for(std::uint32_t m=layer.first;m<n;++m) {
                const Rect& o=rects[m];
                if(r.x0<=o.x1 && o.x0<=r.x1 && r.y0<=o.y1 && o.y0<=r.y1)return false;
            }
            box={std::min(box.x0,r.x0),std::min(box.y0,r.y0),std::max(box.x1,r.x1),std::max(box.y1,r.y1)};
            sum+=std::uint64_t(r.x1-r.x0+1)*std::uint64_t(r.y1-r.y0+1);
        }
        if(sum!=std::uint64_t(box.x1-box.x0+1)*std::uint64_t(box.y1-box.y0+1))return false;
        area={std::min(area.x0,box.x0),std::min(area.y0,box.y0),std::max(area.x1,box.x1),std::max(area.y1,box.y1)};
        const auto& state=rp.states[layer.state_index];
        std::uint32_t flags=0;
        if(layer.tex!=UINT32_MAX) {
            // Every texel read lies inside the texture: its wrap mode does not matter.
            if(box.x0+layer.offset_x<0 || box.y0+layer.offset_y<0 || box.x1+layer.offset_x>=tex_w || box.y1+layer.offset_y>=tex_h)
                return false;
            read_y0=std::min(read_y0,box.y0+layer.offset_y);read_y1=std::max(read_y1,box.y1+layer.offset_y);
            flags|=flagTextured|((state.combiner&COMBINER_TCC_BIT)?flagTcc:0u);
        }
        if(state.blend_mode&BLEND_MODE_ABE_BIT)flags|=flagBlend;
        if(state.blend_mode&BLEND_MODE_COLCLAMP_BIT)flags|=flagColclamp;
        uniforms.layers[l]={{std::uint32_t(box.x0),std::uint32_t(box.y0),std::uint32_t(box.x1),std::uint32_t(box.y1)},
                            {layer.fbmsk,layer.rgba,blendFields(state.blend_mode,layer.alpha>>ALPHA_AFIX_OFFSET),flags},
                            {layer.offset_x,layer.offset_y,0,0}};
    }
    if(read_y0>read_y1)return false; // No layer reads the ids: not the edge pass.

    // The edge buffer must not share memory with what the pass reads.
    const Words written=pageRows(frame.FBP*32u,frame.FBW,area.y0,area.y1,64);
    const Words ids=pageRows(tex0.TBP0,tex0.TBW,read_y0,read_y1,32);
    const Words palette{std::uint64_t(tex0.CBP)*64u,std::uint64_t(tex0.CBP)*64u+256u};
    if(overlap(written,ids) || overlap(written,palette))return false;

    auto& gs=context.gs;
    const std::uint32_t width=std::uint32_t(area.x1-area.x0+1),height=std::uint32_t(area.y1-area.y0+1);
    const std::uint32_t samples=1u<<(pass.sampling_rate_x_log2+pass.sampling_rate_y_log2);
    const std::uint32_t plane_words=gs.get_vram_size()/4u,reference=pass.super_sample_reference?1u:0u;
    uniforms.area[0]=std::uint32_t(area.x0);uniforms.area[1]=std::uint32_t(area.y0);uniforms.area[2]=width;uniforms.area[3]=height;
    uniforms.buffers[0]=frame.FBP*32u;uniforms.buffers[1]=frame.FBW;uniforms.buffers[2]=tex0.TBP0;uniforms.buffers[3]=tex0.TBW;
    uniforms.texture[0]=std::uint32_t(tex_w-1);uniforms.texture[1]=std::uint32_t(tex_h-1);uniforms.texture[2]=tex0.CBP;uniforms.texture[3]=count;
    uniforms.memory[0]=plane_words;uniforms.memory[1]=samples;uniforms.memory[2]=reference;
    const ResolvePush push{{std::uint32_t(area.x0),std::uint32_t(area.y0)},{width,height},frame.FBP*32u,frame.FBW,
                           plane_words*2u,samples,reference};

    auto* edges=nativeComputeProgram(context.device,ink_edges_spv,2,0,1);
    auto* resolve=nativeComputeProgram(context.device,ink_edges_resolve_spv,2,sizeof push);
    const auto& scratch=context.scratch(VkDeviceSize(width)*height*samples*4u);
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        cmd.set_program(edges);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_storage_buffer(0,1,scratch);
        *static_cast<Uniforms*>(cmd.allocate_constant_data(0,2,sizeof uniforms))=uniforms;
        cmd.dispatch((width+7)/8,(height+7)/8,samples);
        cmd.barrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        cmd.set_program(resolve);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_storage_buffer(0,1,scratch);
        cmd.push_constants(&push,0,sizeof push);
        cmd.dispatch((width+7)/8,(height+7)/8,1);
    });
    return true;
}
