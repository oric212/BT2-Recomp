// The viewer window's pictures on the GPU (host/window_presenter.h): a swapchain on the window
// (Granite's WSI on the renderer's own device) into which the renderer's scanout, or a picture drawn
// by the processor, is copied scaled (docs/plans/ee/PERFORMANCE_PLAN.md, presenting without readback).
// The pictures are presented on a thread of their own: in a window the driver's present waits until the
// desktop compositor has taken the last picture (about 4 ms on average with 60 pictures a second,
// measured 2026-10-26), and Granite holds the device's lock meanwhile, so the caller only hands the
// picture over and goes on. A picture handed over while the one before still waits replaces it.
// Built only with BT2_GPU_GS; used by gpu_gs_backend.cpp.
#pragma once
#include "host/window_presenter.h"
#include "context.hpp"
#include "device.hpp"
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

class GpuWindowPresenter final : public WindowPresenter {
public:
    // `renderer_picture`: the renderer's picture to show (empty when there is none), in
    // TRANSFER_SRC_OPTIMAL and visible to transfer reads; taken when a picture is handed over.
    // `around_frame` runs the thread's frame (where the device changes its frame) given to it.
    GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<Vulkan::ImageHandle()> renderer_picture,
                       std::function<void(const std::function<void()>&)> around_frame);
    ~GpuWindowPresenter() override;
    // Hands the picture over to the thread; rethrows a failure of the thread's.
    void show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
              const DisplayFrame& status,Rect status_at) override;
    static constexpr unsigned thread_index=2; // The thread's command pools (gpu_gs_backend.h).
private:
    struct Request {
        void* window=nullptr;std::optional<DisplayFrame> frame;Vulkan::ImageHandle picture;Rect at;
        unsigned client_width=0,client_height=0;bool smooth=false;DisplayFrame status;Rect status_at;
    };
    // On the thread: the picture copied into the swapchain and the device's frame context ended; whether
    // it is to be presented (WSI::end_frame, after around_frame).
    bool present(const Request& request);
    void run();
    struct State;
    std::unique_ptr<State> state_;
    Vulkan::ContextHandle context_;Vulkan::DeviceHandle device_;
    std::function<Vulkan::ImageHandle()> renderer_picture_;
    std::function<void(const std::function<void()>&)> around_frame_;
    std::mutex lock_;std::condition_variable changed_;
    std::optional<Request> pending_;bool stop_=false;std::exception_ptr failure_;
    std::thread thread_; // Last: it starts in the constructor.
};
