# Status Screen Dashboard Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the on-device status screen's flat title/subtitle/3-line layout with a 3-tile icon dashboard (battery/Wi-Fi/next-photo), demote raw RSSI/voltage to a small caption line, and drop the separate landscape two-column layout in favor of one adaptive vertical stack used on every panel orientation.

**Architecture:** Two new pure-logic bucketing modules (Wi-Fi RSSI, battery color level) feed both the layout-independent icon-drawing helpers in `display.cpp` and the rewritten `drawStatusScreen()` in `ui.cpp`. `layout_math.h`'s `computeLayout()` is rewritten to produce one vertical-stack geometry (title/badge, divider rule, 3-tile row, caption, QR, legend) instead of branching on orientation; the provisioning-screen half of that file is untouched.

**Tech Stack:** C++17 (Arduino/ESP32 framework via PlatformIO), Seeed_GFX (TFT_eSPI fork) for drawing primitives, Unity test framework for host-side (`native` env) unit tests.

## Global Constraints

- Pure logic (bucketing functions) must live in `src/logic/*.h`, have zero Arduino dependencies, and be covered by `pio test -e native`.
- Every board firmware environment (`ee02`, `ee03`, `ee04`) must build clean (`pio run -e <env>`) after every task that touches shared headers.
- No new fonts or image assets — reuse the five GFXFF faces already linked in (`FreeSansBold24pt7b`, `FreeSansBold12pt7b`, `FreeSans18pt7b`, `FreeSans9pt7b`, `FreeSans12pt7b`) plus the classic bitmap Font2/Font4, matching the existing small/large tier fallback pattern.
- Functional battery-level color only applies on `USE_COLORFULL_EPAPER` (EE02's 6-color Spectra panel); every other panel (`USE_MUTIGRAY_EPAPER`, plain mono) always renders battery/charging glyphs in solid black — no information is lost since percentage text and bar-fill length already carry the level.
- The web settings portal (`portal.cpp`, `portal_html.h`) and the provisioning screen (`net.cpp`) are out of scope — do not modify them.
- The on-device visual result cannot be captured/screenshotted by the assistant — final visual tuning is confirmed by a human looking at the flashed panel, not by an automated check.

---

### Task 1: Wi-Fi strength and battery-level pure-logic bucketing

**Files:**
- Create: `src/logic/wifi_strength.h`
- Modify: `src/logic/battery_curve.h`
- Modify: `test/test_battery/main.cpp`
- Create: `test/test_wifi_strength/main.cpp`

**Interfaces:**
- Consumes: nothing (pure logic, no dependency on earlier tasks).
- Produces:
  - `enum class WifiStrength : uint8_t { Weak, Fair, Strong };`
  - `WifiStrength wifiStrengthBucket(int rssiDbm)`
  - `const char *wifiStrengthLabel(WifiStrength s)`
  - `enum class BatteryLevel : uint8_t { Low, Medium, High };`
  - `BatteryLevel batteryLevelBucket(int pct)`
  - Task 3 (icon color helpers) and Task 4 (`ui.cpp`) both consume these four symbols.

- [ ] **Step 1: Write the failing tests for `wifi_strength.h`**

Create `test/test_wifi_strength/main.cpp`:

```cpp
#include <unity.h>
#include "logic/wifi_strength.h"

void setUp() {}
void tearDown() {}

void test_strong_at_good_signal() { TEST_ASSERT_TRUE(wifiStrengthBucket(-50) == WifiStrength::Strong); }
void test_strong_boundary() { TEST_ASSERT_TRUE(wifiStrengthBucket(-60) == WifiStrength::Strong); }
void test_fair_just_below_strong() { TEST_ASSERT_TRUE(wifiStrengthBucket(-61) == WifiStrength::Fair); }
void test_fair_boundary() { TEST_ASSERT_TRUE(wifiStrengthBucket(-75) == WifiStrength::Fair); }
void test_weak_below_fair() { TEST_ASSERT_TRUE(wifiStrengthBucket(-76) == WifiStrength::Weak); }
void test_weak_very_low() { TEST_ASSERT_TRUE(wifiStrengthBucket(-95) == WifiStrength::Weak); }

void test_label_strings() {
    TEST_ASSERT_EQUAL_STRING("strong", wifiStrengthLabel(WifiStrength::Strong));
    TEST_ASSERT_EQUAL_STRING("fair", wifiStrengthLabel(WifiStrength::Fair));
    TEST_ASSERT_EQUAL_STRING("weak", wifiStrengthLabel(WifiStrength::Weak));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_strong_at_good_signal);
    RUN_TEST(test_strong_boundary);
    RUN_TEST(test_fair_just_below_strong);
    RUN_TEST(test_fair_boundary);
    RUN_TEST(test_weak_below_fair);
    RUN_TEST(test_weak_very_low);
    RUN_TEST(test_label_strings);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run: `pio test -e native -f test_wifi_strength`
Expected: FAIL to build — `logic/wifi_strength.h: No such file or directory`

- [ ] **Step 3: Create `src/logic/wifi_strength.h`**

```cpp
#pragma once
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
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_wifi_strength`
Expected: `7 test cases: 7 succeeded`

- [ ] **Step 5: Write the failing tests for the battery color bucket**

Append to `test/test_battery/main.cpp` (add the include, the new test functions, and the new `RUN_TEST` lines to `main()` — do not remove any existing test):

```cpp
#include <unity.h>
#include "logic/battery_curve.h"

void setUp() {}
void tearDown() {}

void test_full_at_4200() { TEST_ASSERT_EQUAL(100, batteryPercentFromMv(4200)); }
void test_clamped_above() { TEST_ASSERT_EQUAL(100, batteryPercentFromMv(4350)); }
void test_empty_at_3000() { TEST_ASSERT_EQUAL(0, batteryPercentFromMv(3000)); }
void test_clamped_below() { TEST_ASSERT_EQUAL(0, batteryPercentFromMv(2500)); }
void test_interpolates_midcurve() { TEST_ASSERT_EQUAL(38, batteryPercentFromMv(3650)); }
void test_interpolates_top() { TEST_ASSERT_EQUAL(97, batteryPercentFromMv(4150)); }
void test_interpolates_bottom() { TEST_ASSERT_EQUAL(2, batteryPercentFromMv(3250)); }

void test_level_high_at_full() { TEST_ASSERT_TRUE(batteryLevelBucket(100) == BatteryLevel::High); }
void test_level_high_boundary() { TEST_ASSERT_TRUE(batteryLevelBucket(41) == BatteryLevel::High); }
void test_level_medium_boundary_high() { TEST_ASSERT_TRUE(batteryLevelBucket(40) == BatteryLevel::Medium); }
void test_level_medium_boundary_low() { TEST_ASSERT_TRUE(batteryLevelBucket(16) == BatteryLevel::Medium); }
void test_level_low_boundary() { TEST_ASSERT_TRUE(batteryLevelBucket(15) == BatteryLevel::Low); }
void test_level_low_at_zero() { TEST_ASSERT_TRUE(batteryLevelBucket(0) == BatteryLevel::Low); }

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_at_4200);
    RUN_TEST(test_clamped_above);
    RUN_TEST(test_empty_at_3000);
    RUN_TEST(test_clamped_below);
    RUN_TEST(test_interpolates_midcurve);
    RUN_TEST(test_interpolates_top);
    RUN_TEST(test_interpolates_bottom);
    RUN_TEST(test_level_high_at_full);
    RUN_TEST(test_level_high_boundary);
    RUN_TEST(test_level_medium_boundary_high);
    RUN_TEST(test_level_medium_boundary_low);
    RUN_TEST(test_level_low_boundary);
    RUN_TEST(test_level_low_at_zero);
    return UNITY_END();
}
```

- [ ] **Step 6: Run the test to verify it fails**

Run: `pio test -e native -f test_battery`
Expected: FAIL to build — `'BatteryLevel' does not name a type` (or equivalent undeclared-identifier error) for the new test functions.

- [ ] **Step 7: Add the color bucket to `src/logic/battery_curve.h`**

Append to the end of `src/logic/battery_curve.h` (keep the existing `batteryPercentFromMv` untouched):

```cpp
enum class BatteryLevel : uint8_t { Low, Medium, High };

