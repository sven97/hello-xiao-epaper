# EE02 Playground Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A learning firmware for the Seeed XIAO ePaper Display Board EE02 (XIAO ESP32-S3 Plus + 13.3" Spectra 6 panel) built in four on-hardware milestones: display hello-world, buttons, Wi-Fi image fetch, battery + deep sleep.

**Architecture:** Single PlatformIO project, Arduino framework, one `src/main.cpp` that grows milestone by milestone. Seeed_GFX drives the panel (selected entirely via two build flags — no library file edits). Verification is on-hardware: every task ends with something observable on the panel or serial monitor.

**Tech Stack:** PlatformIO CLI, Arduino-ESP32, Seeed_GFX (Seeed-Studio/Seeed_GFX), JPEGDecoder (bodmer), ESP32 deep-sleep APIs.

## Global Constraints

- Work in `/Users/sven/Developer/ee02-playground` (git repo already exists with the spec committed).
- Panel is 1200×1600 native portrait, six colors: `TFT_BLACK`, `TFT_WHITE`, `TFT_RED`, `TFT_YELLOW`, `TFT_GREEN`, `TFT_BLUE`. A full Spectra 6 refresh takes ~20–30 seconds — this is normal, not a hang.
- Display selection is exactly these two compile-time defines (from Seeed's config tool): `BOARD_SCREEN_COMBO=510` and `USE_XIAO_EPAPER_DISPLAY_BOARD_EE02`. They go in `platformio.ini` `build_flags`, never edited into library files.
- PSRAM is required (framebuffer > internal RAM). The `seeed_xiao_esp32s3` board definition already sets `-DBOARD_HAS_PSRAM` and `memory_type: qio_opi` — do not remove it.
- Board pin facts (verified against the EE02 v1.0 schematic and working community firmware):
  - Buttons (active-LOW, external 10 kΩ pull-ups to 3V3): GPIO2 = "previous", GPIO3 = "refresh", GPIO5 = "next". All three are RTC-capable (valid EXT1 wake sources).
  - Battery: voltage on GPIO1 (A0) through a ÷2 divider; GPIO6 must be driven HIGH to enable the divider, LOW otherwise to save power.
  - ePaper power enable: GPIO43 (the library drives it during use; drive it LOW before deep sleep).
  - XIAO user LED: GPIO21, active-LOW.
- Serial is USB-CDC at 115200 (`ARDUINO_USB_CDC_ON_BOOT=1` comes from the board definition). After flashing, reopen the monitor with `pio device monitor`.
- Wi-Fi is 2.4 GHz only (ESP32-S3 has no 5 GHz).
- All builds/flashes via PlatformIO CLI: `pio run`, `pio run -t upload`, `pio device monitor`. Never the Arduino IDE.
- `src/secrets.h` is git-ignored; `src/secrets.h.example` is committed.
- No unit-test scaffolding. The test cycle per task is: build → flash → observe on panel/serial → commit.

**Troubleshooting facts (apply in any task):**
- If no serial port appears: hold the BOOT button while plugging in USB, then flash; press RESET after.
- Find the port with `pio device list` (expect `/dev/cu.usbmodem…`).
- If the panel stays blank but the build/flash succeeded, the two build-flag defines are wrong or missing — re-check `platformio.ini` before touching code.
- If the build fails inside Seeed_GFX with Arduino-core API errors, pin the newer core by replacing the `platform =` line with `platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip` and rebuild.

---

### Task 1: Toolchain + project scaffold (serial hello)

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `src/main.cpp`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: a building, flashing PlatformIO project; `platformio.ini` env name `ee02`; `src/main.cpp` with `setup()`/`loop()` that later tasks replace wholesale.

- [ ] **Step 1: Install PlatformIO CLI (skip if `pio --version` already works)**

```bash
brew install platformio
pio --version
```

Expected: a version string like `PlatformIO Core, version 6.x.x`.

- [ ] **Step 2: Write `platformio.ini`**

```ini
[env:ee02]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200

; XIAO ESP32-S3 Plus has 16 MB flash (stock board def assumes 8 MB)
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv

lib_deps =
    https://github.com/Seeed-Studio/Seeed_GFX.git

build_flags =
    -DBOARD_SCREEN_COMBO=510
    -DUSE_XIAO_EPAPER_DISPLAY_BOARD_EE02
```

- [ ] **Step 3: Write `.gitignore`**

```gitignore
.pio/
src/secrets.h
```

- [ ] **Step 4: Write `src/main.cpp` (serial + LED heartbeat)**

```cpp
#include <Arduino.h>

constexpr uint8_t LED_PIN = 21; // XIAO user LED, active-LOW

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    // USB-CDC needs a moment before prints are visible
    delay(2000);
    Serial.println("ee02-playground: task 1 — serial hello");
}

void loop() {
    digitalWrite(LED_PIN, LOW);  // LED on
    delay(500);
    digitalWrite(LED_PIN, HIGH); // LED off
    delay(500);
    Serial.println("heartbeat");
}
```

- [ ] **Step 5: Build**

```bash
cd /Users/sven/Developer/ee02-playground && pio run
```

Expected: first run downloads the ESP32 platform + toolchain (several minutes), then `SUCCESS`. RAM/Flash usage lines printed.

- [ ] **Step 6: Flash and observe**

Plug the EE02 in via USB-C, power switch ON.

```bash
pio run -t upload
pio device monitor
```

Expected: upload ends with `Hash of data verified` / `SUCCESS`; monitor shows `heartbeat` once per second; XIAO's orange user LED blinks. Ctrl-C exits the monitor. If no port: hold BOOT while plugging in, retry upload, press RESET.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini .gitignore src/main.cpp
git commit -m "Task 1: PlatformIO scaffold, serial+LED heartbeat"
```

---

### Task 2: Milestone 1 — hello display (six-color test pattern)

**Files:**
- Modify: `src/main.cpp` (replace entirely)

**Interfaces:**
- Consumes: project scaffold from Task 1.
- Produces: global `EPaper epaper;` object and the six-color array `const uint16_t COLORS[6]` with names `COLOR_NAMES[6]` — Task 3 reuses both; the draw-then-`epaper.update()` idiom all later tasks follow.

- [ ] **Step 1: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h> // Seeed_GFX; provides EPaper when the ePaper combo is selected

EPaper epaper;

const uint16_t COLORS[6] = {TFT_BLACK, TFT_WHITE, TFT_RED,
                            TFT_YELLOW, TFT_GREEN, TFT_BLUE};
const char *COLOR_NAMES[6] = {"BLACK", "WHITE", "RED",
                              "YELLOW", "GREEN", "BLUE"};

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("ee02-playground: task 2 — six-color test pattern");

    epaper.begin();
    Serial.printf("panel: %d x %d\n", epaper.width(), epaper.height());

    epaper.fillScreen(TFT_WHITE);

    // Six vertical bars, 200 px each across the 1200 px width
    for (int i = 0; i < 6; i++) {
        epaper.fillRect(i * 200, 0, 200, 1200, COLORS[i]);
    }

    // Labels under the bars, on white background
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    for (int i = 0; i < 6; i++) {
        epaper.drawString(COLOR_NAMES[i], i * 200 + 20, 1250, 4);
    }
    epaper.drawString("ee02-playground / milestone 1", 20, 1400, 4);

    Serial.println("updating panel (takes ~20-30 s)...");
    epaper.update();
    Serial.println("done");
}

void loop() {}
```

- [ ] **Step 2: Build**

```bash
pio run
```

Expected: `SUCCESS`. If the compiler cannot find `EPaper`, the two `build_flags` defines are missing/typoed — fix `platformio.ini`, not the code.

- [ ] **Step 3: Flash and observe**

```bash
pio run -t upload && pio device monitor
```

Expected: serial prints `panel: 1200 x 1600`, then `updating panel...`; after ~20–30 s of flickering the panel shows six labeled color bars. This step also proves the Plus flash/PSRAM overrides are correct — if the panel renders, both unknowns from the spec are resolved.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "Task 2: six-color test pattern on 13.3\" Spectra 6"
```

---

### Task 3: Milestone 2 — buttons switch demo screens

**Files:**
- Modify: `src/main.cpp` (replace entirely)

**Interfaces:**
- Consumes: `EPaper epaper`, `COLORS`/`COLOR_NAMES` from Task 2.
- Produces: button constants `BTN_PREV=2, BTN_REFRESH=3, BTN_NEXT=5` and the poll/debounce pattern; `void showScreen(int idx)` dispatcher. Task 4 keeps the buttons; Task 5 reuses the pin constants as wake sources.

- [ ] **Step 1: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>

EPaper epaper;

const uint16_t COLORS[6] = {TFT_BLACK, TFT_WHITE, TFT_RED,
                            TFT_YELLOW, TFT_GREEN, TFT_BLUE};
const char *COLOR_NAMES[6] = {"BLACK", "WHITE", "RED",
                              "YELLOW", "GREEN", "BLUE"};

// EE02 user buttons: active-LOW, external pull-ups on board
constexpr uint8_t BTN_PREV = 2;
constexpr uint8_t BTN_REFRESH = 3;
constexpr uint8_t BTN_NEXT = 5;
constexpr uint8_t LED_PIN = 21; // active-LOW

constexpr int SCREEN_COUNT = 3;
int screenIndex = 0;

void drawColorBars() {
    epaper.fillScreen(TFT_WHITE);
    for (int i = 0; i < 6; i++)
        epaper.fillRect(i * 200, 0, 200, 1200, COLORS[i]);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    for (int i = 0; i < 6; i++)
        epaper.drawString(COLOR_NAMES[i], i * 200 + 20, 1250, 4);
    epaper.drawString("screen 1/3: color bars", 20, 1400, 4);
}

void drawInfoScreen() {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("screen 2/3: info", 20, 40, 4);
    epaper.setTextColor(TFT_RED, TFT_WHITE);
    epaper.drawString("XIAO ePaper Display Board EE02", 20, 140, 4);
    epaper.setTextColor(TFT_BLUE, TFT_WHITE);
    epaper.drawString("13.3\" Spectra 6, 1200 x 1600", 20, 220, 4);
    epaper.setTextColor(TFT_GREEN, TFT_WHITE);
    epaper.drawString("buttons: GPIO2 prev / GPIO3 refresh / GPIO5 next",
                      20, 300, 4);
    epaper.setTextColor(TFT_BLACK, TFT_YELLOW);
    epaper.drawString("uptime (ms): " + String(millis()), 20, 380, 4);
}

void drawCheckerboard() {
    epaper.fillScreen(TFT_WHITE);
    for (int y = 0; y < 1600; y += 100)
        for (int x = 0; x < 1200; x += 100)
            epaper.fillRect(x, y, 100, 100,
                            COLORS[((x + y) / 100) % 6]);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString("screen 3/3: checkerboard", 20, 20, 4);
}

void showScreen(int idx) {
    digitalWrite(LED_PIN, LOW); // LED on = busy
    Serial.printf("drawing screen %d (update takes ~20-30 s)...\n", idx + 1);
    switch (idx) {
        case 0: drawColorBars(); break;
        case 1: drawInfoScreen(); break;
        case 2: drawCheckerboard(); break;
    }
    epaper.update();
    digitalWrite(LED_PIN, HIGH); // LED off = ready
    Serial.println("ready — press a button");
}

// Returns true once per physical press (falling edge + debounce)
bool pressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        delay(30); // debounce
        if (digitalRead(pin) == LOW) {
            while (digitalRead(pin) == LOW) delay(10); // wait for release
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("ee02-playground: task 3 — button demo");

    pinMode(BTN_PREV, INPUT);    // external pull-ups on board
    pinMode(BTN_REFRESH, INPUT);
    pinMode(BTN_NEXT, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    epaper.begin();
    showScreen(screenIndex);
}

void loop() {
    if (pressed(BTN_NEXT)) {
        screenIndex = (screenIndex + 1) % SCREEN_COUNT;
        showScreen(screenIndex);
    } else if (pressed(BTN_PREV)) {
        screenIndex = (screenIndex + SCREEN_COUNT - 1) % SCREEN_COUNT;
        showScreen(screenIndex);
    } else if (pressed(BTN_REFRESH)) {
        showScreen(screenIndex);
    }
    delay(10);
}
```

- [ ] **Step 2: Build**

```bash
pio run
```

Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe**

```bash
pio run -t upload && pio device monitor
```

Expected: color bars appear; pressing **next** shows the info screen, **next** again the checkerboard, **prev** goes back, **refresh** redraws the current screen. LED is lit during each ~20–30 s refresh; presses during a refresh are ignored (the update call blocks — that's expected e-ink behavior, note it and move on).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "Task 3: three demo screens switched by EE02 user buttons"
```

---

### Task 4: Milestone 3 — Wi-Fi + HTTP image fetch

**Files:**
- Create: `src/secrets.h.example`
- Create: `src/secrets.h` (git-ignored; real values)
- Modify: `platformio.ini` (add JPEGDecoder)
- Modify: `src/main.cpp` (replace entirely)

**Interfaces:**
- Consumes: `EPaper epaper`, button constants and `pressed()` from Task 3.
- Produces: `bool connectWifi()`, `bool fetchAndShowImage()` — Task 5 calls both. `secrets.h` macros: `WIFI_SSID`, `WIFI_PASS`, `IMAGE_URL` (all `#define` strings).

- [ ] **Step 1: Add JPEGDecoder to `platformio.ini`**

Replace the `lib_deps` block with:

```ini
lib_deps =
    https://github.com/Seeed-Studio/Seeed_GFX.git
    bodmer/JPEGDecoder@^2.0.0
```

- [ ] **Step 2: Write `src/secrets.h.example`**

```cpp
#pragma once
#define WIFI_SSID "your-2.4ghz-ssid"
#define WIFI_PASS "your-password"
// Any URL returning a baseline JPEG sized exactly 1200x1600.
// e.g. the myframe server: http://<nas-ip>:8080/photo/1200/1600
#define IMAGE_URL "http://192.168.1.10:8080/photo/1200/1600"
```

- [ ] **Step 3: Create `src/secrets.h`**

Copy the example and fill in real values (ask the user for SSID/password/NAS IP if not on hand — do not invent them):

```bash
cp src/secrets.h.example src/secrets.h
# then edit src/secrets.h with real values
```

Confirm it is ignored: `git status` must NOT list `src/secrets.h`.

- [ ] **Step 4: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <JPEGDecoder.h>
#include "secrets.h"

EPaper epaper;

constexpr uint8_t BTN_PREV = 2;
constexpr uint8_t BTN_REFRESH = 3;
constexpr uint8_t BTN_NEXT = 5;
constexpr uint8_t LED_PIN = 21; // active-LOW

void showError(const String &msg) {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_RED, TFT_WHITE);
    epaper.drawString("ERROR", 20, 40, 4);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString(msg, 20, 120, 4);
    epaper.update();
}

bool connectWifi() {
    Serial.printf("connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    for (int attempt = 0; attempt < 3; attempt++) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nconnected, IP %s, RSSI %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
        Serial.printf("\nattempt %d failed, retrying\n", attempt + 1);
        WiFi.disconnect(true);
        delay(1000);
    }
    return false;
}

// Decode the JPEG in buf and push it MCU block by MCU block.
// Image must be exactly 1200x1600 so blocks tile without clipping.
void renderJpeg(uint8_t *buf, size_t len) {
    JpegDec.decodeArray(buf, len);
    Serial.printf("jpeg: %d x %d, MCU %d x %d\n", JpegDec.width,
                  JpegDec.height, JpegDec.MCUWidth, JpegDec.MCUHeight);
    while (JpegDec.read()) {
        int x = JpegDec.MCUx * JpegDec.MCUWidth;
        int y = JpegDec.MCUy * JpegDec.MCUHeight;
        epaper.pushImage(x, y, JpegDec.MCUWidth, JpegDec.MCUHeight,
                         JpegDec.pImage);
    }
}

bool fetchAndShowImage() {
    digitalWrite(LED_PIN, LOW);
    HTTPClient http;
    http.begin(IMAGE_URL);
    http.setTimeout(20000);
    Serial.printf("GET %s\n", IMAGE_URL);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("HTTP error: %d\n", code);
        http.end();
        showError("HTTP " + String(code) + " from image server");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    int len = http.getSize();
    Serial.printf("content-length: %d\n", len);
    if (len <= 0) {
        http.end();
        showError("server sent no Content-Length");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    uint8_t *buf = (uint8_t *)ps_malloc(len); // PSRAM
    if (!buf) {
        http.end();
        showError("PSRAM alloc failed");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    size_t got = 0;
    uint32_t lastData = millis();
    while (got < (size_t)len && millis() - lastData < 20000) {
        size_t avail = stream->available();
        if (avail) {
            got += stream->readBytes(buf + got, min(avail, (size_t)len - got));
            lastData = millis();
        } else {
            delay(10);
        }
    }
    http.end();
    Serial.printf("received %u / %d bytes\n", (unsigned)got, len);
    if (got < (size_t)len) {
        free(buf);
        showError("download incomplete");
        digitalWrite(LED_PIN, HIGH);
        return false;
    }

    epaper.fillScreen(TFT_WHITE);
    renderJpeg(buf, got);
    free(buf);
    Serial.println("updating panel (takes ~20-30 s)...");
    epaper.update();
    Serial.println("done — press refresh (GPIO3) for a new image");
    digitalWrite(LED_PIN, HIGH);
    return true;
}

bool pressed(uint8_t pin) {
    if (digitalRead(pin) == LOW) {
        delay(30);
        if (digitalRead(pin) == LOW) {
            while (digitalRead(pin) == LOW) delay(10);
            return true;
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("ee02-playground: task 4 — wifi image fetch");

    pinMode(BTN_PREV, INPUT);
    pinMode(BTN_REFRESH, INPUT);
    pinMode(BTN_NEXT, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    epaper.begin();

    if (!connectWifi()) {
        showError("wifi connect failed: " + String(WIFI_SSID));
        return;
    }
    fetchAndShowImage();
}

void loop() {
    if (pressed(BTN_REFRESH)) {
        if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
            showError("wifi connect failed: " + String(WIFI_SSID));
            return;
        }
        fetchAndShowImage();
    }
    delay(10);
}
```

- [ ] **Step 5: Build**

```bash
pio run
```

Expected: `SUCCESS` (JPEGDecoder downloads on first build).

- [ ] **Step 6: Flash and observe**

Precondition: the image URL must serve a baseline JPEG at exactly 1200×1600 (myframe's `/photo/1200/1600` does).

```bash
pio run -t upload && pio device monitor
```

Expected serial: `connected, IP …`, `GET …`, `content-length: …`, `jpeg: 1200 x 1600 …`, `updating panel…`; the fetched photo appears on the panel (six-color dithered — colors will look posterized, that's the medium, not a bug). Pressing **refresh** fetches a different image.

- [ ] **Step 7: Commit**

```bash
git add platformio.ini src/secrets.h.example src/main.cpp
git commit -m "Task 4: fetch JPEG over wifi and render on panel"
```

---

### Task 5: Milestone 4 — battery reading + deep sleep

**Files:**
- Modify: `src/main.cpp` (replace entirely)

**Interfaces:**
- Consumes: `connectWifi()`, `fetchAndShowImage()` pattern, button/pin constants from Task 4.
- Produces: final firmware shape: boot → fetch → overlay status footer → deep sleep (timer + any-button wake). `float readBatteryVoltage()` and RTC-persistent `bootCount`.

- [ ] **Step 1: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <JPEGDecoder.h>
#include "secrets.h"

EPaper epaper;

constexpr uint8_t BTN_PREV = 2;
constexpr uint8_t BTN_REFRESH = 3;
constexpr uint8_t BTN_NEXT = 5;
constexpr uint64_t BUTTON_WAKE_MASK =
    (1ULL << BTN_PREV) | (1ULL << BTN_REFRESH) | (1ULL << BTN_NEXT);
constexpr uint8_t LED_PIN = 21;          // active-LOW
constexpr uint8_t BATTERY_ADC_PIN = 1;   // A0, via /2 divider
constexpr uint8_t BATTERY_EN_PIN = 6;    // HIGH enables the divider
constexpr uint8_t EPAPER_EN_PIN = 43;    // panel power enable
constexpr uint64_t SLEEP_SECONDS = 60 * 60; // 1 hour

RTC_DATA_ATTR uint32_t bootCount = 0;

float readBatteryVoltage() {
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);
    pinMode(BATTERY_EN_PIN, OUTPUT);
    digitalWrite(BATTERY_EN_PIN, HIGH);
    delay(5);
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
        delay(2);
    }
    digitalWrite(BATTERY_EN_PIN, LOW);
    return (sum / 10 / 4095.0f) * 3.3f * 2.0f; // /2 divider
}

const char *wakeReason() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: return "timer";
        case ESP_SLEEP_WAKEUP_EXT1:  return "button";
        default:                     return "power-on/reset";
    }
}

void showError(const String &msg) {
    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_RED, TFT_WHITE);
    epaper.drawString("ERROR", 20, 40, 4);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString(msg, 20, 120, 4);
    epaper.update();
}

bool connectWifi() {
    Serial.printf("connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    for (int attempt = 0; attempt < 3; attempt++) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nconnected, IP %s, RSSI %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
        Serial.printf("\nattempt %d failed, retrying\n", attempt + 1);
        WiFi.disconnect(true);
        delay(1000);
    }
    return false;
}

void renderJpeg(uint8_t *buf, size_t len) {
    JpegDec.decodeArray(buf, len);
    Serial.printf("jpeg: %d x %d, MCU %d x %d\n", JpegDec.width,
                  JpegDec.height, JpegDec.MCUWidth, JpegDec.MCUHeight);
    while (JpegDec.read()) {
        int x = JpegDec.MCUx * JpegDec.MCUWidth;
        int y = JpegDec.MCUy * JpegDec.MCUHeight;
        epaper.pushImage(x, y, JpegDec.MCUWidth, JpegDec.MCUHeight,
                         JpegDec.pImage);
    }
}

// Fetch the image into the framebuffer (no update() yet — the caller
// overlays the status footer first). Returns false after drawing an
// error screen (already updated) on failure.
bool fetchImage() {
    HTTPClient http;
    http.begin(IMAGE_URL);
    http.setTimeout(20000);
    Serial.printf("GET %s\n", IMAGE_URL);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        showError("HTTP " + String(code) + " from image server");
        return false;
    }
    int len = http.getSize();
    if (len <= 0) {
        http.end();
        showError("server sent no Content-Length");
        return false;
    }
    uint8_t *buf = (uint8_t *)ps_malloc(len);
    if (!buf) {
        http.end();
        showError("PSRAM alloc failed");
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    size_t got = 0;
    uint32_t lastData = millis();
    while (got < (size_t)len && millis() - lastData < 20000) {
        size_t avail = stream->available();
        if (avail) {
            got += stream->readBytes(buf + got, min(avail, (size_t)len - got));
            lastData = millis();
        } else {
            delay(10);
        }
    }
    http.end();
    if (got < (size_t)len) {
        free(buf);
        showError("download incomplete");
        return false;
    }
    epaper.fillScreen(TFT_WHITE);
    renderJpeg(buf, got);
    free(buf);
    return true;
}

void drawStatusFooter(float vbat) {
    String status = "boot #" + String(bootCount) + "  wake: " +
                    wakeReason() + "  vbat: " + String(vbat, 2) + " V";
    epaper.fillRect(0, 1560, 1200, 40, TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.drawString(status, 20, 1568, 2);
}

void goToSleep() {
    Serial.printf("sleeping %llu s (buttons also wake)...\n", SLEEP_SECONDS);
    Serial.flush();
    epaper.sleep();                    // panel low-power mode
    pinMode(EPAPER_EN_PIN, OUTPUT);    // cut panel power rail
    digitalWrite(EPAPER_EN_PIN, LOW);
    esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
    esp_sleep_enable_ext1_wakeup(BUTTON_WAKE_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    bootCount++;
    Serial.printf("ee02-playground: task 5 — boot #%u, wake: %s\n",
                  bootCount, wakeReason());

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // LED on while awake

    float vbat = readBatteryVoltage();
    Serial.printf("battery: %.2f V\n", vbat);

    epaper.begin();

    if (!connectWifi()) {
        showError("wifi connect failed: " + String(WIFI_SSID));
    } else if (fetchImage()) {
        drawStatusFooter(vbat);
        Serial.println("updating panel (takes ~20-30 s)...");
        epaper.update();
        Serial.println("done");
    }

    digitalWrite(LED_PIN, HIGH);
    goToSleep(); // never returns
}

void loop() {}
```

- [ ] **Step 2: Build**

```bash
pio run
```

Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe the full cycle**

```bash
pio run -t upload && pio device monitor
```

Expected: serial shows `boot #1, wake: power-on/reset`, a plausible battery voltage (≈3.5–4.2 V with a battery, ≈4.3–5 V shown if USB-only powers VBAT rail — record what you see), image renders with a footer line `boot #1  wake: power-on/reset  vbat: …`, then `sleeping 3600 s…` and serial goes quiet (deep sleep drops the USB port — this is expected).

- [ ] **Step 4: Verify button wake**

Press any of the three user buttons. The USB port re-enumerates; re-run `pio device monitor` quickly.

Expected: `boot #2, wake: button` and a fresh image with updated footer. `bootCount` persisting across sleep proves RTC memory works.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "Task 5: battery reading, status footer, deep sleep with timer+button wake"
```

---

## Done criteria (maps to spec)

- Milestone 1 (hello display) → Task 2
- Milestone 2 (buttons & LED) → Task 3
- Milestone 3 (Wi-Fi + fetch, secrets.h, error screens) → Task 4
- Milestone 4 (deep sleep & battery) → Task 5
- Toolchain/scaffold, PSRAM/flash overrides, checked-in display config (as build flags) → Task 1
