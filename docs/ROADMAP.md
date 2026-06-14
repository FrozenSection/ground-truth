# Ground Truth — Build Roadmap

Status (**v0.6.1**, 2026-06-13): **Gates 0–3 + 5 substantially done on a bare Feather
(verified live against real USGS data); Gate 4 — the e-paper panel layouts — is the
remaining big piece, gated on hardware.** Target: built and gifted within ~2 weeks of
2026-06-07.

This is the single living doc — gate progress, decisions, and the running feature
list. We work conversationally from here rather than against a fixed spec.

**Shipping constraint (decided 2026-06-13):** recipient is in CA, maker in VT — once
handed off, **the maker has no access to the device.** So it must be
**feature-complete and hardened before it leaves VT**; nothing is deferred to
post-ship updates. **OTA is a recipient-operated break-glass valve only** (he runs
it, walked through by phone). Gate 6 ships a one-page "if something goes wrong" card
(reach `groundtruth.local/update`, re-join WiFi via the AP).

## Build gates
- [x] **Gate 0 — Skeleton.** PlatformIO builds; GxEPD2 driver class wired. *(tagged `gate-0`)*
- [x] **Gate 1 — Connectivity + input.** WPA2 captive portal (ported from countdown/
      AirBox), NVS params, **auto-AP fallback**, transient-vs-persistent reconnect;
      smart-button driver (GPIO 26, tap = next view / hold ≈ 3 s = re-provision);
      WiFi **MAC** shown for campus registration; view-state in NVS. *(verified on a
      bare Feather via the `headless` env. The hold's two-step on-screen confirm UI
      lands with the Gate 4 display.)*
- [ ] **Gate 1b — Ethernet (W5500), dual-connect *(needs the board)*.** Bring up W5500
      over SPI (`ETH`); `online = WiFi || Ethernet`; expose both MACs; portal exits when
      Ethernet links; unified Connect screen. See Network posture below.
- [x] **Gate 2 — Seismic data.** HTTPS FDSN fetch, filtered/streamed ArduinoJson parse
      (defensive), haversine distance/bearing, **hybrid headline**, 24 h/7 d counts +
      histogram + range + felt, **swarm clustering** (≥6 within 15 km → `×n`), NVS
      all-time **largest**. *(live-verified against real USGS data.)*
- [x] **Gate 3 — Time + astro.** NTP/TZ, **on-device NOAA** sunrise/sunset, moon phase
      (12/24 h honored), **HTTP `Date:`-header fallback** when NTP is blocked, and
      **time-quality gating** (no derived fields shown until the clock is trustworthy).
- [ ] **Gate 4 — Display integration *(the remaining big piece — needs the panel)*.**
      Port the locked layouts to GxEPD2: **B-tight Map** + **lollipop Timeline** + all
      states (incl. the WiFi **QR**), the view system (full-screen pages, persistent
      sky footer, `●○` indicator, tap-to-flip via partial refresh), moon/sun glyphs.
      **Both pages ship complete** (maker loses access in CA). The web mirror is the
      pixel reference.
- [x] **Gate 5 — Web + OTA.** `/` display mirror (`/api/state` JSON → SVG twin),
      `/settings` diagnostics + **location/behaviour editor** (named-TZ dropdown,
      place-name geocode, live-apply no-reboot), **ElegantOTA `/update` with basic
      auth** (only update path; no espota), mDNS, Reboot / Change-WiFi. *(km-only,
      sticky header, version badge added per review.)*
- [~] **Gate 6 — Gift hardening.** **Done (review pass 2):** trustworthy-time gating +
      Date fallback, async data-race mutex + keep-last-good, bounded/validated USGS
      parse, auto-AP safety (no blocking portal / no auto-erase), settings validation,
      OTA-password warning, CI. **Remaining:** task watchdog + reset-reason logging,
      button hold→confirm UI (with Gate 4), distant-location field test, 72 h soak with
      forced WiFi/DNS/NTP/USGS/power failures, recovery card, → 1.0.0.

Commit per gate. (Gates landed somewhat out of order — web/OTA came early to preview
real data; only Gate 4 and the Gate 3 leftover remain before hardening.)

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
2. **Headline = HYBRID** — most-significant in the last 24 h, else most-significant
   over 7 d. Real data showed plain 7-d-significant could pin a 6-day-old hero; the
   24 h-first rule keeps it fresh. → **DONE** (`HEADLINE_HYBRID` in config.h).
3. **Liveness on quiet days:** rolling 7-day quake count so the "no events" state
   still feels alive. → Gate 2/4.
4. **Personal high-water mark in NVS:** "Largest since setup: M4.8 · May 3." → Gate 2/4.
5. **Personalization splash** (recipient name + "UC Davis Geology"). Keep the name
   in an **uncommitted** config for the public repo (see audit below). → Gate 4/6.
6. **km everywhere** — the miles option was dropped entirely (rings + depth are km, so
   a Hero-only mile conversion was inconsistent). → **DONE**.
7. **Switchable views via a smart button** (decided 2026-06-13). See design note below.
   → Gate 1 button **DONE**; Gate 4 multi-view render pending.
