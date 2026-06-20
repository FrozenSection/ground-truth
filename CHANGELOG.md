# Changelog

All notable changes to Ground Truth firmware. SemVer; PATCH may bump per flash
during multi-flash debug sessions so the on-screen version confirms the binary
took.

## [0.8.10] — 2026-06-20 · Accurate moon illumination + age
- **Moon illumination/age upgraded to Meeus** (Astronomical Algorithms ch. 48). The old
  mean-synodic model assumed a uniform cycle rate and ignored the Sun/Moon orbital
  anomalies, so it read up to ~7–8 points low near the crescents (e.g. 24% when the true
  value was ~31%). Now within ~1–2% of any almanac — illuminated fraction, age, and phase
  name all derive from the true phase angle. Still pure on-device math; no network.

## [0.8.9] — 2026-06-20 · Moon glyph: almanac convention (inverted)
- **Moon now inks the shadow, not the light.** Previously the *illuminated* fraction was
  filled black — backwards from reality and from every almanac/weather glyph (it made a
  full moon a solid blob and a new moon an empty circle). Now the **unlit fraction is
  inked and the lit limb stays paper-white**, so the glyph reads like the sky: full = open
  disc, new = solid black, crescent = bright sliver on the lit limb. Outline + terminator
  geometry unchanged. Web mirror inverted to match (black disc, lit region painted to
  paper). Spec §8 updated.

## [0.8.8] — 2026-06-19 · Footer/map polish
- **Moon name** shifted right (and disc nudged left) for ~10px of space off the disc.
- **Ring labels** tightened to "100km / 200km / 300km" (no space before the unit).
- **Stat labels aligned.** "IN 24 H" and "IN 7 DAYS" now share one x (264). The big count
  numeral auto-shrinks only if it would reach the label (a 2-digit count stays 36px; a
  capped "100+" drops a size), so the labels stay aligned regardless of the count width.

## [0.8.7] — 2026-06-19 · Footer fixes: moon overflow, stray "t", ring labels
- **Moon name no longer overflows** (and the stray "t" is gone — same bug). Long phase
  names ("Waxing crescent" = 15 ch) ran past the right edge; with Adafruit GFX text-wrap
  on by default, the overflowing "t" wrapped around to x0 on the next line, landing next
  to the date in the footer's left cell. Fixed by **disabling text-wrap globally** (a
  fixed-layout panel should clip, never wrap to the far edge) and **rendering the phase
  name at 9px** so all eight names fit the cell (longest measures 80px of 88; ellipsize
  guards the edge regardless).
- **Map ring labels fanned apart.** 100 / 200 / 300 km were stacked on one diagonal and
  ran together; they now sit on their own ring at SW / S / SE (white-knockout, legible).

## [0.8.6] — 2026-06-19 · Settings page tidy-up
- **Reordered sections** to Location → Diagnostics → Firmware → Actions (diagnostics were
  awkwardly first; they now sit below the thing you actually edit).
