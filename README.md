# Ground Truth — Seismic Desk Display

An always-on e-paper desk object that shows **live regional earthquake activity**
(USGS) as its hero, with a footer of local time/date, sunrise/sunset, and moon
phase. *Ground and sky:* what's moving below the surface up top, the solar/lunar
cycle below. Built for a geology undergraduate in seismically-active Northern
California — a sealed, set-up-once gift, so robustness and graceful failure matter
more than configurability.

> Codename "Ground Truth" — a geoscience/remote-sensing term for field data that
> validates remote measurements. Provisional; rename freely.

## Hardware
- Adafruit **ESP32 Feather V2** (ESP32-PICO-MINI-02, 8 MB flash)
- **eInk Feather Friend** + FPC cable
- 4.2" **GDEY042T81** e-paper, 400×300 mono
- Gebildet 7 mm panel-mount momentary on **EN/RST** (hardware-reset backstop)
- Continuous USB/mains power; **no RTC, no GPS** (NTP for time)

## Firmware
- **PlatformIO**, Arduino framework, `adafruit_feather_esp32_v2`
- Display: **GxEPD2** (`GxEPD2_420_GDEY042T81`, partial refresh)
- Data: **USGS FDSN** event query (server-side radius filter), ArduinoJson (filtered)
- Time/astro: NTP + POSIX TZ, Dusk2Dawn sunrise/sunset, on-device moon phase
- Connectivity: hand-rolled captive portal + **auto-AP fallback** + **OTA** + status page

See **[docs/ROADMAP.md](docs/ROADMAP.md)** for gate progress, decisions, and the
running feature list. (We work conversationally from here — the roadmap is the only
living doc.)

## Build
```sh
pio run                 # compile
pio run -t upload       # flash over USB (CH9102 bridge; macOS native)
pio device monitor      # 115200 — serial heartbeat
```
OTA (Gate 5+) is WiFi-only: the Feather V2 has no native USB, and the Feather
Friend covers the onboard buttons.

## Status
**Gate 0 — skeleton.** Builds; renders a hello frame; serial heartbeat. Application
logic lands gate by gate (see roadmap).

---
*This repo is intended to be shared publicly. Before going public, run the
personal-data audit in [docs/ROADMAP.md](docs/ROADMAP.md) — names out of committed
code, no secrets, full-history scan.*
