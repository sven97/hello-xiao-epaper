#pragma once
#include <Arduino.h>

// "timer" (hourly refresh), "btn-info"/"btn-new-pic"/"btn-pin" (which
// function's button ended the sleep), or "power-on" (cold start: power
// switch, USB plug, RESET, or a fresh flash).
const char *wakeReason();

// Persist what the info page needs on non-fetch wakes. Must run on EVERY
// successful fetch — otherwise the page shows stale data for a photo it
// doesn't describe.
void recordFetchMetadata();

// Full-screen status page — BTN_INFO toggles between this and the photo.
// Draws into the sprite only; the caller calls epaper.update().
void drawInfoScreen(int32_t vbatMv, int32_t deltaMv, bool haveDelta);
