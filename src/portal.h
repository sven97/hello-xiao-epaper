#pragma once
#include <Arduino.h>

// Settings portal, live while the status page is on the panel.
// Lifecycle: connectWifi() -> startPortal() -> runPortal() -> caller runs
// a fetch cycle and sleeps. Every exit path behaves the same; the enum is
// for logging (ForgetWifi's next fetch cycle reopens provisioning).
enum class PortalResult { KeyExit, Timeout, Saved, ForgetWifi };

bool startPortal();                                   // mDNS + HTTP :80
PortalResult runPortal(uint32_t inactivityTimeoutMs); // blocking loop
String portalUrl();                                   // "http://<name>.local"
