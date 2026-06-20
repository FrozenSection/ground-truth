# Ground Truth — Seismic Desk Display

An always-on e-paper desk object that shows **live regional earthquake activity**
(USGS) as its hero, with a footer of local time/date, sunrise/sunset, and moon
phase. *Ground and sky:* what's moving below the surface up top, the solar/lunar
cycle below. Built for a geology undergraduate in seismically-active Northern
California — a sealed, set-up-once gift, so robustness and graceful failure matter
more than configurability.

> Codename "Ground Truth" — a geoscience/remote-sensing term for field data that
> validates remote measurements.

## Hardware
- Adafruit **ESP32 Feather V2** (ESP32-PICO-MINI-02, 8 MB flash)
- **eInk Feather Friend** + FPC cable
- 4.2" **GDEY042T81** e-paper, 400×300 mono
- **Adafruit WIZ5500** wired-Ethernet breakout (#6348) on the shared SPI bus — dual-connect
  (WiFi *or* Ethernet) so it works on home WiFi or a managed/dorm network (Ethernet + MAC
  registration). See [docs/WIRING.md](docs/WIRING.md).
- Gebildet 7 mm panel-mount momentary **smart button** on a free GPIO (tap = switch view ·
  hold ≈ 3 s = **Config mode**: the device raises its own AP so the full settings page is
  reachable at `192.168.4.1` from a phone). No EN reset — watchdog + power-cycle cover it.
- Continuous USB/mains power; **no RTC, no GPS** (NTP for time)

> **Living with it / moving it to a dorm:** see [docs/USAGE.md](docs/USAGE.md) — the button,
> reaching settings anywhere via Config mode, and registering the device on campus Ethernet.

## Firmware
- **PlatformIO**, Arduino framework, `adafruit_feather_esp32_v2`
- Display: **GxEPD2** (`GxEPD2_420_GDEY042T81`, partial refresh)
- Data: **USGS FDSN** event query (server-side radius filter), ArduinoJson (filtered);
  distance/bearing, hybrid headline, 24 h/7 d stats, swarm clustering, all-time largest
- Time/astro: NTP + POSIX TZ; **on-device** sunrise/sunset (NOAA equation) + moon phase
- Connectivity: hand-rolled **WPA2** captive portal + **auto-AP fallback**; web
  **display mirror** + **settings/location editor** + authenticated **OTA** at `groundtruth.local`

See **[docs/ROADMAP.md](docs/ROADMAP.md)** for gate progress, decisions, and the
running feature list, and **[docs/DESIGN-HANDOFF.md](docs/DESIGN-HANDOFF.md)** for the
display spec.

## The headline — how the big number is chosen
The hero magnitude is a **hybrid**, not a single fixed window:
1. the most *significant* quake in the **last 24 hours**, if there is one — keeps the
   display current with today's activity;
2. otherwise, the most significant over the **last 7 days** — so a calm day still shows
   the region's biggest recent quake instead of a blank hero.

"Significant" ranks by USGS **significance** (magnitude **plus** felt reports / impact),
with magnitude then recency as tiebreakers — almost always the largest quake, but
occasionally a strongly-felt nearer one outranks a bigger remote one. The hero's
"… ago" line tells you which window the shown event came from (e.g. "2 hours ago" = a
fresh 24 h pick; "4 days ago" = the 7-day fallback). The map/timeline counts beneath it
are plain totals (24 h and 7 d), and **Largest** is the all-time maximum since setup.

> **Plain-language version (for a setup card / PDF):** *the big number is the most
> notable earthquake nearby in the last 24 hours — or the past week, if today was quiet.*

## Build
```sh
pio run                 # compile (real e-paper build)
pio run -t upload       # flash over USB (CH9102 bridge; macOS native)
pio run -e headless     # serial-preview build (no panel) — exercises WiFi + web
pio device monitor      # 115200
```
After the first USB flash, updates can go **over WiFi** via ElegantOTA at
`groundtruth.local/update` (upload `.pio/build/<env>/firmware.bin`).

## Status
**v0.11.0 — display + connectivity complete and hardware-verified; on to the hardening pass.**
Three on-panel views (Map · Timeline · Info) render in Public Sans on the GDEY042T81; the full
USGS pipeline + NTP/astro + `groundtruth.local` web mirror / settings / OTA all work; and
**dual connectivity is live** — WiFi *and* wired Ethernet (W5500), with a button-hold **Config
mode** that makes settings reachable on any network and a WiFi on/off toggle for dorm use.
**Remaining:** the Gate 6 hardening tail (watchdog, soak test, recovery card) → 1.0.0. See the
roadmap.

## Colophon
Designed and built with **[Claude Code](https://www.anthropic.com/claude-code)** as
a pair-programming collaborator — architecture, firmware, and display layout were
developed conversationally, gate by gate.
