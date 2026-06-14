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
- Gebildet 7 mm panel-mount momentary **smart button** on a free GPIO (tap = switch
  view · hold ≈ 3 s = re-provision WiFi). No EN reset — watchdog + power-cycle cover it.
- Continuous USB/mains power; **no RTC, no GPS** (NTP for time)

## Firmware
- **PlatformIO**, Arduino framework, `adafruit_feather_esp32_v2`
- Display: **GxEPD2** (`GxEPD2_420_GDEY042T81`, partial refresh)
- Data: **USGS FDSN** event query (server-side radius filter), ArduinoJson (filtered);
  distance/bearing, hybrid headline, 24 h/7 d stats, swarm clustering, all-time largest
- Time/astro: NTP + POSIX TZ; **on-device** sunrise/sunset (NOAA equation) + moon phase
- Connectivity: hand-rolled **WPA2** captive portal + **auto-AP fallback**; web
  **display mirror** + **settings/location editor** + authenticated **OTA** at `groundtruth.local`

See **[docs/ROADMAP.md](docs/ROADMAP.md)** for gate progress, decisions, and the
running feature list. (We work conversationally from here — the roadmap is the only
living doc.)

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
**v0.6.1 — data + web layers complete; e-paper panel layouts pending hardware.**
Working today: WPA2 provisioning + auto-AP, smart-button view/reprovision, the live
USGS pipeline (distance/bearing, hybrid headline, 24 h/7 d stats, swarm clustering,
all-time largest), NTP time + sunrise/sunset + moon phase, and the `groundtruth.local`
web mirror, settings/location editor, and authenticated OTA. **Next (Gate 4):** render
the locked layouts on the GDEY042T81 — needs the panel in hand. See the roadmap.

## Colophon
Designed and built with **[Claude Code](https://www.anthropic.com/claude-code)** as
a pair-programming collaborator — architecture, firmware, and display layout were
developed conversationally, gate by gate.
