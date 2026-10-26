#include "gs/gpu/gpu_gs_backend.h"
#include "gs/gpu/hd_texture_image.h"
#include "gs/gpu/gpu_window_presenter.h"
#include "gs/textures/gs_texture_names.h"
#include "host/display_frame.h"
#include "context.hpp"
#include "device.hpp"
#include "gs_interface.hpp"
#include "thread_id.hpp"
#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_map>
#include <span>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
constexpr std::size_t frame_contexts=8; // The device's: how far the GPU may be behind before a frame change waits.
}

struct GpuGsBackend::State {
    Vulkan::ContextHandle context=Util::make_handle<Vulkan::Context>();
    Vulkan::DeviceHandle device=Util::make_handle<Vulkan::Device>();
    ParallelGS::GSInterface gs;
    std::shared_ptr<HdTexturePack> pack; // After the GS: destroyed first. Shared with a second renderer on the device.
    bool high_resolution=false,timestamps=false;unsigned scanouts=0;
    Vulkan::ImageHandle picture; // The last scanout.
    const GpuGsBackend* shown_from=nullptr; // The renderer whose picture the window shows (null: this one).
    std::unique_ptr<GpuWindowPresenter> presenter; // Destroyed first: it waits for the device.
    struct Kept {Vulkan::BufferHandle vram,clut;};
    std::array<Kept,kept_slots> kept; // keepState's slots.
    // Drawn palettes (gs/textures/gs_texture_names.h): the pages the names watch, copied back after each
    // frame into host memory in turn; a copy the GPU finished goes to every names object of the pack.
    struct PaletteCopy {Vulkan::BufferHandle buffer;Vulkan::Fence fence;std::vector<unsigned> pages;bool pending=false;};
    std::array<PaletteCopy,3> palette_copies;unsigned next_palette_copy=0;
    // The names that take the copies: this renderer's, and a second renderer's, at once when it draws a
    // frame as this one does, or one flip late when it draws each frame after its flip (with the copies
    // this renderer had when it drew it; gs/gpu/in_between_frames.h).
    struct PaletteTarget {GsTextureNames* names;bool late;};
    GsTextureNames* names=nullptr;std::vector<PaletteTarget> palette_targets;
    GpuGsBackend* shared_from=nullptr; // shareHdTextures' primary: its palette target for these names goes with this renderer.
    std::vector<std::pair<unsigned,std::vector<std::uint32_t>>> late_pages; // For the late ones.
    // The lit variant chosen for each drawn palette (texture and format mixed, palette); none: drawn as the original.
    struct PairHash {std::size_t operator()(const std::pair<std::uint64_t,std::uint64_t>& k) const {return std::size_t(k.first^(k.second*0xC2B2AE3D27D4EB4Full));}};
    std::unordered_map<std::pair<std::uint64_t,std::uint64_t>,std::optional<HdTexturePack::Key>,PairHash> variants;
    Vulkan::ImageHandle missing_marker; // BT2_HD_SHOW_MISSING: drawn for a texture without a replacement.
    // The work before each of the last frame changes (awaitFrameChange): one fewer than the frame contexts.
    std::array<Vulkan::Fence,frame_contexts-1> frame_changes;unsigned next_frame_change=0;
    std::mutex frame_change_lock; // The game's thread and the presenter's thread.
    ~State() {presenter.reset();picture.reset();}
};

GpuGsBackend::GpuGsBackend()=default;
GpuGsBackend::~GpuGsBackend() {
    if(state_ && state_->shared_from && state_->names)
        std::erase_if(state_->shared_from->state_->palette_targets,[this](const State::PaletteTarget& t){return t.names==state_->names;});
}

