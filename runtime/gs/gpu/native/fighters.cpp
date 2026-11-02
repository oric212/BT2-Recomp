// Native pass "fighters": the fighters' render passes drawn by the GPU's rasterizer (docs/plans/graphics/
// NATIVE_FIGHT_RENDERING_PLAN.md step 1), with 60 fps on, in both renderers. A fighter's piece is two layers at the same
// positions: an opaque base ((Cs - Cd) x FIX / 128 + Cd with FIX 0x80, depth written) and the cel shading subtracted from
// it ((0 - Cs) x FIX / 128 + Cd, RGB only). The pass loads the frame's and the depth buffer's samples from GS memory
// into a single-sampled image with one pixel per super-sample (paraLLEl-GS's samples lie on a regular grid, so the image
// is the game's size times the grid's side; shaders/fighters_load.frag), draws the triangles there with the GS's
// depth test and the blends above (shaders/fighters.vert, fighters.frag), and stores the samples back as the renderer
// leaves them (shaders/fighters_store.comp). Not exact against the renderer (the rasterizer's own rules and
// interpolation); checked by pictures and by GS memory at 1x within limits.
#include "gs/gpu/native/native_pass.h"
#include "gs/gpu/native/shaders/fighters_frag_spv.h"
#include "gs/gpu/native/shaders/fighters_load_frag_spv.h"
#include "gs/gpu/native/shaders/fighters_load_vert_spv.h"
#include "gs/gpu/native/shaders/fighters_store_spv.h"
#include "gs/gpu/native/shaders/fighters_vert_spv.h"
#include "device.hpp"
#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace {
using namespace ParallelGS;

constexpr std::uint32_t flagPerspective=1,flagTcc=2,flagDecal=4,flagSubtract=8,flagLayered=16,flagClampS=32,flagClampT=64; // shaders/fighters.frag

struct Vertex {std::int32_t x,y;std::uint32_t z,rgba;float s,t,q,s2,t2,q2;}; // s2, t2, q2: the cel-shading layer's (Draw).
struct DrawPush {float area[4],image[4],scale[2][4],region[2][4],sizes[4];std::uint32_t mode[4];};
struct MemoryPush {std::uint32_t area[4],buffers[4],memory[4];};

// Consecutive triangles drawn with the same texture and state.
struct Batch {
    std::uint32_t first,count,texture,flags,fix,write_mask;
    bool clamp_s,clamp_t,linear,depth_write,greater;
    bool same(const Batch& o) const {
        return texture==o.texture && flags==o.flags && fix==o.fix && write_mask==o.write_mask && clamp_s==o.clamp_s
            && clamp_t==o.clamp_t && linear==o.linear && depth_write==o.depth_write && greater==o.greater;
    }
};

std::uint32_t mode(std::uint32_t blend_mode,int offset,int bits) {return (blend_mode>>offset)&((1u<<bits)-1u);}

// The image of one renderer's thread, kept for the next pass (each renderer draws on a thread of its own).
struct Images {Vulkan::Device* device=nullptr;unsigned width=0,height=0;Vulkan::ImageHandle colour,depth;};
thread_local Images images;
struct Programs {Vulkan::Device* device=nullptr;Vulkan::Program *load=nullptr,*draw=nullptr,*store=nullptr;};
thread_local Programs programs;

Vulkan::Program* graphicsProgram(Vulkan::Device& device,std::span<const std::uint32_t> vert,std::span<const std::uint32_t> frag,
                                 std::uint32_t inputs,bool buffer,bool texture,unsigned push_bytes) {
    Vulkan::ResourceLayout v,f;
    v.input_mask=inputs;v.push_constant_size=push_bytes;
    f.output_mask=1;f.push_constant_size=push_bytes;
    if(buffer) {f.sets[0].storage_buffer_mask=1;f.sets[0].meta[0].array_size=1;}
    if(texture) {f.sets[0].sampled_image_mask=3;f.sets[0].fp_mask=3;f.sets[0].meta[0].array_size=1;f.sets[0].meta[1].array_size=1;}
    auto* vs=device.request_shader(vert.data(),vert.size_bytes(),&v);
    auto* fs=device.request_shader(frag.data(),frag.size_bytes(),&f);
    return device.request_program(vs,fs);
}

