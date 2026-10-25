// The process asks Windows not to slow it down (power throttling, EcoQoS): Windows 11 may lower the
// clock for a process whose window is not in front or whose threads wake in short bursts, which a
// paced game's threads do every frame. Games opt out; nothing else changes.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

inline void stopPowerThrottling() {
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version=PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask=PROCESS_POWER_THROTTLING_EXECUTION_SPEED|PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    state.StateMask=0; // Both off: full speed, and the timer resolution asked for is kept.
    SetProcessInformation(GetCurrentProcess(),ProcessPowerThrottling,&state,sizeof state);
}
#else
inline void stopPowerThrottling() {}
#endif