void GpuGsBackend::init(unsigned super_sampling) {
    if(super_sampling!=1 && super_sampling!=2 && super_sampling!=4 && super_sampling!=8 && super_sampling!=16)
        throw std::runtime_error("GPU GS: super-sampling must be 1, 2, 4, 8 or 16");
    if(!Vulkan::Context::init_loader(nullptr))throw std::runtime_error("GPU GS: Vulkan loader unavailable");
    auto state=std::make_unique<State>();
    // 0: the game's thread and the GS thread, one at a time; 1: the second renderer's (gpu_gs_mirror.h); 2: the
    // window presenter's thread (gpu_window_presenter.h); 3: the HD textures' loader (hd_texture_pack.h).
    state->context->set_num_thread_indices(4);
    // The surface extensions let the window presenter make a swapchain on this device.
    const char* instance_extensions[]{"VK_KHR_surface","VK_KHR_win32_surface"};
    const char* device_extensions[]{"VK_KHR_swapchain"};
    if(!state->context->init_instance_and_device(instance_extensions,2,device_extensions,1,Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT))
        throw std::runtime_error("GPU GS: no usable Vulkan device");
    state->device->set_context(*state->context);
    state->device->init_frame_contexts(frame_contexts);
    ParallelGS::GSOptions options;options.super_sampling=ParallelGS::SuperSampling(super_sampling);
    state->high_resolution=super_sampling>=4;
    if(!state->gs.init(state->device.get(),options))throw std::runtime_error("GPU GS: paraLLEl-GS initialization failed");
    // Its flushes only submit: the device changes its frame at a flip, a readback or a picture shown
    // (flush, readback, the window presenter), never in the middle of a frame on the GS thread, where it
    // would wait for the second renderer's open work and for the GPU while holding the device.
    state->gs.set_frame_boundaries(false);
    state_=std::move(state);
}

void GpuGsBackend::initShared(GpuGsBackend& primary,unsigned super_sampling) {
    auto state=std::make_unique<State>();
    state->context=primary.state_->context;state->device=primary.state_->device;
    ParallelGS::GSOptions options;options.super_sampling=ParallelGS::SuperSampling(super_sampling);
    state->high_resolution=super_sampling>=4;
    if(!state->gs.init(state->device.get(),options))throw std::runtime_error("GPU GS: paraLLEl-GS initialization failed (second renderer)");
    // Its flushes only submit: the primary moves the shared device to its next frame context, so this
    // renderer can flush while the primary's frame is open.
    state->gs.set_frame_boundaries(false);
    state_=std::move(state);
}

void GpuGsBackend::showPictureOf(const GpuGsBackend* source) {state_->shown_from=source==this?nullptr:source;}

void GpuGsBackend::transfer(std::span<const std::uint8_t> data) {
    if(data.size()%16)throw std::runtime_error("GPU GS: GIF transfer size is not a multiple of 16");
    state_->gs.gif_transfer(3,data.data(),data.size());
}

std::span<const std::uint32_t> GpuGsBackend::readVram() {
    constexpr std::size_t size=4*1024*1024;
    state_->gs.flush();
    const auto* words=static_cast<const std::uint32_t*>(state_->gs.map_vram_read(0,size));
    return {words,size/4};
}

namespace {
// A copy between buffers of equal size, ordered after all earlier work on the queue and before all later.
void copyBuffers(Vulkan::Device& device,std::initializer_list<std::pair<const Vulkan::Buffer*,const Vulkan::Buffer*>> copies) {
    auto cmd=device.request_command_buffer();
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for(const auto& [to,from]:copies) {
        if(to->get_create_info().size!=from->get_create_info().size)throw std::runtime_error("GPU GS: state copy between buffers of different sizes");
        cmd->copy_buffer(*to,*from);
    }
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT);
    device.submit(cmd);
}
}

void GpuGsBackend::keepState(unsigned slot) {
    auto& s=*state_;
    const auto& vram=s.gs.get_vram_buffer();const auto& clut=s.gs.get_clut_buffer();
    auto& k=s.kept.at(slot);
    if(!k.vram) {
        Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::Device;
        info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.size=vram.get_create_info().size;k.vram=s.device->create_buffer(info);
        info.size=clut.get_create_info().size;k.clut=s.device->create_buffer(info);
    }
    copyBuffers(*s.device,{{k.vram.get(),&vram},{k.clut.get(),&clut}});
}

void GpuGsBackend::takeState(const GpuGsBackend& primary,unsigned slot) {
    auto& s=*state_;
    const auto& k=primary.state_->kept.at(slot);
    if(!k.vram)throw std::runtime_error("GPU GS: no kept state in that slot");
    s.gs.flush(); // This renderer's work so far goes first.
    copyBuffers(*s.device,{{&s.gs.get_vram_buffer(),k.vram.get()},{&s.gs.get_clut_buffer(),k.clut.get()}});
    s.gs.invalidate_vram();
}

void GpuGsBackend::readFifo(std::span<std::uint8_t> out) {
    if(out.size()%16)throw std::runtime_error("GPU GS: FIFO read size is not a multiple of 16");
    state_->gs.read_transfer_fifo(out.data(),std::uint32_t(out.size()/16));
}

