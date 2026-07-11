#pragma once
// Quiet-hours window math. Pure logic: host-testable, no Arduino deps.
// Times are seconds-of-local-day [0, 86400); the window is [start, end)
// in whole hours and may wrap midnight. start == end means "no window".
#include <cstdint>

constexpr int SECONDS_PER_DAY = 86400;

inline bool inQuietWindow(int nowSod, int startHour, int endHour) {
    if (startHour == endHour) return false;
    const int s = startHour * 3600, e = endHour * 3600;
    if (s < e) return nowSod >= s && nowSod < e;
    return nowSod >= s || nowSod < e; // wraps midnight
}

inline uint32_t secondsUntilQuietEnd(int nowSod, int startHour, int endHour) {
    if (!inQuietWindow(nowSod, startHour, endHour)) return 0;
    const int e = endHour * 3600;
    return (uint32_t)((e - nowSod + SECONDS_PER_DAY) % SECONDS_PER_DAY);
}

// If the nominal wake (now + sleepSecs) lands inside the window, sleep
// through to the window end instead. sleepSecs is at most 24 h.
inline uint32_t quietAdjustedSleep(int nowSod, uint32_t sleepSecs,
                                   bool enabled, int startHour, int endHour) {
    if (!enabled) return sleepSecs;
    const int wakeSod = (int)((nowSod + sleepSecs) % SECONDS_PER_DAY);
    const uint32_t extra = secondsUntilQuietEnd(wakeSod, startHour, endHour);
    return sleepSecs + extra;
}
