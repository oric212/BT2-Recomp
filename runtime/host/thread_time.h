// The calling thread's processor time, to tell a thread's work from its waits and from the time it was ready but
// not running (a thread below normal priority on a busy processor). Windows charges it by clock ticks, so only
// sums over many items mean something.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline double threadProcessorSeconds() {
    FILETIME created,exited,kernel,user;
    if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))return 0;
    const auto ticks=[](const FILETIME& t){return (unsigned long long)t.dwHighDateTime<<32|t.dwLowDateTime;};
    return double(ticks(kernel)+ticks(user))*1e-7;
}
#else
#include <ctime>
inline double threadProcessorSeconds() {
    timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);
    return double(t.tv_sec)+double(t.tv_nsec)*1e-9;
}
#endif
