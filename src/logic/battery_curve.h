#pragma once
// Rough Li-ion state-of-charge from resting voltage. Pure logic: host-testable.

inline int batteryPercentFromMv(int mv) {
    struct Point { short mv; unsigned char pct; };
    static const Point CURVE[] = {
        {4200, 100}, {4100, 94}, {4000, 85}, {3900, 74}, {3800, 62},
        {3700, 48},  {3600, 29}, {3500, 13}, {3400, 6},  {3300, 3},
        {3200, 1},   {3000, 0},
    };
    const int N = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv >= CURVE[0].mv) return 100;
    if (mv <= CURVE[N - 1].mv) return 0;
    for (int i = 1; i < N; i++) {
        if (mv >= CURVE[i].mv) {
            return CURVE[i].pct +
                   (int)(mv - CURVE[i].mv) *
                       (CURVE[i - 1].pct - CURVE[i].pct) /
                       (CURVE[i - 1].mv - CURVE[i].mv);
        }
    }
    return 0;
}

enum class BatteryLevel : uint8_t { Low, Medium, High };

// Thresholds match common phone/appliance conventions: red when
// critically low, yellow as a mid-range warning, green otherwise.
inline BatteryLevel batteryLevelBucket(int pct) {
    if (pct <= 15) return BatteryLevel::Low;
    if (pct <= 40) return BatteryLevel::Medium;
    return BatteryLevel::High;
}