void GpuGsBackend::flush() {
    state_->gs.flush();++state_->scanouts;
    if(state_->pack) {
        copyDrawnPalettes();
    }
    state_->device->next_frame_context();
}

void GpuGsBackend::copyDrawnPalettes() {
    auto& s=*state_;
    if(!s.names)return;
    auto& device=*s.device;
    for(const auto& [page,words]:s.late_pages)for(const auto& target:s.palette_targets)if(target.late)target.names->takeDrawnPage(page,words.data());
    s.late_pages.clear();
    // Finished copies, oldest first, so the newest is what the names keep.
    for(unsigned k=1;k<=s.palette_copies.size();++k) {
        auto& copy=s.palette_copies[(s.next_palette_copy+k)%s.palette_copies.size()];
        if(!copy.pending || !copy.fence->wait_timeout(0))continue;
        const auto* words=static_cast<const std::uint32_t*>(device.map_host_buffer(*copy.buffer,Vulkan::MEMORY_ACCESS_READ_BIT));
        for(const auto page:copy.pages) {
            const auto* page_words=words+std::size_t(page)*2048;
            bool late=false;
            for(const auto& target:s.palette_targets)if(target.late)late=true;else target.names->takeDrawnPage(page,page_words);
            if(late)s.late_pages.emplace_back(page,std::vector<std::uint32_t>(page_words,page_words+2048));
        }
        device.unmap_host_buffer(*copy.buffer,Vulkan::MEMORY_ACCESS_READ_BIT);
        copy.pending=false;copy.fence.reset();
    }
    auto pages=s.names->drawnPalettePages();
    if(pages.empty())return;
    auto& copy=s.palette_copies[s.next_palette_copy];
    if(copy.pending)return; // The GPU is three frames behind: this frame's palettes are skipped.
    s.next_palette_copy=(s.next_palette_copy+1)%unsigned(s.palette_copies.size());
    const auto& vram=s.gs.get_vram_buffer(); // Its first 4 MB is GS memory as the host lays it out.
    if(!copy.buffer) {
        Vulkan::BufferCreateInfo info{};info.domain=Vulkan::BufferDomain::CachedHost;
        info.size=4*1024*1024;info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        copy.buffer=device.create_buffer(info);
    }
    std::vector<VkBufferCopy> regions;regions.reserve(pages.size());
    for(const auto page:pages)regions.push_back({VkDeviceSize(page)*8192,VkDeviceSize(page)*8192,8192});
    auto cmd=device.request_command_buffer();
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    cmd->copy_buffer(*copy.buffer,vram,regions.data(),regions.size());
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    device.submit(cmd,&copy.fence);
    copy.pages=std::move(pages);copy.pending=true;
}

void GpuGsBackend::privileged(unsigned index,std::uint64_t value) {
    if(index>14)throw std::runtime_error("GPU GS: privileged register index out of range");
    state_->gs.get_priv_register_state().qwords_lo[index*2]=value; // Same order as the PS2 register block.
}

void GpuGsBackend::submit() {state_->gs.flush();}

void GpuGsBackend::awaitFrameChange() {
    auto& s=*state_;
    std::lock_guard guard(s.frame_change_lock);
    auto& fence=s.frame_changes[s.next_frame_change];
    if(fence) {fence->wait();fence.reset();}
    s.device->submit_empty(Vulkan::CommandBuffer::Type::Generic,&fence);
    s.next_frame_change=(s.next_frame_change+1)%unsigned(s.frame_changes.size());
}

void GpuGsBackend::registerRenderThread(unsigned index) {Util::register_thread_index(index);}

DisplayFrame GpuGsBackend::scanout(unsigned field) {
    vsync(field);
    return readback();
}

std::pair<unsigned,unsigned> GpuGsBackend::vsync(unsigned field) {
    ParallelGS::VSyncInfo info{};
    info.phase=field&1;
    // Read by a copy (readback) or a blit (the window presenter).
    info.dst_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    info.dst_stage=VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    info.dst_access=VK_ACCESS_2_TRANSFER_READ_BIT;
    info.force_progressive=true;info.anti_blur=true;info.high_resolution_scanout=state_->high_resolution;
    const auto result=state_->gs.vsync(info);
    state_->picture=result.image;
    if(!result.image)return {0,0};
    return {result.image->get_width(),result.image->get_height()};
}

