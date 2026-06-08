# Changelog

All notable changes to Ground Truth firmware. SemVer; PATCH may bump per flash
during multi-flash debug sessions so the on-screen version confirms the binary
took.

## [0.1.0] — 2026-06-07 · Gate 0
- Initial PlatformIO scaffold for Adafruit ESP32 Feather V2.
- GxEPD2 `GxEPD2_420_GDEY042T81` full-buffer instance; static "hello" frame.
- Serial heartbeat (uptime / free heap) for bench liveness.
- Config schema defaults seeded in `include/config.h` (charter §6).
- OTA-capable partition table (`min_spiffs.csv`) set from the start.
