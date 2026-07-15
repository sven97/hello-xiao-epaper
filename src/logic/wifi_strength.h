#pragma once
#include <cstdint>
// Wi-Fi signal-quality bucketing from RSSI. Pure logic: host-testable.

enum class WifiStrength : uint8_t { Weak, Fair, Strong };

// Thresholds follow common RSSI quality bands: -60 dBm or better is a
// strong, reliable link; -75 to -60 is usable but marginal; anything
// weaker is a poor signal likely to cause fetch failures/timeouts.
inline WifiStrength wifiStrengthBucket(int rssiDbm) {
    if (rssiDbm >= -60) return WifiStrength::Strong;
    if (rssiDbm >= -75) return WifiStrength::Fair;
    return WifiStrength::Weak;
}

inline const char *wifiStrengthLabel(WifiStrength s) {
    switch (s) {
        case WifiStrength::Strong: return "strong";
        case WifiStrength::Fair:   return "fair";
        default:                   return "weak";
    }
}
