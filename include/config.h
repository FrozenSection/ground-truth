#pragma once
// Ground Truth — Seismic Desk Display · build-time configuration
// Runtime-tunable settings (location, radius, etc.) move to NVS/Preferences
// once Gate 1 lands; the DEFAULT_* values below seed that schema (charter §6).

// ---- Firmware version (SemVer) ----
// Bump PATCH on every flash during multi-flash debug sessions so the on-screen
// version confirms the binary actually took (see memory: firmware-version-bumps).
#define FIRMWARE_VERSION "0.1.0"   // Gate 0 — skeleton / hello frame

// ---- Identity ----
#define PROJECT_NAME   "Ground Truth"
#define AP_SSID        "GroundTruth-Setup"
#define MDNS_HOST      "groundtruth"          // -> groundtruth.local

// ---- E-paper wiring (eInk Feather Friend + GDEY042T81) ----
// Pin map proven on the sibling countdown build. RST/BUSY tied on the Friend,
// so both are -1 (GxEPD2 falls back to timed waits instead of a BUSY line).
#define EPD_CS    9
#define EPD_DC    10
#define EPD_RST   -1
#define EPD_BUSY  -1

// ---- Config-schema defaults (charter §6) — wired up from Gate 1 on ----
#define DEFAULT_LOC_MODE   "manual"   // ship pointed at Davis; auto IP-geo is the option,
                                      // not the default (dorm/VPN geolocates badly).
#define DEFAULT_LAT        38.5449f   // UC Davis
#define DEFAULT_LON        -121.7405f
#define DEFAULT_RADIUS_KM  300
#define DEFAULT_MIN_MAG    2.5f
#define DEFAULT_UNITS      "km"       // geology/USGS convention; depth is already km
#define DEFAULT_CLOCK_24H  false
#define DEFAULT_TZ         "PST8PDT,M3.2.0,M11.1.0"
#define DEFAULT_POLL_MIN   10

// ---- Headline rule (charter §10, decided) ----
// Hero = most-significant event in the window, not most-recent: 300 km of Davis
// includes The Geysers (constant tiny swarms), so most-recent ≈ always trivial.
#define HEADLINE_MOST_SIGNIFICANT  1
