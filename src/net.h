#pragma once
#include <Arduino.h>

// Connect with saved credentials, or open the captive portal (AP_NAME,
// portal at 192.168.4.1) on first boot / after forget. Blocks until
// connected or the 5-minute portal timeout.
bool connectWifi();

// Fetch a random photo (weserv baseline re-encode of picsum) into the
// sprite, dithered. Returns false after drawing an error screen.
bool fetchImage();

// Detect the UTC offset from the network's public IP, then NTP-sync.
// Returns false if NTP never synced (offset may still be cached-stale).
bool syncClock();

// Set the TZ environment to a fixed UTC offset (seconds east of UTC).
// Needed at every boot: the TZ env does not survive deep sleep.
void applyUtcOffset(long offsetSec);
