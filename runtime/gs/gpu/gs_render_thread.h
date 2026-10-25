// The GS thread (docs/plans/ee/PERFORMANCE_PLAN.md, graphics on its own thread): the GS data the game's
// thread accepts is copied in order into a queue and taken, on a thread of its own, by a consumer (the
// GPU renderer, with the texture names in step; gs/gpu/gpu_gs_mirror.h). A thread also runs tasks in
// order with its data (the in-between pictures of 60 fps battles have a thread that runs only tasks).
// The game's thread waits for the queue only where it needs the renderer
// itself (drain): a flip, a scanout or readback, a local->host transfer. A failure of the consumer or of
// a task is rethrown on the game's thread when it next hands over a chunk or drains, and nothing after
// it is taken.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

class GsRenderThread {
public:
    // `transfer`: a GIF transfer (GsCommandSink::transferAll), otherwise a startup packet (consume).
    using Consumer=std::function<void(std::span<const std::uint8_t> data,bool transfer)>;
    // `on_start` runs first on the new thread (the renderer's thread index for its command pools).
    explicit GsRenderThread(Consumer consumer,std::function<void()> on_start={});
    ~GsRenderThread(); // Takes what is queued, then stops; a failure is not rethrown.
    // Copies the data into the queue; the thread takes it when a chunk is full or at the next drain.
    void push(std::span<const std::uint8_t> data,bool transfer);
    // Runs `task` on the thread after everything pushed so far; it starts without waiting for a drain.
    void pushTask(std::function<void()> task);
    // Returns when everything pushed so far has been taken.
    void drain();
    bool onThread() const {return std::this_thread::get_id()==id_.load();} // Called from a task or the consumer.
    // The thread's working time since the last call, in seconds.
    double takeBusySeconds();
    static constexpr std::size_t chunk_bytes=64*1024;
private:
    // Items one after another: a header (the data's size or the task's index; the kind), then the data.
    struct Chunk {std::vector<std::uint8_t> bytes;std::vector<std::function<void()>> tasks;};
    void hand(); // The chunk being filled goes to the thread.
    void run();
    Consumer consumer_;
    Chunk filling_; // The game's thread's.
    std::mutex lock_;std::condition_variable changed_;
    std::deque<Chunk> ready_;   // Filled chunks in order.
    std::vector<Chunk> spare_;  // Taken chunks, reused.
    bool busy_=false,stop_=false;std::exception_ptr failure_;
    std::int64_t busy_ns_=0;
    std::atomic<std::thread::id> id_{}; // The thread's, set first on it.
    std::thread thread_; // Last: it starts in the constructor and uses the members above.
};