void layout(Vulkan::CommandBuffer& cmd,const Vulkan::Image& image,VkImageLayout from,VkImageLayout to,VkPipelineStageFlags2 src_stage,
            VkAccessFlags2 src_access,VkPipelineStageFlags2 dst_stage,VkAccessFlags2 dst_access) {
    cmd.image_barrier(image,from,to,src_stage,src_access,dst_stage,dst_access);
}
}

// A triangle, strip or fan with a texture, depth-tested (greater or equal, or greater) into a PSMCT32 frame over a
// 24-bit depth buffer: MODULATE or DECAL, one mip level, no fog, antialiasing, alpha or destination alpha test, the
// frame written whole or without its alpha; blended as the fighters' two layers are.
bool fightersDraw(const RegisterState& registers) {
    const auto& prim=registers.prim.desc;
    const auto& ctx=registers.ctx[prim.CTXT];
    const auto type=PRIMType(prim.PRIM);
    if((type!=PRIMType::TriangleList && type!=PRIMType::TriangleStrip && type!=PRIMType::TriangleFan) || !prim.TME || !prim.ABE
       || prim.FGE || prim.AA1)return false;
    const auto& alpha=ctx.alpha.desc;
    const bool opaque=alpha.A==0 && alpha.B==1 && alpha.C==2 && alpha.D==1 && alpha.FIX==0x80;
    const bool subtract=alpha.A==2 && alpha.B==0 && alpha.C==2 && alpha.D==1;
    const std::uint32_t mask=std::uint32_t(ctx.frame.desc.FBMSK);
    return (opaque || subtract) && (mask==0 || mask==0xFF000000u) && ctx.frame.desc.PSM==PSMCT32 && (ctx.zbuf.desc.PSM|0x30u)==PSMZ24
        && ctx.test.desc.ZTE && ctx.test.desc.ZTST>=2 && !ctx.test.desc.ATE && !ctx.test.desc.DATE && ctx.tex0.desc.TFX<=1
        && ctx.tex1.desc.MXL==0 && !ctx.fba.desc.FBA && !registers.dthe.desc.DTHE && !registers.pabe.desc.PABE;
}