- **Renamed "Location & behavior" → "Location"** (it's really just location settings).
- **Spaced the green "Saved ✓"** off the Save button (`.msg` margin-left).

## [0.8.5] — 2026-06-19 · Display: Quiet caption spacing
- In the **Quiet** state, "No events in range" and the date were crammed against the big
  54px "Quiet" (both anchored at x150). They're now anchored to the measured right edge of
  "Quiet" plus a 22px gap, so the spacing holds regardless of font metrics.

## [0.8.4] — 2026-06-14 · Web mirror shows both pages
- **The mirror can now show Page 2 (Timeline), not just the Map.** Added a **Map / Timeline
  toggle** above the panel; the SVG renders whichever page you pick. It **defaults to the
  page the device is currently showing** and displays a **"Device is showing: …"** note so
  you always know the physical panel's state.
- The toggle is **mirror-local** — browsing pages on the web does **not** change the
  device (the panel's page stays driven by its button), so opening the mirror on a phone
  won't flip what the recipient is looking at.
- `/api/state` gained `now` (device clock) and per-event `t` (epoch) so the browser can
  place the timeline lollipops on the same 7-day window the device uses. The mirror's
  `render()` was refactored into shared hero/footer + per-page Map/Timeline draws, keeping
  it a faithful twin of `src/display.cpp`.

## [0.8.3] — 2026-06-14 · Settings: Enter = Find, consistent geocode feedback
- **Enter in the search box now runs Find**, not a silent Save. Previously the place box
  was inside the form, so Enter submitted (Save) — which moved the location but only
  updated the "Saving…" message, never the "Found: …" search subtext, so it looked like
  nothing was searched. Enter now geocodes and updates the subtext + coordinates + label;
  Save remains the deliberate commit. The Save path also refreshes that subtext whenever
  it geocodes, so feedback is the same however you trigger it.

## [0.8.2] — 2026-06-14 · Settings: couple geocode to Save; editable verified label
- **Search and Save are now coupled.** Previously the place box was just a label saved as
  text — you could type "davis", Save, and the footer label changed while the coordinates
  never moved. Now **Save geocodes the place** (if it changed since the last resolve),
  sets lat/lon to match, and only then writes — so the label can never drift from where
  the device is actually watching. A failed lookup (typo / no match) **refuses to save**
  with a clear message instead of keeping stale coordinates. "Find" is now an optional
  preview.
- **Editable, verified location label.** A new **Display label** field auto-fills a clean
  "Town, ST" name derived from the geocode (via Nominatim `addressdetails` +
  `ISO3166-2-lvl4`), and you can override it (e.g. "Home"). A custom label is preserved
  across further searches; the device footer + `/api/state.loc.name` show it.

## [0.8.1] — 2026-06-14 · Fix: false "Quiet" after boot
- **Re-fetch the instant the clock becomes trustworthy.** The first USGS fetch can run
  before the clock is synced (it bootstraps time from the HTTPS `Date:` header). That
  pre-sync query omits the 7-day `starttime`, so its window is wrong — and on-device it
  came back empty, leaving a **false "Quiet"** that persisted until the next 10-minute
  poll (so every power-cycle showed an empty map for up to 10 min, even with real recent
  activity). Now, the moment time is known, the device forces a re-fetch with a proper
  7-day window. *Verified live: boot → Date-header sync → auto re-fetch → M3.4 headline,
  24 h 2 / 7 d 8, with no manual save.* Also dropped a stale "Gate 2" boot banner.

## [0.8.0] — 2026-06-14 · Gate 4 — the real e-paper layouts
- **Page renderer** (`src/display.cpp`) — the locked designer frames, drawn on GxEPD2:
  - **Page 1 "Map"** — √-scale radial map (rings at R⁄3 · 2R⁄3 · R, **radius-driven**
    white-knockout labels), magnitude-sized dots with a **depth cue** (hollow <8 km /
    filled ≥8 km), **double-ring headline**, **swarm `×n` collapse**, and the B-tight
    stat column (24 h / 7 d counts, felt, mag range, depth key, all-time **Largest**).
  - **Page 2 "Timeline"** — 7-day **lollipop strip chart** with M2/M3/M4 gridlines,
    weekday letters, magnitude-scaled stalks, ringed/labeled headline, folded stat line.
  - **Shared hero** (magnitude / 2-line place / depth+distance / recency / significance
    badge / `●○` page dots / **offline glyph**) and the **persistent Sky Footer**
    (clock, date, **home-pin monitoring location**, sun rise/set + daylight, **scanline-
    filled moon**).
  - **States** — boot splash, unified **Connect** screen (WiFi-join **QR** + both WiFi/
    Ethernet MACs for managed-network registration), **Acquiring time**, **Reconnecting**,
    **Stale**, **Quiet**, and the **two-step Change-WiFi confirm** (3 s hold → tap).
- **Typography** — **Public Sans** embedded as eight on-device bitmap weights generated
  by `tools/genfonts.sh` (Adafruit `fontconvert` patched to **DPI 72** so the size
  argument equals the spec's CSS-px em). A UTF-8 `·`→drawn-dot shim renders the mid-dot
  separator without carrying Latin-1 in every glyph table.
- **Footer clock partial refresh** — the clock ticks each minute via a no-flash partial
  refresh of the footer band; full renders happen only on poll / view / connectivity
  change (and the instant the clock first becomes trustworthy).
- **`loc.name` plumbed** — the geocode label is stored in NVS and shown in the footer +
  `/api/state` (`loc.name`), with a `lat, lon` fallback for hand-entered coords; the web
  mirror gained the footer location line and the hero offline glyph to stay a true twin.
- *Known cosmetic deferrals (ASCII-only font): swarm count renders `xN` not `×N`, and the
  magnitude range uses `-` not an en-dash. On-panel legibility re-shoot still pending.*

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
