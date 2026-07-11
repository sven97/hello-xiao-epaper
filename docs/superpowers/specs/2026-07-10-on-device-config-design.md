# On-device configuration portal + open-source prep — design

**Date:** 2026-07-10
**Status:** approved by Sven (conversation review, both halves)

## Goal

1. Make every runtime behavior the user cares about (refresh interval, image
   source, quiet hours, timezone, device name, orientation) configurable
   **on the device itself** — no external service, ever. Pressing KEY1
   shows a status page while the ESP32 serves a settings web page on the
   local network.
2. Prepare the repo for open-sourcing (license, README, CI, dependency
   pinning, tests).

## Context and rejected alternatives

- **Hosted portal (TRMNL/BYOS model), rejected:** a self-hosted Node
  portal with device registration and server-side rendering was designed
  first, then dropped. A deep-sleeping device can't be pushed to anyway
  (servers only apply changes when the device polls), so the only real
  loss is couch-config vs walk-up-config. For a photo frame configured
  twice a year, self-contained wins on every stated goal: firmware stays
  clean, setup is "press a button," nothing to host. If a server ever
  becomes desirable, the image-URL setting already is the integration
  point.
- **Extending WiFiManager's portal with custom params, rejected:** generic
  text boxes, awkward validation, settings coupled to a third-party
  library's UI and lifecycle. WiFiManager keeps exactly one job:
  first-time Wi-Fi credential provisioning.

## Architecture

Repo stays firmware-only. Two new modules, following the existing layout:

### `src/settings.*`

- `Settings` struct + `load()`/`save()` over the existing `Preferences`
  namespace (`frame`).
- Defaults exactly match today's behavior. `config.h` keeps true constants
  (pins, panel dims) plus the setting *defaults*; all tunables move behind
  the settings module.
- NVS read failure ⇒ compiled defaults (indistinguishable from first boot).

### `src/portal.*` (status page = config mode)

The former info page and config mode are **one thing**: the status page.
While it is displayed, the settings portal is live. No long-press
gestures anywhere — every button is a short press.

- **KEY1 press** shows the status page: everything the info page shows
  today (wake reason, battery, Wi-Fi, last/next refresh) plus the portal
  URL, numeric IP, QR code (ricmoo/QRCode, MIT), and a compact button
  legend with live state (e.g. "KEY3: unpin — currently pinned").
- **Draw first, network second:** the page renders immediately from
  NVS-cached data (a battery glance stays fast and network-free); Wi-Fi
  join + mDNS (`<name>.local`) + the ESP32 built-in `WebServer` come up
  in the background while the panel refreshes. If Wi-Fi fails, the
  essentials are already on screen.
- `GET /` — single embedded HTML settings page, pre-filled with current
  values. `POST /save` — server-side validation; invalid fields re-render
  the form with an error, nothing partially saved.
- Besides the settings form, the page mirrors device state and actions:
  a **paused checkbox** (KEY3's pin/freeze state — writes the existing
  `held` pref), a **"fetch new picture now"** action, and a
  **"forget Wi-Fi"** action (the KEY2-at-power-on gesture is removed;
  Wi-Fi reset lives only in the portal).
- No saved Wi-Fi, or STA join fails (stale credentials, moved house) ⇒
  fall back to the normal WiFiManager `EE02-Setup` AP provisioning flow.
  This replaces the old hardware forget-Wi-Fi gesture as the recovery
  path.
- **Exit:** pressing KEY1 again, a valid Save on the web page (response
  confirms "saved — device is applying settings"), or a **10-minute
  inactivity timeout** — all three exit the same way: portal stops, a
  normal fetch cycle runs so changes take effect visibly, photo is
  redrawn, device sleeps. The panel never sits showing a dead portal URL.
  A valid save always exits; to adjust again, press KEY1 again.

## Settings surface (v1)

| Setting | Form control | Validation | Default |
|---|---|---|---|
| Refresh interval | dropdown 15 min … 24 h | fixed choices | 1 h |
| Image source URL | text | http(s); `{seed}`/`{width}`/`{height}` tokens optional | current weserv/picsum URL with tokens |
| Quiet hours | two time pickers + enable | hours 0–23, may wrap midnight | off |
| Timezone | auto / manual offset dropdown | ±14 h, 15-min steps | auto (ip-api) |
| Device name | text | `[a-z0-9-]`, ≤ 24 chars; used as mDNS hostname | `ee02` |
| Orientation | dropdown: portrait / portrait-flipped / landscape / landscape-flipped | fixed choices (`setRotation(0–3)`) | portrait |

## Behavior changes

- **Image URL templating:** `{seed}` → random per fetch; `{width}`/
  `{height}` → panel dims *after* rotation, so landscape automatically
  requests 1600×1200. Contract (documented in README): any URL returning a
  baseline (non-progressive) JPEG at the requested size. Default URL:
  `https://images.weserv.nl/?url=picsum.photos/{width}/{height}%3Frandom%3D{seed}&output=jpg`.
- **Orientation:** applied from settings at `epaper.begin()` every boot.
  UI screens (info, provisioning, error, config) switch from hardcoded
  centers to `width()/2`, `height()/2`. `saveFrame`/`loadFrame` store and
  compare the orientation the frame was saved under; mismatch ⇒ treated as
  "no saved frame" (leaving config mode fetches fresh anyway).
- **Quiet hours:** at sleep entry, if the clock is NTP-sane and quiet
  hours enabled: a next timer wake landing inside the window ⇒ sleep until
  window end instead. Timer wakes that still land inside (drift) take the
  existing `quickSleep` fast path. Buttons always wake. Unsynced clock ⇒
  quiet hours silently inactive.
- **Timezone manual mode:** skips the ip-api.com call entirely (privacy +
  offline-friendly). Auto stays default.
- **Button mapping (final, all short presses):** KEY1 = status page +
  portal on/off; KEY2 = new picture; KEY3 = pin/freeze. The status page's
  legend makes the device self-documenting; no README needed at the frame.

## Open-source prep (same milestone)

1. **LICENSE** — MIT at repo root.
2. **README overhaul** — hardware/BOM with links; photo placeholders;
   supported-hardware statement (EE02 + 13.3″ Spectra 6 only); config-mode
   and settings docs; image-URL contract; security caveats (`setInsecure`
   TLS, ip-api plaintext HTTP + non-commercial tier); hobby-project
   support note.
3. **Pin `Seeed_GFX`** to a commit SHA in `platformio.ini`.
4. **CI** — GitHub Actions: `pio run` compile check + native unit tests.
5. **Repo naming** — align README title and repo name (rename to
   `ee02-frame` is Sven's call; GitHub redirects old URLs).
6. **Author email** — history exposes the hotmail address when public;
   accept or switch future commits to GitHub noreply. No history rewrite.
7. **Tag v1.0.0** once this lands.

## Testing

- **Native unit tests** (PlatformIO `native` env, in CI, no hardware):
  quiet-hours window math incl. midnight wrap, URL token substitution,
  settings validation, battery-percent curve. Pure logic is extracted into
  header-only helpers to make this possible.
- **On-hardware checklist** (manual, in the implementation plan): config
  mode entry/exit from sleep and dev mode; save → fetch cycle; orientation
  change end-to-end; quiet-hours sleep duration via logs; first-boot
  provisioning → portal continuation.