bool fighters(const NativeRenderPass& pass,NativeContext& context) {
    const auto& rp=pass.rp;
    const std::uint32_t rate=pass.sampling_rate_x_log2;
    if(rate!=pass.sampling_rate_y_log2 || rate>2 || (rate && !pass.super_sample_reference))return false;
    if(rp.num_instances!=1 || !rp.num_textures || rp.has_aa1 || rp.has_scanmsk || rp.feedback_mode!=RenderPass::None || !rp.num_primitives)
        return false;
    const auto& inst=rp.instances[0];
    const auto& frame=inst.fb.frame.desc;
    const auto& zbuf=inst.fb.z.desc;
    if(frame.PSM!=PSMCT32 || (zbuf.PSM|0x30u)!=PSMZ24 || zbuf.ZBP==frame.FBP || inst.sampling_rate_x_log2!=rate || inst.sampling_rate_y_log2!=rate)
        return false;

    std::vector<Vertex> vertices;vertices.reserve(rp.num_primitives*6);
    std::vector<Batch> batches;
    int lo_x=INT_MAX,lo_y=INT_MAX,hi_x=INT_MIN,hi_y=INT_MIN;
    for(std::uint32_t n=0;n<rp.num_primitives;++n) {
        const auto& prim=pass.primitives[n];
        constexpr std::uint32_t refused=(1u<<STATE_BIT_SPRITE)|(1u<<STATE_BIT_LINE)|(1u<<STATE_BIT_MULTISAMPLE)
            |(1u<<STATE_BIT_SCANMSK_EVEN)|(1u<<STATE_BIT_SCANMSK_ODD);
        if(prim.state&refused)return false;
        if(!(prim.state&(1u<<STATE_BIT_Z_TEST)))return false;
        const auto& state=rp.states[(prim.state>>STATE_INDEX_BIT_OFFSET)&((1u<<STATE_INDEX_BIT_COUNT)-1u)];
        const auto combiner=mode(state.combiner,COMBINER_MODE_OFFSET,COMBINER_MODE_BITS);
        if(!(state.combiner&COMBINER_TME_BIT) || (state.combiner&COMBINER_FOG_BIT) || (combiner!=COMBINER_MODULATE && combiner!=COMBINER_DECAL))return false;
        const auto blend=state.blend_mode;
        if(!(blend&BLEND_MODE_ABE_BIT) || (blend&(BLEND_MODE_ATE_BIT|BLEND_MODE_DATE_BIT|BLEND_MODE_PABE_BIT|BLEND_MODE_FB_ALPHA_BIT|BLEND_MODE_DTHE_BIT)))
            return false;
        const auto a=mode(blend,BLEND_MODE_A_MODE_OFFSET,BLEND_MODE_A_MODE_BITS),b=mode(blend,BLEND_MODE_B_MODE_OFFSET,BLEND_MODE_B_MODE_BITS);
        const auto c=mode(blend,BLEND_MODE_C_MODE_OFFSET,BLEND_MODE_C_MODE_BITS),d=mode(blend,BLEND_MODE_D_MODE_OFFSET,BLEND_MODE_D_MODE_BITS);
        const std::uint32_t fix=(prim.alpha>>ALPHA_AFIX_OFFSET)&0xFFu;
        const bool opaque=a==BLEND_RGB_SOURCE && b==BLEND_RGB_DEST && c==BLEND_ALPHA_FIX && d==BLEND_RGB_DEST && fix==0x80;
        const bool subtract=a==BLEND_RGB_ZERO && b==BLEND_RGB_SOURCE && c==BLEND_ALPHA_FIX && d==BLEND_RGB_DEST && (blend&BLEND_MODE_COLCLAMP_BIT);
        if(!opaque && !subtract)return false;
        if(prim.fbmsk!=0 && prim.fbmsk!=0xFF000000u)return false;
        if(prim.tex&(TEX_SAMPLE_MAPPING_BIT|TEX_SAMPLE_RESOLVED_BIT|TEX_SAMPLER_MIPMAP_LINEAR_BIT
                     |(((1u<<TEX_MAX_MIP_LEVEL_BITS)-1u)<<TEX_MAX_MIP_LEVEL_OFFSET)))return false;
        if((prim.tex&TEX_PER_SAMPLE_BIT) && !(prim.tex&TEX_REPLACEMENT_BIT))return false;
        const std::uint32_t texture=(prim.tex>>TEX_TEXTURE_INDEX_OFFSET)&((1u<<TEX_TEXTURE_INDEX_BITS)-1u);
        if(texture>=rp.num_textures || rp.textures[texture].info.arrayed)return false;
        const bool clamp_s=(prim.tex&TEX_SAMPLER_CLAMP_S_BIT)!=0,clamp_t=(prim.tex&TEX_SAMPLER_CLAMP_T_BIT)!=0;
        const auto& info=rp.textures[texture].info;
        const auto& tex0=pass.tex0[texture];
        // A coordinate that repeats needs the image to be the whole texture along it (as in glow.cpp).
        if((!clamp_s && (info.bias.x!=0.0f || info.sizes.z*float(1u<<tex0.TW)!=1.0f))
           || (!clamp_t && (info.bias.y!=0.0f || info.sizes.w*float(1u<<tex0.TH)!=1.0f)))return false;

        const bool perspective=(prim.state&(1u<<STATE_BIT_PERSPECTIVE))!=0;
        Batch batch{std::uint32_t(vertices.size()/3),0,texture,
                    (perspective?flagPerspective:0u)|((state.combiner&COMBINER_TCC_BIT)?flagTcc:0u)|(combiner==COMBINER_DECAL?flagDecal:0u)|(subtract?flagSubtract:0u),
                    fix,prim.fbmsk?0x7u:0xFu,clamp_s,clamp_t,(prim.tex&TEX_SAMPLER_MAG_LINEAR_BIT)!=0,
                    (prim.state&(1u<<STATE_BIT_Z_WRITE))!=0,(prim.state&(1u<<STATE_BIT_Z_TEST_GREATER))!=0};

        // The triangle's vertices from the provoking one; a parallelogram (two triangles the renderer joined) adds the
        // fourth corner opposite vertex 0, its attributes on the same plane.
        int first=0;
        const bool parallelogram=(prim.state&(1u<<STATE_BIT_PARALLELOGRAM))!=0;
        if(parallelogram) {
            first=int((prim.state>>STATE_PARALLELOGRAM_PROVOKING_OFFSET)&((1u<<STATE_PARALLELOGRAM_PROVOKING_COUNT)-1u));
            if(first>2)return false;
        }
        const bool gouraud=(prim.state&(1u<<STATE_BIT_IIP))!=0;
        Vertex v[4];
        for(int k=0;k<3;++k) {
            const auto& position=pass.positions[3*n+(k+first)%3];
            const auto& attribute=pass.attributes[3*n+(k+first)%3];
            v[k]={position.pos.x,position.pos.y,std::min(position.z,0xFFFFFFu),gouraud?attribute.rgba:pass.attributes[3*n].rgba,0,0,1,0,0,1};
            if(perspective) {v[k].s=attribute.st.x;v[k].t=attribute.st.y;v[k].q=attribute.q;}
            else {v[k].s=float(attribute.uv.x)/16.0f;v[k].t=float(attribute.uv.y)/16.0f;}
        }
        vertices.insert(vertices.end(),v,v+3);
        if(parallelogram) {
            v[3]={v[1].x+v[2].x-v[0].x,v[1].y+v[2].y-v[0].y,
                  std::uint32_t(std::clamp<std::int64_t>(std::int64_t(v[1].z)+v[2].z-v[0].z,0,0xFFFFFF)),v[0].rgba,
                  v[1].s+v[2].s-v[0].s,v[1].t+v[2].t-v[0].t,v[1].q+v[2].q-v[0].q,0,0,1};
            if(gouraud) {
                std::uint32_t rgba=0;
                for(int k=0;k<4;++k) {
                    const auto ch=[&](const Vertex& x){return int((x.rgba>>(8*k))&0xFF);};
                    rgba|=std::uint32_t(std::clamp(ch(v[1])+ch(v[2])-ch(v[0]),0,255))<<(8*k);
                }
                v[3].rgba=rgba;
            }
            vertices.push_back(v[1]);vertices.push_back(v[3]);vertices.push_back(v[2]);
        }
        const std::uint32_t triangles=parallelogram?2u:1u;
        if(!batches.empty() && batches.back().same(batch))batches.back().count+=triangles;
        else {batch.count=triangles;batches.push_back(batch);}
        const auto& bb=prim.bb;
        if(bb.z<bb.x || bb.w<bb.y)return false;
        lo_x=std::min<int>(lo_x,bb.x);lo_y=std::min<int>(lo_y,bb.y);hi_x=std::max<int>(hi_x,bb.z);hi_y=std::max<int>(hi_y,bb.w);
    }
    if(lo_x<0 || lo_y<0)return false;

    // A piece's base followed by its cel-shading layer at the same positions (paraLLEl-GS takes both alike: in the CPU
    // fight every such pair matched, 2026-11-02) is one draw: where the base's triangle passes the depth test the
    // layer's passes too (the same depth, greater or equal), and the layer only subtracts from what the base wrote.
    // Half the draws, and none of them changes the pipeline. Differs from two draws only where two triangles of one
    // piece overlap at exactly equal depth.
    struct Draw {const Batch* base;const Batch* cel;};
    std::vector<Draw> draws;draws.reserve(batches.size());
    for(std::size_t b=0;b<batches.size();++b) {
        const auto& x=batches[b];
        if(b+1<batches.size()) {
            const auto& y=batches[b+1];
            bool layered=!(x.flags&flagSubtract) && (y.flags&flagSubtract) && (y.flags&flagDecal) && y.write_mask==0x7u && !x.greater && !y.greater
                && x.count==y.count;
            for(std::uint32_t k=0;layered && k<x.count*3;++k) {
                const auto& u=vertices[x.first*3+k];const auto& v=vertices[y.first*3+k];
                layered=u.x==v.x && u.y==v.y && u.z==v.z;
            }
            if(layered) {
                for(std::uint32_t k=0;k<x.count*3;++k) {
                    auto& u=vertices[x.first*3+k];const auto& v=vertices[y.first*3+k];
                    u.s2=v.s;u.t2=v.t;u.q2=v.q;
                }
                draws.push_back({&x,&y});++b;continue;
            }
        }
        draws.push_back({&x,nullptr});
    }

    auto& device=context.device;
    auto& gs=context.gs;
    const unsigned side=1u<<rate;
    const std::uint32_t width=std::uint32_t(hi_x-lo_x+1),height=std::uint32_t(hi_y-lo_y+1);
    if(images.device!=&device || images.width<width*side || images.height<height*side) {
        images.device=&device;images.width=std::max(images.width,width*side);images.height=std::max(images.height,height*side);
        auto info=Vulkan::ImageCreateInfo::render_target(images.width,images.height,VK_FORMAT_R8G8B8A8_UNORM);
        info.usage|=VK_IMAGE_USAGE_SAMPLED_BIT;info.initial_layout=VK_IMAGE_LAYOUT_UNDEFINED;
        images.colour=device.create_image(info);
        info=Vulkan::ImageCreateInfo::render_target(images.width,images.height,VK_FORMAT_D32_SFLOAT);
        info.usage|=VK_IMAGE_USAGE_SAMPLED_BIT;info.initial_layout=VK_IMAGE_LAYOUT_UNDEFINED;
        images.depth=device.create_image(info);
        if(!images.colour || !images.depth)return false;
    }
    // The programs once per renderer's thread. Buffers and images are made under the graphics device's lock, which
    // stalls the other renderer's thread: the vertices go into the command buffer's own memory instead.
    if(programs.device!=&device) {
        programs.device=&device;
        programs.load=graphicsProgram(device,fighters_load_vert_spv,fighters_load_frag_spv,0,true,false,sizeof(MemoryPush));
        programs.draw=graphicsProgram(device,fighters_vert_spv,fighters_frag_spv,0x1F,false,true,sizeof(DrawPush));
        programs.store=nativeComputeProgram(device,fighters_store_spv,1,sizeof(MemoryPush),0,2);
    }
    auto* load=programs.load;auto* draw=programs.draw;auto* store=programs.store;

    MemoryPush memory{};
    memory.area[0]=std::uint32_t(lo_x);memory.area[1]=std::uint32_t(lo_y);memory.area[2]=side;memory.area[3]=pass.super_sample_reference?1u:0u;
    memory.buffers[0]=frame.FBP*32u;memory.buffers[1]=frame.FBW;memory.buffers[2]=zbuf.ZBP*32u;memory.buffers[3]=frame.FBW;
    memory.memory[0]=gs.get_vram_size()/4u;memory.memory[1]=width;memory.memory[2]=height;
    gs.record_native_work([&](Vulkan::CommandBuffer& cmd) {
        auto& colour=*images.colour;auto& depth=*images.depth;
        // GS memory as earlier work left it, read by the load pass's fragments.
        cmd.barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
        layout(cmd,colour,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,0,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
        layout(cmd,depth,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,0,
               VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
        Vulkan::RenderPassInfo info;
        info.color_attachments[0]=&colour.get_view();info.num_color_attachments=1;info.depth_stencil=&depth.get_view();
        info.store_attachments=1;info.op_flags=Vulkan::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
        info.render_area={{0,0},{width*side,height*side}};
        cmd.begin_render_pass(info);
        cmd.set_opaque_state();
        cmd.set_cull_mode(VK_CULL_MODE_NONE);
        cmd.set_viewport({0,0,float(width*side),float(height*side),0,1});
        cmd.set_scissor({{0,0},{width*side,height*side}});
        // The samples from GS memory: colour and depth as the earlier work left them.
        cmd.set_program(load);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_depth_test(true,true);cmd.set_depth_compare(VK_COMPARE_OP_ALWAYS);
        cmd.push_constants(&memory,0,sizeof memory);
        cmd.draw(3);
        // The triangles, batch by batch in the draws' order.
        cmd.set_program(draw);
        std::memcpy(cmd.allocate_vertex_data(0,vertices.size()*sizeof(Vertex),sizeof(Vertex)),vertices.data(),vertices.size()*sizeof(Vertex));
        cmd.set_vertex_attrib(0,0,VK_FORMAT_R32G32_SINT,offsetof(Vertex,x));
        cmd.set_vertex_attrib(1,0,VK_FORMAT_R32_UINT,offsetof(Vertex,z));
        cmd.set_vertex_attrib(2,0,VK_FORMAT_R8G8B8A8_UINT,offsetof(Vertex,rgba));
        cmd.set_vertex_attrib(3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,s));
        cmd.set_vertex_attrib(4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,s2));
        cmd.set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        const auto layer=[&](const Batch& batch,unsigned n,DrawPush& push) {
            const auto& texture=rp.textures[batch.texture];
            const auto& tex0=pass.tex0[batch.texture];
            const auto& region=texture.info.region;
            const float scale[4]={texture.info.sizes.z,texture.info.sizes.w,texture.info.bias.x,texture.info.bias.y};
            const float clamp[4]={region.x,region.y,region.z,region.w};
            std::memcpy(push.scale[n],scale,sizeof scale);std::memcpy(push.region[n],clamp,sizeof clamp);
            push.sizes[2*n]=float(1u<<tex0.TW);push.sizes[2*n+1]=float(1u<<tex0.TH);
            push.mode[2*n]=batch.flags|(batch.clamp_s?flagClampS:0u)|(batch.clamp_t?flagClampT:0u);push.mode[2*n+1]=batch.fix;
            const bool clamped=batch.clamp_s || batch.clamp_t;
            cmd.set_texture(0,n,*texture.view,batch.linear?(clamped?Vulkan::StockSampler::LinearClamp:Vulkan::StockSampler::LinearWrap)
                                                            :(clamped?Vulkan::StockSampler::NearestClamp:Vulkan::StockSampler::NearestWrap));
        };
        for(const auto& draw:draws) {
            const auto& batch=*draw.base;
            DrawPush push{{float(lo_x),float(lo_y),float(side),0},{float(width*side),float(height*side),0,0},{},{},{},{}};
            layer(batch,0,push);
            layer(draw.cel?*draw.cel:batch,1,push); // Bound either way: the layout has both.
            if(draw.cel)push.mode[0]|=flagLayered;
            cmd.push_constants(&push,0,sizeof push);
            cmd.set_depth_test(true,batch.depth_write);
            cmd.set_depth_compare(batch.greater?VK_COMPARE_OP_GREATER:VK_COMPARE_OP_GREATER_OR_EQUAL);
            cmd.set_color_write_mask(batch.write_mask);
            if(batch.flags&flagSubtract) {
                cmd.set_blend_enable(true);
                cmd.set_blend_factors(VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ONE);
                cmd.set_blend_op(VK_BLEND_OP_REVERSE_SUBTRACT);
            } else cmd.set_blend_enable(false);
            cmd.draw(batch.count*3,1,batch.first*3);
        }
        cmd.end_render_pass();
        cmd.set_opaque_state();
        // The samples back into GS memory.
        layout(cmd,colour,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        layout(cmd,depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        cmd.barrier(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        cmd.set_program(store);
        cmd.set_storage_buffer(0,0,gs.get_vram_buffer());
        cmd.set_texture(0,1,colour.get_view(),Vulkan::StockSampler::NearestClamp);
        cmd.set_texture(0,2,depth.get_view(),Vulkan::StockSampler::NearestClamp);
        cmd.push_constants(&memory,0,sizeof memory);
        cmd.dispatch((width+7)/8,(height+7)/8,1);
    });
    return true;
}
