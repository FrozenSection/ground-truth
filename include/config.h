#pragma once
// Ground Truth — Seismic Desk Display · single source of truth for identity,
// pins, network, and tuning. Change values here, not inside module .cpp files.
// Runtime-tunable settings (location, radius, …) live in NVS; the DEFAULT_*
// values below seed that schema.

// ---- Firmware version (SemVer) ----
// Bump PATCH on every flash during multi-flash debug so the boot banner / About
// screen confirms the binary took. MINOR per gate/feature.
#define FIRMWARE_VERSION "0.13.0"  // configurable USGS endpoint, prefer-Ethernet route, scan cleanup, intl time zones

// ---- Identity ----
#define PROJECT_NAME   "Ground Truth"
#define AP_NAME        "GroundTruth-Setup"   // captive-portal SSID when unprovisioned
#define AP_PASS        "groundtruth"          // WPA2 (>=8 chars) — printed on the card,
                                              // so the setup AP isn't an open broadcast
#define AP_IP_STR      "192.168.4.1"
#define MDNS_HOSTNAME  "groundtruth"          // -> http://groundtruth.local
#define WEB_PORT       80

// ---- Recipient personalization (boot splash) ----
// The recipient's name is PERSONAL DATA and must never be committed to this
// public repo. Copy include/personalization.example.h -> include/personalization.h
// (gitignored) and define RECIPIENT_SPLASH there. Absent that file, the splash
// falls back to a generic tagline.
#if __has_include("personalization.h")
#include "personalization.h"
#endif
#ifndef RECIPIENT_SPLASH
#define RECIPIENT_SPLASH "Live regional seismicity"
#endif

// ---- OTA credentials (settings-page firmware update) ----
// /update is the one security-critical endpoint (it flashes firmware) so it's
// authenticated. Override OTA_PASSWORD in the gitignored personalization.h for the
// gift build — the committed default is a weak placeholder, not a secret.
#ifndef OTA_USERNAME
#define OTA_USERNAME "admin"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "groundtruth"
#ifdef GIFT_BUILD
#error "GIFT_BUILD with the DEFAULT OTA password — set a unique OTA_PASSWORD in include/personalization.h before shipping."
#else
#warning "Using the DEFAULT OTA password — set a unique OTA_PASSWORD in include/personalization.h before the gift build (build with -DGIFT_BUILD to enforce)."
#endif
#endif

// ---- E-paper wiring (ESP32 Feather V2 + eInk Feather Friend) ----
// CAREFUL: the Friend's silk/docs name Feather header POSITIONS (D9/D10/D6/D5),
// which equal those GPIO numbers only on SAMD Feathers. On the ESP32 Feather V2
// the same positions carry DIFFERENT GPIOs. Map below is the one proven on the
// sibling countdown build (see that repo's docs/pinmap.md) — do not "simplify"
// back to 9/10. The Friend ties EPD RST to the Feather reset line; BUSY is
// unwired unless hand-soldered, so it defaults to -1 (GxEPD2 uses timed waits).
#define EPD_CS    15   // Friend ECS  (header position "D9")
#define EPD_DC    33   // Friend DC   (header position "D10")
#define EPD_RST   -1
#define EPD_BUSY  27   // Friend BUSY pad hand-wired to the D11 header position (=GPIO27),
                       // matching the countdown build (faster, more reliable refresh).
                       // Set back to -1 if the wire is ever absent (GxEPD2 then uses
                       // timed waits). NOTE: 27 is now taken — keep the W5500 off it.
#define SRAM_CS   32   // Friend SRAM CS (position "D6") — unused; held HIGH
#define SD_CS     14   // Friend SD  CS (position "D5") — unused; held HIGH
// SCK/MOSI use hardware SPI defaults (GPIO 5 / 19).