DisplayFrame GpuGsBackend::readback() {
    DisplayFrame frame;
    if(!state_->picture)return frame;
    submit(); // Between flips (the in-game menu opens there); the next frame context below waits for it.
    auto& device=*state_->device;
    const auto& image=*state_->picture;
    frame.width=image.get_width();frame.height=image.get_height();
    Vulkan::BufferCreateInfo buffer_info{};
    buffer_info.domain=Vulkan::BufferDomain::CachedHost;
    buffer_info.size=VkDeviceSize(frame.width)*frame.height*4;
    buffer_info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const auto buffer=device.create_buffer(buffer_info);
    auto cmd=device.request_command_buffer();
    cmd->copy_image_to_buffer(*buffer,image,0,{0,0,0},{frame.width,frame.height,1},0,0,{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1});
    cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    Vulkan::Fence fence;
    device.submit(cmd,&fence);
    fence->wait();
    const auto* rgba=static_cast<const std::uint32_t*>(device.map_host_buffer(*buffer,Vulkan::MEMORY_ACCESS_READ_BIT));
    frame.bgra.resize(std::size_t(frame.width)*frame.height);
    for(std::size_t i=0;i<frame.bgra.size();++i) { // RGBA8 to the window's BGRA, opaque.
        const auto p=rgba[i];
        frame.bgra[i]=0xFF000000u|((p&0xFF)<<16)|(p&0xFF00)|((p>>16)&0xFF);
    }
    device.unmap_host_buffer(*buffer,Vulkan::MEMORY_ACCESS_READ_BIT);
    device.next_frame_context();
    return frame;
}

WindowPresenter& GpuGsBackend::windowPresenter(std::function<void(const std::function<void()>&)> around_frame) {
    if(!state_->presenter)
        state_->presenter=std::make_unique<GpuWindowPresenter>(state_->context,state_->device,[this]{
            return state_->shown_from?state_->shown_from->state_->picture:state_->picture;
        },std::move(around_frame));
    return *state_->presenter;
}

void GpuGsBackend::closeWindowPresenter() {state_->presenter.reset();}

void GpuGsBackend::enableHdTextures(const std::filesystem::path& folder,std::uint64_t budget_bytes,GsTextureNames& names,bool playstation_buttons) {
    if(!budget_bytes) {
        // The largest device-local heap's room left (VK_EXT_memory_budget), halved: the rest for the renderer and others.
        auto& device=*state_->device;
        Vulkan::HeapBudget heaps[VK_MAX_MEMORY_HEAPS]{};device.get_memory_budget(heaps);
        const auto& properties=device.get_memory_properties();
        std::uint64_t room=0;
        for(std::uint32_t h=0;h<properties.memoryHeapCount;++h)
            if((properties.memoryHeaps[h].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) && heaps[h].budget_size>heaps[h].device_usage)
                room=std::max<std::uint64_t>(room,heaps[h].budget_size-heaps[h].device_usage);
        budget_bytes=std::clamp<std::uint64_t>(room/2,256ull<<20,8ull<<30);
    }
    state_->pack=std::make_shared<HdTexturePack>(*state_->device,folder,budget_bytes,playstation_buttons);
    replaceTextures(names);
}

void GpuGsBackend::shareHdTextures(const GpuGsBackend& primary,GsTextureNames& names,bool late_palettes) {
    if(!primary.state_->pack)return;
    state_->pack=primary.state_->pack;
    primary.state_->palette_targets.push_back({&names,late_palettes}); // Drawn palettes from the primary's copies.
    state_->shared_from=const_cast<GpuGsBackend*>(&primary);
    if(primary.state_->names)names.shareHashesWith(*primary.state_->names); // It names the same textures.
    replaceTextures(names);
}