8. **All-time "Largest" + swarm `×n` collapse + double-ring headline** (from real data /
   designer round 2). → **DONE** in firmware; render on panel at Gate 4.

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

### Locked from designer mockups (2026-06-13)
Designer delivered Hero + SkyFooter as parametric components plus 3 variants per
page, all system states, and a type spec. Chosen:
- **Page 1 = Map "B-tight"** (depth-coded dots; the 24 h / 7 d numerals filled out with
  right-hand descriptors). Designer delivered production renders (round 2, 2026-06-13)
  adding a **double-ring headline** + **swarm `×n` collapse** — both now in firmware.
  Labels since refined to **"Largest:"** and **"Daylight:"**; km-only.
- **Page 2 = Timeline variant A** (magnitude lollipop strip chart). Beats B (filled
  spikes blob at 1-bit) and C (daily columns lose within-day timing).
- **√ (square-root) ring scale** on the map — 100 km lands at ~34 px instead of 19 px,
  so near-home events (the ones that matter) get room. Good call; adopted.
- **Footer approved**; **12/24-hour clock is a setting** (`clock_24h`) that applies to
  **both the clock and the sunrise/sunset times**. The third sun line is labeled
  **"Daylight: …"**. The SkyFooter splits time + am/pm, so 24 h just blanks the suffix.
- **System states approved** (boot splash, setup, stale, quiet/no-events, confirm-WiFi).
  **Setup screen ADDS a WiFi QR** — port `ricmoo/QRCode` from countdown; encode
  `WIFI:T:WPA;S:GroundTruth-Setup;P:<AP_PASS>;;` (works because the setup AP is WPA2
  with a known password — scan joins it, captive portal then pops). Keep 192.168.4.1 +
  auto-pop as the reliable fallback; don't over-promise a typed `setup.ground.truth`.
- **Type = Public Sans** (open-licence, Helvetica metrics) — fontconvert one TTF at
  the ~6 spec sizes (54/27/24–36/15/13/9–11). The 8.5 px micro-labels sit at the GFX
  comfort floor; bump to 9–10 px if they read tight on the panel.
- **Personalization:** the splash recipient line is PERSONAL DATA — loaded from
  gitignored `include/personalization.h` (`RECIPIENT_SPLASH`), never committed; a
  generic fallback ships in the repo. The designer's mockup HTML hardcodes the name,
  so **scrub it before any repo inclusion**.
- Open: depth-as-fill cue (Map B) loads a 3rd encoding onto each dot — tune the
  shallow/deep threshold to NorCal (most crustal quakes <15 km) or treat as optional.

Implementation maps cleanly to Gate 4: Hero/SkyFooter → display functions; moon
terminator x-radius = R·|1−2·illum|, lit-right when waxing (from the SkyFooter logic).

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
- **RESOLVED (2026-06-13) — go DUAL-CONNECTED: WiFi *and* wired Ethernet (W5500).**
  Background: UC Davis dorm WiFi is **eduroam-only (802.1x)** — IoT/console devices
  can't use it; the sanctioned path is **wired Ethernet (ResNet) + MAC registration**
  (jack in every bedroom). Rather than choose, the device does **both**:
  **home/apartment → WiFi; managed/dorm → Ethernet + MAC register.** No
  WPA2-Enterprise/PEAP, no NetID password stored, works anywhere.
  **New work this creates:**
  - *Firmware:* bring up the W5500 via **arduino-esp32 `ETH` (SPI, lwIP-integrated)** so
    the existing HTTPS fetch routes over either interface transparently — **NOT** the
    standalone Arduino Ethernet library (separate stack; can't share our TLS/HTTP code).
    `online = WiFi || Ethernet`; expose **both MACs** (`WiFi.macAddress()` +
    `ETH.macAddress()`, both efuse-stable); captive portal exits when Ethernet links;
    unified Connect screen (DISPLAY-SPEC §10.9).
  - *Hardware (board chosen 2026-06-13):* **Adafruit WIZ5500 breakout #6348** — plain
    SPI W5500 ("co-processor" = its built-in TCP/IP stack), on-board level-shifting +
    RJ45 link/act LEDs. **Breakout, not a stacked Wing** (cleaner with the eInk Friend's
    FPC; flexible RJ45 placement in the custom case). Shares SPI (SCK 5 / MOSI 19 /
    MISO 21); **proposed control pins CS=GPIO4 (A5), INT=25 (A1), RST=22 (SDA)** — avoid
    EPD 15/33/32/14, **EPD BUSY 27** (hand-wired), button 26, strapping 0/2/12/15,
    input-only 34/36/39 (SCL 20 stays free). Power the breakout from **5 V** (its own
    reg) to spare the 3.3 V rail. RJ45 enclosure cutout. *Bench-validate: SPI sharing +
    dual-stack default route.*

  **Wiring done 2026-06-13:** EPD **BUSY → GPIO27** (D11 position, like countdown) and
  the smart **button → GPIO26 (A0) ↔ GND** (`INPUT_PULLUP`). `EPD_BUSY` set to 27 in
  config.h.
  Firmware/display work continues in parallel; the Ethernet bring-up waits on the board.
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
