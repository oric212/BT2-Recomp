#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "gs/gpu/gpu_window_presenter.h"
#include "thread_id.hpp"
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
    Vulkan::ImageHandle enlarged; // The picture at whole multiples of its size, kept while the size stays.
    ~State() {enlarged.reset();wsi.reset();} // The swapchain goes before its platform.
};

GpuWindowPresenter::GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<Vulkan::ImageHandle()> renderer_picture,
                                       std::function<void(const std::function<void()>&)> around_frame)
    :state_(std::make_unique<State>()),context_(std::move(context)),device_(std::move(device)),
     renderer_picture_(std::move(renderer_picture)),around_frame_(std::move(around_frame)),thread_([this]{run();}) {}

GpuWindowPresenter::~GpuWindowPresenter() {
    {std::lock_guard guard(lock_);stop_=true;}
    changed_.notify_all();
    thread_.join();
    if(device_)device_->wait_idle();
    state_.reset();
}

void GpuWindowPresenter::show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
                              const DisplayFrame& status,Rect status_at) {
    Request request;
    request.window=window;request.at=at;request.client_width=client_width;request.client_height=client_height;request.smooth=smooth;
    request.status=status;request.status_at=status_at;
    if(frame)request.frame=*frame;else request.picture=renderer_picture_();
    {
        std::lock_guard guard(lock_);
        if(failure_) {auto failure=failure_;failure_=nullptr;std::rethrow_exception(failure);}
        pending_=std::move(request);
    }
    changed_.notify_all();
}

void GpuWindowPresenter::run() {
    Util::register_thread_index(thread_index);
    for(;;) {
        Request request;
        {
            std::unique_lock guard(lock_);
            changed_.wait(guard,[this]{return pending_ || stop_;});
            if(!pending_)return;
            request=std::move(*pending_);pending_.reset();
        }
        try {
            bool presenting=false;
            around_frame_([&]{presenting=present(request);});
            if(presenting && !state_->wsi->end_frame())throw std::runtime_error("GPU presenter: presenting failed");
        }
        catch(...) {std::lock_guard guard(lock_);if(!failure_)failure_=std::current_exception();}
    }
}

bool GpuWindowPresenter::present(const Request& request) {
    void* window=request.window;const Rect at=request.at;const bool smooth=request.smooth;
    const unsigned client_width=request.client_width,client_height=request.client_height;
    const DisplayFrame* frame=request.frame?&*request.frame:nullptr;
    const DisplayFrame& status=request.status;const Rect status_at=request.status_at;
    auto& s=*state_;
    if(s.window!=static_cast<HWND>(window)) { // The first picture (or a new window): its swapchain.
        s.wsi.reset();
        s.window=static_cast<HWND>(window);
        s.platform=std::make_unique<WindowPlatform>(s.window);
        s.platform->size(client_width,client_height);
        s.wsi=std::make_unique<Vulkan::WSI>();
        s.wsi->set_platform(s.platform.get());
        // Our pacer decides when a picture appears. A Vulkan swapchain must not wait for the display's refresh:
        // its present holds the device, so every renderer would wait with it.
        s.wsi->set_present_mode(Vulkan::PresentMode::UnlockedMaybeTear);
        s.wsi->set_backbuffer_format(Vulkan::BackbufferFormat::UNORM); // The picture's values as they are.
        s.wsi->set_extra_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);   // Pictures are copied in.
        s.wsi->set_end_frame_context_by_caller(true); // Ended in around_frame, presented after it (run).
        if(!s.wsi->init_from_existing_context(context_) || !s.wsi->init_device(device_) || !s.wsi->init_surface_swapchain())
            throw std::runtime_error("GPU presenter: no swapchain for the window");
        // On Windows the picture goes out through a DXGI flip-model swapchain (granite_dxgi_copy_target.patch),
        // presented on D3D12's queue without holding the Vulkan device: it is shown at the display's refresh,
        // without tearing.
        if(s.wsi->uses_dxgi())s.wsi->set_present_mode(Vulkan::PresentMode::SyncToVBlank);
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
    else source=request.picture.get();
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
    // A stretch that is not a whole multiple (widescreen, scaling off) is done in two steps: first the
    // largest whole multiple that fits, pixels copied, then the rest filtered. A plain filtered stretch
    // makes one-pixel lines thinner or thicker with where they fall, so thin edges pulse as the
    // picture moves; this way only the lines' edges are smoothed.
    if(source && smooth && at.width>0 && at.height>0) {
        const unsigned kx=std::max(1u,unsigned(at.width)/source->get_width()),ky=std::max(1u,unsigned(at.height)/source->get_height());
        if(kx>1 || ky>1) {
            const unsigned w=source->get_width()*kx,h=source->get_height()*ky;
            if(!s.enlarged || s.enlarged->get_width()!=w || s.enlarged->get_height()!=h) {
                auto info=Vulkan::ImageCreateInfo::render_target(w,h,VK_FORMAT_R8G8B8A8_UNORM);
                info.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;info.initial_layout=VK_IMAGE_LAYOUT_UNDEFINED;
                s.enlarged=device.create_image(info);
            }
            auto& enlarged=*s.enlarged;
            cmd->image_barrier(enlarged,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,0,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
            cmd->blit_image(enlarged,*source,corner(0,0),extent(int(w),int(h)),corner(0,0),
                extent(int(source->get_width()),int(source->get_height())),0,0,0,0,1,VK_FILTER_NEAREST);
            cmd->image_barrier(enlarged,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
            source=&enlarged;
        }
    }
    if(source)blit(*source,at,smooth?VK_FILTER_LINEAR:VK_FILTER_NEAREST);
    if(status_image)blit(*status_image,status_at,VK_FILTER_NEAREST);
    cmd->image_barrier(target,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_NONE,0);
    cmd->swapchain_touch_in_stages(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
    device.submit(cmd);
    device.end_frame_context();
    return true;
}
