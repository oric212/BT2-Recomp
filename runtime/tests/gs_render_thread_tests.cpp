// The GS thread's queue (runtime/gs/gpu/gs_render_thread.h) on synthetic data (no GPU).
#include "gs/gpu/gs_render_thread.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"GS thread check failed: "<<what<<'\n';std::exit(1);}}
// Item n: n % 7 + 1 quadwords (and every 50th a few chunks long), each byte (n + offset) mod 256.
std::vector<std::uint8_t> item(unsigned n) {
    const std::size_t size=n%50==49?3*GsRenderThread::chunk_bytes+16:16*(n%7+1);
    std::vector<std::uint8_t> data(size);
    for(std::size_t i=0;i<size;++i)data[i]=std::uint8_t(n+i);
    return data;
}
struct Seen {std::vector<std::uint8_t> data;bool transfer;};
}

int main() {
    // Order, contents and kinds, across chunks, with drains in between; the consumer runs on its own
    // thread, after on_start.
    {
        std::vector<Seen> seen;std::thread::id consumer_thread;bool started_first=false;std::atomic<bool> started{false};
        GsRenderThread thread([&](std::span<const std::uint8_t> data,bool transfer) {
            if(seen.empty()) {consumer_thread=std::this_thread::get_id();started_first=started;}
            seen.push_back({{data.begin(),data.end()},transfer});
        },[&]{started=true;});
        constexpr unsigned count=3000;
        for(unsigned n=0;n<count;++n) {
            const auto data=item(n);thread.push(data,n%3!=0);
            if(n==1000)thread.drain(),check(seen.size()==1001,"a drain returns when everything pushed was taken");
        }
        thread.drain();
        check(seen.size()==count,"every item taken once");
        for(unsigned n=0;n<count;++n) {
            check(seen[n].data==item(n),"items taken in order with their bytes");
            check(seen[n].transfer==(n%3!=0),"each item's kind kept");
        }
        check(consumer_thread!=std::this_thread::get_id() && started_first,"the consumer runs on the thread, after on_start");
        check(thread.takeBusySeconds()>=0 && thread.takeBusySeconds()==0,"busy time is taken once");
        thread.drain(); // Nothing pushed: returns at once.
    }
    // Tasks run on the thread in order with the data, without waiting for a drain.
    {
        std::vector<int> order;std::mutex seen_lock;std::condition_variable seen_task;bool task_ran=false,on_thread=false,off_thread=true;
        GsRenderThread* self=nullptr;
        GsRenderThread thread([&](std::span<const std::uint8_t> data,bool) {std::lock_guard g(seen_lock);order.push_back(int(data.size()));});
        self=&thread;off_thread=thread.onThread();
        std::vector<std::uint8_t> data(16);
        thread.push(data,true);
        thread.pushTask([&]{std::lock_guard g(seen_lock);order.push_back(-1);on_thread=self->onThread();task_ran=true;seen_task.notify_all();});
        {
            std::unique_lock g(seen_lock);
            check(seen_task.wait_for(g,std::chrono::seconds(10),[&]{return task_ran;}),"a task runs without a drain");
        }
        thread.push(std::vector<std::uint8_t>(32),false);thread.drain();
        check(order==std::vector<int>({16,-1,32}),"tasks run in order with the data");
        check(on_thread && !off_thread,"onThread is true on the thread only");
        thread.pushTask([]{throw std::runtime_error("task failed");});
        bool thrown=false;
        try {thread.drain();} catch(const std::runtime_error& e) {thrown=std::string(e.what())=="task failed";}
        check(thrown,"a task's failure is rethrown at the drain");
    }
    // An empty item is passed on as one.
    {
        unsigned empties=0;
        GsRenderThread thread([&](std::span<const std::uint8_t> data,bool) {if(data.empty())++empties;});
        thread.push({},true);thread.drain();
        check(empties==1,"an empty item is taken");
    }
    // A failure is rethrown at the drain, and what follows it is not taken.
    {
        unsigned taken=0;
        GsRenderThread thread([&](std::span<const std::uint8_t> data,bool) {
            ++taken;
            if(data.size()==48)throw std::runtime_error("refused by the renderer");
        });
        std::vector<std::uint8_t> small(16),bad(48);
        thread.push(small,true);thread.push(bad,true);thread.push(small,true);
        bool thrown=false;
        try {thread.drain();} catch(const std::runtime_error& e) {thrown=std::string(e.what())=="refused by the renderer";}
        check(thrown,"the consumer's failure is rethrown at the drain");
        check(taken==2,"nothing after the failure is taken");
        thrown=false;
        try {thread.push(small,true);thread.drain();} catch(const std::runtime_error&) {thrown=true;}
        check(thrown && taken==2,"the failure stays: later data is refused too");
    }
    // The destructor takes what is still queued.
    {
        unsigned taken=0;
        {
            GsRenderThread thread([&](std::span<const std::uint8_t>,bool) {++taken;});
            std::vector<std::uint8_t> data(32);
            for(unsigned n=0;n<5000;++n)thread.push(data,false);
        }
        check(taken==5000,"everything pushed is taken before the thread stops");
    }
    std::cout<<"GS thread checks passed\n";
    return 0;
}
