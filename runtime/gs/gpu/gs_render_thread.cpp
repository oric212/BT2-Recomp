#include "gs/gpu/gs_render_thread.h"
#include "host/sample_profiler.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr std::size_t header_bytes=8;
enum Kind : std::uint32_t {Packet,Transfer,Task};
}

GsRenderThread::GsRenderThread(Consumer consumer,std::function<void()> on_start):consumer_(std::move(consumer)) {
    filling_.bytes.reserve(chunk_bytes);
    thread_=std::thread([this,on_start=std::move(on_start)] {
        id_=std::this_thread::get_id();
#ifdef _WIN32
        // BT2_SAMPLE_GS_THREADS=<prefix> (diagnostic): each GS thread samples itself into <prefix>-<n>.txt,
        // n counting the threads in their order of creation (tools/perf/profile_report.py reads them).
        static std::atomic<unsigned> created{0};
        std::unique_ptr<SampleProfiler> profiler;
        if(const char* prefix=std::getenv("BT2_SAMPLE_GS_THREADS"))
            profiler=std::make_unique<SampleProfiler>(std::string(prefix)+"-"+std::to_string(created++)+".txt");
#endif
        if(on_start)on_start();
        run();
    });
}

GsRenderThread::~GsRenderThread() {
    try {if(!filling_.bytes.empty())hand();} catch(...) {} // A failure was reported at a drain or ends the run anyway.
    {std::lock_guard guard(lock_);stop_=true;}
    changed_.notify_all();
    thread_.join();
}

void GsRenderThread::push(std::span<const std::uint8_t> data,bool transfer) {
    if(data.size()>0xFFFFFFFFu)throw std::runtime_error("GS thread: data too large");
    auto& bytes=filling_.bytes;
    if(!bytes.empty() && bytes.size()+header_bytes+data.size()>chunk_bytes)hand();
    const std::uint32_t header[2]{std::uint32_t(data.size()),transfer?Transfer:Packet};
    const auto at=filling_.bytes.size();
    filling_.bytes.resize(at+header_bytes+data.size());
    std::memcpy(filling_.bytes.data()+at,header,header_bytes);
    if(!data.empty())std::memcpy(filling_.bytes.data()+at+header_bytes,data.data(),data.size());
    if(filling_.bytes.size()>=chunk_bytes)hand();
}

void GsRenderThread::pushTask(std::function<void()> task) {
    const std::uint32_t header[2]{std::uint32_t(filling_.tasks.size()),Task};
    filling_.tasks.push_back(std::move(task));
    const auto at=filling_.bytes.size();
    filling_.bytes.resize(at+header_bytes);
    std::memcpy(filling_.bytes.data()+at,header,header_bytes);
    hand();
}

void GsRenderThread::hand() {
    Chunk next;
    {
        std::lock_guard guard(lock_);
        if(failure_)std::rethrow_exception(failure_);
        ready_.push_back(std::move(filling_));
        if(!spare_.empty()) {next=std::move(spare_.back());spare_.pop_back();}
    }
    changed_.notify_all();
    next.bytes.clear();next.bytes.reserve(chunk_bytes);next.tasks.clear();
    filling_=std::move(next);
}

void GsRenderThread::drain() {
    if(!filling_.bytes.empty())hand();
    std::unique_lock guard(lock_);
    changed_.wait(guard,[this]{return (ready_.empty() && !busy_) || failure_;});
    if(failure_)std::rethrow_exception(failure_);
}

void GsRenderThread::whilePaused(const std::function<void()>& at_pause,const std::function<void()>& paused) {
    if(onThread()) {at_pause();paused();return;}
    std::lock_guard one(pause_requests_);
    std::unique_lock guard(lock_);
    at_pause_=&at_pause;pause_wanted_=true;
    changed_.notify_all();
    changed_.wait(guard,[this]{return paused_;});
    struct Resume {
        GsRenderThread& t;std::unique_lock<std::mutex>& guard;
        ~Resume() {
            guard.lock();
            t.at_pause_=nullptr;t.pause_wanted_=false;
            t.changed_.notify_all();
            t.changed_.wait(guard,[this]{return !t.paused_;}); // Answered before the next request.
        }
    };
    guard.unlock();
    Resume resume{*this,guard};
    paused();
}

void GsRenderThread::pauseHere(std::unique_lock<std::mutex>& guard) {
    const bool was_busy=busy_;
    busy_=true; // Drains wait until it goes on.
    const auto* at_pause=at_pause_;
    guard.unlock();
    std::exception_ptr failure;
    try {(*at_pause)();} catch(...) {failure=std::current_exception();}
    guard.lock();
    if(failure && !failure_)failure_=failure;
    paused_=true;
    changed_.notify_all();
    changed_.wait(guard,[this]{return at_pause_==nullptr;});
    paused_=false;busy_=was_busy;
    changed_.notify_all();
}

double GsRenderThread::takeBusySeconds() {
    std::lock_guard guard(lock_);
    const double seconds=double(busy_ns_)*1e-9;busy_ns_=0;
    return seconds;
}

void GsRenderThread::run() {
    for(;;) {
        Chunk chunk;
        {
            std::unique_lock guard(lock_);
            changed_.wait(guard,[this]{return !ready_.empty() || stop_ || at_pause_;});
            if(at_pause_) {pauseHere(guard);continue;}
            if(ready_.empty())return; // Stopped with nothing left.
            chunk=std::move(ready_.front());ready_.pop_front();busy_=true;
        }
        const auto start=std::chrono::steady_clock::now();
        std::exception_ptr failure;
        try {
            const auto& bytes=chunk.bytes;
            for(std::size_t at=0;at<bytes.size();) {
                std::uint32_t header[2];std::memcpy(header,bytes.data()+at,header_bytes);at+=header_bytes;
                if(header[1]==Task)chunk.tasks.at(header[0])();
                else {
                    consumer_(std::span<const std::uint8_t>(bytes.data()+at,header[0]),header[1]==Transfer);
                    at+=header[0];
                }
                if(pause_wanted_) {std::unique_lock guard(lock_);if(at_pause_)pauseHere(guard);}
            }
        } catch(...) {failure=std::current_exception();}
        const auto took=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        {
            std::lock_guard guard(lock_);
            busy_ns_+=took;busy_=false;
            if(failure && !failure_)failure_=failure;
            if(failure_)ready_.clear(); // Nothing after a failure is taken.
            chunk.bytes.clear();chunk.tasks.clear();spare_.push_back(std::move(chunk));
        }
        changed_.notify_all();
    }
}
