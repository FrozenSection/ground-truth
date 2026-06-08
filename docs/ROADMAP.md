# Ground Truth — Build Roadmap

Status: **Gate 0 complete (scaffold)**. Target: built and gifted within ~2 weeks
of 2026-06-07. BOM largely arriving with the countdown order this week.

This is the single living doc — gate progress, decisions, and the running feature
list. We work conversationally from here rather than against a fixed spec.

## Build gates
- [x] **Gate 0 — Skeleton.** PlatformIO builds + uploads; serial alive; GxEPD2
      renders a hello frame on the GDEY042T81. *(Build verified; upload pending
      hardware.)*
- [ ] **Gate 1 — Connectivity.** Captive-portal provisioning, NVS-persisted
      params, auto-AP fallback (simulate dorm move), transient-vs-persistent tuning.
- [ ] **Gate 2 — Seismic data.** HTTPS FDSN fetch, filtered ArduinoJson parse,
      haversine distance/bearing, headline selection. Serial-print only.
- [ ] **Gate 3 — Time + astro.** NTP/TZ, sunrise/sunset, moon phase. Serial-verify.
- [ ] **Gate 4 — Display integration.** Full layout, partial/full refresh, footer
      cells, moon glyph, all required states. *(Layout design — possibly with
      family input once the companion countdown gift is in hand.)*
- [ ] **Gate 5 — Web + OTA.** Status page, ElegantOTA `/update`, mDNS, Change-WiFi.
- [ ] **Gate 6 — Gift hardening.** Failure-state polish, watchdog, EN-reset test,
      LA-override field test, finalize defaults, version bump to 1.0.0.

Commit per gate; tag at each gate boundary.

## Stack decision (delta from charter)
Charter §3 specs **WiFiManager**. We are instead **porting the proven hand-rolled
captive portal** from the countdown/AirBox builds (ESPAsyncWebServer + DNSServer) —
WiFiManager was the #1 compile risk on countdown and was dropped there. ElegantOTA
runs in async mode (`-D ELEGANTOTA_USE_ASYNC_WEBSERVER=1`). The genuinely new
firmware vs countdown is the **auto-AP fallback + transient/persistent recovery**
state machine (charter §5).

## Agreed feature deltas from charter v0.1
Add **all** of these *if the 400×300 display budget allows* (confirm during Gate 4
layout). They are decisions, not maybes — carried forward intentionally.

1. **Default to manual-Davis, not auto IP-geo.** Ship pointed at Davis; auto is an
   option. Dorm/university/VPN networks geolocate to a datacenter/wrong city.
   *Also:* resolve location once and cache in NVS — the device is stationary, don't
   re-geo every poll. → Gate 1/2. *(seeded in `config.h` DEFAULT_LOC_MODE="manual")*
2. **Headline = most-significant-in-window, not most-recent.** The Geysers field
   inside 300 km produces constant tiny swarms, so most-recent ≈ always trivial.
   → Gate 2. *(seeded: `HEADLINE_MOST_SIGNIFICANT`)*
3. **Liveness on quiet days:** rolling 7-day quake count so the "no events" state
   still feels alive. → Gate 2/4.
4. **Personal high-water mark in NVS:** "Largest since setup: M4.8 · May 3." → Gate 2/4.
5. **Personalization splash** (recipient name + "UC Davis Geology"). Keep the name
   in an **uncommitted** config for the public repo (see audit below). → Gate 4/6.
6. **Distance units default km** (geology/USGS convention; depth already km). → Gate 2/4.
   *(seeded: `config.h` DEFAULT_UNITS="km")*

Scope held to charter §1: no weather/river-gauge/Kp/GPS/RTC/color in v1.

## Before this repo goes public — personal-data audit
This repo is intended to be shared publicly. Before the push that exposes it:
- Recipient name kept out of committed code (splash text loads from uncommitted
  config or is a build flag set locally).
- Commit author = GitHub noreply (already the global git identity here).
- No secrets committed (`.gitignore` covers `secrets.h`; WiFi is runtime-provisioned).
- Scan **full history**, not just HEAD (gitleaks/trufflehog) before flipping public.