// ---- Smart button (single panel-mount momentary) ----
// GPIO 26 (Feather A0 position) -> button -> GND, INPUT_PULLUP (pressed = LOW).
// 26 is NOT a strapping pin (safe at boot). Tap = next view; 3 s hold = Config mode.
#define BUTTON_PIN          26
#define BUTTON_DEBOUNCE_MS  50
#define BUTTON_HOLD_MS      3000   // hold >= this -> open Config mode (AP-reachable settings)

// ---- W5500 wired Ethernet (Gate 1b) — shares the e-paper SPI bus (SCK5/MISO21/MOSI19),
// adds its own control pins. Full wiring + wire colours in docs/WIRING.md. ----
#define W5500_CS      4    // A5
#define W5500_IRQ     25   // A1
#define W5500_RST     22   // SDA
#define W5500_SPI_MHZ 8    // 8 MHz — faster was unreliable on this board (per the NetPulse build)

// ---- Views (tap cycles through these; selection persists in NVS) ----
#define VIEW_MAP        0
#define VIEW_TIMELINE   1
#define VIEW_INFO       2   // big clock + device diagnostics (MAC/IP/version) — no web needed
#define VIEW_COUNT      3

// ---- Connectivity supervision ----
#define WIFI_CONNECT_TIMEOUT_MS  20000UL
#define RECONNECT_EVERY_MS       30000UL
#define TICK_INTERVAL_MS         1000UL
// Config mode (button-hold AP) auto-closes after this idle window so the device can't be
// left broadcasting its setup AP indefinitely.
#define CONFIG_AP_TIMEOUT_MS     (10UL * 60 * 1000)

// ---- Health / hardening (Gate 6) ----
// Task-watchdog timeout: must clear our longest blocking loop call (the ~15-17 s USGS
// fetch) with margin, while still rebooting a genuine hang promptly.
#define WDT_TIMEOUT_MS           30000UL
#define HEALTH_LOG_EVERY_MS      (5UL * 60 * 1000)   // periodic heap/uptime line for the soak log

// ---- Time ----
#define NTP_SERVER_1   "pool.ntp.org"
#define NTP_SERVER_2   "time.nist.gov"

// ---- Config-schema defaults (NVS) — wired up from Gate 2 on ----
#define DEFAULT_LOC_MODE   "manual"   // ship pointed at Davis; auto IP-geo is the option
#define DEFAULT_LAT        38.5449f   // UC Davis
#define DEFAULT_LON        -121.7405f
#define DEFAULT_RADIUS_KM  300
#define DEFAULT_MIN_MAG    2.5f
#define DEFAULT_UNITS      "km"       // geology/USGS convention; depth already km
#define DEFAULT_CLOCK_24H  false
#define DEFAULT_TZ         "PST8PDT,M3.2.0,M11.1.0"
#define DEFAULT_POLL_MIN   10

// Hero = HYBRID: most-significant in the last 24 h (fresh when active), else
// most-significant over the 7-day window (quiet days still show the biggest recent
// event). The Geysers swarms make plain most-recent ≈ trivial. (seismic.cpp)
#define HEADLINE_HYBRID  1

// Swarm collapse (designer rule): a tight cluster of >= N events within R km
// collapses to one "×n" map marker so a swarm (e.g. The Geysers) doesn't ink-blob.
// The headline always breaks out as its own dot; the cluster counts the remainder.
#define SWARM_MIN_COUNT   6
#define SWARM_RADIUS_KM   15.0f

// Reject a USGS response larger than this (defensive vs an oversized/hostile body —
// TLS is setInsecure for v1). A legit 100-event geojson is well under this.
#define MAX_RESP_BYTES    600000

// USGS FDSN event endpoint. Stored in NVS (settings.fdsnUrl) so it can be corrected from the
// web UI WITHOUT a reflash if USGS ever moves it — the one long-term break the maker can't
// otherwise fix after gifting. The /fdsnws/event/1/ path is an FDSN standard, stable ~10 yr.
#define DEFAULT_FDSN_URL  "https://earthquake.usgs.gov/fdsnws/event/1/query"
