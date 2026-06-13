# Changelog

All notable changes to Ground Truth firmware. SemVer; PATCH may bump per flash
during multi-flash debug sessions so the on-screen version confirms the binary
took.

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
