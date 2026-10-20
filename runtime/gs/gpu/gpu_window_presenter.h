// The viewer window's pictures on the GPU (host/window_presenter.h): a swapchain on the window
// (Granite's WSI on the renderer's own device) into which the renderer's scanout, or a picture drawn
// by the processor, is copied scaled (docs/plans/ee/PERFORMANCE_PLAN.md, presenting without readback).
// Built only with BT2_GPU_GS; used by gpu_gs_backend.cpp.
#pragma once
#include "host/window_presenter.h"
#include "context.hpp"
#include "device.hpp"
#include <functional>
#include <memory>

class GpuWindowPresenter final : public WindowPresenter {
public:
    // `renderer_picture`: the renderer's last scanout (null when there is none), in
    // TRANSFER_SRC_OPTIMAL and visible to transfer reads.
    GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<const Vulkan::Image*()> renderer_picture);
    ~GpuWindowPresenter() override;
    void show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
              const DisplayFrame& status,Rect status_at) override;
private:
    struct State;
    std::unique_ptr<State> state_;
    Vulkan::ContextHandle context_;Vulkan::DeviceHandle device_;
    std::function<const Vulkan::Image*()> renderer_picture_;
};
