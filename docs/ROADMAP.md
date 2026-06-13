# Ground Truth — Build Roadmap

Status: **Gate 0 complete (scaffold)**. Target: built and gifted within ~2 weeks
of 2026-06-07. BOM largely arriving with the countdown order this week.

This is the single living doc — gate progress, decisions, and the running feature
list. We work conversationally from here rather than against a fixed spec.

**Shipping constraint (decided 2026-06-13):** recipient is in CA, maker in VT — once
handed off, **the maker has no access to the device.** So it must be
**feature-complete and hardened before it leaves VT**; nothing is deferred to
post-ship updates. **OTA is a recipient-operated break-glass valve only** (he runs
it, walked through by phone). Gate 6 ships a one-page "if something goes wrong" card
(reach `groundtruth.local/update`, re-join WiFi via the AP).

## Build gates
- [x] **Gate 0 — Skeleton.** PlatformIO builds + uploads; serial alive; GxEPD2
      renders a hello frame on the GDEY042T81. *(Build verified; upload pending
      hardware.)*
- [ ] **Gate 1 — Connectivity + input.** Captive-portal provisioning, NVS-persisted
      params, auto-AP fallback (simulate dorm move), transient-vs-persistent tuning.
      Also: smart-button driver (debounced GPIO, `INPUT_PULLUP` to GND) — tap vs
      ~3 s hold; hold arms re-provision via a **two-step discrete-frame confirm**
      (hold → static "Change WiFi?" frame → tap to confirm; no live countdown —
      e-paper is too slow to animate one). **WPA2** on the `GroundTruth-Setup` AP
      (not open); **show the WiFi MAC** on the setup screen + use the **stable factory
      MAC** (for campus device registration). See Network & security posture below.
- [ ] **Gate 2 — Seismic data.** HTTPS FDSN fetch, filtered ArduinoJson parse,
      haversine distance/bearing, headline selection. Serial-print only.
      **Defensive parsing** (untrusted input): cap read size, don't trust
      `Content-Length`, no unbounded `String`, fail gracefully on malformed JSON.