void GpuGsBackend::replaceTextures(GsTextureNames& names) {
    if(!state_->names) {state_->names=&names;state_->palette_targets.push_back({&names,false});}
    state_->gs.texture_replacement=[this,&names](const ParallelGS::TextureDescriptor& desc)->Vulkan::ImageHandle {
        const auto name=names.name(desc.tex0.bits,desc.texa.bits);
        if(!name)return {};
        // As older PCSX2 versions named it (the used palette entries), then as newer ones do (the whole palette).
        const auto key=[&](std::uint64_t palette){return HdTexturePack::Key{name->texture,palette,name->format,name->has_palette};};
        bool loading=false;
        const auto look=[&](std::uint64_t palette){const auto found=state_->pack->find(key(palette));loading=loading || found.loading;return found.image;};
        const auto* image=name->has_palette?look(name->palette_used):nullptr;
        if(!image)image=look(name->palette);
        // A palette the game drew (the fighters' lit bodies) without a file of its own: the closest lit
        // variant (step 9), chosen once per palette.
        if(!image && !loading && name->has_palette && name->palette_drawn) {
            const auto at=std::make_pair(name->texture^(std::uint64_t(name->format)*0x9E3779B97F4A7C15ull),name->palette);
            auto chosen=state_->variants.find(at);
            if(chosen==state_->variants.end()) {
                if(const auto colours=names.colours(desc.tex0.bits,desc.texa.bits)) {
                    const auto choice=state_->pack->closest(name->texture,name->format,*colours);
                    if(choice.loading)loading=true;
                    else {
                        if(state_->variants.size()>=4096)state_->variants.clear();
                        chosen=state_->variants.emplace(at,choice.key).first;
                    }
                }
            }
            if(chosen!=state_->variants.end() && chosen->second)image=look(chosen->second->palette);
        }
        if(!image && loading)state_->gs.texture_replacement_pending=true; // The original is not kept meanwhile.
        if(!image && !loading && state_->missing_marker)return state_->missing_marker;
        return image?image->handle:Vulkan::ImageHandle{};
    };
    // BT2_HD_SHOW_MISSING=1 (a diagnostic): a texture the pack has no replacement for is drawn magenta, so a
    // picture shows which areas are still the original's.
    if(std::getenv("BT2_HD_SHOW_MISSING") && !state_->missing_marker) {
        const std::uint8_t magenta[8]{0x1F,0xF8,0x1F,0xF8,0,0,0,0}; // One BC1 block, both colours 0xF81F.
        const Vulkan::ImageInitialData initial{magenta,0,0};
        state_->missing_marker=state_->device->create_image(Vulkan::ImageCreateInfo::immutable_2d_image(4,4,VK_FORMAT_BC1_RGBA_UNORM_BLOCK),&initial);
    }
}

void GpuGsBackend::enableTimestamps() {
    ParallelGS::DebugMode mode;mode.timestamps=true;
    state_->gs.set_debug_mode(mode);state_->timestamps=true;
}

std::string GpuGsBackend::timestampSummary() const {
    if(!state_ || !state_->timestamps || !state_->scanouts)return {};
    static constexpr const char* names[]{"host to VRAM","VRAM copies","palettes","texture uploads","triangle setup","binning","shading","readback","scanout"};
    double total=0;std::string parts;
    for(int n=0;n<int(ParallelGS::TimestampType::Count);++n) {
        const double ms=state_->gs.get_accumulated_timestamps(ParallelGS::TimestampType(n))*1000/state_->scanouts;total+=ms;
        char text[64];std::snprintf(text,sizeof text,"%s%s %.3f",parts.empty()?"":", ",names[n],ms);parts+=text;
    }
    char head[96];std::snprintf(head,sizeof head,"GPU time per frame (%u frames): %.3f ms: ",state_->scanouts,total);
    return head+parts;
}

HdTexturePack* GpuGsBackend::hdTexturePack() {return state_?state_->pack.get():nullptr;}

std::string GpuGsBackend::hdTextureSummary() const {
    if(!state_ || !state_->pack)return {};
    const auto& p=*state_->pack;
    return "HD textures: "+std::to_string(p.names())+" in the pack ("+std::to_string(p.duplicates)+" duplicate names), "
        +std::to_string(p.found)+" draws replaced, "+std::to_string(p.missing)+" without a replacement; "+std::to_string(p.variants_chosen)+" drawn palettes by a lit variant, "
        +std::to_string(p.variants_too_far)+" with none near enough, "+std::to_string(p.read_ahead)+" variants read ahead; "+std::to_string(p.loaded)+" loaded, "
        +std::to_string(p.late)+" not ready at their first draw, "+std::to_string(p.evicted)+" dropped for the budget, "+std::to_string(p.refused)+" files refused; peak "+std::to_string(p.peak_bytes>>20)+" MB of a budget of "+std::to_string(p.budget()>>20)+" MB";
}
