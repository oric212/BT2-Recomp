// The viewer window's pictures on the GPU (host/window_presenter.h): a swapchain on the window
// (Granite's WSI on the renderer's own device) into which the renderer's scanout, or a picture drawn
// by the processor, is copied scaled (docs/plans/ee/PERFORMANCE_PLAN.md, presenting without readback).
// The pictures are presented on a thread of their own: in a window the driver's present waits until the
// desktop compositor has taken the last picture (about 4 ms on average with 60 pictures a second,
// measured 2026-10-26), and Granite holds the device's lock meanwhile, so the caller only hands the
// picture over and goes on. A picture handed over while the one before still waits replaces it; a picture
// scheduled ahead (presentAt, showPicture) is prepared first and waits on the thread for its time, so only
// its present comes at that time; the next one queues behind it.
// A picture is presented only once its copy is done on the GPU: a VRR display shows a picture when its GPU
// work is done, not when it is presented, and a picture of a frame just drawn is done about 8.5 ms after
// one drawn ahead (a 60 fps fight at 16x with HD, 2026-11-03), so pictures presented a field apart appeared
// 10 and 24 ms apart. A picture shown on its own is presented a steady time after it was handed over, as
// late as nearly all recent ones were done (usualCompletion); one scheduled a field after the last
// (showPicture) a field after that one as presented.
// Built only with BT2_GPU_GS; used by gpu_gs_backend.cpp.
#pragma once
#include "host/window_presenter.h"
#include "context.hpp"
#include "device.hpp"
#include <array>
#include <condition_variable>
#include <deque>
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
    // `around_frame` runs the thread's frame (where the device changes its frame) given to it; `before_change` runs
    // in it right before the device's frame change (GpuGsBackend::markFrameChange).
    GpuWindowPresenter(Vulkan::ContextHandle context,Vulkan::DeviceHandle device,std::function<Vulkan::ImageHandle()> renderer_picture,
                       std::function<void(const std::function<void()>&)> around_frame,std::function<void()> before_change);
    ~GpuWindowPresenter() override;
    // Hands the picture over to the thread; rethrows a failure of the thread's.
    void show(void* window,const DisplayFrame* frame,Rect at,unsigned client_width,unsigned client_height,bool smooth,
              const DisplayFrame& status,Rect status_at) override;
    void presentAt(std::chrono::steady_clock::time_point at) override {std::lock_guard guard(lock_);next_at_=at;}
    void freeRun(bool free_run) override {std::lock_guard guard(lock_);free_run_=free_run;}
    // From any thread: `picture` shown with the window's layout as last shown, `after` the picture before it was
    // presented. False before any picture was shown (no layout yet).
    bool showPicture(Vulkan::ImageHandle picture,std::chrono::steady_clock::duration after);
    bool showAgain(const DisplayFrame& status) override;
    static constexpr unsigned thread_index=2; // The thread's command pools (gpu_gs_backend.h).
private:
    struct Request {
        void* window=nullptr;std::optional<DisplayFrame> frame;Vulkan::ImageHandle picture;Rect at;
        unsigned client_width=0,client_height=0;bool smooth=false;DisplayFrame status;Rect status_at;
        long long queued_us=0; // When it was handed over (the steady clock), for the latency log.
        std::chrono::steady_clock::time_point due{}; // When it was handed over, or its presentAt time if later.
        bool free_run=false; // Handed over in a free run (freeRun): presented when done, not counted in usualCompletion.
        std::chrono::steady_clock::time_point present_at{}; // Presented no earlier (presentAt).
        std::optional<std::chrono::steady_clock::duration> after_last; // Presented this long after the last (showPicture).
    };
    // BT2_LATENCY_LOG=<path>: <path>.presents gets each present (its DXGI present count, when it was handed
    // over, when presented and when its copy was done on the GPU) and each present the display has shown
    // since (its count and the refresh's time), in microseconds of the steady clock.
    void logPresent(long long queued_us,std::chrono::steady_clock::time_point done);
    // On the thread: the picture copied into the swapchain and the device's frame context ended; whether
    // it is to be presented (WSI::end_frame, after around_frame). `done`: signalled when the copy is done.
    bool present(const Request& request,Vulkan::Fence& done);
    // How long after handover a picture shown on its own is presented: when 99% of the last 120 such pictures
    // were done on the GPU, plus half a millisecond. `took`: this one's (run).
    std::chrono::steady_clock::duration usualCompletion(std::chrono::steady_clock::duration took);
    void run();
    struct State;
    std::unique_ptr<State> state_;
    Vulkan::ContextHandle context_;Vulkan::DeviceHandle device_;
    std::function<Vulkan::ImageHandle()> renderer_picture_;
    std::function<void(const std::function<void()>&)> around_frame_;
    std::function<void()> before_change_;
    std::mutex lock_;std::condition_variable changed_;
    // Pictures handed over and not yet taken: at most two (a picture and the one scheduled a field after
    // it); a further one replaces the newer.
    std::deque<Request> pending_;bool stop_=false;std::exception_ptr failure_;
    std::optional<std::chrono::steady_clock::time_point> next_at_;bool free_run_=false;
    std::optional<Request> layout_; // The last picture's window, placement and status line (show), for showPicture.
    std::optional<Request> last_shown_; // The last picture presented, for showAgain.
    std::chrono::steady_clock::time_point last_present_{}; // On the thread: when the last picture was presented.
    std::array<std::chrono::steady_clock::duration,120> completion_{};std::size_t completion_next_=0,completion_count_=0; // usualCompletion.
    std::thread thread_; // Last: it starts in the constructor.
};