// Thresholds match common phone/appliance conventions: red when
// critically low, yellow as a mid-range warning, green otherwise.
inline BatteryLevel batteryLevelBucket(int pct) {
    if (pct <= 15) return BatteryLevel::Low;
    if (pct <= 40) return BatteryLevel::Medium;
    return BatteryLevel::High;
}
```

- [ ] **Step 8: Run the test to verify it passes**

Run: `pio test -e native -f test_battery`
Expected: `13 test cases: 13 succeeded`

- [ ] **Step 9: Run the full native suite and commit**

Run: `pio test -e native`
Expected: all test groups (`test_quiet_hours`, `test_layout_math`, `test_validate`, `test_url_template`, `test_battery`, `test_wifi_strength`) PASS.

```bash
git add src/logic/wifi_strength.h src/logic/battery_curve.h test/test_battery/main.cpp test/test_wifi_strength/main.cpp
git commit -m "Add Wi-Fi strength and battery color-level bucketing (pure logic)"
```

---

### Task 2: Rewrite `layout_math.h`'s status-screen geometry (single adaptive stack)

**Files:**
- Modify: `src/logic/layout_math.h`
- Modify: `test/test_layout_math/main.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: new `LayoutMetrics` fields consumed by Task 4 (`ui.cpp`):
  `titleH, tileValueH, chromeH, chromeLineH, cx, marginX, titleY, ruleY, tileTop, tileH, tileW, tileGap, tile0Cx, tile1Cx, tile2Cx, tileIconCy, tileValueY, tileLabelY, captionY, qrScale, qrCy, scanY, urlY, legend0Y, legend1Y, legend2Y`. Also keeps unchanged: `lineH, smallLineH, bodySize, smallSize`, and all `prov*` provisioning-screen fields.
  Removed fields (no longer exist — any remaining reference is a compile error to fix in Task 4): `landscape, leftCx, rightCx, subtitleY, info0Y, info1Y, info2Y, subH`.
  New constants: `TILE_VALUE_PX_LARGE = 42, TILE_VALUE_PX_SMALL = 22` (renamed from `SUB_PX_LARGE/SMALL` — same values, same meaning: the tile row's big value-number font size tier).

- [ ] **Step 1: Update the failing test expectations first**

Replace the full contents of `test/test_layout_math/main.cpp`:

```cpp
#include <unity.h>
#include "logic/layout_math.h"

void setUp() {}
void tearDown() {}

// The bug this guards against: line spacing that's narrower than the font
// actually drawn at that spacing causes overlapping text. lineHeightFor()
// must always leave more room than the font's rendered height.
static void assertLineHeightFitsFont(int textSize) {
    int h = lineHeightFor(textSize);
    TEST_ASSERT_TRUE(h > FONT4_NATIVE_PX * textSize);
}

void test_line_height_always_exceeds_font_height() {
    assertLineHeightFitsFont(1);
    assertLineHeightFitsFont(2);
}

// Sanity only (non-negative, ordered, no crash-inducing values) -- NOT a
// fit guarantee. Panels this small (well under anything in
// docs/panel-combos.md) are a documented, accepted gap (see
// docs/hardware-checklist.md's compact-layout note). The fits-asserters
// below are the real fit guarantee, applied to the panels this firmware
// actually ships.
static void assertSane(int width, int height) {
    LayoutMetrics m = computeLayout(width, height);
    TEST_ASSERT_TRUE(m.lineH > 0);
    TEST_ASSERT_TRUE(m.smallLineH > 0);
    TEST_ASSERT_TRUE(m.titleY > 0);
    TEST_ASSERT_TRUE(m.bodySize >= 1);
    TEST_ASSERT_TRUE(m.smallSize >= 1);
    TEST_ASSERT_TRUE(m.qrScale >= 1 && m.qrScale <= 4);
    TEST_ASSERT_TRUE(m.provQrScale >= 1 && m.provQrScale <= 4);
    TEST_ASSERT_TRUE(m.provTitleY >= 0);
    TEST_ASSERT_TRUE(m.provChangeY >= 0);
    // Provisioning anchors stay in top-to-bottom order regardless of size.
    TEST_ASSERT_TRUE(m.provTitleY < m.provStep1Y);
    TEST_ASSERT_TRUE(m.provStep1Y < m.provQr1Y);
    TEST_ASSERT_TRUE(m.provQr1Y < m.provJoinManualY);
    TEST_ASSERT_TRUE(m.provJoinManualY < m.provStep2Y);
    TEST_ASSERT_TRUE(m.provStep2Y < m.provQrHintY);
    TEST_ASSERT_TRUE(m.provQrHintY < m.provQr2Y);
    TEST_ASSERT_TRUE(m.provQr2Y < m.provStep3Y);
    TEST_ASSERT_TRUE(m.provStep3Y < m.provChangeY);
}

// One adaptive vertical stack, used for every panel size/orientation (no
// separate landscape layout anymore) -- the real fit guarantee is that the
// whole sequence stays on-panel and the tile row's three cells are
// left-to-right ordered with positive width.
static void assertStatusScreenFits(int width, int height) {
    LayoutMetrics m = computeLayout(width, height);
    TEST_ASSERT_TRUE(m.tile0Cx < m.tile1Cx);
    TEST_ASSERT_TRUE(m.tile1Cx < m.tile2Cx);
    TEST_ASSERT_TRUE(m.tileW > 0);
    TEST_ASSERT_TRUE(m.titleY < m.ruleY);
    TEST_ASSERT_TRUE(m.ruleY < m.tileTop);
    TEST_ASSERT_TRUE(m.tileTop < m.captionY);
    TEST_ASSERT_TRUE(m.captionY < m.qrCy);
    TEST_ASSERT_TRUE(m.qrCy < m.scanY);
    TEST_ASSERT_TRUE(m.scanY < m.urlY);
    TEST_ASSERT_TRUE(m.urlY < m.legend0Y);
    TEST_ASSERT_TRUE(m.legend0Y < m.legend1Y);
    TEST_ASSERT_TRUE(m.legend1Y < m.legend2Y);
    TEST_ASSERT_TRUE(m.legend2Y + m.chromeLineH / 2 < height);
}

void test_status_screen_fits_ee02_portrait() { assertStatusScreenFits(1200, 1600); }
void test_status_screen_fits_ee02_landscape() { assertStatusScreenFits(1600, 1200); }
void test_status_screen_fits_ee03_landscape() { assertStatusScreenFits(1872, 1404); }
void test_status_screen_fits_ee03_portrait() { assertStatusScreenFits(1404, 1872); }
void test_status_screen_fits_ee04_landscape() { assertStatusScreenFits(800, 480); }
void test_status_screen_fits_ee04_portrait() { assertStatusScreenFits(480, 800); }

// The provisioning screen draws 4 body lines + 3 caption lines + 2 QR
// codes, stacked top-down by computeLayout() itself (provTitleY..
// provChangeY); the real fit guarantee is just that the last anchor plus
// its own line height stays on-panel. Unchanged by this rework.
static void assertProvisioningScreenFits(int width, int height) {
    LayoutMetrics m = computeLayout(width, height);
    TEST_ASSERT_TRUE(m.provChangeY + m.smallLineH < height);
}

void test_provisioning_screen_fits_ee02() { assertProvisioningScreenFits(1200, 1600); }
void test_provisioning_screen_fits_ee02_landscape() { assertProvisioningScreenFits(1600, 1200); }
void test_provisioning_screen_fits_ee03() { assertProvisioningScreenFits(1872, 1404); }
void test_provisioning_screen_fits_ee03_portrait() { assertProvisioningScreenFits(1404, 1872); }
void test_provisioning_screen_fits_ee04_default() { assertProvisioningScreenFits(800, 480); }
void test_provisioning_screen_fits_ee04_portrait() { assertProvisioningScreenFits(480, 800); }

void test_no_overflow_400x300() { assertSane(400, 300); }
void test_no_overflow_200x200() { assertSane(200, 200); }

void test_small_panel_uses_smaller_body_text() {
    TEST_ASSERT_EQUAL(1, computeLayout(200, 200).bodySize);
    TEST_ASSERT_EQUAL(1, computeLayout(400, 300).bodySize);
    TEST_ASSERT_EQUAL(1, computeLayout(800, 480).bodySize); // below the 600px tier
    TEST_ASSERT_EQUAL(2, computeLayout(1200, 1600).bodySize);
}

// Font tiers: large panels get the big FreeSans faces, small panels the
// smaller ones -- and the small tier's chrome text (16px) must stay
// smaller than its own tile-value text (22px), matching the "chrome
// smaller than tile value, tile value smaller than title" ordering on
// every tier, not just the large one.
void test_font_tiers_scale_together() {
    LayoutMetrics large = computeLayout(1200, 1600);
    TEST_ASSERT_EQUAL(TITLE_PX_LARGE, large.titleH);
    TEST_ASSERT_EQUAL(TILE_VALUE_PX_LARGE, large.tileValueH);
    TEST_ASSERT_EQUAL(CHROME_PX_LARGE, large.chromeH);
    TEST_ASSERT_TRUE(large.chromeH < large.tileValueH);
    TEST_ASSERT_TRUE(large.tileValueH < large.titleH);

    LayoutMetrics small = computeLayout(800, 480);
    TEST_ASSERT_EQUAL(TITLE_PX_SMALL, small.titleH);
    TEST_ASSERT_EQUAL(TILE_VALUE_PX_SMALL, small.tileValueH);
    TEST_ASSERT_EQUAL(CHROME_PX_SMALL, small.chromeH);
    TEST_ASSERT_TRUE(small.chromeH < small.tileValueH);
    TEST_ASSERT_TRUE(small.tileValueH < small.titleH);
}

// Provisioning QR scale must shrink (not overlap text) on the panel this
// bug was found on, and stay at the original fixed size on large panels.
void test_provisioning_qr_scale_adapts() {
    TEST_ASSERT_EQUAL(4, computeLayout(1200, 1600).provQrScale);
    TEST_ASSERT_EQUAL(4, computeLayout(1872, 1404).provQrScale);
    TEST_ASSERT_TRUE(computeLayout(800, 480).provQrScale < 4);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_height_always_exceeds_font_height);
    RUN_TEST(test_status_screen_fits_ee02_portrait);
    RUN_TEST(test_status_screen_fits_ee02_landscape);
    RUN_TEST(test_status_screen_fits_ee03_landscape);
    RUN_TEST(test_status_screen_fits_ee03_portrait);
    RUN_TEST(test_status_screen_fits_ee04_landscape);
    RUN_TEST(test_status_screen_fits_ee04_portrait);
    RUN_TEST(test_provisioning_screen_fits_ee02);
    RUN_TEST(test_provisioning_screen_fits_ee02_landscape);
    RUN_TEST(test_provisioning_screen_fits_ee03);
    RUN_TEST(test_provisioning_screen_fits_ee03_portrait);
    RUN_TEST(test_provisioning_screen_fits_ee04_default);
    RUN_TEST(test_provisioning_screen_fits_ee04_portrait);
    RUN_TEST(test_no_overflow_400x300);
    RUN_TEST(test_no_overflow_200x200);
    RUN_TEST(test_small_panel_uses_smaller_body_text);
    RUN_TEST(test_font_tiers_scale_together);
    RUN_TEST(test_provisioning_qr_scale_adapts);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `pio test -e native -f test_layout_math`
Expected: FAIL to build — `'LayoutMetrics' has no member named 'tile0Cx'` (or similar) since `layout_math.h` hasn't been rewritten yet.

- [ ] **Step 3: Replace the full contents of `src/logic/layout_math.h`**

```cpp
#pragma once
// Proportional screen layout math. Pure logic: host-testable, no Arduino
// deps. Line spacing is derived from the actual rendered font size (GFXFF
// font 4 is ~26px tall per its native size; setTextSize(N) scales that by
// N), not a fixed fraction of panel height -- a fixed-height-fraction
// spacing broke on EE04/EE05's 800x480 default panel: bodySize stayed at
// its large tier (font stays ~52px tall) while height-fraction spacing
// shrank well below that, so lines overlapped each other outright, not
// just clipped at the edges. Deriving spacing from the font actually being
// drawn keeps text non-overlapping at any panel size.
//
// The status screen (ui.cpp) uses one adaptive vertical stack -- title +
// board-model badge, a divider rule, a 3-tile dashboard row (battery/
// Wi-Fi/next-photo), a caption line, the settings QR, and a button legend
// -- for every panel size and orientation. There is deliberately no
// separate landscape (two-column) layout: a short landscape panel (e.g.
// EE04's 800x480) just shrinks the same stack via the existing
// scale-to-fit QR logic and font tiers below.

// Native pixel height of GFXFF font 4 (see the LOAD_FONT4 comment in
// Seeed_GFX's Setup5xx headers: "Medium 26 pixel high font").
constexpr int FONT4_NATIVE_PX = 26;
// QR modules (33) plus the 4-module quiet zone on each side (drawQrCode).
constexpr int QR_MODULES_WITH_QUIET_ZONE = 33 + 8;

// Status-screen font sizes (yAdvance, taken directly from the vendored
// Fonts/GFXFF/FreeSans*.h headers) for the two size tiers. Large tier:
// FreeSansBold24pt7b (title) / FreeSans18pt7b (tile value) / FreeSans12pt7b
// (chrome: badge/tile label/caption/scan/url/legend). Small tier:
// FreeSansBold12pt7b / FreeSans9pt7b / classic Font2 (16px -- there's no
// FreeSans size below 9pt vendored, and 9pt is already used for the tile
// value on this tier, so chrome text falls back to the smaller classic
// bitmap font instead).
constexpr int TITLE_PX_LARGE = 56, TITLE_PX_SMALL = 29;
constexpr int TILE_VALUE_PX_LARGE = 42, TILE_VALUE_PX_SMALL = 22;
constexpr int CHROME_PX_LARGE = 29, CHROME_PX_SMALL = 16;

struct LayoutMetrics {
    int lineH;      // provisioning: body-line spacing (classic Font4)
    int smallLineH; // provisioning: caption-line spacing (classic Font4)
    int bodySize;   // provisioning setTextSize(); also selects the status
                    // screen's font tier (2 = large, 1 = small)
    int smallSize;  // provisioning setTextSize() for captions

    // Status screen (ui.cpp): one adaptive vertical stack, used for every
    // panel size/orientation.
    int titleH, tileValueH, chromeH, chromeLineH;
    int cx;                                 // page horizontal center
    int marginX;                            // shared left/right margin
    int titleY;                             // title + badge mid-anchor y
    int ruleY;                              // header divider line y
    int tileTop, tileH, tileW, tileGap;     // tile box geometry
    int tile0Cx, tile1Cx, tile2Cx;          // tile center x positions
    int tileIconCy, tileValueY, tileLabelY; // vertical anchors inside a tile
    int captionY;
    int qrScale;
    int qrCy;
    int scanY, urlY, legend0Y, legend1Y, legend2Y;

    // Provisioning screen anchors (net.cpp) -- unchanged by this rework.
    int provTitleY, provStep1Y, provQr1Y, provJoinManualY, provStep2Y,
        provQrHintY, provQr2Y, provStep3Y, provChangeY;
    int provQrScale;
};

// 4/3 headroom: enough gap for ascenders/descenders between stacked lines
// at this font's native size, without wasting space on panels where every
// pixel of vertical room matters (see EE04/EE05 below).
inline int lineHeightFor(int textSize) {
    return FONT4_NATIVE_PX * textSize * 4 / 3;
}

inline LayoutMetrics computeLayout(int width, int height) {
    LayoutMetrics m{};
    const int shortSide = width < height ? width : height;
    // Below ~600px, a giant body font (2x) leaves no room for this screen's
    // content no matter how tight the spacing gets -- drop to the smaller
    // tier instead.
    const bool large = shortSide >= 600;
    m.bodySize = large ? 2 : 1;
    m.smallSize = 1;
    m.lineH = lineHeightFor(m.bodySize);
    m.smallLineH = lineHeightFor(m.smallSize);

    m.titleH = large ? TITLE_PX_LARGE : TITLE_PX_SMALL;
    m.tileValueH = large ? TILE_VALUE_PX_LARGE : TILE_VALUE_PX_SMALL;
    m.chromeH = large ? CHROME_PX_LARGE : CHROME_PX_SMALL;
    m.chromeLineH = m.chromeH * 4 / 3;
    const int gap = m.titleH / 6;

    m.cx = width / 2;
    m.marginX = width / 12;

    // ---- Tile row geometry: fixed once the font tier is known ----
    const int tilePad = m.chromeH / 3;
    const int iconSize = m.tileValueH;
    m.tileH = 2 * tilePad + iconSize + gap / 3 + m.tileValueH + gap / 3 + m.chromeH;
    const int rowW = width - 2 * m.marginX;
    m.tileGap = rowW / 20;
    m.tileW = (rowW - 2 * m.tileGap) / 3;
    m.tile0Cx = m.marginX + m.tileW / 2;
    m.tile1Cx = m.tile0Cx + m.tileW + m.tileGap;
    m.tile2Cx = m.tile1Cx + m.tileW + m.tileGap;

    // ---- Stack the whole screen top-to-bottom, capping the QR at a sane
    // size instead of growing it to fill whatever's left, and pushing the
    // freed space to the top/bottom margins -- same principle as the
    // provisioning screen below, so a physical picture-frame mat doesn't
    // clip content at the edges ----
    const int ruleGap = gap;          // header -> rule
    const int tileGapAbove = 2 * gap; // rule -> tile row
    const int captionGap = gap;       // tile row -> caption
    const int qrGap = m.lineH / 3;

    const int aboveQrH = m.titleH + ruleGap + tileGapAbove + m.tileH +
                        captionGap + m.chromeLineH;
    const int belowQrH = 5 * m.chromeLineH; // scan + url + 3 legend lines
    const int fixedH = aboveQrH + 2 * qrGap + belowQrH;

    const int naturalQrBudget = height > fixedH ? height - fixedH : 0;
    int scale = 4;
    while (scale > 1 && naturalQrBudget / QR_MODULES_WITH_QUIET_ZONE < scale)
        scale--;
    m.qrScale = scale;
    const int qrPx = QR_MODULES_WITH_QUIET_ZONE * scale;
    const int slack = naturalQrBudget > qrPx ? naturalQrBudget - qrPx : 0;
    const int margin = slack / 2;

    m.titleY = margin + m.titleH / 2;
    m.ruleY = m.titleY + m.titleH / 2 + ruleGap;
    m.tileTop = m.ruleY + tileGapAbove;
    m.tileIconCy = m.tileTop + tilePad + iconSize / 2;
    m.tileValueY = m.tileTop + tilePad + iconSize + gap / 3 + m.tileValueH / 2;
    m.tileLabelY = m.tileValueY + m.tileValueH / 2 + gap / 3 + m.chromeH / 2;
    m.captionY = m.tileTop + m.tileH + captionGap;
    m.qrCy = m.captionY + m.chromeLineH / 2 + qrGap + qrPx / 2;
    m.scanY = m.qrCy + qrPx / 2 + qrGap;
    m.urlY = m.scanY + m.chromeLineH;
    m.legend0Y = m.urlY + m.chromeLineH;
    m.legend1Y = m.legend0Y + m.chromeLineH;
    m.legend2Y = m.legend1Y + m.chromeLineH;

    // ---- Provisioning screen (net.cpp): 4 body lines + 3 caption lines of
    // text, plus 2 QR codes, stacked top-down. Pick the largest QR scale
    // (capped at 4, matching the original fixed size on large panels) that
    // still lets everything fit within the panel height, then center the
    // whole block vertically the same way as the status screen above.
    // Unchanged by this rework. ----
    const int textTotal = 4 * m.lineH + 3 * m.smallLineH;
    int pscale = 4;
    while (pscale > 1 &&
           textTotal + 2 * QR_MODULES_WITH_QUIET_ZONE * pscale + m.lineH > height)
        pscale--;
    m.provQrScale = pscale;
    const int provQrPx = QR_MODULES_WITH_QUIET_ZONE * pscale;
    const int totalNeeded = textTotal + 2 * provQrPx + m.lineH;
    const int provSlack = height > totalNeeded ? height - totalNeeded : 0;
    const int provMargin = provSlack / 2;

    int y = provMargin + m.lineH / 2;
    m.provTitleY = y;              y += m.lineH;
    m.provStep1Y = y;              y += m.lineH;
    m.provQr1Y = y + provQrPx / 2; y += provQrPx + m.lineH / 2;
    m.provJoinManualY = y;         y += m.smallLineH;
    m.provStep2Y = y;              y += m.lineH;
    m.provQrHintY = y;             y += m.smallLineH;
    m.provQr2Y = y + provQrPx / 2; y += provQrPx + m.lineH / 2;
    m.provStep3Y = y;              y += m.lineH;
    m.provChangeY = y;
    return m;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_layout_math`
Expected: `18 test cases: 18 succeeded`

- [ ] **Step 5: Build every firmware environment**

Run: `pio run -e ee02 && pio run -e ee03 && pio run -e ee04`
Expected: all three `SUCCESS` — this only compiles `layout_math.h` in isolation via `net.cpp` (provisioning screen); `ui.cpp` will not yet compile (fixed in Task 4) if you try `pio run` before then, so it's expected that `net.cpp`'s own translation unit succeeds while a full link may still fail on unrelated `ui.cpp` errors. If the build fails specifically inside `net.cpp` or `layout_math.h`, that is a real regression to fix before continuing.

- [ ] **Step 6: Commit**

```bash
git add src/logic/layout_math.h test/test_layout_math/main.cpp
git commit -m "Rewrite status-screen layout math as one adaptive tile-dashboard stack"
```

---

### Task 3: Icon-drawing primitives in `display.h`/`display.cpp`

**Files:**
- Modify: `src/display.h`
- Modify: `src/display.cpp`

**Interfaces:**
- Consumes: `BatteryLevel`, `WifiStrength` enums from Task 1 (`src/logic/battery_curve.h`, `src/logic/wifi_strength.h`).
- Produces (consumed by Task 4's `ui.cpp`):
  - `void drawBatteryIcon(int cx, int cy, int r, int pct, uint32_t fillColor, bool charging);`
  - `void drawWifiIcon(int cx, int baseY, int r, WifiStrength level);`
  - `void drawNextPhotoIcon(int cx, int cy, int r, bool pinned);`
  - `uint32_t batteryColorForLevel(BatteryLevel level);`
  - `uint32_t chargingBoltColor();`

There is no host-side unit test for this task: these functions call `epaper.*` drawing primitives (Arduino/Seeed_GFX), which only exist in the ESP32 build, matching this codebase's existing convention (`drawQrCode`/`renderJpeg` in the same file are likewise untested — only `src/logic/*` is host-tested). The verifiable deliverable is a clean build across all three firmware environments.

- [ ] **Step 1: Add the new includes and function declarations to `src/display.h`**

Replace the full contents of `src/display.h`:

```cpp
#pragma once
#include <TFT_eSPI.h> // Seeed_GFX; provides EPaper for the selected combo
#include "logic/battery_curve.h"
#include "logic/wifi_strength.h"

extern EPaper epaper;

// True if this panel's native (rotation 0) shape is wider than tall.
// EE02's native panel is portrait (1200x1600), but EE03/EE04/EE05's native
// panels are landscape (e.g. 800x480) -- rotation 0 does NOT universally
// mean "portrait", so the rotation dropdown's labels must be computed from
// this per board, not hardcoded (see portal.cpp's rotOptions()).
constexpr bool PANEL_NATIVE_LANDSCAPE = TFT_WIDTH > TFT_HEIGHT;

// Apply the configured orientation (settings.rotation -> setRotation).
// Call once after epaper.begin(), before any drawing.
void applyOrientation();

// Version-4 (33x33) QR centered at (cx, cy), scale px per module, with a
// 4-module white quiet zone. Draws into the sprite only. Payload must fit
// version 4 at ECC_LOW (78 bytes).
void drawQrCode(const String &text, int cx, int cy, int scale);

// Status-screen dashboard icons (battery/Wi-Fi/next-photo tiles). All
// built from fillRect/fillCircle/fillTriangle primitives -- no image
// assets, same approach drawQrCode already uses. Each is centered at
// (cx, cy) with r setting overall icon scale.
void drawBatteryIcon(int cx, int cy, int r, int pct, uint32_t fillColor, bool charging);
void drawWifiIcon(int cx, int baseY, int r, WifiStrength level);
void drawNextPhotoIcon(int cx, int cy, int r, bool pinned);

// Board-appropriate battery fill color: functional green/yellow/red on
// 6-color Spectra panels (EE02), solid black everywhere else -- the
// percentage number and bar fill length already carry the information on
// grayscale/mono panels, so no data is lost by dropping the color cue.
uint32_t batteryColorForLevel(BatteryLevel level);

// Board-appropriate charging-indicator color: blue on 6-color Spectra
// panels, black everywhere else.
uint32_t chargingBoltColor();

// Decode a baseline JPEG into PSRAM, Floyd-Steinberg dither it to the
// panel's palette, and write it into the sprite (no update()).
bool renderJpeg(uint8_t *buf, size_t len);

// For gray-capable panels (e.g. EE03): switch the sprite into
// USE_MUTIGRAY_EPAPER's gray mode. No-op on panels that don't support it.
// Call once after epaper.begin() / applyOrientation(), before drawing.
void initPanelColorMode();

// Full-panel error screen (calls update()).
void showError(const String &msg);
```

- [ ] **Step 2: Add the implementations to `src/display.cpp`**

Insert the following block immediately after `drawQrCode()`'s closing brace (i.e. right before the `// Panel color/gray index...` comment that precedes `struct PaletteEntry`):

```cpp
uint32_t batteryColorForLevel(BatteryLevel level) {
#if defined(USE_COLORFULL_EPAPER)
    switch (level) {
        case BatteryLevel::Low:    return TFT_RED;
        case BatteryLevel::Medium: return TFT_YELLOW;
        default:                   return TFT_GREEN;
    }
#else
    (void)level;
    return TFT_BLACK;
#endif
}

uint32_t chargingBoltColor() {
#if defined(USE_COLORFULL_EPAPER)
    return TFT_BLUE;
#else
    return TFT_BLACK;
#endif
}

// Classic battery glyph: rounded-rect outline + right-side nub, inner fill
// proportional to pct. Outline is always black regardless of fill color
// (readability); fillColor should already be board-appropriate (see
// batteryColorForLevel above).
void drawBatteryIcon(int cx, int cy, int r, int pct, uint32_t fillColor, bool charging) {
    const int bodyW = r * 3, bodyH = r * 2;
    const int nubW = r / 2, nubH = r;
    const int x0 = cx - (bodyW + nubW) / 2;
    const int y0 = cy - bodyH / 2;
    epaper.drawRoundRect(x0, y0, bodyW, bodyH, r / 4, TFT_BLACK);
    epaper.fillRect(x0 + bodyW, cy - nubH / 2, nubW, nubH, TFT_BLACK);
    const int pad = r / 5;
    const int innerX = x0 + pad, innerY = y0 + pad;
    const int innerW = bodyW - 2 * pad, innerH = bodyH - 2 * pad;
    epaper.fillRect(innerX, innerY, innerW, innerH, TFT_WHITE);
    const int clamped = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    const int filledW = innerW * clamped / 100;
    if (filledW > 0) epaper.fillRect(innerX, innerY, filledW, innerH, fillColor);
    if (charging) {
        const uint32_t bolt = chargingBoltColor();
        const int bx = cx + bodyW / 2 - r / 4;
        epaper.fillTriangle(bx, cy - r, bx - r / 2, cy, bx, cy, bolt);
        epaper.fillTriangle(bx, cy, bx + r / 2, cy, bx, cy + r, bolt);
    }
}

// Classic Wi-Fi glyph (dot + up to 2 upward-opening arcs). Built from
// fillCircle "donut" cuts (draw a filled disc, then a smaller white disc
// inside it to leave a ring) instead of drawArc(), which avoids depending
// on this library's arc-angle convention -- masking the lower half of each
// ring with one final white fillRect converts the full rings into the
// classic upward-opening arcs. Rings are drawn outermost-first so the
// always-drawn center dot is painted last and never gets erased by a
// later "cut".
void drawWifiIcon(int cx, int baseY, int r, WifiStrength level) {
    const int dotR = r / 5;
    const int ring1 = r * 3 / 5, ring1In = ring1 - r / 6;
    const int ring2 = r,        ring2In = ring2 - r / 6;
    if (level == WifiStrength::Strong) {
        epaper.fillCircle(cx, baseY, ring2, TFT_BLACK);
        epaper.fillCircle(cx, baseY, ring2In, TFT_WHITE);
    }
    if (level != WifiStrength::Weak) {
        epaper.fillCircle(cx, baseY, ring1, TFT_BLACK);
        epaper.fillCircle(cx, baseY, ring1In, TFT_WHITE);
    }
    epaper.fillCircle(cx, baseY, dotR, TFT_BLACK);
    epaper.fillRect(cx - r - 1, baseY, 2 * (r + 1), r + 2, TFT_WHITE);
}

// Clock face (next scheduled fetch), or a map-pin marker when the current
// picture is pinned (KEY3) -- neither hand position is a real clock, both
// are purely decorative glyphs paired with the tile's own time/"pinned"
// text.
void drawNextPhotoIcon(int cx, int cy, int r, bool pinned) {
    if (pinned) {
        const int headR = r * 2 / 3;
        epaper.fillCircle(cx, cy - r / 3, headR, TFT_BLACK);
        epaper.fillCircle(cx, cy - r / 3, headR - r / 4, TFT_WHITE);
        epaper.fillTriangle(cx - headR / 2, cy, cx + headR / 2, cy, cx, cy + r, TFT_BLACK);
        return;
    }
    epaper.drawCircle(cx, cy, r, TFT_BLACK);
    epaper.drawLine(cx, cy, cx, cy - r * 3 / 5, TFT_BLACK);
    epaper.drawLine(cx, cy, cx + r * 2 / 5, cy + r / 5, TFT_BLACK);
}
```

- [ ] **Step 3: Build every firmware environment**

Run: `pio run -e ee02 && pio run -e ee03 && pio run -e ee04`
Expected: all three `SUCCESS`. `USE_COLORFULL_EPAPER` is defined for `ee02` (combo 510) and undefined for `ee03`/`ee04`, so this build check exercises both branches of `batteryColorForLevel`/`chargingBoltColor`.

- [ ] **Step 4: Commit**

```bash
git add src/display.h src/display.cpp
git commit -m "Add battery/Wi-Fi/next-photo icon primitives for the status dashboard"
```

---

### Task 4: Rewrite `drawStatusScreen()` and split Wi-Fi metadata storage

**Files:**
- Modify: `src/ui.cpp`
- Modify: `src/state.h`

**Interfaces:**
- Consumes:
  - Task 1: `WifiStrength`, `wifiStrengthBucket`, `wifiStrengthLabel`, `BatteryLevel`, `batteryLevelBucket`.
  - Task 2: the rewritten `LayoutMetrics` fields (`titleY, ruleY, tileTop, tile0Cx/1Cx/2Cx, tileIconCy, tileValueY, tileLabelY, captionY, qrCy, qrScale, scanY, urlY, legend0Y/1Y/2Y, cx, marginX`).
  - Task 3: `drawBatteryIcon`, `drawWifiIcon`, `drawNextPhotoIcon`, `batteryColorForLevel`, `chargingBoltColor`.
- Produces: `drawStatusScreen(int32_t vbatMv, int32_t deltaMv, bool haveDelta)` and `recordFetchMetadata()` keep their existing signatures (declared in `src/ui.h`, unchanged) — no caller elsewhere in the codebase needs to change.

Data-storage change: `recordFetchMetadata()` currently stores Wi-Fi info as one combined string under the NVS key `wifiDesc`. This task splits it into two keys, `wifiSsid` (string) and `wifiRssi` (int) so the caption row (SSID) and the tile row (RSSI bucket) can read them independently. The old `wifiDesc` key is abandoned (not migrated) — this project has no versioned-settings-migration mechanism, and a stale leftover `wifiDesc` key in NVS is harmless (nothing reads it after this change).

- [ ] **Step 1: Update the NVS key comment in `src/state.h`**

In `src/state.h`, change:

```cpp
// NVS namespace "frame": held / lastEpoch / wifiDesc / tzOff / lastIp,
```

to:

```cpp
// NVS namespace "frame": held / lastEpoch / wifiSsid / wifiRssi / tzOff /
// lastIp,
```

- [ ] **Step 2: Replace the full contents of `src/ui.cpp`**

```cpp
#include "ui.h"
#include "config.h"
#include "display.h"
#include "layout.h"
#include "portal.h"
#include "power.h"
#include "settings.h"
#include "state.h"
#include "logic/battery_curve.h"
#include "logic/wifi_strength.h"
#include <WiFi.h>
#include <time.h>
// FreeSansBold24pt7b/FreeSansBold12pt7b/FreeSans18pt7b/FreeSans9pt7b/
// FreeSans12pt7b are already pulled in unconditionally by Seeed_GFX's
// gfxfont.h (via <TFT_eSPI.h>, included through display.h) whenever
// LOAD_GFXFF is defined -- these headers have no include guards, so
// including them again here would double-define their font data.

const char *wakeReason() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: return "timer";
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t bits = esp_sleep_get_ext1_wakeup_status();
            if (bits & (1ULL << BTN_INFO))    return "btn-info";
            if (bits & (1ULL << BTN_NEW_PIC)) return "btn-new-pic";
            if (bits & (1ULL << BTN_PIN))     return "btn-pin";
            return "button";
        }
        default: return "power-on";
    }
}

void recordFetchMetadata() {
    // Don't stamp a never-synced clock as the fetch time — keep the
    // previous (accurate) epoch instead and let the page show that.
    time_t now = time(nullptr);
    if (now > CLOCK_SANE_EPOCH)
        prefs.putULong("lastEpoch", (uint32_t)now);
    // Stored separately (not one pre-formatted string) so the status
    // screen's caption row (SSID) and tile row (RSSI bucket) can each
    // read only what they need.
    prefs.putString("wifiSsid", WiFi.SSID());
    prefs.putInt("wifiRssi", WiFi.RSSI());
}

// "next photo" tile value ("HH:MM", "pinned", or "--"), plus whether it's
// the pinned state (drives the pin-icon swap in drawStatusScreen).
static String nextPhotoValue(bool *pinnedOut) {
    *pinnedOut = held;
    if (held) return "pinned";
    if (time(nullptr) <= CLOCK_SANE_EPOCH) return "--";
    time_t nextT = time(nullptr) + (time_t)plannedSleepSecs();
    struct tm t;
    localtime_r(&nextT, &t);
    char hm[8];
    strftime(hm, sizeof(hm), "%H:%M", &t);
    return String(hm);
}

void drawStatusScreen(int32_t vbatMv, int32_t deltaMv, bool haveDelta) {
    // Metadata comes from recordFetchMetadata(); this page only reads it.
    const LayoutMetrics lm = currentLayout();
    const bool large = lm.bodySize == 2;

    const int pct = batteryPercent(vbatMv);
    const BatteryLevel battLevel = batteryLevelBucket(pct);
    const bool charging = haveDelta && deltaMv >= 20;

    const int rssi = prefs.getInt("wifiRssi", -100);
    const WifiStrength wifiLevel = wifiStrengthBucket(rssi);
    const String ssid = prefs.getString("wifiSsid", "?");

    bool pinned = false;
    const String nextValue = nextPhotoValue(&pinned);

    time_t lastEpoch = (time_t)prefs.getULong("lastEpoch", 0);
    String lastStr = "--";
    if (lastEpoch > CLOCK_SANE_EPOCH) {
        struct tm t;
        localtime_r(&lastEpoch, &t);
        char buf[16];
        strftime(buf, sizeof(buf), "%a %H:%M", &t);
        lastStr = buf;
    }

    char voltBuf[8];
    snprintf(voltBuf, sizeof(voltBuf), "%.2fV", vbatMv / 1000.0f);
    const String caption = "last " + lastStr + "  ·  " + ssid + "  ·  " + voltBuf;

    const String url = portalUrl();
    const String lastIp = prefs.getString("lastIp", "");

    epaper.fillScreen(TFT_WHITE);
    epaper.setTextColor(TFT_BLACK, TFT_WHITE);
    epaper.setTextDatum(MC_DATUM);

    // Header: title centered; board-model badge right-anchored on the
    // same mid-line (a fixed corner anchor avoids needing textWidth() to
    // place it next to the title).
    epaper.setFreeFont(large ? &FreeSansBold24pt7b : &FreeSansBold12pt7b);
    epaper.drawString("Hello ePaper", lm.cx, lm.titleY);
    epaper.setTextDatum(MR_DATUM);
    if (large) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString(BOARD_MODEL, epaper.width() - lm.marginX, lm.titleY);
    } else {
        epaper.setTextSize(1);
        epaper.drawString(BOARD_MODEL, epaper.width() - lm.marginX, lm.titleY, 2);
    }
    epaper.setTextDatum(MC_DATUM);

    // Divider rule under the header.
    epaper.fillRect(lm.marginX, lm.ruleY, epaper.width() - 2 * lm.marginX, 3, TFT_BLACK);

    // Tile row: battery / Wi-Fi / next photo, icon + big value + small label.
    const int iconR = lm.tileValueH / 2;
    drawBatteryIcon(lm.tile0Cx, lm.tileIconCy, iconR, pct,
                    batteryColorForLevel(battLevel), charging);
    drawWifiIcon(lm.tile1Cx, lm.tileIconCy + iconR, iconR, wifiLevel);
    drawNextPhotoIcon(lm.tile2Cx, lm.tileIconCy, iconR, pinned);

    epaper.setFreeFont(large ? &FreeSans18pt7b : &FreeSans9pt7b);
    epaper.drawString(String(pct) + "%", lm.tile0Cx, lm.tileValueY);
    epaper.drawString(wifiStrengthLabel(wifiLevel), lm.tile1Cx, lm.tileValueY);
    epaper.drawString(nextValue, lm.tile2Cx, lm.tileValueY);

    if (large) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString("battery", lm.tile0Cx, lm.tileLabelY);
        epaper.drawString("Wi-Fi", lm.tile1Cx, lm.tileLabelY);
        epaper.drawString("next", lm.tile2Cx, lm.tileLabelY);
        epaper.drawString(caption, lm.cx, lm.captionY);
    } else {
        epaper.setTextSize(1);
        epaper.drawString("battery", lm.tile0Cx, lm.tileLabelY, 2);
        epaper.drawString("Wi-Fi", lm.tile1Cx, lm.tileLabelY, 2);
        epaper.drawString("next", lm.tile2Cx, lm.tileLabelY, 2);
        epaper.drawString(caption, lm.cx, lm.captionY, 2);
    }

    drawQrCode(url, lm.cx, lm.qrCy, lm.qrScale);

    if (large) {
        epaper.setFreeFont(&FreeSans12pt7b);
        epaper.drawString("Scan to open settings", lm.cx, lm.scanY);
        epaper.drawString(url + (lastIp.isEmpty() ? "" : "   (" + lastIp + ")"),
                          lm.cx, lm.urlY);
        epaper.drawString("KEY1: back to photo — closes settings", lm.cx, lm.legend0Y);
        epaper.drawString("KEY2: new picture", lm.cx, lm.legend1Y);
        epaper.drawString(held ? "KEY3: unpin — refreshes resume"
                               : "KEY3: pin this picture",
                          lm.cx, lm.legend2Y);
    } else {
        epaper.setTextSize(1);
        epaper.drawString("Scan to open settings", lm.cx, lm.scanY, 2);
        epaper.drawString(url + (lastIp.isEmpty() ? "" : "   (" + lastIp + ")"),
                          lm.cx, lm.urlY, 2);
        epaper.drawString("KEY1: back to photo — closes settings", lm.cx, lm.legend0Y, 2);
        epaper.drawString("KEY2: new picture", lm.cx, lm.legend1Y, 2);
        epaper.drawString(held ? "KEY3: unpin — refreshes resume"
                               : "KEY3: pin this picture",
                          lm.cx, lm.legend2Y, 2);
    }

    epaper.setTextDatum(TL_DATUM);
}
```

- [ ] **Step 3: Build every firmware environment**

Run: `pio run -e ee02 && pio run -e ee03 && pio run -e ee04`
Expected: all three `SUCCESS`.

- [ ] **Step 4: Run the full native test suite (regression check)**

Run: `pio test -e native`
Expected: all groups PASS (`test_quiet_hours`, `test_layout_math`, `test_validate`, `test_url_template`, `test_battery`, `test_wifi_strength`).

- [ ] **Step 5: Commit**

```bash
git add src/ui.cpp src/state.h
git commit -m "Rewrite the status screen as a 3-tile icon dashboard"
```

- [ ] **Step 6: Flash to the device and visually verify**

Run: `pio run -e ee02 -t upload --upload-port <your device's serial port>`

This step's outcome cannot be verified by the assistant — the e-paper output can't be captured or screenshotted. After flashing, press KEY1 on the device and confirm by eye:
- Title + `EE02` badge on one row, with a divider rule beneath it.
- Three bordered tiles (battery %, Wi-Fi strength word, next-photo time) each with an icon above the value and a small label below.
- Battery tile's fill color is green/yellow/red depending on charge level (EE02 only); a small blue bolt badge appears while charging.
- A caption line below the tiles showing `last <day> <time>  ·  <SSID>  ·  <voltage>V`.
- The QR code, "Scan to open settings" line, URL, and 3-line KEY1/KEY2/KEY3 legend below that, all still readable and non-overlapping.
- If any element overlaps, clips, or looks wrong, note exactly what's off (which element, which direction) — the pixel constants in Task 2/3 are a first calibration pass, same as this codebase's original layout math was, and may need a follow-up numeric tweak once seen on the real panel.

## Self-Review

**1. Spec coverage:**
- Information architecture (header badge, 3-tile row, caption row, unchanged footer) — Task 4.
- Visual layout / single adaptive stack, no landscape branch — Task 2.
- Color rules (functional on EE02, black elsewhere) and icons (battery/Wi-Fi/clock-pin, primitives only) — Tasks 1 and 3.
- Data changes (`wifiSsid`/`wifiRssi` split) — Task 4.
- Testing (native tests, per-board builds, device flash + human visual check) — present in every task and called out explicitly in Task 4 Step 6.
- Out of scope (portal, provisioning screen, rotation labeling) — untouched by every task; Task 2 explicitly preserves the provisioning section of `layout_math.h` verbatim in structure.

**2. Placeholder scan:** No TBD/TODO; every step has complete, runnable code. Task 4 Step 6 (device flash) is explicitly and correctly marked as human-verified rather than a fabricated automated check, consistent with the spec's own "Testing" section.

**3. Type consistency:** `WifiStrength`/`BatteryLevel` (Task 1) match the exact signatures consumed in Task 3's declarations and Task 4's `ui.cpp` usage. `LayoutMetrics` field names introduced in Task 2 (`tile0Cx`, `tileIconCy`, `tileValueY`, `tileLabelY`, `marginX`, `cx`, `ruleY`, `scanY`, etc.) match exactly what Task 4's `drawStatusScreen()` reads — cross-checked field-by-field while writing Task 4. `drawBatteryIcon`/`drawWifiIcon`/`drawNextPhotoIcon` signatures in Task 3's `display.h` declaration match both the `display.cpp` definitions and Task 4's call sites exactly (same parameter order and types).
