# Changelog

All notable changes to Ground Truth firmware. SemVer; PATCH may bump per flash
during multi-flash debug sessions so the on-screen version confirms the binary
took.

## [0.7.0] — 2026-06-13 · Review hardening, pass 2 (reliability)
- **Trustworthy time** — HTTP `Date:`-header fallback sets the clock when NTP is blocked
  (campus, no RTC). When the clock isn't trustworthy the device + web mirror show a
  **"syncing time"** state instead of 1970 dates / "just now" ages / bogus 24 h counts.
- **Async data race fixed** — seismic results are built locally then **published under a
  mutex**; `/api/state` and `settings::update` take the same lock. This also yields
  **keep-last-good**: a failed/garbled fetch no longer wipes the current dataset.
- **Bounded + validated USGS parse** — reject an oversized response (Content-Length cap)
  and drop any event with non-finite / out-of-range magnitude, coordinates, depth, or
  timestamp.
- **Auto-AP safety** — a previously-provisioned device that loses WiFi now **keeps
  retrying STA and holds its last frame**; it never auto-opens a blocking setup portal
  or auto-erases working creds (a managed-network maintenance window can't strand it).
  Re-provisioning a real move is the deliberate **button-hold / web Change-WiFi**.
- **All-time "Largest" resets** when the configured location moves materially.
- **CI** — GitHub Actions builds both PlatformIO envs on push/PR.

## [0.6.2] — 2026-06-13 · Review hardening, pass 1
External code review (Codex). Bounded fixes applied:
- **Server-side settings validation** — lat/lon/radius/min-mag/poll bounds + TZ
  allowlist; rejects NaN/out-of-range with HTTP 400 + field message; the settings page
  shows the rejection. NVS values are **repaired** (clamped) on boot.
- **"Felt nearby" count** now uses the same **≤50 km** rule as the badge (was any
  felt-in-radius).
- **OTA default-password compile `#warning`** — fails loud until a unique
  `OTA_PASSWORD` is set in `personalization.h` for the gift build.
- Pinned **ElegantOTA to exact 3.1.6**; replaced deprecated `send_P` calls.
- *(Deferred to hardening pass 2 — see ROADMAP Gate 6: trustworthy-time gating + HTTP
  Date fallback, the async config/data race, bounded USGS parsing, auto-AP safety.)*

## [0.6.1] — 2026-06-13 · Web header + label polish
- **Firmware version shown** next to "Ground Truth" in the main page header (dev aid).
- **Header is now sticky** — the title + Settings link stay put when scrolling.
- Labels: **"Largest:"** (colon) and **"Daylight:"** moved in front of the duration.

## [0.6.0] — 2026-06-13 · Settings UX + footer labels (review feedback)
- **"Record" → "Largest"** on the display and settings (clearer).
- **Daylight duration labeled** in the footer (`13h 12m daylight`, was a bare interval).
- **km everywhere** — dropped the miles option (rings + depth were always km, so the
  Hero-only mile conversion was inconsistent). Units selector removed.
- **24-hour setting now also applies to sunrise/sunset**, not just the clock.
- **Settings page friendlier:** time zone is a **named US-zone dropdown** (no POSIX
  string); a **place-name search** (OpenStreetMap/Nominatim) fills lat/lon for you,
  while the fields stay editable.
- **Save no longer stalls** — settings apply **live** (re-TZ + immediate re-fetch),
  no reboot; the page shows "Saved ✓" and refreshes its data. (Reboot/Change-WiFi
  buttons still restart, with clearer copy.)

## [0.5.0] — 2026-06-13 · Swarm clustering + double-ring headline (designer round 2)
- **Swarm collapse:** ≥ 6 events within ~15 km collapse to one `×n` map marker
  (computed in firmware so the panel and web mirror share one source of truth); the
  headline always breaks out as its own dot. Stops The Geysers swarm from ink-blobbing.
  `/api/state` gains a `clusters[]` array and a per-event `cl` flag.
- **Double-ring headline** in the map mirror — distinct from the hollow "shallow"
  glyph so a shallow headline never reads as a plain shallow dot.
- **Descriptor collapse:** 7-day range shows a single `M2.6` when it's one magnitude;
  `1 felt nearby` → `none felt` when none.
- Folds in the designer's production round (Public Sans, WiFi-QR-on-setup, PII
  scrubbed to a build-time placeholder — all already reflected in the spec/firmware).

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
