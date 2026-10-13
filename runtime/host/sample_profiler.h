// A sampling profiler for the runner's own thread (diagnostic, BT2_SAMPLE_PROFILE=<path>): a helper
// thread stops the sampled thread about a thousand times per second, notes where it is executing
// and lets it go on. At the end it writes one line per place: the module's file name, the offset in
// it and the number of samples. `tools/perf/profile_report.py` turns the offsets into function
// names with the linker's map file. Only the place of execution is noted, not the callers.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>

class SampleProfiler {
public:
    // Samples the calling thread from now until `stop` or destruction.
    explicit SampleProfiler(std::string path):path_(std::move(path)) {
        DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&target_,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,0);
        sampler_=std::thread([this]{run();});
    }
    SampleProfiler(const SampleProfiler&)=delete;
    SampleProfiler& operator=(const SampleProfiler&)=delete;
    ~SampleProfiler() {
        stop_=true;sampler_.join();CloseHandle(target_);
        // Group by module: the offset of a sample is relative to the module that contains it.
        std::map<std::pair<std::string,std::uint64_t>,unsigned> places;
        for(const auto& [address,count]:samples_) {
            HMODULE module=nullptr;char name[MAX_PATH]="?";
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(address),&module) && module) {
                char full[MAX_PATH];const auto length=GetModuleFileNameA(module,full,MAX_PATH);
                const std::string file(full,length);const auto slash=file.find_last_of("\\/");
                places[{slash==std::string::npos?file:file.substr(slash+1),address-reinterpret_cast<std::uint64_t>(module)}]+=count;
            } else places[{name,address}]+=count;
        }
        std::ofstream out(path_);
        out<<"samples "<<total_<<"\n";
        for(const auto& [place,count]:places)out<<place.first<<' '<<std::hex<<place.second<<std::dec<<' '<<count<<"\n";
    }

private:
    void run() {
        while(!stop_) {
            Sleep(1);
            if(SuspendThread(target_)==DWORD(-1))continue;
            CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
            if(GetThreadContext(target_,&context)) {++samples_[context.Rip];++total_;}
            ResumeThread(target_);
        }
    }
    std::string path_;HANDLE target_=nullptr;
    std::unordered_map<std::uint64_t,unsigned> samples_;unsigned total_=0;
    std::atomic<bool> stop_{false};std::thread sampler_;
};
#endif
