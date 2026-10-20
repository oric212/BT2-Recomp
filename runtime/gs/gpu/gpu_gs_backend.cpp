#include "gs/gpu/gpu_gs_backend.h"
#include "gs/gpu/hd_texture_image.h"
#include "gs/textures/gs_texture_names.h"
#include "host/display_frame.h"
#include "context.hpp"
#include "device.hpp"
#include "gs_interface.hpp"
#include <stdexcept>

struct GpuGsBackend::State {
    Vulkan::Context context;
    Vulkan::Device device;
    ParallelGS::GSInterface gs;
    std::unique_ptr<HdTexturePack> pack; // After the GS: destroyed first.
    bool high_resolution=false;
};

GpuGsBackend::GpuGsBackend()=default;
GpuGsBackend::~GpuGsBackend()=default;

void GpuGsBackend::init(unsigned super_sampling) {
    if(super_sampling!=1 && super_sampling!=2 && super_sampling!=4 && super_sampling!=8 && super_sampling!=16)
        throw std::runtime_error("GPU GS: super-sampling must be 1, 2, 4, 8 or 16");
    if(!Vulkan::Context::init_loader(nullptr))throw std::runtime_error("GPU GS: Vulkan loader unavailable");
    auto state=std::make_unique<State>();
    state->context.set_num_thread_indices(1);
    if(!state->context.init_instance_and_device(nullptr,0,nullptr,0,Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT))
        throw std::runtime_error("GPU GS: no usable Vulkan device");
    state->device.set_context(state->context);
    state->device.init_frame_contexts(4);
    ParallelGS::GSOptions options;options.super_sampling=ParallelGS::SuperSampling(super_sampling);
    state->high_resolution=super_sampling>=4;
    if(!state->gs.init(&state->device,options))throw std::runtime_error("GPU GS: paraLLEl-GS initialization failed");
    state_=std::move(state);
}

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

void GpuGsBackend::readFifo(std::span<std::uint8_t> out) {
    if(out.size()%16)throw std::runtime_error("GPU GS: FIFO read size is not a multiple of 16");
    state_->gs.read_transfer_fifo(out.data(),std::uint32_t(out.size()/16));
}

void GpuGsBackend::flush() {
    state_->gs.flush();
    if(state_->pack)state_->pack->loadPending(); // Between frames, after the renderer's work is submitted.
    state_->device.next_frame_context();
}

void GpuGsBackend::privileged(unsigned index,std::uint64_t value) {
    if(index>14)throw std::runtime_error("GPU GS: privileged register index out of range");
    state_->gs.get_priv_register_state().qwords_lo[index*2]=value; // Same order as the PS2 register block.
}

DisplayFrame GpuGsBackend::scanout(unsigned field) {
    ParallelGS::VSyncInfo info{};
    info.phase=field&1;
    info.dst_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    info.dst_stage=VK_PIPELINE_STAGE_2_COPY_BIT;
    info.dst_access=VK_ACCESS_2_TRANSFER_READ_BIT;
    info.force_progressive=true;info.anti_blur=true;info.high_resolution_scanout=state_->high_resolution;
    const auto result=state_->gs.vsync(info);
    DisplayFrame frame;
    if(!result.image)return frame;
    auto& device=state_->device;
    const auto& image=*result.image;
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

void GpuGsBackend::enableHdTextures(const std::filesystem::path& folder,std::uint64_t budget_bytes,GsTextureNames& names) {
    state_->pack=std::make_unique<HdTexturePack>(state_->device,folder,budget_bytes);
    state_->gs.texture_replacement=[this,&names](const ParallelGS::TextureDescriptor& desc)->Vulkan::ImageHandle {
        const auto name=names.name(desc.tex0.bits,desc.texa.bits);
        if(!name)return {};
        const auto* image=state_->pack->find(name->text());
        return image?image->handle:Vulkan::ImageHandle{};
    };
}

std::string GpuGsBackend::hdTextureSummary() const {
    if(!state_ || !state_->pack)return {};
    const auto& p=*state_->pack;
    return "HD textures: "+std::to_string(p.names())+" in the pack ("+std::to_string(p.duplicates)+" duplicate names), "
        +std::to_string(p.found)+" draws replaced, "+std::to_string(p.missing)+" without a replacement; "+std::to_string(p.loaded)+" loaded, "
        +std::to_string(p.evicted)+" dropped for the budget, "+std::to_string(p.refused)+" files refused; peak "+std::to_string(p.peak_bytes>>20)+" MB";
}
