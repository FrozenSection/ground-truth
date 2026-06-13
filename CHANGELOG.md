# Changelog

All notable changes to Ground Truth firmware. SemVer; PATCH may bump per flash
during multi-flash debug sessions so the on-screen version confirms the binary
took.

## [0.4.1] — 2026-06-13 · Hybrid headline
- **Headline rule = hybrid:** most-significant event in the **last 24 h** (keeps the
  hero fresh when there's recent activity), falling back to most-significant over the
  **7-day** window when the last day is quiet. Replaces plain most-significant/7d,
  which could surface a several-days-old event as the hero.

## [0.4.0] — 2026-06-13 · Settings page, OTA, sunrise/sunset
- **Web split:** `/` is now just the display mirror + recent-events table; a new
  **`/settings`** page holds diagnostics (firmware, signal, IP/MAC, uptime, record),
  a **location/behaviour editor** (lat/lon, radius, min-mag, units, poll, 12/24 h, TZ),
  and **actions** (reboot, change WiFi). Frees the main page for the data.
- **OTA:** ElegantOTA at `/update`, **basic-auth** (the one security-critical endpoint;
  `OTA_PASSWORD` overridable via gitignored `personalization.h`).
- **Sunrise/sunset:** NOAA sunrise equation computed on-device (no library — Dusk2Dawn
  won't build under this toolchain). Footer now shows real `↑ rise / ↓ set / daylight`.
- **Record** label spelled out (was "REC"); **badge** fix — "felt nearby" now requires
  ≤50 km, else "felt" / "significant" / "alert".
- Verified live against real USGS data on the Feather (`groundtruth.local`).

## [0.3.0] — 2026-06-13 · Gate 2 (seismic data + web mirror) — compiles, field-test pending
- **USGS FDSN** query (7-day window, limit-capped) over HTTPS; **filtered, streamed
  ArduinoJson** parse (only the displayed fields); defensive against malformed input.
- **Haversine distance + bearing** from home; **headline = most-significant** event;
  aggregates: 24 h / 7 d counts, magnitude histogram, mag range, felt count.
- **All-time record** high-water mark persisted in NVS.
- **NTP time** (needed for recency/windows) + relative-time formatting; **moon phase**
  computed on-device (location-independent; sunrise/sunset still Gate 3).
- **`settings`** module: location/radius/min-mag/units/poll/tz from NVS (manual-Davis).
- **Web mirror:** `AsyncWebServer` serving `/api/state` JSON and a live, 1-bit
  **B-tight mirror page** at `groundtruth.local` (hero + √-scale map + stats + footer)
  plus a status panel and a recent-events table. Auto-refreshes.
- Flash the `headless` build to a bare Feather to see real data on the web page
  before the panel arrives.

## [0.2.0] — 2026-06-13 · Gate 1 (connectivity + input) — compiles, field-test pending
- **Pin-map fix:** corrected EPD pins to the real ESP32 Feather V2 GPIOs
  (CS=15, DC=33, SRAM_CS=32/SD_CS=14 held HIGH) — Gate 0 carried the SAMD
  header-position numbers (9/10), which would not drive the panel.
- **Captive portal** ported from the countdown/AirBox build (ESPAsyncWebServer +
  DNSServer), rebranded; **WPA2 setup AP** + device **MAC** shown on the portal
  (`/info`) and the e-paper setup screen for campus device registration.
- **Smart button** driver (GPIO 26, `INPUT_PULLUP`): tap = next view (persisted),
  hold ≥ 3 s = re-provision.
- **View-state** persisted in NVS (map / timeline) — plumbing for Gate 4 layouts.
- **Connect / reconnect supervision:** auto-AP on a sustained move (stored SSID
  absent), transient blips keep retrying with periodic full-radio restarts; mDNS
  up/down tracking.
- **`headless` env** (`-DHEADLESS_DISPLAY`) renders frames to serial — exercises
  all of the above on a bare Feather before the panel arrives.

## [0.1.0] — 2026-06-07 · Gate 0
- Initial PlatformIO scaffold for Adafruit ESP32 Feather V2.
- GxEPD2 `GxEPD2_420_GDEY042T81` full-buffer instance; static "hello" frame.
- Serial heartbeat (uptime / free heap) for bench liveness.
- Config schema defaults seeded in `include/config.h` (charter §6).
- OTA-capable partition table (`min_spiffs.csv`) set from the start.
