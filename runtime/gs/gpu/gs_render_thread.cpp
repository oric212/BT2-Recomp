#include "gs/gpu/gs_render_thread.h"
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace {
constexpr std::size_t header_bytes=8;
enum Kind : std::uint32_t {Packet,Transfer,Task};
}

GsRenderThread::GsRenderThread(Consumer consumer,std::function<void()> on_start):consumer_(std::move(consumer)) {
    filling_.bytes.reserve(chunk_bytes);
    thread_=std::thread([this,on_start=std::move(on_start)] {
        id_=std::this_thread::get_id();
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
            changed_.wait(guard,[this]{return !ready_.empty() || stop_;});
            if(ready_.empty())return; // Stopped with nothing left.
            chunk=std::move(ready_.front());ready_.pop_front();busy_=true;
        }
        const auto start=std::chrono::steady_clock::now();
        std::exception_ptr failure;
        try {
            const auto& bytes=chunk.bytes;
            for(std::size_t at=0;at<bytes.size();) {
                std::uint32_t header[2];std::memcpy(header,bytes.data()+at,header_bytes);at+=header_bytes;
                if(header[1]==Task) {chunk.tasks.at(header[0])();continue;}
                consumer_(std::span<const std::uint8_t>(bytes.data()+at,header[0]),header[1]==Transfer);
                at+=header[0];
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
