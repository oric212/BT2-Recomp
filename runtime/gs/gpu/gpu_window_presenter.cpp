#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "gs/gpu/gpu_window_presenter.h"
#include "wsi.hpp"
#include <vulkan/vulkan_win32.h>
#include <algorithm>
#include <stdexcept>

namespace {
// Granite's WSI asks its platform for the surface and its size; here that is the viewer's window.
class WindowPlatform final : public Vulkan::WSIPlatform {
public:
    explicit WindowPlatform(HWND window):window_(window) {}
    VkSurfaceKHR create_surface(VkInstance instance,VkPhysicalDevice) override {
        const auto create=reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(instance,"vkCreateWin32SurfaceKHR"));
        if(!create)return VK_NULL_HANDLE;
        VkWin32SurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        info.hinstance=GetModuleHandleA(nullptr);info.hwnd=window_;
        VkSurfaceKHR surface=VK_NULL_HANDLE;
        return create(instance,&info,nullptr,&surface)==VK_SUCCESS?surface:VK_NULL_HANDLE;
    }
    std::vector<const char*> get_instance_extensions() override {return {"VK_KHR_surface","VK_KHR_win32_surface"};}
    uint32_t get_surface_width() override {return width_;}
    uint32_t get_surface_height() override {return height_;}
    bool alive(Vulkan::WSI&) override {return true;}
    void poll_input() override {}
    void poll_input_async(Granite::InputTrackerHandler*) override {}
    uintptr_t get_native_window() override {return reinterpret_cast<uintptr_t>(window_);}
    // The client area's size; a change makes the WSI build the swapchain again at the next frame.
    void size(unsigned width,unsigned height) {
        if(width==width_ && height==height_)return;
        width_=width;height_=height;resize=true;
    }
private:
    HWND window_;unsigned width_=1,height_=1;
};

VkOffset3D corner(int x,int y) {return {x,y,0};}
VkOffset3D extent(int width,int height) {return {width,height,1};}
}

struct GpuWindowPresenter::State {
    HWND window=nullptr;
    std::unique_ptr<WindowPlatform> platform;
    std::unique_ptr<Vulkan::WSI> wsi;
    ~State() {wsi.reset();} // The swapchain goes before its platform.
};

GpuWindowPresenter::GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<const Vulkan::Image*()> renderer_picture)
    :state_(std::make_unique<State>()),context_(std::move(context)),device_(std::move(device)),renderer_picture_(std::move(renderer_picture)) {}

GpuWindowPresenter::~GpuWindowPresenter() {
    if(device_)device_->wait_idle();
    state_.reset();
}

void GpuWindowPresenter::show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
                              const DisplayFrame& status,Rect status_at) {
    auto& s=*state_;
    if(s.window!=static_cast<HWND>(window)) { // The first picture (or a new window): its swapchain.
        s.wsi.reset();
        s.window=static_cast<HWND>(window);
        s.platform=std::make_unique<WindowPlatform>(s.window);
        s.platform->size(client_width,client_height);
        s.wsi=std::make_unique<Vulkan::WSI>();
        s.wsi->set_platform(s.platform.get());
        // Our pacer decides when a picture appears: no waiting for the display's refresh here.
        s.wsi->set_present_mode(Vulkan::PresentMode::UnlockedMaybeTear);
        s.wsi->set_backbuffer_format(Vulkan::BackbufferFormat::UNORM); // The picture's values as they are.
        s.wsi->set_extra_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);   // Pictures are copied in.
        if(!s.wsi->init_from_existing_context(context_) || !s.wsi->init_device(device_) || !s.wsi->init_surface_swapchain())
            throw std::runtime_error("GPU presenter: no swapchain for the window");
    }
    s.platform->size(client_width,client_height);
    auto& device=*device_;
    if(!s.wsi->begin_frame())throw std::runtime_error("GPU presenter: the swapchain was lost");
    // The source: a picture of the processor's uploaded for this frame, or the renderer's scanout.
    Vulkan::ImageHandle uploaded,status_image;
    const auto upload=[&](const DisplayFrame& picture) {
        auto info=Vulkan::ImageCreateInfo::immutable_2d_image(picture.width,picture.height,VK_FORMAT_B8G8R8A8_UNORM);
        info.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;info.initial_layout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        const Vulkan::ImageInitialData data{picture.bgra.data(),0,0};
        return device.create_image(info,&data);
    };
    const Vulkan::Image* source=nullptr;
    if(frame) {if(frame->width)uploaded=upload(*frame);source=uploaded.get();}
    else source=renderer_picture_();
    if(status.width)status_image=upload(status);
    auto& target=device.get_swapchain_view().get_image();
    const int width=int(target.get_width()),height=int(target.get_height());
    auto cmd=device.request_command_buffer();
    cmd->image_barrier(target,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,0,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkClearValue black{};
    cmd->clear_image(target,black);
    cmd->barrier(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    // A rectangle of the source copied into one of the target, both cut to the target's area.
    const auto blit=[&](const Vulkan::Image& from,Rect to,VkFilter filter) {
        const int x0=std::max(to.x,0),y0=std::max(to.y,0),x1=std::min(to.x+to.width,width),y1=std::min(to.y+to.height,height);
        if(x0>=x1 || y0>=y1 || to.width<=0 || to.height<=0)return;
        const double sx=double(from.get_width())/to.width,sy=double(from.get_height())/to.height;
        const int u0=int((x0-to.x)*sx+0.5),v0=int((y0-to.y)*sy+0.5),u1=int((x1-to.x)*sx+0.5),v1=int((y1-to.y)*sy+0.5);
        if(u0>=u1 || v0>=v1)return;
        cmd->blit_image(target,from,corner(x0,y0),extent(x1-x0,y1-y0),corner(u0,v0),extent(u1-u0,v1-v0),0,0,0,0,1,filter);
    };
    if(source)blit(*source,at,smooth?VK_FILTER_LINEAR:VK_FILTER_NEAREST);
    if(status_image)blit(*status_image,status_at,VK_FILTER_NEAREST);
    cmd->image_barrier(target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_NONE,0);
    cmd->swapchain_touch_in_stages(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
    device.submit(cmd);
    if(!s.wsi->end_frame())throw std::runtime_error("GPU presenter: presenting failed");
}