- [ ] **Gate 3 — Time + astro.** NTP/TZ, sunrise/sunset, moon phase. Serial-verify.
      **NTP primary + HTTP `Date:`-header fallback** (campus often blocks UDP 123,
      and there's no RTC) — set the clock from the USGS HTTPS response header.
- [ ] **Gate 4 — Display integration.** View system: full-screen pages with a
      **persistent sky footer**, tap-to-flip via the smart button, NVS-saved
      selection, `●○` page indicator. **Ship BOTH pages (map + timeline) — must be
      feature-complete before shipment** (maker loses all access once it's in CA;
      OTA is recipient-only break-glass, not a deployment channel). View swap =
      **partial refresh of the upper region** (footer persists → ~0.5 s, faster than
      a full flash); periodic full refresh clears ghosting. Moon glyph, all required
      states. *(Layout possibly refined with family input once the companion
      countdown gift is in hand.)*
- [ ] **Gate 5 — Web + OTA.** Status page, display mirror (`/api/state` JSON →
      SVG twin), ElegantOTA `/update` with **basic auth** (the one security-critical
      endpoint), mDNS, config/location editor, Change-WiFi. **Only ElegantOTA** as the
      update path — no Arduino/espota (port 3232).
- [ ] **Gate 6 — Gift hardening.** Failure-state polish, watchdog, button
      hold-to-reprovision guard test (accidental-brush safe), LA-override field
      test, **security pass** (no extra listeners, bounded handlers, authed-OTA
      verified), 72 h soak, finalize defaults, version bump to 1.0.0.

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
7. **Switchable views via a smart button** (decided 2026-06-13). See design note below.
   → Gate 1 (button) + Gate 4 (multi-view render).

Scope held to charter §1: no weather/river-gauge/Kp/GPS/RTC/color in v1.

## Display & input design (decided 2026-06-13)
Mockups compared two layout directions (both keep a big-magnitude hero + sky footer):
- **A — radial map** (*where*): Davis-centered rings at 100/200/300 km, quakes as
  dots sized by magnitude. **Chosen as the default page** — most distinctive, never
  looks empty on a quiet day.
- **B — seismograph** (*when*): 7-day strip-chart, each quake a lollipop by
  magnitude; surfaces swarms well, but can read flat on a dead week. **Page 2.**

**Both pages ship complete** (decided 2026-06-13) — recipient in CA, maker in VT
with no remote access after handoff, so nothing is deferred to post-ship OTA.

Rather than cram both into one panel, they are **full-screen pages** sharing a
**persistent sky footer** (time/sun/moon always visible). A `●○` footer indicator
shows there's more than one screen. View swap is a **partial refresh of the upper
region only** (footer untouched, ~0.5 s vs a ~1–2 s full flash). Architecture is a
view registry → room for a Page 3 later (recent-quakes list / astro page).

**Input — single smart button (no EN reset).** One Gebildet 7 mm momentary on a free
GPIO, `INPUT_PULLUP` → GND:
- **Tap** → next view (wraps; saved to NVS so it persists).
- **Hold ~3 s → two-step discrete-frame confirm**, NOT a live countdown (e-paper is
  too slow to animate one — full refresh ~1–2 s, partial ~0.3–0.5 s). Hold draws a
  static "Change WiFi? Tap to confirm · ignore to cancel" frame; a confirming tap
  wipes creds + reboots to portal, else it auto-cancels (~15 s). Two deliberate
  actions = accidental-brush safe.
- No hardware-reset button: the **watchdog** catches hangs and a **power-cycle** is
  the manual reset. (Drops the charter's EN-button backstop by decision.)
- **Input ack on a laggy display:** **no LED** (decided 2026-06-13 — enclosure is
  carried over from the countdown build and stays sealed/clean). The partial-refresh
  redraw of the upper region *is* the acknowledgment that a press registered.

## Network & security posture (decided 2026-06-13)
Priority (Scott's framing): the device must **not be a liability on any network it
joins** — especially the eventual **home** network — and that matters **more than
locking down settings**. Keep day-to-day use **frictionless**.

**Security (don't be exploitable):**
- **OTA is the one security-critical endpoint** — `/update` flashes firmware, i.e.
  remote code execution by design. **Basic-auth it** (creds on the recovery card).
  This is a *security* control, not settings-access control.
- **Only ElegantOTA** as the update path — do **not** also enable Arduino/espota OTA
  (port 3232), an unauthenticated second flash path.
- **Defensive parsing** of all untrusted network data (see Gate 2) — the real
  embedded exploit surface.
- **Minimal surface / no phone-home:** small sync WebServer only; no UPnP/NAT-PMP; no
  telemetry; outbound only to USGS / NTP / IP-geo.
- **WPA2 on the `GroundTruth-Setup` AP** (not an open broadcast during provisioning).
- TLS `setInsecure()` OK for v1 — MITM can feed *wrong data* but can't take the
  device over (display-only data into a bounded parser). Root-CA pin = optional later.
- **Settings/config left open** (low-friction, low-stakes, auto-recoverable) per Scott
  — worst case is griefing a value or a WiFi knock-off, not compromise. Destructive
  actions (Change WiFi / Reboot) *may* share the OTA auth at near-zero friction —
  optional.

**School-network connectivity (likely first home is a UC Davis dorm):**
- **Show the WiFi MAC** (setup screen + web) and use the **stable factory MAC** —
  campus networks usually require device registration by MAC (ResNet-style).
- **NTP-blocked fallback** → clock from the USGS HTTPS `Date:` header (Gate 3); no RTC.
- **Work fully standalone** when client-isolation / blocked mDNS make the web UI
  unreachable; **show the IP on-screen** as a `groundtruth.local` fallback. 2.4 GHz.
- **OPEN RISK — WPA2-Enterprise / eduroam:** our portal collects a **PSK only**. If the
  dorm offers no PSK "device/IoT/ResNet" network, enterprise (PEAP) support is extra
  scope. **Action:** confirm UC Davis dorm IoT onboarding. **Hedge:** set up on home
  WiFi first, then re-provision at the dorm via auto-AP.
- **Pin caveat:** must be a *non-strapping* GPIO (avoid PICO GPIO0/2/12/15) so a
  pressed button at power-up can't change boot mode. I²C/analog pins are good
  candidates (I²C unused in v1). Firmware debounce; keep the lead sane re: EMI.

## Before this repo goes public — personal-data audit
This repo is intended to be shared publicly. Before the push that exposes it:
- Recipient name kept out of committed code (splash text loads from uncommitted
  config or is a build flag set locally).
- Commit author = GitHub noreply (already the global git identity here).
- No secrets committed (`.gitignore` covers `secrets.h`; WiFi is runtime-provisioned).
- Scan **full history**, not just HEAD (gitleaks/trufflehog) before flipping public.
