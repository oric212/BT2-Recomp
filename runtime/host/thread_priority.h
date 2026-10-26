// Threads that do optional work (the in-between pictures of 60 fps battles, the HD textures' loader) give
// way to the game's own threads when the processor is busy (docs/plans/ee/PERFORMANCE_PLAN.md, smaller
// processors): on a 4-core processor every thread of the port competes, and the game's speed must not
// depend on the enhancements. Called first on such a thread.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline void giveWayToTheGame() {SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);}
#else
inline void giveWayToTheGame() {}
#endif
