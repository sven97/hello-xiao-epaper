# On-device status screen redesign: dashboard tile layout

## Context

The status screen (drawn by `drawStatusScreen()` in `src/ui.cpp`, shown on
KEY1 press) currently renders a title, a subtitle, three flat text lines
(last/next photo, Wi-Fi SSID + RSSI, battery % + voltage), a QR code to the
settings portal, and a button legend. Layout math in
`src/logic/layout_math.h` adapts this to any panel size/orientation via a
portrait (one column) and landscape (two column) branch.

Two problems, roughly equal weight:

1. **Flat visual hierarchy** — every line is `drawString()` at similar
   weight; no icons, no grouping, nothing to scan at a glance versus read
   line by line.
2. **Wrong emphasis** — technical details (Wi-Fi RSSI in dBm, raw battery
   voltage) get equal billing with what actually matters at a glance
   (battery %, next photo time).

This spec redesigns the information architecture and visual layout of this
one screen. It does not touch the web settings portal (`portal.cpp`,
`portal_html.h`) or the provisioning screen (`net.cpp`'s
`showProvisioningScreen()`), and it does not change the rotation setting or
`portal.cpp`'s `rotOptions()` labeling logic.

## Information architecture

Content shown, from top to bottom:

1. **Header row**: "Hello ePaper" title (unchanged text), plus a small
   rounded badge showing the board model (e.g. `EE02`) in place of today's
   "XIAO ePaper Display Board EE02" subtitle line.
2. **Tile row** — three equal-width cells (no visible border — spacing and
   icon/value/label grouping alone separate them), the glanceable layer:
   - Battery: icon + `NN%`
   - Wi-Fi: icon + qualitative bucket word (`strong` / `fair` / `weak`)
     instead of raw dBm
   - Next photo: icon + time, or a pin icon + `pinned` when KEY3-held
3. **Caption row** — small, muted, demoted technical detail (not deleted):
   last-photo timestamp, Wi-Fi SSID name, and battery voltage, e.g.
   `last Tue 14:36 · FUIYOH · 4.15V`
4. **Footer** (unchanged in substance): QR code to the settings portal +
   "Scan to open settings" + `<name>.local` URL, then the KEY1/KEY2/KEY3
   button legend.

## Visual layout

One vertical stack, centered, used identically regardless of panel
orientation. **This replaces the existing landscape/portrait branch in
`computeLayout()` entirely** — there is no longer a two-column layout for
landscape-native panels. On a short landscape panel (e.g. EE04's default
800×480), the whole stack shrinks via the existing adaptive-sizing
approach (font tier by short-side, QR scale-to-fit) to still fit without
overlap; it is expected and acceptable for the QR/text to end up smaller
there than on a tall portrait panel.

```
        Hello ePaper          [EE02]
   ────────────────────────────────
   [ 🔋 97% ] [ 📶 strong ] [ 🕐 15:36 ]
     battery      Wi-Fi          next
   last Tue 14:36 · FUIYOH · 4.15V

              [ QR ]
        Scan to open settings
            ee02.local
   KEY1 back · KEY2 new pic · KEY3 pin
```

## Color rules & icons

All icons are drawn with primitives (`fillRect`/`drawLine`/`drawCircle`/
arcs), the same approach `drawQrCode()` already uses — no image assets.

- **Battery icon**: outline + nub (classic battery glyph), inner fill rect
  sized to percentage.
  - EE02 (6-color Spectra panel): fill color is *functional* — green
    above 40%, yellow 16–40%, red ≤15%. Charging adds a small blue bolt
    badge in the corner.
  - EE03/EE04/EE05 (grayscale/mono): fill is always solid black/gray —
    percentage number and bar length already carry the information, so no
    data is lost by dropping the color cue. Charging bolt renders in
    black.
- **Wi-Fi icon**: nested-arc signal glyph (2–3 arcs); arc count filled
  matches the strong/fair/weak bucket. Same icon on every board — no
  color dependency.
- **Next-photo icon**: clock outline; swaps to a pin icon + `pinned` label
  when KEY3-pinned.

Thresholds are pure functions in `src/logic/`, host-testable, no Arduino
deps — same pattern as the existing `battery_curve.h`:

- `src/logic/battery_curve.h`: add a battery color-bucket function
  (green/yellow/red by the thresholds above).
- `src/logic/wifi_strength.h` (new): bucket RSSI dBm into
  strong/fair/weak.

## Data changes

`recordFetchMetadata()` (`src/ui.cpp`) currently stores Wi-Fi info as one
combined string (`"<SSID> <RSSI>dBm"`) under the `wifiDesc` NVS key. The
caption row needs the SSID and the tile row needs the RSSI bucket
independently, so this becomes two separate stored values (SSID string,
RSSI int) instead of one pre-formatted string.

## Implementation surface

- `src/logic/layout_math.h`: `computeLayout()` drops the landscape branch;
  replaced by one vertical-stack calculation (badge, tile-row geometry,
  caption, QR, legend), scaling by panel height/short-side as today.
- `src/logic/wifi_strength.h` (new): RSSI → strong/fair/weak bucket.
- `src/logic/battery_curve.h`: add color-bucket function.
- `src/display.h` / `src/display.cpp`: add icon-drawing helpers (battery,
  Wi-Fi, clock/pin) built from primitives.
- `src/ui.cpp`: `drawStatusScreen()` rewritten around the new structure;
  `recordFetchMetadata()` stores SSID/RSSI separately (see Data changes).
- `test/test_layout_math/main.cpp`: landscape-specific cases replaced with
  tile-geometry/fit checks across all board combos, including the tight
  EE04 800×480 case.
- New native test coverage for the Wi-Fi strength buckets and the battery
  color buckets.

## Testing

- `pio test -e native` — updated/extended layout, wifi-strength, and
  battery-bucket test cases.
- `pio run` for `ee02`/`ee03`/`ee04` — build check across boards.
- Flash to the device and visually check the panel — the on-device output
  can't be captured/screenshotted by the assistant, so this step needs a
  human look at the physical panel.

## Out of scope

- The web settings portal (`portal.cpp`, `portal_html.h`).
- The provisioning screen (`net.cpp`'s `showProvisioningScreen()`).
- The rotation setting and `portal.cpp`'s `rotOptions()` labeling logic —
  unaffected; rotation still changes `epaper.width()`/`height()`, which
  still feeds the single adaptive layout above.
